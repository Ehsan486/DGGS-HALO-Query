#include "bafe/bafe.hpp"

#include "internal.hpp"
#include "topology.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#if BAFE_HAS_OPENMP
#include <omp.h>
#endif

namespace bafe {
namespace {

std::uint64_t mix64_host(std::uint64_t value) noexcept {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

std::size_t next_power_of_two_size(std::size_t value) {
    std::size_t result = 1U;
    while (result < value) {
        if (result > std::numeric_limits<std::size_t>::max() / 2U) {
            throw std::overflow_error("BAFE parallel CPU shard-count overflow");
        }
        result <<= 1U;
    }
    return result;
}

class ParallelCpuHaloEngine final : public HaloEngine {
public:
    explicit ParallelCpuHaloEngine(std::uint32_t requested_threads) {
#if BAFE_HAS_OPENMP
        const int maximum = std::max(1, omp_get_max_threads());
        threads_ = requested_threads == 0U
            ? maximum
            : std::max(1, std::min<int>(static_cast<int>(requested_threads), maximum));
#else
        (void)requested_threads;
        threads_ = 1;
#endif
        shard_count_ = next_power_of_two_size(
            std::max<std::size_t>(64U, static_cast<std::size_t>(threads_) * 8U));
    }

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
        for (std::size_t index = 0; index < input_count; ++index) {
            if (!is_canonical(input[index], options.level)) {
                throw std::invalid_argument(
                    "Input contains a non-canonical or mixed-level BAFE index");
            }
        }

        const auto started = std::chrono::steady_clock::now();
        HaloResult result;
        result.layer_offsets.push_back(0U);
        result.metrics.backend = "cpu_parallel_partitioned_frontier";
        result.metrics.device = "host CPU (" + std::to_string(threads_) +
            " OpenMP threads)";
        result.metrics.input_cells = input_count;

        const auto decode_chunks = detail::make_decode_chunks();
        const auto encode_chunks = detail::make_encode_chunks();
        std::vector<std::uint64_t> coordinates(input_count);

#if BAFE_HAS_OPENMP
#pragma omp parallel for schedule(static) num_threads(threads_)
#endif
        for (std::int64_t index = 0;
             index < static_cast<std::int64_t>(input_count); ++index) {
            coordinates[static_cast<std::size_t>(index)] = detail::encode_coord(
                detail::path_to_cell_chunked(
                    input[static_cast<std::size_t>(index)], options.level,
                    decode_chunks.data()));
        }

        std::vector<std::unordered_set<std::uint64_t>> visited(shard_count_);
        for (auto& shard : visited) {
            shard.max_load_factor(0.70F);
            shard.reserve(input_count / shard_count_ + 16U);
        }

        std::vector<std::uint64_t> frontier;
        insert_partitioned(coordinates, visited, frontier);
        result.metrics.unique_input_cells = frontier.size();

        const std::uint64_t cap = options.max_output_cells == 0U
            ? std::numeric_limits<std::uint64_t>::max()
            : options.max_output_cells;

        for (std::uint32_t hop = 0; hop < options.hops; ++hop) {
            if (frontier.size() > std::numeric_limits<std::size_t>::max() / 3U) {
                throw std::overflow_error("BAFE parallel CPU candidate overflow");
            }
            std::vector<std::uint64_t> candidates(frontier.size() * 3U);
            std::vector<std::uint64_t> seam_counts(static_cast<std::size_t>(threads_), 0U);
            std::vector<std::uint64_t> macro_counts(static_cast<std::size_t>(threads_), 0U);
            std::vector<std::uint64_t> direct_counts(static_cast<std::size_t>(threads_), 0U);

#if BAFE_HAS_OPENMP
#pragma omp parallel num_threads(threads_)
            {
                const int worker = omp_get_thread_num();
#pragma omp for schedule(static)
                for (std::int64_t index = 0;
                     index < static_cast<std::int64_t>(frontier.size()); ++index) {
#else
            {
                const int worker = 0;
                for (std::int64_t index = 0;
                     index < static_cast<std::int64_t>(frontier.size()); ++index) {
#endif
                    detail::Cell local[3];
                    std::uint32_t seams = 0U;
                    std::uint32_t macrochart_crossings = 0U;
                    std::uint32_t paired_direct = 0U;
                    const detail::Cell source = detail::decode_coord(
                        frontier[static_cast<std::size_t>(index)]);
                    if (options.chart_mode == ChartMode::RhombicMacrocharts) {
                        detail::neighbors_rhombic_host(
                            source, options.level, local, &seams,
                            &macrochart_crossings, &paired_direct);
                    } else {
                        detail::neighbors_host(source, options.level, local, &seams);
                    }
                    const std::size_t base = 3U * static_cast<std::size_t>(index);
                    candidates[base] = detail::encode_coord(local[0]);
                    candidates[base + 1U] = detail::encode_coord(local[1]);
                    candidates[base + 2U] = detail::encode_coord(local[2]);
                    seam_counts[static_cast<std::size_t>(worker)] += seams;
                    macro_counts[static_cast<std::size_t>(worker)] += macrochart_crossings;
                    direct_counts[static_cast<std::size_t>(worker)] += paired_direct;
                }
            }

            result.metrics.touched_neighbor_references += candidates.size();
            for (int worker = 0; worker < threads_; ++worker) {
                const std::size_t offset = static_cast<std::size_t>(worker);
                result.metrics.root_edge_crossings += seam_counts[offset];
                result.metrics.macrochart_boundary_crossings += macro_counts[offset];
                result.metrics.paired_edge_direct_transforms += direct_counts[offset];
            }

            std::vector<std::uint64_t> next;
            insert_partitioned(candidates, visited, next);
            if (next.size() > cap - std::min<std::uint64_t>(
                    cap, static_cast<std::uint64_t>(result.cells.size()))) {
                throw std::length_error("BAFE halo exceeded max_output_cells");
            }

            const std::size_t output_begin = result.cells.size();
            result.cells.resize(output_begin + next.size());
#if BAFE_HAS_OPENMP
#pragma omp parallel for schedule(static) num_threads(threads_)
#endif
            for (std::int64_t index = 0;
                 index < static_cast<std::int64_t>(next.size()); ++index) {
                const detail::Cell cell = detail::decode_coord(
                    next[static_cast<std::size_t>(index)]);
                result.cells[output_begin + static_cast<std::size_t>(index)] =
                    detail::cell_to_path_chunked(
                        cell, options.level, encode_chunks.data());
            }
            if (options.deterministic_layers) {
                std::sort(
                    result.cells.begin() + static_cast<std::ptrdiff_t>(output_begin),
                    result.cells.end());
            }
            result.layer_offsets.push_back(result.cells.size());
            frontier.swap(next);
        }

        result.metrics.output_cells = result.cells.size();
        result.metrics.end_to_end_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
        return result;
    }

    DeviceInfo device_info() const override {
        return DeviceInfo{true, "host CPU (parallel)", 0, 0, 0};
    }

private:
    std::size_t shard_for(std::uint64_t key) const noexcept {
        return static_cast<std::size_t>(mix64_host(key)) & (shard_count_ - 1U);
    }

    void insert_partitioned(
        const std::vector<std::uint64_t>& candidates,
        std::vector<std::unordered_set<std::uint64_t>>& visited,
        std::vector<std::uint64_t>& discovered) const {

        const std::size_t workers = static_cast<std::size_t>(threads_);
        std::vector<std::size_t> counts(workers * shard_count_, 0U);

#if BAFE_HAS_OPENMP
#pragma omp parallel num_threads(threads_)
        {
            const std::size_t worker = static_cast<std::size_t>(omp_get_thread_num());
#pragma omp for schedule(static)
            for (std::int64_t index = 0;
                 index < static_cast<std::int64_t>(candidates.size()); ++index) {
                const std::size_t shard = shard_for(
                    candidates[static_cast<std::size_t>(index)]);
                ++counts[worker * shard_count_ + shard];
            }
        }
#else
        for (const std::uint64_t key : candidates) ++counts[shard_for(key)];
#endif

        std::vector<std::size_t> shard_offsets(shard_count_ + 1U, 0U);
        for (std::size_t shard = 0; shard < shard_count_; ++shard) {
            std::size_t total = 0U;
            for (std::size_t worker = 0; worker < workers; ++worker) {
                total += counts[worker * shard_count_ + shard];
            }
            shard_offsets[shard + 1U] = shard_offsets[shard] + total;
        }

        std::vector<std::size_t> cursors(workers * shard_count_, 0U);
        for (std::size_t shard = 0; shard < shard_count_; ++shard) {
            std::size_t cursor = shard_offsets[shard];
            for (std::size_t worker = 0; worker < workers; ++worker) {
                const std::size_t slot = worker * shard_count_ + shard;
                cursors[slot] = cursor;
                cursor += counts[slot];
            }
        }

        std::vector<std::uint64_t> partitioned(candidates.size());
#if BAFE_HAS_OPENMP
#pragma omp parallel num_threads(threads_)
        {
            const std::size_t worker = static_cast<std::size_t>(omp_get_thread_num());
#pragma omp for schedule(static)
            for (std::int64_t index = 0;
                 index < static_cast<std::int64_t>(candidates.size()); ++index) {
                const std::uint64_t key = candidates[static_cast<std::size_t>(index)];
                const std::size_t shard = shard_for(key);
                partitioned[cursors[worker * shard_count_ + shard]++] = key;
            }
        }
#else
        for (const std::uint64_t key : candidates) {
            const std::size_t shard = shard_for(key);
            partitioned[cursors[shard]++] = key;
        }
#endif

        std::vector<std::vector<std::uint64_t>> per_shard(shard_count_);
#if BAFE_HAS_OPENMP
#pragma omp parallel for schedule(dynamic, 1) num_threads(threads_)
#endif
        for (std::int64_t shard_index = 0;
             shard_index < static_cast<std::int64_t>(shard_count_); ++shard_index) {
            const std::size_t shard = static_cast<std::size_t>(shard_index);
            const std::size_t begin = shard_offsets[shard];
            const std::size_t end = shard_offsets[shard + 1U];
            auto& set = visited[shard];
            auto& output = per_shard[shard];
            output.reserve(end - begin);
            set.reserve(set.size() + end - begin + 8U);
            for (std::size_t index = begin; index < end; ++index) {
                const std::uint64_t key = partitioned[index];
                if (set.insert(key).second) output.push_back(key);
            }
        }

        std::vector<std::size_t> output_offsets(shard_count_ + 1U, 0U);
        for (std::size_t shard = 0; shard < shard_count_; ++shard) {
            output_offsets[shard + 1U] =
                output_offsets[shard] + per_shard[shard].size();
        }
        discovered.resize(output_offsets.back());
#if BAFE_HAS_OPENMP
#pragma omp parallel for schedule(static) num_threads(threads_)
#endif
        for (std::int64_t shard_index = 0;
             shard_index < static_cast<std::int64_t>(shard_count_); ++shard_index) {
            const std::size_t shard = static_cast<std::size_t>(shard_index);
            std::copy(per_shard[shard].begin(), per_shard[shard].end(),
                      discovered.begin() + static_cast<std::ptrdiff_t>(
                          output_offsets[shard]));
        }
    }

    int threads_ = 1;
    std::size_t shard_count_ = 64U;
};

}  // namespace

std::unique_ptr<HaloEngine> make_parallel_cpu_engine(std::uint32_t thread_count) {
    return std::make_unique<ParallelCpuHaloEngine>(thread_count);
}

}  // namespace bafe

