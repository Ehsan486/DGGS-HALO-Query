// Global ring validation of every implementation (revision 2026-11).
//
// Seeds are placed on every directed base mesh edge (five positions along each
// of the 180 edges) and at every base mesh vertex (the corner cells of every
// face chart, which covers the 12 vertices where five faces meet and the 20
// vertices where six faces meet).  For levels {2,6,12,20,28} and hop depths
// {1,2,4,8} every ring returned by each implementation, in each delivery, is
// compared with the rings of the serial container reference, whose adjacency
// is validated against references that do not use the atlas in
// bafe_surface_tests.  The program also checks that an insufficient output
// capacity is reported as an error and that the same engine then answers a
// following query correctly.
#include "bafe/bafe.hpp"
#include "internal.hpp"
#include "topology.hpp"
#include "bafe_exp/cli.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using bafe::CellIndex;

std::vector<CellIndex> edge_seeds(std::uint32_t level, const bafe::detail::EncodeChunk* encode) {
    const std::uint32_t n = 1U << level;
    std::set<CellIndex> unique;
    for (std::uint32_t face = 0; face < bafe::kRootFaceCount; ++face) {
        for (std::uint32_t edge = 0; edge < 3U; ++edge) {
            const std::uint32_t positions[5] = {0U, n / 4U, n / 2U, (3U * n) / 4U, n - 1U};
            for (const std::uint32_t t : positions) {
                unique.insert(bafe::detail::cell_to_path_chunked(
                    bafe::detail::boundary_up(face, edge, std::min(t, n - 1U), n), level, encode));
            }
        }
    }
    return {unique.begin(), unique.end()};
}

std::vector<CellIndex> vertex_seeds(std::uint32_t level, const bafe::detail::EncodeChunk* encode) {
    const std::uint32_t n = 1U << level;
    std::set<CellIndex> unique;
    for (std::uint32_t face = 0; face < bafe::kRootFaceCount; ++face) {
        const bafe::detail::Cell corners[3] = {
            {face, 0U, 0U, 0U}, {face, n - 1U, 0U, 0U}, {face, 0U, n - 1U, 0U}};
        for (const auto& corner : corners) {
            unique.insert(bafe::detail::cell_to_path_chunked(corner, level, encode));
        }
    }
    return {unique.begin(), unique.end()};
}

std::vector<std::vector<CellIndex>> rings_of(const bafe::HaloResult& result) {
    std::vector<std::vector<CellIndex>> rings;
    const CellIndex* data = result.data();
    for (std::size_t layer = 0; layer + 1U < result.layer_offsets.size(); ++layer) {
        rings.emplace_back(data + result.layer_offsets[layer], data + result.layer_offsets[layer + 1U]);
        std::sort(rings.back().begin(), rings.back().end());
    }
    return rings;
}

struct Candidate {
    std::string name;
    bafe::HaloEngine* engine;
    bafe::QueryOptions options;   // level and hops are filled per case
};
}  // namespace

int main(int argc, char** argv) {
    try {
        bafe_exp::Cli cli(argc, argv);
        const bool with_gpu = !cli.flag("no-gpu");
        const auto threads = cli.u32("cpu-threads", 0);
        const auto csv_path = cli.get("csv", "results/global_validation.csv");
        const auto encode = bafe::detail::make_encode_chunks();

        auto reference = bafe::make_cpu_engine();
        auto flat = bafe::make_atomic_cpu_engine(1U);
        auto atomic = bafe::make_atomic_cpu_engine(threads);
        std::unique_ptr<bafe::HaloEngine> gpu;
        if (with_gpu) gpu = bafe::make_cuda_engine();

        bafe::QueryOptions fresh;
        bafe::QueryOptions reused;
        reused.reuse_output = true;
        // The GPU implementation of the paper: the four mechanisms on.
        bafe::QueryOptions gpu_fresh;
        gpu_fresh.cuda_tuning.skip_arrival_edge = true;
        gpu_fresh.cuda_tuning.test_before_cas = true;
        gpu_fresh.cuda_tuning.pinned_output = true;
        gpu_fresh.cuda_tuning.skip_memory_preflight = true;
        gpu_fresh.cuda_tuning.fused_control_clear = true;
        bafe::QueryOptions gpu_reused = gpu_fresh;
        gpu_reused.reuse_output = true;
        bafe::QueryOptions gpu_host = gpu_fresh;
        gpu_host.cuda_schedule = bafe::CudaSchedule::HostDriven;
        bafe::QueryOptions gpu_plain;   // every mechanism off, pageable output

        std::vector<Candidate> candidates = {
            {"cpu_flat_fresh", flat.get(), fresh},
            {"cpu_flat_reused", flat.get(), reused},
            {"cpu_threads_fresh", atomic.get(), fresh},
            {"cpu_threads_reused", atomic.get(), reused}};
        if (with_gpu) {
            candidates.push_back({"gpu_fresh", gpu.get(), gpu_fresh});
            candidates.push_back({"gpu_reused", gpu.get(), gpu_reused});
            candidates.push_back({"gpu_kernel_per_ring", gpu.get(), gpu_host});
            candidates.push_back({"gpu_mechanisms_off", gpu.get(), gpu_plain});
        }

        std::ofstream csv(csv_path);
        if (!csv) throw std::runtime_error("cannot open " + csv_path);
        csv << "scenario,level,hops,seeds,output_cells,rings,base_edge_crossings,implementation,status\n";

        const std::uint32_t levels[] = {2U, 6U, 12U, 20U, 28U};
        const std::uint32_t depths[] = {1U, 2U, 4U, 8U};
        std::uint64_t cases = 0U, comparisons = 0U, ring_comparisons = 0U, crossings_total = 0U;
        for (const char* scenario : {"edges", "vertices"}) {
            for (const std::uint32_t level : levels) {
                const auto seeds = std::string(scenario) == "edges"
                    ? edge_seeds(level, encode.data())
                    : vertex_seeds(level, encode.data());
                for (const std::uint32_t hops : depths) {
                    bafe::QueryOptions base;
                    base.level = level;
                    base.hops = hops;
                    const auto expected = reference->halo(seeds, base);
                    const auto expected_rings = rings_of(expected);
                    ++cases;
                    crossings_total += expected.metrics.root_edge_crossings;
                    for (const Candidate& candidate : candidates) {
                        bafe::QueryOptions options = candidate.options;
                        options.level = level;
                        options.hops = hops;
                        const auto actual = candidate.engine->halo(seeds, options);
                        if (actual.layer_offsets != expected.layer_offsets ||
                            rings_of(actual) != expected_rings) {
                            throw std::runtime_error(
                                "ring mismatch: " + candidate.name + " scenario=" + scenario +
                                " level=" + std::to_string(level) + " hops=" + std::to_string(hops));
                        }
                        ++comparisons;
                        ring_comparisons += expected_rings.size();
                        csv << scenario << ',' << level << ',' << hops << ',' << seeds.size() << ','
                            << expected.cells.size() << ',' << expected_rings.size() << ','
                            << expected.metrics.root_edge_crossings << ',' << candidate.name << ",PASS\n";
                    }
                }
            }
        }

        // Insufficient output capacity must be an error, never a partial
        // result, and the engine must answer the next query correctly.
        std::uint64_t failure_checks = 0U;
        {
            const std::uint32_t level = 12U, hops = 8U;
            const auto seeds = edge_seeds(level, encode.data());
            bafe::QueryOptions base;
            base.level = level;
            base.hops = hops;
            const auto expected_rings = rings_of(reference->halo(seeds, base));
            for (const Candidate& candidate : candidates) {
                if (candidate.name.rfind("gpu", 0) != 0) continue;
                bafe::QueryOptions small = candidate.options;
                small.level = level;
                small.hops = hops;
                small.max_output_cells = 64U;
                bool reported = false;
                try {
                    (void)candidate.engine->halo(seeds, small);
                } catch (const std::exception&) {
                    reported = true;
                }
                if (!reported) {
                    throw std::runtime_error("capacity exhaustion was not reported: " + candidate.name);
                }
                bafe::QueryOptions again = candidate.options;
                again.level = level;
                again.hops = hops;
                if (rings_of(candidate.engine->halo(seeds, again)) != expected_rings) {
                    throw std::runtime_error("query after a capacity error differs: " + candidate.name);
                }
                ++failure_checks;
                csv << "capacity_error_then_retry," << level << ',' << hops << ',' << seeds.size()
                    << ",,,," << candidate.name << ",PASS\n";
            }
        }

        std::cout << "PASS cases=" << cases << " implementations=" << candidates.size()
                  << " comparisons=" << comparisons << " ring_comparisons=" << ring_comparisons
                  << " reference_base_edge_crossings=" << crossings_total
                  << " capacity_error_checks=" << failure_checks << '\n'
                  << "CSV: " << csv_path << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "bafe_global_validation: " << error.what() << '\n';
        return 1;
    }
}
