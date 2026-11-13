#pragma once

#include "bafe/index.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace bafe {

constexpr const char* kLibraryVersion = "1.0.0";
constexpr const char* kAtlasSchema = "PENTAKIS-60-ACM-1";
constexpr std::uint64_t kAtlasFnv1a64 = 0x86b0327618b12155ULL;

enum class Backend {
    CpuReference,
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

struct QueryOptions {
    std::uint32_t level = 0;
    std::uint32_t hops = 1;
    std::uint64_t max_output_cells = 0;  // 0 selects a safe graph upper bound.
    bool deterministic_layers = false;
    CudaSchedule cuda_schedule = CudaSchedule::Auto;
    ChartMode chart_mode = ChartMode::NaturalTriangles;
};

struct Metrics {
    std::string backend;
    std::string device;
    std::uint64_t input_cells = 0;
    std::uint64_t unique_input_cells = 0;
    std::uint64_t output_cells = 0;
    std::uint64_t touched_neighbor_references = 0;
    std::uint64_t root_edge_crossings = 0;
    std::uint64_t macrochart_boundary_crossings = 0;
    std::uint64_t paired_edge_direct_transforms = 0;
    std::uint64_t host_to_device_bytes = 0;
    std::uint64_t device_to_host_bytes = 0;
    std::uint64_t device_peak_bytes = 0;
    std::uint64_t control_transfer_bytes = 0;
    std::uint32_t kernel_launches = 0;
    std::uint32_t hash_capacity_log2 = 0;
    // GPU stage timings use CUDA events on the query stream.  They are
    // reported separately so applications can distinguish data movement,
    // workspace initialization, public-index decoding and seed insertion,
    // halo expansion, and final public-index encoding.
    double gpu_upload_ms = 0.0;
    double gpu_workspace_clear_ms = 0.0;
    double gpu_decode_seed_hash_ms = 0.0;
    double gpu_halo_expand_hash_ms = 0.0;
    double gpu_output_encode_ms = 0.0;
    double gpu_download_ms = 0.0;
    double device_pipeline_ms = 0.0;
    double end_to_end_ms = 0.0;
};

struct HaloResult {
    // Unique cells, grouped by exact hop layer.  Layer h occupies
    // [layer_offsets[h], layer_offsets[h+1]).  Input cells are excluded.
    std::vector<CellIndex> cells;
    std::vector<std::uint64_t> layer_offsets;
    Metrics metrics;
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
std::unique_ptr<HaloEngine> make_cuda_engine(int device_ordinal = 0);
std::unique_ptr<HaloEngine> make_engine(
    Backend backend = Backend::Auto,
    int device_ordinal = 0);

}  // namespace bafe
