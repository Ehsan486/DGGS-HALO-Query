// Real world halo benchmark with stage decomposition, additional CPU
// baselines, and CUDA tuning/ablation switches.
//
// Backends (comma separated in --backend): cpu (serial unordered_set
// reference), parallel (20 thread partitioned frontier), flat (serial flat
// table, same algorithm as the GPU), atomic (multicore atomic flat table),
// gpu (CUDA), auto.  Legacy tokens both, parallel-gpu and all still work.
// The first backend listed is the correctness reference for the others.

#include "bafe/bafe.hpp"
#include "bafe_exp/benchmark_common.hpp"
#include "bafe_exp/cli.hpp"
#include "bafe_exp/geo.hpp"
#include "bafe_exp/point_csv.hpp"
#include "bafe_exp/process_control.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace { std::string g_process_control = "process: default"; }

namespace {
using Clock = std::chrono::steady_clock;

std::vector<bafe::CellIndex> map_points(
    const std::vector<std::pair<double, double>>& points,
    std::uint32_t level,
    std::uint64_t maximum) {
    const std::size_t n = maximum == 0
        ? points.size()
        : std::min<std::size_t>(points.size(), maximum);
    std::unordered_set<bafe::CellIndex> unique;
    unique.reserve(n * 2U + 16U);
    // The CSV is sorted; evenly striding avoids a spatially biased prefix.
    for (std::size_t sample = 0; sample < n; ++sample) {
        const std::size_t index = n == points.size()
            ? sample
            : (sample * points.size()) / n;
        unique.insert(bafe::geo::lonlat_to_cell(
            points[index].first, points[index].second, level));
    }
    std::vector<bafe::CellIndex> result(unique.begin(), unique.end());
    std::sort(result.begin(), result.end());
    return result;
}

double median_area(
    const std::vector<bafe::CellIndex>& cells,
    std::uint32_t level) {
    std::vector<double> values;
    const auto stride = std::max<std::size_t>(1, cells.size() / 256U);
    for (std::size_t i = 0; i < cells.size() && values.size() < 256U; i += stride) {
        values.push_back(bafe::geo::cell_area_m2(cells[i], level));
    }
    return bafe_exp::percentile(values, 0.5);
}

struct Output {
    bafe_exp::ResultRow row;
    std::vector<std::uint64_t> cells;
};

std::vector<std::string> split_list(const std::string& text) {
    std::vector<std::string> items;
    std::stringstream stream(text);
    std::string item;
    while (std::getline(stream, item, ',')) {
        if (!item.empty()) items.push_back(item);
    }
    return items;
}

Output execute(
    bafe::HaloEngine& engine,
    const std::vector<bafe::CellIndex>& input,
    bafe::QueryOptions query,
    std::uint32_t warmup,
    std::uint32_t repeats,
    double mapping_ms,
    const std::string& scenario,
    const std::string& variant,
    std::uint32_t threads,
    bool oracle_capacity,
    bool reuse_output,
    bool wall_timer,
    std::ofstream* samples_csv) {
    query.reuse_output = reuse_output;
    if (oracle_capacity) {
        // Size the workspace to the realized output (learned from one untimed
        // probe call) instead of the automatic isolated seed budget.
        bafe::QueryOptions probe = query;
        probe.max_output_cells = 0U;
        const auto probed = engine.halo(input, probe);
        const std::uint64_t realized = probed.size();
        query.max_output_cells = std::max<std::uint64_t>(
            1024U, static_cast<std::uint64_t>(std::ceil(1.05 * double(realized))) + 64U);
    }
    for (std::uint32_t i = 0; i < warmup; ++i) {
        (void)engine.halo(input, query);
    }
    std::vector<double> samples;
    samples.reserve(repeats);
    std::vector<double> engine_samples;
    engine_samples.reserve(repeats);
    std::vector<double> pipeline_samples;
    std::vector<double> download_samples;
    bafe_exp::StageSummary stages;
    bafe::HaloResult result;
    for (std::uint32_t i = 0; i < repeats; ++i) {
        // Wall clock around the complete public call: it starts with the seed
        // indices in host memory and stops when the caller holds the complete
        // result (a new vector, or a view of the reused host buffer).  The
        // previous result is released outside the timed interval.
        const auto call_start = Clock::now();
        bafe::HaloResult current = engine.halo(input, query);
        const double wall_ms = std::chrono::duration<double, std::milli>(
            Clock::now() - call_start).count();
        result = std::move(current);
        const auto& m = result.metrics;
        samples.push_back(wall_timer ? wall_ms : m.end_to_end_ms);
        engine_samples.push_back(m.end_to_end_ms);
        if (samples_csv != nullptr) {
            *samples_csv << scenario << ',' << query.level << ',' << query.hops << ','
                         << input.size() << ',' << m.backend << ',' << variant << ','
                         << (reuse_output ? "reused" : "fresh") << ',' << threads << ','
                         << i << ',' << std::setprecision(9) << wall_ms << ','
                         << m.end_to_end_ms << '\n';
        }
        pipeline_samples.push_back(m.device_pipeline_ms);
        download_samples.push_back(m.output_download_ms);
        stages.device_pipeline_ms += m.device_pipeline_ms;
        stages.gpu_input_workspace_ms += m.gpu_input_and_workspace_ms;
        stages.gpu_wavefront_ms += m.gpu_fused_index_hash_wavefront_ms;
        stages.gpu_encode_ms += m.gpu_output_encode_ms;
        stages.output_download_ms += m.output_download_ms;
        stages.host_materialize_ms += m.host_materialize_ms;
        stages.post_timing_copy_ms += m.post_timing_copy_ms;
        stages.host_overhead_ms += m.end_to_end_ms - m.device_pipeline_ms -
            m.output_download_ms - m.host_materialize_ms;
    }
    if (repeats != 0U) {
        const double n = double(repeats);
        stages.device_pipeline_ms /= n;
        stages.gpu_input_workspace_ms /= n;
        stages.gpu_wavefront_ms /= n;
        stages.gpu_encode_ms /= n;
        stages.output_download_ms /= n;
        stages.host_materialize_ms /= n;
        stages.post_timing_copy_ms /= n;
        stages.host_overhead_ms /= n;
        stages.median_device_pipeline_ms = bafe_exp::percentile(pipeline_samples, 0.5);
        stages.median_output_download_ms = bafe_exp::percentile(download_samples, 0.5);
    }
    bafe_exp::ResultRow row;
    row.experiment_id = "real_world_layer_halo";
    row.backend = result.metrics.backend;
    row.library_version = bafe::kLibraryVersion;
    row.compiler = bafe_exp::compiler_string();
    row.pattern = "real_geographic_distribution";
    row.scenario = scenario;
    row.level_or_resolution = query.level;
    row.median_cell_area_m2 = median_area(input, query.level);
    row.input_cells = input.size();
    row.unique_input_cells = result.metrics.unique_input_cells;
    row.hops = query.hops;
    row.output_cells = result.size();
    row.candidate_references = result.metrics.touched_neighbor_references;
    row.seam_crossings = result.metrics.root_edge_crossings;
    row.peak_memory_bytes = result.metrics.device_peak_bytes;
    row.h2d_bytes = result.metrics.host_to_device_bytes;
    row.d2h_bytes = result.metrics.device_to_host_bytes;
    row.mapping_ms = mapping_ms;
    row.timing = bafe_exp::summarize(samples);
    const std::vector<std::uint64_t> returned(result.data(), result.data() + result.size());
    row.result_hash = bafe_exp::fnv1a64(returned);
    row.correctness_status = "SELF";
    row.notes = "official Calgary point distribution; exact halo after point-to-cell mapping; " + g_process_control;
    row.variant = variant;
    row.stages = stages;
    row.hash_capacity_log2 = result.metrics.hash_capacity_log2;
    row.skipped_probes = result.metrics.skipped_arrival_probes;
    row.threads = threads;
    row.max_output_cells = query.max_output_cells;
    row.delivery = reuse_output ? "reused" : "fresh";
    row.timer = wall_timer ? "wall" : "engine";
    row.engine_median_ms = bafe_exp::percentile(engine_samples, 0.5);
    return {row, returned};
}
} // namespace

int main(int argc, char** argv) {
    try {
        g_process_control = bafe_exp::stabilize_process();
        std::cout << g_process_control << "\n";
        bafe_exp::Cli cli(argc, argv);
        const auto input_path = cli.get("input");
        if (input_path.empty()) {
            throw std::invalid_argument("--input points.csv is required");
        }
        const auto level = cli.u32("level", 28);
        const auto hops = cli.u32("hops", 4);
        const auto warmup = cli.u32("warmup", 10);
        const auto repeats = cli.u32("repeats", 30);
        // With several backends the first one is only the correctness
        // reference; --reference-repeats N limits it to one warmup and N timed
        // repetitions so that a slow reference does not dominate an ablation.
        const auto reference_repeats = cli.u32("reference-repeats", 0);
        const auto cpu_threads = cli.u32("cpu-threads", 0);
        const auto maximum = cli.u64("max-points", 0);
        const auto backend = cli.get("backend", "both");
        const auto scenario = cli.get("scenario", "real_world");
        const auto csv_path = cli.get("csv", "results/real_world_halo.csv");
        const auto variant = cli.get("variant", "baseline");
        const bool oracle_capacity = cli.flag("oracle-capacity");
        // --timer wall times the complete public call with the harness clock
        // (the protocol of the revised manuscript); --timer engine keeps the
        // backend's internal timer of the earlier sessions.
        const auto timer = cli.get("timer", "engine");
        if (timer != "engine" && timer != "wall") {
            throw std::invalid_argument("--timer must be engine or wall");
        }
        const bool wall_timer = timer == "wall";
        // --delivery fresh (new vector per call), reused (engine owned host
        // buffer), or both (two blocks per backend, fresh first).
        const auto delivery = cli.get("delivery", "fresh");
        if (delivery != "fresh" && delivery != "reused" && delivery != "both") {
            throw std::invalid_argument("--delivery must be fresh, reused, or both");
        }
        // Optional file that receives every timed call (one line per call).
        const auto samples_path = cli.get("samples-csv", "");
        std::ofstream samples_file;
        if (!samples_path.empty()) {
            samples_file.open(samples_path);
            if (!samples_file) throw std::runtime_error("cannot open " + samples_path);
            samples_file << "scenario,level,hops,input_cells,backend,variant,delivery,"
                            "threads,call,wall_ms,engine_ms\n";
        }

        bafe::QueryOptions query;
        query.level = level;
        query.hops = hops;
        query.max_output_cells = cli.u64("max-output-cells", 0);
        const auto schedule = cli.get("cuda-schedule", "auto");
        if (schedule == "host") query.cuda_schedule = bafe::CudaSchedule::HostDriven;
        else if (schedule == "cooperative") query.cuda_schedule = bafe::CudaSchedule::CooperativePersistent;
        else if (schedule != "auto") throw std::invalid_argument("--cuda-schedule must be auto, cooperative, or host");
        auto& tuning = query.cuda_tuning;
        tuning.skip_arrival_edge = cli.flag("cuda-skip-arrival");
        tuning.test_before_cas = cli.flag("cuda-test-before-cas");
        tuning.warp_aggregated_append = !cli.flag("cuda-no-warp-append");
        tuning.hash_mode = static_cast<int>(cli.u32("cuda-hash-mode", 0));
        tuning.load_target = cli.f64("cuda-load-target", 0.5);
        tuning.lazy_clear = cli.flag("cuda-lazy-clear");
        tuning.pinned_output = cli.flag("cuda-pinned-output");
        tuning.skip_memory_preflight = cli.flag("cuda-skip-preflight");
        tuning.fused_control_clear = cli.flag("cuda-fused-clear");
        if (cli.flag("cuda-all-opt")) {
            tuning.skip_arrival_edge = true;
            tuning.test_before_cas = true;
            tuning.lazy_clear = true;
            tuning.pinned_output = true;
            tuning.skip_memory_preflight = true;
            tuning.fused_control_clear = true;
        }
        // --cuda-all-opt-except a,b,...  turns individual mechanisms of the
        // full configuration off again (leave one out ablation).  Names:
        // arrival, cas, lazy, pinned, trims.
        {
            const auto except = cli.get("cuda-all-opt-except", "");
            std::size_t begin = 0;
            while (begin <= except.size() && !except.empty()) {
                const std::size_t end = except.find(',', begin);
                const std::string name = except.substr(
                    begin, end == std::string::npos ? std::string::npos : end - begin);
                if (name == "arrival") tuning.skip_arrival_edge = false;
                else if (name == "cas") tuning.test_before_cas = false;
                else if (name == "lazy") tuning.lazy_clear = false;
                else if (name == "pinned") tuning.pinned_output = false;
                else if (name == "trims") {
                    tuning.skip_memory_preflight = false;
                    tuning.fused_control_clear = false;
                }
                else if (name == "preflight") tuning.skip_memory_preflight = false;
                else if (name == "fused") tuning.fused_control_clear = false;
                else if (!name.empty()) {
                    throw std::invalid_argument("unknown --cuda-all-opt-except name: " + name);
                }
                if (end == std::string::npos) break;
                begin = end + 1U;
            }
        }

        const auto points = bafe_exp::read_lonlat_csv(input_path);
        const auto map_start = Clock::now();
        auto cells = map_points(points, level, maximum);
        const double mapping_ms = std::chrono::duration<double, std::milli>(
            Clock::now() - map_start).count();
        if (cells.empty()) {
            throw std::runtime_error("point mapping produced no cells");
        }

        std::vector<std::string> tokens;
        if (backend == "both") tokens = {"cpu", "gpu"};
        else if (backend == "parallel-gpu") tokens = {"parallel", "gpu"};
        else if (backend == "all") tokens = {"cpu", "parallel", "gpu"};
        else tokens = split_list(backend);
        if (tokens.empty()) {
            throw std::invalid_argument(
                "--backend must list cpu, parallel, flat, atomic, gpu, auto (comma separated), "
                "or both, parallel-gpu, all");
        }

        std::vector<Output> outputs;
        for (const auto& token : tokens) {
            std::unique_ptr<bafe::HaloEngine> engine;
            std::uint32_t threads = 1U;
            if (token == "cpu") {
                engine = bafe::make_cpu_engine();
            } else if (token == "parallel") {
                engine = bafe::make_parallel_cpu_engine(cpu_threads);
                threads = cpu_threads;
            } else if (token == "flat") {
                engine = bafe::make_atomic_cpu_engine(1U);
            } else if (token == "atomic") {
                engine = bafe::make_atomic_cpu_engine(cpu_threads);
                threads = cpu_threads;
            } else if (token == "gpu") {
                engine = bafe::make_cuda_engine();
                threads = 0U;
            } else if (token == "auto") {
                engine = bafe::make_engine(bafe::Backend::Auto);
                threads = 0U;
            } else {
                throw std::invalid_argument("unknown backend token: " + token);
            }
            const bool cuda_like = token == "gpu" || token == "auto";
            const bool trimmed_reference =
                outputs.empty() && tokens.size() > 1U && reference_repeats != 0U;
            // The reused buffer delivery exists for the flat table CPU
            // backends and the CUDA backend only.
            const bool can_reuse = cuda_like || token == "flat" || token == "atomic";
            std::vector<bool> modes;
            if (delivery != "reused") modes.push_back(false);
            if (delivery != "fresh" && can_reuse) modes.push_back(true);
            if (modes.empty()) modes.push_back(false);
            for (const bool reuse_output : modes) {
                outputs.push_back(execute(
                    *engine, cells, query,
                    trimmed_reference ? std::min<std::uint32_t>(warmup, 1U) : warmup,
                    trimmed_reference ? reference_repeats : repeats,
                    mapping_ms, scenario,
                    cuda_like ? variant : "baseline", threads,
                    cuda_like && oracle_capacity,
                    reuse_output, wall_timer,
                    samples_file.is_open() ? &samples_file : nullptr));
                if (trimmed_reference) {
                    outputs.back().row.notes += "; reference timed with --reference-repeats";
                }
            }
        }
        if (outputs.size() > 1U) {
            auto reference = outputs.front().cells;
            std::sort(reference.begin(), reference.end());
            for (std::size_t index = 1U; index < outputs.size(); ++index) {
                auto candidate = outputs[index].cells;
                std::sort(candidate.begin(), candidate.end());
                if (reference != candidate) {
                    throw std::runtime_error(
                        "real-world halo mismatch between " + outputs.front().row.backend +
                        " and " + outputs[index].row.backend);
                }
            }
            for (auto& output : outputs) {
                output.row.correctness_status = "PASS";
            }
        }
        std::ofstream csv(csv_path);
        if (!csv) {
            throw std::runtime_error("cannot open " + csv_path);
        }
        bafe_exp::write_csv_header(csv);
        for (const auto& output : outputs) {
            bafe_exp::write_csv_row(csv, output.row);
            std::cout << output.row.backend
                      << " variant=" << output.row.variant
                      << " delivery=" << output.row.delivery
                      << " timer=" << output.row.timer
                      << " level=" << level
                      << " input=" << cells.size()
                      << " hops=" << hops
                      << " output=" << output.row.output_cells
                      << " mean_ms=" << output.row.timing.mean_ms
                      << " median_ms=" << output.row.timing.median_ms
                      << " device_ms=" << output.row.stages.device_pipeline_ms
                      << " download_ms=" << output.row.stages.output_download_ms
                      << " status=" << output.row.correctness_status << '\n';
        }
        std::cout << "CSV: " << csv_path << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "bafe_realworld_halo: " << error.what() << '\n';
        return 1;
    }
}
