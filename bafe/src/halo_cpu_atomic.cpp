// CPU realization of the same algorithm as the CUDA backend: one flat open
// addressing table claimed with compare and exchange, per thread output
// fragments concatenated after each ring, and a table sized from the same
// output bound as the GPU workspace (a growth path remains for bounds that
// exceed the pre-sizing limit).  With one thread it is a flat table serial
// reference (no node based container); with several threads it is the
// multicore baseline whose only synchronization is the atomic claim of a slot
// and one barrier per ring.
//
// Revision 3 (2026-11-12): the discovered keys live in one scratch array that
// is kept between calls (the frontier is a slice of it, so no array is
// allocated per ring), and QueryOptions::reuse_output delivers the result in
// host memory that the engine reuses instead of a newly allocated vector.
// The internal timer now also covers input validation.
//
// Revision 2 (2026-09-17): per thread state is padded to its own cache lines
// (the first version kept the per thread fragment headers and counters in
// adjacent words, and the resulting false sharing made two threads slower
// than one); the table is kept between calls and only its used prefix is
// cleared; the expansion loop uses dynamic scheduling so that heterogeneous
// cores do not idle at the ring barrier.

#include "bafe/bafe.hpp"

#include "internal.hpp"
#include "topology.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#if BAFE_HAS_OPENMP
#include <omp.h>
#endif

namespace bafe {
namespace {

constexpr std::uint64_t kEmpty = 0xffffffffffffffffULL;
// Largest table that is allocated up front from the output bound (2^28 slots,
// 2 GB); a larger bound falls back to growth between rings.
constexpr std::uint64_t kPresizeLimit = std::uint64_t(1) << 28U;
constexpr std::int64_t kChunk = 2048;

inline std::uint64_t mix64(std::uint64_t value) noexcept {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

inline std::uint64_t next_power_of_two(std::uint64_t value) {
    std::uint64_t result = 1U;
    while (result < value) {
        if (result > std::numeric_limits<std::uint64_t>::max() / 2U) {
            throw std::overflow_error("BAFE atomic CPU table overflow");
        }
        result <<= 1U;
    }
    return result;
}

// Same bound as the CUDA backend: a k hop disk without its source has at most
// 3k(k+1)/2 cells on the refined Pentakis surface.
inline std::uint64_t graph_output_bound(std::uint64_t sources, std::uint32_t hops) {
    if (hops == 0U || sources == 0U) return 0U;
    const std::uint64_t h = hops;
    if (h > (std::numeric_limits<std::uint64_t>::max() / 3U) / (h + 1U)) {
        throw std::overflow_error("BAFE automatic output bound overflow");
    }
    const std::uint64_t per_source = 3U * h * (h + 1U) / 2U;
    if (sources > std::numeric_limits<std::uint64_t>::max() / per_source) {
        throw std::overflow_error("BAFE automatic output bound overflow");
    }
    return sources * per_source;
}

class FlatTable {
public:
    // Makes the first `capacity` slots empty, allocating a larger array only
    // when the previous one is too small.  Only the used prefix is cleared.
    void prepare(std::uint64_t capacity, int threads) {
        if (capacity > allocated_) {
            slots_.reset();  // release before allocating to bound the peak
            slots_.reset(new std::atomic<std::uint64_t>[capacity]);
            allocated_ = capacity;
        }
        capacity_ = capacity;
        mask_ = capacity - 1U;
        std::atomic<std::uint64_t>* slots = slots_.get();
        const std::int64_t count = static_cast<std::int64_t>(capacity);
#if BAFE_HAS_OPENMP
#pragma omp parallel for schedule(static) num_threads(threads)
#endif
        for (std::int64_t index = 0; index < count; ++index) {
            slots[static_cast<std::size_t>(index)].store(kEmpty, std::memory_order_relaxed);
        }
        (void)threads;
    }

    // Returns true when the key was absent and has now been claimed; throws
    // when the table is full (cannot happen with a table sized from the bound).
    bool insert(std::uint64_t key) {
        std::atomic<std::uint64_t>* slots = slots_.get();
        std::uint64_t position = mix64(key) & mask_;
        for (std::uint64_t probe = 0; probe < capacity_; ++probe) {
            std::uint64_t seen = slots[position].load(std::memory_order_relaxed);
            if (seen == key) return false;
            if (seen == kEmpty) {
                if (slots[position].compare_exchange_strong(
                        seen, key, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    return true;
                }
                if (seen == key) return false;
                --probe;
                continue;  // another key claimed the slot; re-examine it
            }
            position = (position + 1U) & mask_;
        }
        throw std::length_error("BAFE atomic CPU visited table exhausted");
    }

    std::uint64_t capacity() const noexcept { return capacity_; }
    std::uint64_t allocated() const noexcept { return allocated_; }

private:
    std::unique_ptr<std::atomic<std::uint64_t>[]> slots_;
    std::uint64_t capacity_ = 0U;
    std::uint64_t allocated_ = 0U;
    std::uint64_t mask_ = 0U;
};

// Host array that is kept between calls, grows geometrically, and is never
// zero filled: the caller writes every value before reading it.
class RawBuffer {
public:
    // Guarantees room for `needed` values and preserves the first `keep`.
    void ensure(std::uint64_t needed, std::uint64_t keep) {
        if (needed <= capacity_) return;
        const std::uint64_t grown = std::max<std::uint64_t>(
            needed, capacity_ + capacity_ / 2U + 1024U);
        std::unique_ptr<std::uint64_t[]> fresh(new std::uint64_t[grown]);
        if (keep != 0U) std::copy(data_.get(), data_.get() + keep, fresh.get());
        data_ = std::move(fresh);
        capacity_ = grown;
    }
    std::uint64_t* data() noexcept { return data_.get(); }

private:
    std::unique_ptr<std::uint64_t[]> data_;
    std::uint64_t capacity_ = 0U;
};

// Per thread state on its own cache lines: the fragment header (written on
// every append) and the counters never share a line with another thread's.
struct alignas(128) Worker {
    std::vector<std::uint64_t> fragment;
    std::uint64_t seams = 0U;
    std::uint64_t macro = 0U;
    std::uint64_t direct = 0U;
};

class AtomicCpuHaloEngine final : public HaloEngine {
public:
    explicit AtomicCpuHaloEngine(std::uint32_t requested_threads) {
#if BAFE_HAS_OPENMP
        const int maximum = std::max(1, omp_get_max_threads());
        threads_ = requested_threads == 0U
            ? maximum
            : std::max(1, std::min<int>(static_cast<int>(requested_threads), maximum));
#else
        (void)requested_threads;
        threads_ = 1;
#endif
        workers_.resize(static_cast<std::size_t>(threads_));
    }

    HaloResult halo(
        const CellIndex* input,
        std::size_t input_count,
        const QueryOptions& options) override {

        const auto started = std::chrono::steady_clock::now();
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

        HaloResult result;
        result.layer_offsets.push_back(0U);
        result.metrics.backend = threads_ == 1
            ? "cpu_flat_hash_frontier"
            : "cpu_atomic_hash_frontier";
        result.metrics.device = "host CPU (" + std::to_string(threads_) +
            (threads_ == 1 ? " thread, flat table)" : " OpenMP threads, atomic flat table)");
        result.metrics.input_cells = input_count;

        const auto decode_chunks = detail::make_decode_chunks();
        const auto encode_chunks = detail::make_encode_chunks();
        const std::uint64_t cap = options.max_output_cells == 0U
            ? std::numeric_limits<std::uint64_t>::max()
            : options.max_output_cells;

        // Table sized from the same bound as the GPU workspace (load at most
        // one half when every candidate is new).  Beyond the pre-sizing limit
        // the table grows between rings and re-inserts the discovered keys.
        const std::uint64_t bound = options.max_output_cells == 0U
            ? graph_output_bound(input_count, options.hops)
            : options.max_output_cells;
        const std::uint64_t key_bound = std::uint64_t(input_count) + bound;
        const bool presized = key_bound <= kPresizeLimit / 2U;
        const std::uint64_t initial_capacity = presized
            ? next_power_of_two(std::max<std::uint64_t>(1024U, 2U * key_bound))
            : next_power_of_two(std::max<std::uint64_t>(1024U, 4U * input_count));
        table_.prepare(initial_capacity, threads_);

        // Delivery: a newly allocated array owned by the caller, or host
        // memory that this engine keeps and reuses for later calls.
        const bool reuse = options.reuse_output;
        if (!reuse) {
            result.cells.reserve(static_cast<std::size_t>(
                std::min<std::uint64_t>(bound, std::uint64_t(1) << 27U)));
        }

        // keys_ holds every discovered compact index in discovery order: the
        // unique seeds first, then ring after ring.  It is scratch memory kept
        // between calls (like the device buffers of the CUDA backend).  The
        // frontier of a ring is the slice keys_[frontier_begin, key_count).
        std::uint64_t key_count = 0U;
        std::uint64_t frontier_begin = 0U;
        std::uint64_t out_count = 0U;
        {
            const std::int64_t count = static_cast<std::int64_t>(input_count);
            const std::int64_t share = count / threads_ + 16;
            for (Worker& worker : workers_) {
                worker.fragment.clear();
                worker.fragment.reserve(static_cast<std::size_t>(share));
            }
#if BAFE_HAS_OPENMP
#pragma omp parallel num_threads(threads_)
            {
                Worker& worker = workers_[static_cast<std::size_t>(omp_get_thread_num())];
#pragma omp for schedule(dynamic, kChunk)
                for (std::int64_t index = 0; index < count; ++index) {
#else
            {
                Worker& worker = workers_[0];
                for (std::int64_t index = 0; index < count; ++index) {
#endif
                    const std::uint64_t key = detail::encode_coord(
                        detail::path_to_cell_chunked(
                            input[static_cast<std::size_t>(index)], options.level,
                            decode_chunks.data()));
                    if (table_.insert(key)) worker.fragment.push_back(key);
                }
            }
            key_count = append_fragments(0U);
        }
        result.metrics.unique_input_cells = key_count;

        for (std::uint32_t hop = 0; hop < options.hops; ++hop) {
            const std::uint64_t frontier_count = key_count - frontier_begin;
            if (frontier_count == 0U) {
                result.layer_offsets.push_back(out_count);
                continue;
            }
            if (!presized) {
                // Keep the realized load at or below one half even if every
                // candidate of this ring is new; rebuild the table when needed.
                const std::uint64_t worst_case_keys = key_count + 3ULL * frontier_count;
                if (worst_case_keys * 2ULL > table_.capacity()) {
                    table_.prepare(next_power_of_two(worst_case_keys * 2ULL), threads_);
                    const std::uint64_t* keys = keys_.data();
                    const std::int64_t count = static_cast<std::int64_t>(key_count);
#if BAFE_HAS_OPENMP
#pragma omp parallel for schedule(dynamic, kChunk) num_threads(threads_)
#endif
                    for (std::int64_t index = 0; index < count; ++index) {
                        (void)table_.insert(keys[static_cast<std::size_t>(index)]);
                    }
                }
            }

            result.metrics.touched_neighbor_references += 3ULL * frontier_count;
            const std::uint64_t* frontier_keys = keys_.data() + frontier_begin;
            const std::int64_t frontier_size = static_cast<std::int64_t>(frontier_count);
            const std::int64_t share = 3 * (frontier_size / threads_) + 64;
            for (Worker& worker : workers_) {
                worker.fragment.clear();
                worker.fragment.reserve(static_cast<std::size_t>(share));
                worker.seams = 0U;
                worker.macro = 0U;
                worker.direct = 0U;
            }
            const bool rhombic = options.chart_mode == ChartMode::RhombicMacrocharts;

#if BAFE_HAS_OPENMP
#pragma omp parallel num_threads(threads_)
            {
                Worker& worker = workers_[static_cast<std::size_t>(omp_get_thread_num())];
                std::vector<std::uint64_t>& fragment = worker.fragment;
                std::uint64_t seam_total = 0U;
                std::uint64_t macro_total = 0U;
                std::uint64_t direct_total = 0U;
#pragma omp for schedule(dynamic, kChunk)
                for (std::int64_t index = 0; index < frontier_size; ++index) {
#else
            {
                Worker& worker = workers_[0];
                std::vector<std::uint64_t>& fragment = worker.fragment;
                std::uint64_t seam_total = 0U;
                std::uint64_t macro_total = 0U;
                std::uint64_t direct_total = 0U;
                for (std::int64_t index = 0; index < frontier_size; ++index) {
#endif
                    detail::Cell local[3];
                    std::uint32_t seams = 0U;
                    std::uint32_t macrochart_crossings = 0U;
                    std::uint32_t paired_direct = 0U;
                    const detail::Cell source = detail::decode_coord(
                        frontier_keys[static_cast<std::size_t>(index)]);
                    if (rhombic) {
                        detail::neighbors_rhombic_host(
                            source, options.level, local, &seams,
                            &macrochart_crossings, &paired_direct);
                    } else {
                        detail::neighbors_host(source, options.level, local, &seams);
                    }
                    seam_total += seams;
                    macro_total += macrochart_crossings;
                    direct_total += paired_direct;
                    for (const detail::Cell& neighbor : local) {
                        const std::uint64_t candidate = detail::encode_coord(neighbor);
                        if (table_.insert(candidate)) fragment.push_back(candidate);
                    }
                }
                worker.seams = seam_total;
                worker.macro = macro_total;
                worker.direct = direct_total;
            }
            std::uint64_t ring_count = 0U;
            for (const Worker& worker : workers_) {
                result.metrics.root_edge_crossings += worker.seams;
                result.metrics.macrochart_boundary_crossings += worker.macro;
                result.metrics.paired_edge_direct_transforms += worker.direct;
                ring_count += worker.fragment.size();
            }
            if (ring_count > cap - std::min<std::uint64_t>(cap, out_count)) {
                throw std::length_error("BAFE halo exceeded max_output_cells");
            }

            // Append the new ring behind the discovered keys; it becomes the
            // frontier of the next ring.
            const std::uint64_t ring_begin = key_count;
            key_count = append_fragments(key_count);

            CellIndex* cells = nullptr;
            if (reuse) {
                out_.ensure(out_count + ring_count, out_count);
                cells = out_.data() + out_count;
            } else {
                result.cells.resize(static_cast<std::size_t>(out_count + ring_count));
                cells = result.cells.data() + out_count;
            }
            const std::uint64_t* next_keys = keys_.data() + ring_begin;
            const std::int64_t next_size = static_cast<std::int64_t>(ring_count);
#if BAFE_HAS_OPENMP
#pragma omp parallel for schedule(dynamic, kChunk) num_threads(threads_)
#endif
            for (std::int64_t index = 0; index < next_size; ++index) {
                cells[static_cast<std::size_t>(index)] = detail::cell_to_path_chunked(
                    detail::decode_coord(next_keys[static_cast<std::size_t>(index)]),
                    options.level, encode_chunks.data());
            }
            if (options.deterministic_layers) {
                std::sort(cells, cells + ring_count);
            }
            out_count += ring_count;
            result.layer_offsets.push_back(out_count);
            frontier_begin = ring_begin;
        }

        if (reuse) {
            result.view = out_.data();
            result.view_count = static_cast<std::size_t>(out_count);
        }
        result.metrics.output_cells = out_count;
        result.metrics.device_peak_bytes = table_.capacity() * sizeof(std::uint64_t);
        std::uint64_t hash_value = table_.capacity();
        while (hash_value > 1U) {
            ++result.metrics.hash_capacity_log2;
            hash_value >>= 1U;
        }
        result.metrics.end_to_end_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
        return result;
    }

    DeviceInfo device_info() const override {
        return DeviceInfo{true, "host CPU (atomic flat table)", 0, 0, 0};
    }

private:
    // Copies the per thread fragments, in thread order, behind the first
    // `offset` keys of keys_ and returns the new key count.  The order of
    // cells inside a ring is not part of the query semantics.
    std::uint64_t append_fragments(std::uint64_t offset) {
        std::vector<std::uint64_t> offsets(workers_.size() + 1U, offset);
        for (std::size_t index = 0; index < workers_.size(); ++index) {
            offsets[index + 1U] = offsets[index] + workers_[index].fragment.size();
        }
        keys_.ensure(offsets.back(), offset);
        std::uint64_t* destination = keys_.data();
        const std::int64_t count = static_cast<std::int64_t>(workers_.size());
#if BAFE_HAS_OPENMP
#pragma omp parallel for schedule(static) num_threads(threads_)
#endif
        for (std::int64_t index = 0; index < count; ++index) {
            const auto& fragment = workers_[static_cast<std::size_t>(index)].fragment;
            std::copy(fragment.begin(), fragment.end(),
                      destination + offsets[static_cast<std::size_t>(index)]);
        }
        return offsets.back();
    }

    int threads_ = 1;
    FlatTable table_;
    std::vector<Worker> workers_;
    RawBuffer keys_;  // discovered compact indices (scratch, kept between calls)
    RawBuffer out_;   // public indices of the result in the reused buffer delivery

};

}  // namespace

std::unique_ptr<HaloEngine> make_atomic_cpu_engine(std::uint32_t thread_count) {
    return std::make_unique<AtomicCpuHaloEngine>(thread_count);
}

}  // namespace bafe
