#pragma once

#include "bafe/index.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace bafe {

constexpr const char* kLibraryVersion = "1.2.0-rev";
constexpr const char* kAtlasSchema = "PENTAKIS-60-ACM-1";
constexpr std::uint64_t kAtlasFnv1a64 = 0x86b0327618b12155ULL;

enum class Backend {
    CpuReference,
    CpuParallel,
    CudaHashFrontier,
    Auto
};

// Auto selects the one-launch cooperative wavefront when the device supports
// cooperative grids and falls back to the universally supported per-hop
// scheduler otherwise.  HostDriven is useful for validation and profiling.
enum class CudaSchedule {
    Auto,
    CooperativePersistent,
    HostDriven
};

enum class ChartMode {
    NaturalTriangles,
    RhombicMacrocharts
};

// Tuning and ablation switches for the CUDA backend.  Every default reproduces
// the code path measured for the manuscript exactly; each switch changes one
// mechanism so that its contribution can be measured in isolation.
struct CudaTuning {
    // G4: a cell in ring k>=1 was discovered through one of its edges and that
    // neighbour is already in ring k-1, so probing it is a guaranteed rejection.
    bool skip_arrival_edge = false;
    // G5: read the slot before issuing the atomic; only claim when it reads empty.
    bool test_before_cas = false;
    // Ablation: false issues one atomicAdd per discovered cell instead of one per warp.
    bool warp_aggregated_append = true;
    // 0 = SplitMix mixing (manuscript), 1 = masked key (no mixing),
    // 2 = block local Morton placement (experimental locality aware hashing).
    int hash_mode = 0;
    // Table capacity = nextPow2(max(1024, keys / load_target)); 0.5 is the manuscript value.
    double load_target = 0.5;
    // G2: restore only the occupied slots after a query instead of clearing the whole table.
    bool lazy_clear = false;
    // G1: copy the result into a reusable page locked host buffer instead of a fresh
    // pageable std::vector.  result.cells is filled after the timed region.
    bool pinned_output = false;
    // Host overhead trims: skip cudaMemGetInfo when no buffer grows, and clear the
    // control words with one memset instead of nine.
    bool skip_memory_preflight = false;
    bool fused_control_clear = false;
};

struct QueryOptions {
    std::uint32_t level = 0;
    std::uint32_t hops = 1;
    std::uint64_t max_output_cells = 0;  // 0 selects a safe graph upper bound.
    bool deterministic_layers = false;
    CudaSchedule cuda_schedule = CudaSchedule::Auto;
    ChartMode chart_mode = ChartMode::NaturalTriangles;
    CudaTuning cuda_tuning;
    // Delivery of the result.  false (default, "fresh container"): result.cells
    // is a newly allocated array owned by the caller.  true ("reused buffer"):
    // the result stays in host memory that the engine allocated once and reuses
    // for later calls (page locked for the CUDA backend); result.view and
    // result.view_count describe it, remain valid until the next call on the
    // same engine, and result.cells stays empty.  Supported by the flat table
    // CPU backends and the CUDA backend; other backends ignore the flag.
    bool reuse_output = false;
};

struct Metrics {
    std::string backend;
    std::string device;
    std::uint64_t input_cells = 0;
    std::uint64_t unique_input_cells = 0;
    std::uint64_t output_cells = 0;
    std::uint64_t touched_neighbor_references = 0;
    std::uint64_t skipped_arrival_probes = 0;
    std::uint64_t root_edge_crossings = 0;
    std::uint64_t macrochart_boundary_crossings = 0;
    std::uint64_t paired_edge_direct_transforms = 0;
    std::uint64_t host_to_device_bytes = 0;
    std::uint64_t device_to_host_bytes = 0;
    std::uint64_t device_peak_bytes = 0;
    std::uint64_t control_transfer_bytes = 0;
    std::uint32_t kernel_launches = 0;
    std::uint32_t hash_capacity_log2 = 0;
    bool lazy_cleared = false;
    double gpu_input_and_workspace_ms = 0.0;
    double gpu_fused_index_hash_wavefront_ms = 0.0;
    double gpu_output_encode_ms = 0.0;
    double output_download_ms = 0.0;
    double host_materialize_ms = 0.0;   // host allocation of the result array (inside end_to_end)
    double post_timing_copy_ms = 0.0;   // pinned buffer -> result.cells copy (outside end_to_end)
    double deterministic_sort_ms = 0.0;
    double device_pipeline_ms = 0.0;
    double end_to_end_ms = 0.0;
};

struct HaloResult {
    // Unique cells, grouped by exact hop layer.  Layer h occupies
    // [layer_offsets[h], layer_offsets[h+1]).  Input cells are excluded.
    std::vector<CellIndex> cells;
    std::vector<std::uint64_t> layer_offsets;
    Metrics metrics;
    // Set only when QueryOptions::reuse_output was honoured (see there).
    const CellIndex* view = nullptr;
    std::size_t view_count = 0;

    const CellIndex* data() const noexcept { return view ? view : cells.data(); }
    std::size_t size() const noexcept { return view ? view_count : cells.size(); }
};

struct DeviceInfo {
    bool available = false;
    std::string name;
    int compute_major = 0;
    int compute_minor = 0;
    std::uint64_t total_memory_bytes = 0;
};

class HaloEngine {
public:
    virtual ~HaloEngine() = default;

    virtual HaloResult halo(
        const CellIndex* input,
        std::size_t input_count,
        const QueryOptions& options) = 0;

    HaloResult halo(
        const std::vector<CellIndex>& input,
        const QueryOptions& options) {
        return halo(input.data(), input.size(), options);
    }

    virtual DeviceInfo device_info() const = 0;
};

std::unique_ptr<HaloEngine> make_cpu_engine();
// thread_count == 0 selects the OpenMP runtime maximum.
std::unique_ptr<HaloEngine> make_parallel_cpu_engine(
    std::uint32_t thread_count = 0);
// Same algorithm as the CUDA backend on the CPU: one flat open addressing
// table claimed with compare and exchange, per thread output fragments, and a
// table that grows between rings.  thread_count == 1 is the flat serial
// reference; thread_count == 0 selects the OpenMP runtime maximum.
std::unique_ptr<HaloEngine> make_atomic_cpu_engine(
    std::uint32_t thread_count = 0);
std::unique_ptr<HaloEngine> make_cuda_engine(int device_ordinal = 0);
std::unique_ptr<HaloEngine> make_engine(
    Backend backend = Backend::Auto,
    int device_ordinal = 0);

}  // namespace bafe
