#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace bafe_exp {

inline std::uint64_t fnv1a64(const std::vector<std::uint64_t>& values) {
    std::vector<std::uint64_t> sorted=values;
    std::sort(sorted.begin(),sorted.end());
    std::uint64_t h=1469598103934665603ULL;
    for (std::uint64_t v:sorted) {
        for (int b=0;b<8;++b) { h^=(v>>(8*b))&0xffULL; h*=1099511628211ULL; }
    }
    return h;
}

inline double percentile(std::vector<double> v,double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(),v.end());
    const double idx=p*double(v.size()-1);
    const std::size_t lo=static_cast<std::size_t>(std::floor(idx));
    const std::size_t hi=static_cast<std::size_t>(std::ceil(idx));
    if (lo==hi) return v[lo];
    return v[lo]*(double(hi)-idx)+v[hi]*(idx-double(lo));
}

struct TimingSummary {
    double mean_ms=0, median_ms=0, p95_ms=0, min_ms=0, max_ms=0, stddev_ms=0;
};

inline TimingSummary summarize(const std::vector<double>& samples) {
    if (samples.empty()) return {};
    TimingSummary s;
    s.mean_ms=std::accumulate(samples.begin(),samples.end(),0.0)/samples.size();
    s.median_ms=percentile(samples,0.5);
    s.p95_ms=percentile(samples,0.95);
    s.min_ms=*std::min_element(samples.begin(),samples.end());
    s.max_ms=*std::max_element(samples.begin(),samples.end());
    double var=0; for(double x:samples){const double d=x-s.mean_ms;var+=d*d;} var/=samples.size();
    s.stddev_ms=std::sqrt(var);
    return s;
}

// Stage decomposition of one backend call, averaged over the timed repetitions.
// All values are milliseconds; zero for backends that do not report a stage.
struct StageSummary {
    double device_pipeline_ms=0;      // CUDA events: input copy .. output encoding
    double gpu_input_workspace_ms=0;  // input copy + table clear + control clears
    double gpu_wavefront_ms=0;        // seed decode + all rings
    double gpu_encode_ms=0;           // compact -> public index conversion
    double output_download_ms=0;      // device -> host copy of the result
    double host_materialize_ms=0;     // host allocation of the result array
    double host_overhead_ms=0;        // end_to_end - device_pipeline - download - materialize
    double post_timing_copy_ms=0;     // pinned buffer -> std::vector (outside the timed region)
    double median_device_pipeline_ms=0;
    double median_output_download_ms=0;
};

struct ResultRow {
    std::string experiment_id;
    std::string backend;
    std::string library_version;
    std::string compiler;
    std::string pattern;
    std::string scenario;
    std::uint32_t level_or_resolution=0;
    double median_cell_area_m2=0;
    std::uint64_t input_cells=0;
    std::uint64_t unique_input_cells=0;
    std::uint32_t hops=0;
    std::uint64_t output_cells=0;
    std::uint64_t conflict_cells=0;
    std::uint64_t candidate_references=0;
    std::uint64_t seam_crossings=0;
    std::uint64_t peak_memory_bytes=0;
    std::uint64_t h2d_bytes=0;
    std::uint64_t d2h_bytes=0;
    double mapping_ms=0;
    double neighbor_step_m=0;
    double requested_clearance_m=0;
    double achieved_clearance_m=0;
    TimingSummary timing;
    std::uint64_t result_hash=0;
    std::string correctness_status="UNVERIFIED";
    std::string notes;
    // Columns added for the optimization study (appended at the end of the
    // CSV so that existing analysis scripts keep working).
    std::string variant;
    StageSummary stages;
    std::uint32_t hash_capacity_log2=0;
    std::uint64_t skipped_probes=0;
    std::uint32_t threads=0;
    std::uint64_t max_output_cells=0;
    // Columns added for the revision (2026-11): how the result is delivered
    // ("fresh" container or "reused" buffer), which timer fills mean_ms..stddev_ms
    // ("engine" = timer inside the backend, "wall" = harness clock around the
    // complete call), and the median of the backend's own timer for reference.
    std::string delivery="fresh";
    std::string timer="engine";
    double engine_median_ms=0;
};

inline std::string csv_quote(const std::string& s) {
    if (s.find_first_of(",\"\n") == std::string::npos) return s;
    std::string out="\""; for(char c:s){if(c=='\"')out+="\"\"";else out+=c;} out+='\"'; return out;
}

inline void write_csv_header(std::ostream& o) {
    o << "experiment_id,backend,library_version,compiler,pattern,scenario,level_or_resolution,"
         "median_cell_area_m2,input_cells,unique_input_cells,hops,output_cells,conflict_cells,"
         "candidate_references,seam_crossings,mean_ms,median_ms,p95_ms,min_ms,max_ms,"
         "stddev_ms,peak_memory_bytes,h2d_bytes,d2h_bytes,mapping_ms,neighbor_step_m,"
         "requested_clearance_m,achieved_clearance_m,result_hash,correctness_status,notes,"
         "variant,device_pipeline_ms,gpu_input_workspace_ms,gpu_wavefront_ms,gpu_encode_ms,"
         "output_download_ms,host_materialize_ms,host_overhead_ms,post_timing_copy_ms,"
         "median_device_pipeline_ms,median_output_download_ms,hash_capacity_log2,"
         "skipped_probes,threads,max_output_cells,delivery,timer,engine_median_ms\n";
}

inline void write_csv_row(std::ostream& o,const ResultRow& r) {
    o<<csv_quote(r.experiment_id)<<','<<csv_quote(r.backend)<<','<<csv_quote(r.library_version)<<','
     <<csv_quote(r.compiler)<<','<<csv_quote(r.pattern)<<','<<csv_quote(r.scenario)<<','<<r.level_or_resolution<<','
     <<std::setprecision(12)<<r.median_cell_area_m2<<','<<r.input_cells<<','<<r.unique_input_cells<<','
     <<r.hops<<','<<r.output_cells<<','<<r.conflict_cells<<','<<r.candidate_references<<','<<r.seam_crossings<<','
     <<r.timing.mean_ms<<','<<r.timing.median_ms<<','<<r.timing.p95_ms<<','<<r.timing.min_ms<<','
     <<r.timing.max_ms<<','<<r.timing.stddev_ms<<','<<r.peak_memory_bytes<<','<<r.h2d_bytes<<','
     <<r.d2h_bytes<<','<<r.mapping_ms<<','<<r.neighbor_step_m<<','<<r.requested_clearance_m<<','
     <<r.achieved_clearance_m<<','<<r.result_hash<<','<<csv_quote(r.correctness_status)<<','<<csv_quote(r.notes)<<','
     <<csv_quote(r.variant)<<','<<r.stages.device_pipeline_ms<<','<<r.stages.gpu_input_workspace_ms<<','
     <<r.stages.gpu_wavefront_ms<<','<<r.stages.gpu_encode_ms<<','<<r.stages.output_download_ms<<','
     <<r.stages.host_materialize_ms<<','<<r.stages.host_overhead_ms<<','<<r.stages.post_timing_copy_ms<<','
     <<r.stages.median_device_pipeline_ms<<','<<r.stages.median_output_download_ms<<','
     <<r.hash_capacity_log2<<','<<r.skipped_probes<<','<<r.threads<<','<<r.max_output_cells<<','
     <<csv_quote(r.delivery)<<','<<csv_quote(r.timer)<<','<<r.engine_median_ms<<'\n';
}

inline std::string compiler_string() {
#if defined(_MSC_VER)
    return "MSVC " + std::to_string(_MSC_VER);
#elif defined(__clang__)
    return std::string("Clang ")+__clang_version__;
#elif defined(__GNUC__)
    return "GCC " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__);
#else
    return "unknown";
#endif
}

} // namespace bafe_exp
