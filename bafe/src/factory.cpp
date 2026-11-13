#include "bafe/bafe.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>

namespace bafe {
namespace {

// Auto is a query-time policy rather than a construction-time alias.  Tiny,
// spatially coherent frontiers are faster on a CPU because a CUDA launch has a
// fixed cost; scattered sets expose enough parallelism even at smaller sizes.
class AdaptiveHaloEngine final : public HaloEngine {
public:
    AdaptiveHaloEngine(std::unique_ptr<HaloEngine> cpu_serial,
                       std::unique_ptr<HaloEngine> cpu_parallel,
                       std::unique_ptr<HaloEngine> gpu)
        : cpu_serial_(std::move(cpu_serial)),
          cpu_parallel_(std::move(cpu_parallel)),
          gpu_(std::move(gpu)) {}

    HaloResult halo(const CellIndex* input,
                    std::size_t input_count,
                    const QueryOptions& options) override {
        if (input_count == 0U || options.hops == 0U || !gpu_) {
            return cpu_serial_->halo(input, input_count, options);
        }

        // Empirical log-cost boundary fitted to the complete 81-query Calgary
        // matrix (levels 12/20/28, depths 1/4/8, and three spatial layers).
        // Positive score predicts that resident CUDA is faster than the best
        // CPU implementation.  Leave-one-configuration-out accuracy is 90.1%
        // with mean slowdown 1.052x relative to the per-query oracle.  Keeping
        // the formula here makes Auto deterministic, auditable, and cheap.
        const double log_cells = std::log2(static_cast<double>(
            std::max<std::size_t>(input_count, 1U)));
        const double log_hops = std::log2(static_cast<double>(
            std::max<std::uint32_t>(options.hops, 1U)));
        const double level = static_cast<double>(options.level);
        const double gpu_score =
            -4.04216009 + 0.22645204 * log_cells
            + 0.42326433 * log_hops + 0.10705738 * level;
        if (gpu_score > 0.0) {
            return gpu_->halo(input, input_count, options);
        }

        // On a machine without a profitable GPU route, select the CPU engine
        // with a second measured cost boundary.  This protects tiny queries
        // from OpenMP startup/partitioning overhead while still using all CPU
        // cores for sufficiently large CPU-only work.
        const double parallel_score =
            -6.54171511 + 0.36852840 * log_cells
            + 0.23953610 * log_hops + 0.08304386 * level;
        if (cpu_parallel_ && parallel_score > 0.0) {
            return cpu_parallel_->halo(input, input_count, options);
        }
        return cpu_serial_->halo(input, input_count, options);
    }

    DeviceInfo device_info() const override {
        return gpu_ ? gpu_->device_info() : cpu_serial_->device_info();
    }

private:
    std::unique_ptr<HaloEngine> cpu_serial_;
    std::unique_ptr<HaloEngine> cpu_parallel_;
    std::unique_ptr<HaloEngine> gpu_;
};

}  // namespace

std::unique_ptr<HaloEngine> make_engine(Backend backend, int device_ordinal) {
    if (backend == Backend::CpuReference) return make_cpu_engine();
    if (backend == Backend::CpuParallel) return make_parallel_cpu_engine();
    if (backend == Backend::CudaHashFrontier) {
        return make_cuda_engine(device_ordinal);
    }
    try {
        auto gpu = make_cuda_engine(device_ordinal);
        if (gpu && gpu->device_info().available) {
            return std::make_unique<AdaptiveHaloEngine>(
                make_cpu_engine(), make_parallel_cpu_engine(), std::move(gpu));
        }
    } catch (...) {
        // Auto is deliberately resilient; explicit CUDA requests still throw.
    }
    return make_cpu_engine();
}

}  // namespace bafe
