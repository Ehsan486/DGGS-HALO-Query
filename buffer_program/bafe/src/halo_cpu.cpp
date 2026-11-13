#include "bafe/bafe.hpp"

#include "internal.hpp"
#include "topology.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <memory>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace bafe {
namespace {

class CpuHaloEngine final : public HaloEngine {
public:
    HaloResult halo(
        const CellIndex* input,
        std::size_t input_count,
        const QueryOptions& options) override {

        if (options.level > kMaxLevel) {
            throw std::invalid_argument("BAFE supports levels 0 through 28");
        }
        if (input_count != 0U && input == nullptr) {
            throw std::invalid_argument("BAFE input pointer is null");
        }

        const auto started = std::chrono::steady_clock::now();
        HaloResult result;
        result.layer_offsets.push_back(0U);
        result.metrics.backend = "cpu_reference_hash_frontier";
        result.metrics.device = "host CPU";
        result.metrics.input_cells = input_count;

        std::unordered_set<std::uint64_t> visited;
        visited.reserve(input_count * 2U + 64U);
        std::vector<std::uint64_t> frontier;
        frontier.reserve(input_count);

        const auto chunks = detail::make_decode_chunks();
        const auto encode_chunks = detail::make_encode_chunks();
        for (std::size_t index = 0; index < input_count; ++index) {
            if (!is_canonical(input[index], options.level)) {
                throw std::invalid_argument("Input contains a non-canonical or mixed-level BAFE index");
            }
            const detail::Cell cell = detail::path_to_cell_chunked(
                input[index], options.level, chunks.data());
            const std::uint64_t key = detail::encode_coord(cell);
            if (visited.insert(key).second) frontier.push_back(key);
        }
        result.metrics.unique_input_cells = frontier.size();

        const std::uint64_t cap = options.max_output_cells == 0U
            ? std::numeric_limits<std::uint64_t>::max()
            : options.max_output_cells;

        for (std::uint32_t hop = 0; hop < options.hops; ++hop) {
            std::vector<std::uint64_t> next;
            next.reserve(frontier.size() * 2U + 16U);
            result.metrics.touched_neighbor_references += 3ULL * frontier.size();

            for (const std::uint64_t key : frontier) {
                detail::Cell local[3];
                std::uint32_t seams = 0U;
                std::uint32_t macrochart_crossings = 0U;
                std::uint32_t paired_direct = 0U;
                if (options.chart_mode == ChartMode::RhombicMacrocharts) {
                    detail::neighbors_rhombic_host(
                        detail::decode_coord(key), options.level, local, &seams,
                        &macrochart_crossings, &paired_direct);
                } else {
                    detail::neighbors_host(
                        detail::decode_coord(key), options.level, local, &seams);
                }
                result.metrics.root_edge_crossings += seams;
                result.metrics.macrochart_boundary_crossings += macrochart_crossings;
                result.metrics.paired_edge_direct_transforms += paired_direct;
                for (const detail::Cell& neighbor : local) {
                    const std::uint64_t candidate = detail::encode_coord(neighbor);
                    if (visited.insert(candidate).second) {
                        if (result.cells.size() >= cap) {
                            throw std::length_error("BAFE halo exceeded max_output_cells");
                        }
                        next.push_back(candidate);
                        result.cells.push_back(
                            detail::cell_to_path_chunked(
                                neighbor, options.level, encode_chunks.data()));
                    }
                }
            }

            if (options.deterministic_layers) {
                const auto begin = result.cells.begin() +
                    static_cast<std::ptrdiff_t>(result.layer_offsets.back());
                std::sort(begin, result.cells.end());
            }
            result.layer_offsets.push_back(result.cells.size());
            frontier.swap(next);
        }

        result.metrics.output_cells = result.cells.size();
        result.metrics.device_to_host_bytes = 0U;
        result.metrics.host_to_device_bytes = 0U;
        result.metrics.device_peak_bytes = 0U;
        result.metrics.end_to_end_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
        return result;
    }

    DeviceInfo device_info() const override {
        return DeviceInfo{true, "host CPU", 0, 0, 0};
    }
};

}  // namespace

std::unique_ptr<HaloEngine> make_cpu_engine() {
    return std::make_unique<CpuHaloEngine>();
}

}  // namespace bafe
