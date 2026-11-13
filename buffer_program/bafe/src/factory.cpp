#include "bafe/bafe.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <unordered_set>

namespace bafe {
namespace {

// Auto is a query-time policy rather than a construction-time alias.  Tiny,
// spatially coherent frontiers are faster on a CPU because a CUDA launch has a
// fixed cost; scattered sets expose enough parallelism even at smaller sizes.
class AdaptiveHaloEngine final : public HaloEngine {
public:
    AdaptiveHaloEngine(std::unique_ptr<HaloEngine> cpu,
                       std::unique_ptr<HaloEngine> gpu)
        : cpu_(std::move(cpu)), gpu_(std::move(gpu)) {}

    HaloResult halo(const CellIndex* input,
                    std::size_t input_count,
                    const QueryOptions& options) override {
        if (!gpu_ || prefer_cpu(input, input_count, options)) {
            return cpu_->halo(input, input_count, options);
        }
        return gpu_->halo(input, input_count, options);
    }

    DeviceInfo device_info() const override {
        return gpu_ ? gpu_->device_info() : cpu_->device_info();
    }

private:
    static bool prefer_cpu(const CellIndex* input,
                           std::size_t count,
                           const QueryOptions& options) {
        if (count == 0U || options.hops == 0U) return true;
        if (count >= 4096U) return false;
        if (input == nullptr) return true;  // The selected backend reports the error.

        const std::size_t samples = std::min<std::size_t>(count, 256U);
        const std::uint32_t coarse_level = options.level > 8U
            ? options.level - 8U
            : 0U;
        std::unordered_set<CellIndex> coarse;
        coarse.reserve(samples * 2U);
        for (std::size_t sample = 0; sample < samples; ++sample) {
            const std::size_t index = sample * count / samples;
            if (!is_canonical(input[index], options.level)) return true;
            coarse.insert(ancestor(input[index], options.level, coarse_level));
        }
        const double dispersion = static_cast<double>(coarse.size()) / samples;
        return !(count >= 64U && dispersion >= 0.25);
    }

    std::unique_ptr<HaloEngine> cpu_;
    std::unique_ptr<HaloEngine> gpu_;
};

}  // namespace

std::unique_ptr<HaloEngine> make_engine(Backend backend, int device_ordinal) {
    if (backend == Backend::CpuReference) return make_cpu_engine();
    if (backend == Backend::CudaHashFrontier) {
        return make_cuda_engine(device_ordinal);
    }
    try {
        auto gpu = make_cuda_engine(device_ordinal);
        if (gpu && gpu->device_info().available) {
            return std::make_unique<AdaptiveHaloEngine>(
                make_cpu_engine(), std::move(gpu));
        }
    } catch (...) {
        // Auto is deliberately resilient; explicit CUDA requests still throw.
    }
    return make_cpu_engine();
}

}  // namespace bafe
