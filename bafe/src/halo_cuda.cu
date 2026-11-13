// CUDA halo backend with tuning and ablation switches.
//
// With every CudaTuning switch at its default value this file executes the
// same operations, in the same order, as the version measured for the
// manuscript (nine control memsets, full table clear, SplitMix placement, one
// thread per frontier cell with three probes, warp aggregated append, pageable
// std::vector output).  Each switch changes exactly one mechanism.

#include "bafe/bafe.hpp"

#include "atlas.hpp"
#include "internal.hpp"

#include <cooperative_groups.h>
#include <cuda_runtime.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace bafe {
namespace {

using detail::Cell;
using detail::DecodeChunk;
using detail::EncodeChunk;
using detail::Transition;
namespace cg = cooperative_groups;

constexpr std::uint64_t kEmptyKey = 0xffffffffffffffffULL;
constexpr std::uint32_t kNoSlot = 0xffffffffU;
constexpr std::uint8_t kNoArrival = 3U;
constexpr int kThreads = 256;
// Lazy clearing is used only when the occupied slots are at most this
// fraction of the table; the slot index array is sized accordingly.
constexpr std::uint64_t kLazyClearDivisor = 16U;

__constant__ Transition d_atlas[180];
__constant__ DecodeChunk d_decode_chunks[512];
__constant__ EncodeChunk d_encode_chunks[512];

inline void cuda_check(cudaError_t status, const char* operation) {
    if (status == cudaSuccess) return;
    std::ostringstream message;
    message << operation << ": " << cudaGetErrorString(status);
    throw std::runtime_error(message.str());
}

__device__ inline std::uint64_t mix64(std::uint64_t x) noexcept {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27U)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31U);
}

// Table slot of the first probe.  hash_mode 0 is the manuscript mixer.
// hash_mode 1 uses the packed coordinate bits directly (no mixing).
// hash_mode 2 gives every 64 x 64 cell block of a face a mixed window of
// 2^14 slots and places cells inside the window in Morton order with a
// stride of two, so that neighbouring candidates share cache lines while the
// window keeps its own load at or below one half.
__device__ inline std::uint64_t home_slot(
    std::uint64_t key, std::uint64_t mask, int hash_mode) noexcept {
    if (hash_mode == 1) return (key >> 1U) & mask;
    if (hash_mode == 2) {
        const std::uint32_t i = std::uint32_t((key >> 29U) & detail::kCoordMask28);
        const std::uint32_t j = std::uint32_t((key >> 1U) & detail::kCoordMask28);
        const std::uint64_t block_key = (key & (0x3FULL << 57U)) |
            (std::uint64_t(i >> 6U) << 29U) | (std::uint64_t(j >> 6U) << 1U);
        std::uint32_t low = 0U;
        #pragma unroll
        for (int b = 0; b < 6; ++b) {
            low |= ((i >> b) & 1U) << (2 * b + 1);
            low |= ((j >> b) & 1U) << (2 * b);
        }
        low = (low << 1U) | std::uint32_t(key & 1ULL);
        return (mix64(block_key) + (std::uint64_t(low) << 1U)) & mask;
    }
    return mix64(key) & mask;
}

__device__ inline bool canonical_path(CellIndex id, std::uint32_t level) noexcept {
    if ((id >> 63U) != 0U || ((id >> kFaceShift) & 0x3fULL) >= kRootFaceCount ||
        level > kMaxLevel) {
        return false;
    }
    const std::uint32_t sentinel = kRootSentinelBit - 2U * level;
    if ((id & (CellIndex(1) << sentinel)) == 0U) return false;
    const CellIndex low_mask = sentinel == 0U
        ? 0U
        : (CellIndex(1) << sentinel) - 1U;
    return (id & low_mask) == 0U;
}

__device__ inline Cell path_to_cell(CellIndex id, std::uint32_t level) noexcept {
    Cell cell{std::uint32_t((id >> kFaceShift) & 0x3fULL), 0U, 0U, 0U};
    std::uint32_t processed = 0U;
    while (level - processed >= 4U) {
        const std::uint32_t shift = kFaceShift - 2U * (processed + 4U);
        const std::uint32_t packed = std::uint32_t((id >> shift) & 0xffULL);
        const DecodeChunk entry = d_decode_chunks[cell.orientation * 256U + packed];
        cell.i = (cell.i << 4U) | entry.delta_i;
        cell.j = (cell.j << 4U) | entry.delta_j;
        cell.orientation = entry.end_orientation;
        processed += 4U;
    }
    while (processed < level) {
        ++processed;
        const std::uint32_t shift = kFaceShift - 2U * processed;
        cell = detail::child_once(cell, std::uint32_t((id >> shift) & 3ULL));
    }
    return cell;
}

__device__ inline CellIndex cell_to_path(Cell cell, std::uint32_t level) noexcept {
    CellIndex id = CellIndex(cell.face) << kFaceShift;
    std::uint32_t remaining = level;
    while (remaining >= 4U) {
        const std::uint32_t index =
            cell.orientation * 256U + (cell.i & 15U) * 16U + (cell.j & 15U);
        const EncodeChunk entry = d_encode_chunks[index];
        id |= CellIndex(entry.packed_digits) << (kFaceShift - 2U * remaining);
        cell.i >>= 4U;
        cell.j >>= 4U;
        cell.orientation = entry.parent_orientation;
        remaining -= 4U;
    }
    while (remaining > 0U) {
        const std::uint32_t digit = detail::inverse_child_digit(cell);
        id |= CellIndex(digit) << (kFaceShift - 2U * remaining);
        cell = detail::parent_once(cell);
        --remaining;
    }
    return id | (CellIndex(1) << (kRootSentinelBit - 2U * level));
}

__device__ inline Cell boundary_up_device(
    std::uint32_t face,
    std::uint32_t edge,
    std::uint32_t parameter,
    std::uint32_t n) noexcept {
    if (edge == 0U) return Cell{face, n - 1U - parameter, parameter, 0U};
    if (edge == 1U) return Cell{face, 0U, parameter, 0U};
    return Cell{face, parameter, 0U, 0U};
}

// Index, in the candidate's own neighbour list, of the neighbour that leads
// back across the given edge.  For an upward cell, neighbour 0 crosses edge
// 0, neighbour 1 crosses edge 2 (j == 0) and neighbour 2 crosses edge 1
// (i == 0); inside a face the same table applies to the parent's edge index.
__device__ inline std::uint8_t reverse_index(std::uint32_t edge) noexcept {
    return edge == 0U ? 0U : (edge == 1U ? 2U : 1U);
}

__device__ inline Cell cross_edge_device(
    const Cell& cell,
    std::uint32_t edge,
    std::uint32_t parameter,
    std::uint32_t n,
    bool rhombic_macrocharts,
    std::uint32_t& paired_direct,
    std::uint8_t& reverse) noexcept {
    if (rhombic_macrocharts && edge == 0U) {
        ++paired_direct;
        reverse = 0U;
        return Cell{cell.face ^ 1U, parameter, n - 1U - parameter, 0U};
    }
    const Transition transition = d_atlas[cell.face * 3U + edge];
    const std::uint32_t mapped = transition.reverse
        ? n - 1U - parameter
        : parameter;
    reverse = reverse_index(transition.dst_edge);
    return boundary_up_device(
        transition.dst_face, transition.dst_edge, mapped, n);
}

// The three edge adjacent cells and, for each, the index under which the
// source cell appears in that candidate's own neighbour list.
__device__ inline void neighbors_device(
    const Cell& cell,
    std::uint32_t level,
    bool rhombic_macrocharts,
    Cell output[3],
    std::uint8_t reverse[3],
    std::uint32_t& seams,
    std::uint32_t& macrochart_crossings,
    std::uint32_t& paired_direct) noexcept {
    const std::uint32_t n = 1U << level;
    seams = 0U;
    macrochart_crossings = 0U;
    paired_direct = 0U;
    reverse[0] = 0U;
    reverse[1] = 2U;
    reverse[2] = 1U;
    if (cell.orientation == 1U) {
        output[0] = Cell{cell.face, cell.i, cell.j, 0U};
        output[1] = Cell{cell.face, cell.i + 1U, cell.j, 0U};
        output[2] = Cell{cell.face, cell.i, cell.j + 1U, 0U};
        return;
    }
    if (cell.i + cell.j + 1U < n) {
        output[0] = Cell{cell.face, cell.i, cell.j, 1U};
    } else {
        output[0] = cross_edge_device(
            cell, 0U, cell.j, n, rhombic_macrocharts, paired_direct, reverse[0]);
        ++seams;
    }
    if (cell.j > 0U) {
        output[1] = Cell{cell.face, cell.i, cell.j - 1U, 1U};
    } else {
        output[1] = cross_edge_device(
            cell, 2U, cell.i, n, rhombic_macrocharts, paired_direct, reverse[1]);
        ++seams;
    }
    if (cell.i > 0U) {
        output[2] = Cell{cell.face, cell.i - 1U, cell.j, 1U};
    } else {
        output[2] = cross_edge_device(
            cell, 1U, cell.j, n, rhombic_macrocharts, paired_direct, reverse[2]);
        ++seams;
    }
    if (rhombic_macrocharts) {
        #pragma unroll
        for (int edge = 0; edge < 3; ++edge) {
            if ((output[edge].face >> 1U) != (cell.face >> 1U)) {
                ++macrochart_crossings;
            }
        }
    }
}

// Returns 1 for a new insertion, 0 for an existing key, and -1 if the table
// cannot accept the key.  With test_first the slot is read (bypassing L1)
// before an atomic is issued; a slot never changes once it holds a key, so a
// stale read can only make the thread fall back to the atomic path.
__device__ inline int hash_insert(
    std::uint64_t* table,
    std::uint64_t capacity,
    std::uint64_t key,
    int hash_mode,
    bool test_first) noexcept {
    const std::uint64_t mask = capacity - 1U;
    std::uint64_t position = home_slot(key, mask, hash_mode);
    for (std::uint64_t probe = 0; probe < capacity; ++probe) {
        auto* address = reinterpret_cast<unsigned long long*>(table + position);
        if (test_first) {
            const std::uint64_t seen = __ldcg(address);
            if (seen == key) return 0;
            if (seen != kEmptyKey) {
                position = (position + 1U) & mask;
                continue;
            }
        }
        const std::uint64_t previous = atomicCAS(
            address,
            static_cast<unsigned long long>(kEmptyKey),
            static_cast<unsigned long long>(key));
        if (previous == kEmptyKey) return 1;
        if (previous == key) return 0;
        position = (position + 1U) & mask;
    }
    return -1;
}

struct KernelTuning {
    int hash_mode;
    bool test_before_cas;
    bool warp_append;
    bool skip_arrival;
};

__global__ void initialize_feature_kernel(
    const CellIndex* input,
    std::uint64_t input_count,
    std::uint32_t level,
    std::uint64_t* visited,
    std::uint64_t hash_capacity,
    std::uint64_t* unique_feature,
    unsigned long long* unique_count,
    int* status,
    KernelTuning tuning) {
    const std::uint64_t index = std::uint64_t(blockIdx.x) * blockDim.x + threadIdx.x;
    if (index >= input_count || atomicAdd(status, 0) != 0) return;
    const CellIndex path = input[index];
    if (!canonical_path(path, level)) {
        atomicCAS(status, 0, 3);
        return;
    }
    const std::uint64_t key = detail::encode_coord(path_to_cell(path, level));
    const int inserted = hash_insert(
        visited, hash_capacity, key, tuning.hash_mode, tuning.test_before_cas);
    if (inserted < 0) {
        atomicCAS(status, 0, 1);
    } else if (inserted > 0) {
        const unsigned long long position = atomicAdd(unique_count, 1ULL);
        unique_feature[position] = key;
    }
}

__global__ void expand_frontier_kernel(
    const std::uint64_t* frontier,
    const std::uint8_t* frontier_arrival,
    std::uint64_t frontier_count,
    std::uint32_t level,
    std::uint64_t* visited,
    std::uint64_t hash_capacity,
    std::uint64_t* output,
    std::uint8_t* output_arrival,
    std::uint64_t output_capacity,
    bool rhombic_macrocharts,
    unsigned long long* output_count,
    unsigned long long* seam_count,
    unsigned long long* macrochart_crossing_count,
    unsigned long long* paired_direct_count,
    int* status,
    KernelTuning tuning) {
    const std::uint64_t index = std::uint64_t(blockIdx.x) * blockDim.x + threadIdx.x;
    if (index >= frontier_count || atomicAdd(status, 0) != 0) return;

    Cell local[3];
    std::uint8_t reverse[3];
    std::uint32_t seams = 0U;
    std::uint32_t macrochart_crossings = 0U;
    std::uint32_t paired_direct = 0U;
    neighbors_device(
        detail::decode_coord(frontier[index]), level, rhombic_macrocharts,
        local, reverse, seams, macrochart_crossings, paired_direct);
    if (seams != 0U) atomicAdd(seam_count, static_cast<unsigned long long>(seams));
    if (macrochart_crossings != 0U) {
        atomicAdd(macrochart_crossing_count,
                  static_cast<unsigned long long>(macrochart_crossings));
    }
    if (paired_direct != 0U) {
        atomicAdd(paired_direct_count,
                  static_cast<unsigned long long>(paired_direct));
    }
    const std::uint8_t arrival = (tuning.skip_arrival && frontier_arrival != nullptr)
        ? frontier_arrival[index]
        : kNoArrival;

    #pragma unroll
    for (int edge = 0; edge < 3; ++edge) {
        if (std::uint8_t(edge) == arrival) continue;
        const std::uint64_t key = detail::encode_coord(local[edge]);
        const int inserted = hash_insert(
            visited, hash_capacity, key, tuning.hash_mode, tuning.test_before_cas);
        if (inserted < 0) {
            atomicCAS(status, 0, 1);
            return;
        }
        if (inserted > 0) {
            const unsigned long long position = atomicAdd(output_count, 1ULL);
            if (position >= output_capacity) {
                atomicCAS(status, 0, 2);
                return;
            }
            output[position] = key;
            if (output_arrival != nullptr) output_arrival[position] = reverse[edge];
        }
    }
}

struct PersistentState {
    unsigned long long unique_count;
    unsigned long long output_count;
    unsigned long long seam_count;
    unsigned long long macrochart_crossing_count;
    unsigned long long paired_direct_count;
    unsigned long long touched_references;
    unsigned long long skipped_references;
    int status;
    int padding;
};

// Append discovered cells to the output.  In warp mode one lane reserves a
// contiguous range for the whole warp (all lanes in active_mask must call this
// helper); otherwise every appending thread reserves its own position.
__device__ inline void append_output(
    bool append,
    std::uint64_t key,
    std::uint8_t arrival,
    std::uint64_t* output,
    std::uint8_t* output_arrival,
    std::uint64_t output_capacity,
    unsigned long long* counter,
    int* status,
    bool warp_mode,
    unsigned active_mask = 0xffffffffU) noexcept {
    unsigned long long position = 0ULL;
    if (warp_mode) {
        const unsigned selected = __ballot_sync(active_mask, append);
        if (selected == 0U) return;
        const int lane = int(threadIdx.x & 31U);
        const int leader = __ffs(selected) - 1;
        unsigned long long base = 0ULL;
        if (lane == leader) {
            base = atomicAdd(counter, static_cast<unsigned long long>(__popc(selected)));
        }
        base = __shfl_sync(active_mask, base, leader);
        if (!append) return;
        const unsigned lower_lanes = lane == 0 ? 0U : ((1U << lane) - 1U);
        position = base + __popc(selected & lower_lanes);
    } else {
        if (!append) return;
        position = atomicAdd(counter, 1ULL);
    }
    if (position < output_capacity) {
        output[position] = key;
        if (output_arrival != nullptr) output_arrival[position] = arrival;
    } else {
        atomicCAS(status, 0, 2);
    }
}

// Complete fixed-level BFS in one cooperative launch.  Frontier sizes and
// layer offsets never leave the device during traversal.  The grid is sized to
// the device's cooperative residency limit and uses grid-stride work loops.
__global__ void persistent_halo_kernel(
    const CellIndex* input,
    std::uint64_t input_count,
    std::uint32_t level,
    std::uint32_t hops,
    std::uint64_t* visited,
    std::uint64_t hash_capacity,
    std::uint64_t* unique_feature,
    std::uint64_t* output,
    std::uint8_t* output_arrival,
    std::uint64_t output_capacity,
    bool rhombic_macrocharts,
    unsigned long long* layer_offsets,
    PersistentState* state,
    KernelTuning tuning) {
    cg::grid_group grid = cg::this_grid();
    const std::uint64_t global_thread =
        std::uint64_t(blockIdx.x) * blockDim.x + threadIdx.x;
    const std::uint64_t grid_threads = std::uint64_t(gridDim.x) * blockDim.x;
    const bool use_arrival = tuning.skip_arrival && output_arrival != nullptr;

    for (std::uint64_t base = 0U; base < input_count; base += grid_threads) {
        const std::uint64_t index = base + global_thread;
        bool inserted_new = false;
        std::uint64_t key = 0U;
        if (index < input_count) {
            const CellIndex path = input[index];
            if (!canonical_path(path, level)) {
                atomicCAS(&state->status, 0, 3);
            } else {
                key = detail::encode_coord(path_to_cell(path, level));
                const int inserted = hash_insert(
                    visited, hash_capacity, key, tuning.hash_mode, tuning.test_before_cas);
                if (inserted < 0) atomicCAS(&state->status, 0, 1);
                inserted_new = inserted > 0;
            }
        }
        append_output(
            inserted_new, key, kNoArrival, unique_feature, nullptr, input_count,
            &state->unique_count, &state->status, tuning.warp_append);
    }

    grid.sync();
    if (grid.thread_rank() == 0U) layer_offsets[0] = 0ULL;
    grid.sync();

    for (std::uint32_t hop = 0U; hop < hops; ++hop) {
        const unsigned long long layer_begin = hop == 0U
            ? 0ULL
            : layer_offsets[hop - 1U];
        const unsigned long long layer_end = hop == 0U
            ? state->unique_count
            : layer_offsets[hop];
        const unsigned long long frontier_count = layer_end - layer_begin;
        const bool skipping = use_arrival && hop != 0U;
        if (grid.thread_rank() == 0U && state->status == 0) {
            state->touched_references +=
                (skipping ? 2ULL : 3ULL) * frontier_count;
            if (skipping) state->skipped_references += frontier_count;
        }
        grid.sync();

        for (std::uint64_t base = 0U; base < frontier_count; base += grid_threads) {
            const std::uint64_t index = base + global_thread;
            Cell local[3]{};
            std::uint8_t reverse[3]{0U, 2U, 1U};
            std::uint8_t arrival = kNoArrival;
            std::uint32_t seams = 0U;
            std::uint32_t macrochart_crossings = 0U;
            std::uint32_t paired_direct = 0U;
            const bool active = index < frontier_count && state->status == 0;
            if (active) {
                const std::uint64_t key = hop == 0U
                    ? unique_feature[index]
                    : output[layer_begin + index];
                if (skipping) arrival = output_arrival[layer_begin + index];
                neighbors_device(
                    detail::decode_coord(key), level, rhombic_macrocharts,
                    local, reverse, seams, macrochart_crossings, paired_direct);
                if (seams != 0U) {
                    atomicAdd(&state->seam_count,
                              static_cast<unsigned long long>(seams));
                }
                if (macrochart_crossings != 0U) {
                    atomicAdd(&state->macrochart_crossing_count,
                              static_cast<unsigned long long>(macrochart_crossings));
                }
                if (paired_direct != 0U) {
                    atomicAdd(&state->paired_direct_count,
                              static_cast<unsigned long long>(paired_direct));
                }
            }

            #pragma unroll
            for (int edge = 0; edge < 3; ++edge) {
                std::uint64_t key = 0U;
                bool inserted_new = false;
                if (active && std::uint8_t(edge) != arrival) {
                    key = detail::encode_coord(local[edge]);
                    const int inserted = hash_insert(
                        visited, hash_capacity, key, tuning.hash_mode,
                        tuning.test_before_cas);
                    if (inserted < 0) atomicCAS(&state->status, 0, 1);
                    inserted_new = inserted > 0;
                }
                append_output(
                    inserted_new, key, reverse[edge], output,
                    use_arrival ? output_arrival : nullptr, output_capacity,
                    &state->output_count, &state->status, tuning.warp_append);
            }
        }

        grid.sync();
        if (grid.thread_rank() == 0U) {
            layer_offsets[hop + 1U] =
                state->output_count < output_capacity
                ? state->output_count
                : output_capacity;
        }
        grid.sync();
    }
}

__global__ void encode_output_kernel(
    const std::uint64_t* coordinates,
    CellIndex* output,
    std::uint64_t count,
    std::uint32_t level) {
    const std::uint64_t index = std::uint64_t(blockIdx.x) * blockDim.x + threadIdx.x;
    if (index >= count) return;
    output[index] = cell_to_path(detail::decode_coord(coordinates[index]), level);
}

// Lazy clearing.  Every occupied slot holds exactly one key of the previous
// query, and all of those keys are still in the seed and output arrays.  The
// first kernel only reads (so chains stay intact while slots are located);
// the second writes the empty word to the located slots.
__global__ void locate_keys_kernel(
    const std::uint64_t* feature,
    std::uint64_t unique_count,
    const std::uint64_t* output,
    std::uint64_t output_count,
    const std::uint64_t* table,
    std::uint64_t capacity,
    int hash_mode,
    std::uint32_t* slots) {
    const std::uint64_t index = std::uint64_t(blockIdx.x) * blockDim.x + threadIdx.x;
    const std::uint64_t total = unique_count + output_count;
    if (index >= total) return;
    const std::uint64_t key = index < unique_count
        ? feature[index]
        : output[index - unique_count];
    const std::uint64_t mask = capacity - 1U;
    std::uint64_t position = home_slot(key, mask, hash_mode);
    for (std::uint64_t probe = 0; probe < capacity; ++probe) {
        const std::uint64_t seen = table[position];
        if (seen == key) {
            slots[index] = std::uint32_t(position);
            return;
        }
        if (seen == kEmptyKey) break;
        position = (position + 1U) & mask;
    }
    slots[index] = kNoSlot;
}

__global__ void clear_slots_kernel(
    const std::uint32_t* slots,
    std::uint64_t count,
    std::uint64_t* table) {
    const std::uint64_t index = std::uint64_t(blockIdx.x) * blockDim.x + threadIdx.x;
    if (index >= count) return;
    const std::uint32_t slot = slots[index];
    if (slot != kNoSlot) table[slot] = kEmptyKey;
}

std::uint64_t next_power_of_two(std::uint64_t value) {
    if (value <= 1U) return 1U;
    --value;
    for (std::uint32_t shift = 1U; shift < 64U; shift <<= 1U) value |= value >> shift;
    if (value == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("BAFE capacity overflow");
    }
    return value + 1U;
}

std::uint64_t graph_output_bound(std::uint64_t sources, std::uint32_t hops) {
    if (hops == 0U || sources == 0U) return 0U;
    // The dual of every refined Pentakis surface is a fullerene graph: cubic,
    // with only pentagonal and hexagonal faces.  Its distance-k coordination
    // sequence is bounded by the infinite hexagonal lattice's 3k cells.
    // Therefore a k-hop disk excluding its source has at most 3k(k+1)/2
    // cells.  This is dramatically tighter than the generic cubic-tree bound
    // and remains conservative at the twelve positive-curvature vertices.
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

template <typename T>
bool grow_device_buffer(T*& pointer, std::uint64_t& capacity, std::uint64_t required) {
    if (required <= capacity) return false;
    T* replacement = nullptr;
    if (required != 0U) {
        cuda_check(cudaMalloc(reinterpret_cast<void**>(&replacement), required * sizeof(T)),
                   "cudaMalloc replacement buffer");
    }
    if (pointer) {
        const cudaError_t release_status = cudaFree(pointer);
        if (release_status != cudaSuccess) {
            cudaFree(replacement);
            cuda_check(release_status, "cudaFree replaced buffer");
        }
    }
    pointer = replacement;
    capacity = required;
    return true;
}

class CudaHaloEngine final : public HaloEngine {
public:
    explicit CudaHaloEngine(int ordinal) : ordinal_(ordinal) {
        try {
            cuda_check(cudaSetDevice(ordinal_), "cudaSetDevice");
            cuda_check(cudaGetDeviceProperties(&properties_, ordinal_), "cudaGetDeviceProperties");
            cuda_check(cudaStreamCreateWithFlags(&stream_, cudaStreamNonBlocking), "cudaStreamCreate");
            cuda_check(cudaEventCreate(&start_), "cudaEventCreate start");
            cuda_check(cudaEventCreate(&stop_), "cudaEventCreate stop");
            cuda_check(cudaEventCreate(&input_done_), "cudaEventCreate input_done");
            cuda_check(cudaEventCreate(&wavefront_done_), "cudaEventCreate wavefront_done");
            cuda_check(cudaEventCreate(&encode_done_), "cudaEventCreate encode_done");
            cuda_check(cudaMemcpyToSymbol(d_atlas, detail::kAtlas, sizeof(detail::kAtlas)),
                       "upload Pentakis atlas");
            const auto chunks = detail::make_decode_chunks();
            cuda_check(cudaMemcpyToSymbol(d_decode_chunks, chunks.data(), sizeof(chunks)),
                       "upload prefix decode LUT");
            const auto encode_chunks = detail::make_encode_chunks();
            cuda_check(cudaMemcpyToSymbol(
                           d_encode_chunks, encode_chunks.data(), sizeof(encode_chunks)),
                       "upload prefix encode LUT");
            // One contiguous control block: six host driven counters, the
            // status word, and the persistent state.  The manuscript path
            // clears them with separate memsets; fused_control_clear uses one.
            cuda_check(cudaMalloc(reinterpret_cast<void**>(&d_control_), kControlBytes),
                       "allocate control block");
            d_unique_count_ = reinterpret_cast<unsigned long long*>(d_control_ + 0);
            d_output_count_ = reinterpret_cast<unsigned long long*>(d_control_ + 8);
            d_seam_count_ = reinterpret_cast<unsigned long long*>(d_control_ + 16);
            d_macrochart_crossing_count_ =
                reinterpret_cast<unsigned long long*>(d_control_ + 24);
            d_paired_direct_count_ = reinterpret_cast<unsigned long long*>(d_control_ + 32);
            d_status_ = reinterpret_cast<int*>(d_control_ + 40);
            d_persistent_state_ = reinterpret_cast<PersistentState*>(d_control_ + 64);
        } catch (...) {
            release_noexcept();
            throw;
        }
    }

    ~CudaHaloEngine() override {
        release_noexcept();
    }

    HaloResult halo(
        const CellIndex* input,
        std::size_t input_count,
        const QueryOptions& options) override {
        std::lock_guard<std::mutex> guard(mutex_);
        cuda_check(cudaSetDevice(ordinal_), "cudaSetDevice query");
        if (options.level > kMaxLevel) {
            throw std::invalid_argument("BAFE supports levels 0 through 28");
        }
        if (input_count != 0U && input == nullptr) {
            throw std::invalid_argument("BAFE input pointer is null");
        }
        CudaTuning tuning = options.cuda_tuning;
        // The reused buffer delivery hands the page locked host buffer to the
        // caller, so it implies the page locked download path.
        if (options.reuse_output) tuning.pinned_output = true;
        if (tuning.hash_mode < 0 || tuning.hash_mode > 2) {
            throw std::invalid_argument("cuda_tuning.hash_mode must be 0, 1, or 2");
        }
        if (!(tuning.load_target > 0.0 && tuning.load_target <= 0.9)) {
            throw std::invalid_argument("cuda_tuning.load_target must be in (0, 0.9]");
        }

        const auto wall_start = std::chrono::steady_clock::now();
        HaloResult result;
        result.layer_offsets.push_back(0U);
        result.metrics.backend = "cuda_resident_hash_frontier";
        result.metrics.device = properties_.name;
        result.metrics.input_cells = input_count;

        if (input_count == 0U) {
            for (std::uint32_t h = 0; h < options.hops; ++h) {
                result.layer_offsets.push_back(0U);
            }
            result.metrics.end_to_end_ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - wall_start).count();
            return result;
        }

        const std::uint64_t requested_output = options.hops == 0U
            ? 0U
            : options.max_output_cells == 0U
                ? graph_output_bound(input_count, options.hops)
                : options.max_output_cells;
        if (options.hops != 0U && requested_output == 0U) {
            throw std::invalid_argument("max_output_cells must be positive for a non-empty halo");
        }

        const std::uint64_t total_keys = std::uint64_t(input_count) + requested_output;
        if (total_keys > std::numeric_limits<std::uint64_t>::max() / 4U) {
            throw std::overflow_error("BAFE hash capacity overflow");
        }
        const long double scaled_keys =
            static_cast<long double>(total_keys) / static_cast<long double>(tuning.load_target);
        const std::uint64_t hash_capacity = next_power_of_two(std::max<std::uint64_t>(
            1024U, static_cast<std::uint64_t>(std::ceil(scaled_keys))));
        if (hash_capacity > (std::uint64_t(1) << 32U)) {
            throw std::overflow_error("BAFE hash capacity exceeds the 32 bit slot index range");
        }
        const bool want_arrival = tuning.skip_arrival_edge;
        const std::uint64_t lazy_slots = hash_capacity / kLazyClearDivisor;

        const bool needs_growth =
            input_count > input_capacity_ ||
            input_count > feature_capacity_ ||
            requested_output > output_capacity_ ||
            requested_output > path_capacity_ ||
            hash_capacity > hash_capacity_ ||
            (want_arrival && requested_output > arrival_capacity_) ||
            (tuning.lazy_clear && lazy_slots > slot_capacity_) ||
            (tuning.pinned_output && requested_output > pinned_capacity_) ||
            std::uint64_t(options.hops) + 1U > layer_capacity_;

        if (needs_growth || !tuning.skip_memory_preflight) {
            std::size_t free_bytes = 0U, total_bytes = 0U;
            cuda_check(cudaMemGetInfo(&free_bytes, &total_bytes), "cudaMemGetInfo");
            // Existing buffers are already deducted from free_bytes.  Count only
            // allocations that must grow; allocate-before-swap temporarily needs
            // the complete replacement allocation, not merely its delta.
            const long double required_bytes =
                (input_count > input_capacity_
                    ? static_cast<long double>(input_count) * sizeof(CellIndex) : 0.0L) +
                (input_count > feature_capacity_
                    ? static_cast<long double>(input_count) * sizeof(std::uint64_t) : 0.0L) +
                (requested_output > output_capacity_
                    ? static_cast<long double>(requested_output) * sizeof(std::uint64_t) : 0.0L) +
                (requested_output > path_capacity_
                    ? static_cast<long double>(requested_output) * sizeof(CellIndex) : 0.0L) +
                (hash_capacity > hash_capacity_
                    ? static_cast<long double>(hash_capacity) * sizeof(std::uint64_t) : 0.0L) +
                (want_arrival && requested_output > arrival_capacity_
                    ? static_cast<long double>(requested_output) : 0.0L) +
                (tuning.lazy_clear && lazy_slots > slot_capacity_
                    ? static_cast<long double>(lazy_slots) * sizeof(std::uint32_t) : 0.0L) +
                (std::uint64_t(options.hops) + 1U > layer_capacity_
                    ? static_cast<long double>(std::uint64_t(options.hops) + 1U) *
                        sizeof(unsigned long long) : 0.0L) +
                sizeof(PersistentState) + 64.0L;
            if (required_bytes > static_cast<long double>(free_bytes) * 0.85L) {
                throw std::length_error(
                    "BAFE query workspace exceeds 85% of currently free GPU memory; "
                    "set a realistic max_output_cells or split the query");
            }
        }

        grow_device_buffer(d_input_, input_capacity_, input_count);
        const bool feature_grew = grow_device_buffer(d_feature_, feature_capacity_, input_count);
        const bool output_grew =
            grow_device_buffer(d_output_coord_, output_capacity_, requested_output);
        grow_device_buffer(d_output_path_, path_capacity_, requested_output);
        if (grow_device_buffer(d_hash_, hash_capacity_, hash_capacity)) {
            // A new allocation is cleared completely once; afterwards only the
            // prefixes used by queries can hold keys (tracked by dirty_span_).
            cuda_check(cudaMemsetAsync(
                           d_hash_, 0xff, hash_capacity_ * sizeof(std::uint64_t), stream_),
                       "clear new visited hash");
            table_dirty_ = false;
            dirty_span_ = 0U;
        }
        if (want_arrival) {
            grow_device_buffer(d_arrival_, arrival_capacity_, requested_output);
        }
        if (tuning.lazy_clear) {
            grow_device_buffer(d_slots_, slot_capacity_, lazy_slots);
        }
        grow_device_buffer(
            d_layer_offsets_, layer_capacity_, std::uint64_t(options.hops) + 1U);
        if (tuning.pinned_output && requested_output > pinned_capacity_) {
            if (h_pinned_) {
                cuda_check(cudaFreeHost(h_pinned_), "cudaFreeHost pinned output");
                h_pinned_ = nullptr;
                pinned_capacity_ = 0U;
            }
            cuda_check(cudaHostAlloc(reinterpret_cast<void**>(&h_pinned_),
                                     requested_output * sizeof(CellIndex),
                                     cudaHostAllocDefault),
                       "cudaHostAlloc pinned output");
            pinned_capacity_ = requested_output;
        }
        if (feature_grew || output_grew) {
            // The keys of the last query are no longer available for lazy clearing.
            lazy_possible_ = false;
        }

        result.metrics.device_peak_bytes =
            input_capacity_ * sizeof(CellIndex) +
            feature_capacity_ * sizeof(std::uint64_t) +
            output_capacity_ * sizeof(std::uint64_t) +
            path_capacity_ * sizeof(CellIndex) +
            hash_capacity_ * sizeof(std::uint64_t) +
            arrival_capacity_ +
            slot_capacity_ * sizeof(std::uint32_t) +
            layer_capacity_ * sizeof(unsigned long long) +
            sizeof(PersistentState) + 32U;
        std::uint64_t hash_value = hash_capacity;
        while (hash_value > 1U) {
            ++result.metrics.hash_capacity_log2;
            hash_value >>= 1U;
        }
        result.metrics.host_to_device_bytes = input_count * sizeof(CellIndex);

        const KernelTuning kernel_tuning{
            tuning.hash_mode,
            tuning.test_before_cas,
            tuning.warp_aggregated_append,
            tuning.skip_arrival_edge};

        cuda_check(cudaEventRecord(start_, stream_), "record pipeline start");
        cuda_check(cudaMemcpyAsync(
            d_input_, input, input_count * sizeof(CellIndex),
            cudaMemcpyHostToDevice, stream_), "copy input to GPU");

        // Restore the empty table.  The manuscript path clears the used prefix
        // with one memset on every query.  Lazy clearing scatters the empty
        // word to the slots occupied by the previous query when they are few;
        // dirty_span_ tracks the longest prefix that may still hold keys so
        // that a lazy clear is only used when all of them belong to the last
        // query (which is the only query whose keys are still available).
        std::uint64_t span_to_clear = hash_capacity;
        bool cleared_all = false;
        if (tuning.lazy_clear) {
            const std::uint64_t previous_keys = previous_unique_ + previous_output_;
            const bool lazy_now = table_dirty_ && lazy_possible_ &&
                previous_keys != 0U &&
                previous_keys <= slot_capacity_ &&
                previous_keys * kLazyClearDivisor <= previous_hash_capacity_ &&
                dirty_span_ <= previous_hash_capacity_;
            if (lazy_now) {
                const int blocks = static_cast<int>((previous_keys + kThreads - 1U) / kThreads);
                locate_keys_kernel<<<blocks, kThreads, 0, stream_>>>(
                    d_feature_, previous_unique_, d_output_coord_, previous_output_,
                    d_hash_, previous_hash_capacity_, previous_hash_mode_, d_slots_);
                cuda_check(cudaGetLastError(), "launch locate_keys_kernel");
                clear_slots_kernel<<<blocks, kThreads, 0, stream_>>>(
                    d_slots_, previous_keys, d_hash_);
                cuda_check(cudaGetLastError(), "launch clear_slots_kernel");
                result.metrics.kernel_launches += 2U;
                result.metrics.lazy_cleared = true;
                cleared_all = true;
                span_to_clear = 0U;
            } else if (dirty_span_ == 0U) {
                span_to_clear = 0U;  // freshly allocated or already restored
            } else {
                span_to_clear = std::max(dirty_span_, hash_capacity);
            }
        }
        if (span_to_clear != 0U) {
            cuda_check(cudaMemsetAsync(
                d_hash_, 0xff, span_to_clear * sizeof(std::uint64_t), stream_),
                "clear visited hash");
        }
        if (cleared_all || dirty_span_ <= span_to_clear) dirty_span_ = 0U;
        dirty_span_ = std::max(dirty_span_, hash_capacity);
        table_dirty_ = true;
        lazy_possible_ = false;

        if (tuning.fused_control_clear) {
            cuda_check(cudaMemsetAsync(d_control_, 0, kControlBytes, stream_),
                       "clear control block");
        } else {
            cuda_check(cudaMemsetAsync(d_unique_count_, 0, sizeof(unsigned long long), stream_),
                       "clear unique counter");
            cuda_check(cudaMemsetAsync(d_output_count_, 0, sizeof(unsigned long long), stream_),
                       "clear output counter");
            cuda_check(cudaMemsetAsync(d_seam_count_, 0, sizeof(unsigned long long), stream_),
                       "clear seam counter");
            cuda_check(cudaMemsetAsync(
                           d_macrochart_crossing_count_, 0,
                           sizeof(unsigned long long), stream_),
                       "clear macrochart crossing counter");
            cuda_check(cudaMemsetAsync(
                           d_paired_direct_count_, 0,
                           sizeof(unsigned long long), stream_),
                       "clear paired edge counter");
            cuda_check(cudaMemsetAsync(d_status_, 0, sizeof(int), stream_), "clear status");
        }
        cuda_check(cudaEventRecord(input_done_, stream_), "record input/workspace complete");
        unsigned long long unique_count = 0U;
        unsigned long long output_count = 0U;
        unsigned long long seams = 0U;
        unsigned long long macrochart_crossings = 0U;
        unsigned long long paired_direct = 0U;
        unsigned long long skipped_references = 0U;
        int status = 0;
        const bool cooperative_supported = properties_.cooperativeLaunch != 0;
        if (options.cuda_schedule == CudaSchedule::CooperativePersistent &&
            !cooperative_supported) {
            throw std::runtime_error(
                "The selected CUDA device does not support cooperative grid launch");
        }
        const bool use_cooperative = cooperative_supported &&
            options.cuda_schedule != CudaSchedule::HostDriven;
        std::uint8_t* arrival_pointer = want_arrival ? d_arrival_ : nullptr;

        if (use_cooperative) {
            result.metrics.backend = "cuda_cooperative_hash_wavefront";
            if (!tuning.fused_control_clear) {
                cuda_check(cudaMemsetAsync(
                               d_persistent_state_, 0, sizeof(PersistentState), stream_),
                           "clear persistent state");
            }
            cuda_check(cudaMemsetAsync(
                           d_layer_offsets_, 0,
                           (std::uint64_t(options.hops) + 1U) *
                               sizeof(unsigned long long),
                           stream_),
                       "clear device layer offsets");

            int resident_blocks_per_sm = 0;
            cuda_check(cudaOccupancyMaxActiveBlocksPerMultiprocessor(
                           &resident_blocks_per_sm,
                           persistent_halo_kernel,
                           kThreads,
                           0),
                       "query cooperative kernel occupancy");
            const std::uint64_t maximum_blocks =
                std::uint64_t(resident_blocks_per_sm) * properties_.multiProcessorCount;
            const std::uint64_t work_hint = std::max<std::uint64_t>(
                input_count,
                std::min<std::uint64_t>(
                    requested_output,
                    std::max<std::uint64_t>(input_count * 4U, kThreads)));
            const int blocks = static_cast<int>(std::max<std::uint64_t>(
                1U,
                std::min<std::uint64_t>(
                    maximum_blocks,
                    (work_hint + kThreads - 1U) / kThreads)));

            std::uint64_t input_count_argument = input_count;
            std::uint64_t hash_capacity_argument = hash_capacity;
            std::uint64_t output_capacity_argument = requested_output;
            std::uint32_t level_argument = options.level;
            std::uint32_t hops_argument = options.hops;
            bool rhombic_macrocharts_argument =
                options.chart_mode == ChartMode::RhombicMacrocharts;
            KernelTuning tuning_argument = kernel_tuning;
            void* arguments[] = {
                &d_input_,
                &input_count_argument,
                &level_argument,
                &hops_argument,
                &d_hash_,
                &hash_capacity_argument,
                &d_feature_,
                &d_output_coord_,
                &arrival_pointer,
                &output_capacity_argument,
                &rhombic_macrocharts_argument,
                &d_layer_offsets_,
                &d_persistent_state_,
                &tuning_argument};
            cuda_check(cudaLaunchCooperativeKernel(
                           reinterpret_cast<const void*>(persistent_halo_kernel),
                           blocks,
                           kThreads,
                           arguments,
                           0,
                           stream_),
                       "launch cooperative persistent halo kernel");
            ++result.metrics.kernel_launches;

            PersistentState host_state{};
            std::vector<unsigned long long> host_layers(
                std::uint64_t(options.hops) + 1U, 0U);
            cuda_check(cudaMemcpyAsync(
                           &host_state, d_persistent_state_, sizeof(host_state),
                           cudaMemcpyDeviceToHost, stream_),
                       "read persistent state");
            cuda_check(cudaMemcpyAsync(
                           host_layers.data(), d_layer_offsets_,
                           host_layers.size() * sizeof(unsigned long long),
                           cudaMemcpyDeviceToHost, stream_),
                       "read device layer offsets");
            cuda_check(cudaStreamSynchronize(stream_),
                       "synchronize cooperative wavefront");

            unique_count = host_state.unique_count;
            output_count = host_state.output_count;
            seams = host_state.seam_count;
            macrochart_crossings = host_state.macrochart_crossing_count;
            paired_direct = host_state.paired_direct_count;
            skipped_references = host_state.skipped_references;
            status = host_state.status;
            result.metrics.touched_neighbor_references =
                host_state.touched_references;
            result.layer_offsets.assign(host_layers.begin(), host_layers.end());
            result.metrics.control_transfer_bytes =
                sizeof(host_state) +
                host_layers.size() * sizeof(unsigned long long);
        } else {
            result.metrics.backend = "cuda_host_driven_hash_frontier";
            const int input_blocks =
                static_cast<int>((input_count + kThreads - 1U) / kThreads);
            initialize_feature_kernel<<<input_blocks, kThreads, 0, stream_>>>(
                d_input_, input_count, options.level, d_hash_, hash_capacity,
                d_feature_, d_unique_count_, d_status_, kernel_tuning);
            cuda_check(cudaGetLastError(), "launch initialize_feature_kernel");
            ++result.metrics.kernel_launches;

            cuda_check(cudaMemcpyAsync(
                &unique_count, d_unique_count_, sizeof(unique_count),
                cudaMemcpyDeviceToHost, stream_), "read unique feature count");
            cuda_check(cudaMemcpyAsync(
                &status, d_status_, sizeof(status), cudaMemcpyDeviceToHost, stream_),
                "read feature initialization status");
            cuda_check(cudaStreamSynchronize(stream_),
                       "synchronize feature initialization");
            result.metrics.control_transfer_bytes += sizeof(unique_count) + sizeof(status);

            std::uint64_t layer_start = 0U;
            std::uint64_t frontier_count = unique_count;
            const std::uint64_t* frontier = d_feature_;
            const std::uint8_t* frontier_arrival = nullptr;
            for (std::uint32_t hop = 0; hop < options.hops && status == 0; ++hop) {
                if (frontier_count != 0U) {
                    const bool skipping = want_arrival && frontier_arrival != nullptr;
                    result.metrics.touched_neighbor_references +=
                        (skipping ? 2ULL : 3ULL) * frontier_count;
                    if (skipping) skipped_references += frontier_count;
                    const int blocks =
                        static_cast<int>((frontier_count + kThreads - 1U) / kThreads);
                    expand_frontier_kernel<<<blocks, kThreads, 0, stream_>>>(
                        frontier, frontier_arrival, frontier_count, options.level,
                        d_hash_, hash_capacity,
                        d_output_coord_, arrival_pointer, requested_output,
                        options.chart_mode == ChartMode::RhombicMacrocharts,
                        d_output_count_, d_seam_count_,
                        d_macrochart_crossing_count_, d_paired_direct_count_,
                        d_status_, kernel_tuning);
                    cuda_check(cudaGetLastError(), "launch expand_frontier_kernel");
                    ++result.metrics.kernel_launches;
                    cuda_check(cudaMemcpyAsync(
                        &output_count, d_output_count_, sizeof(output_count),
                        cudaMemcpyDeviceToHost, stream_), "read halo output count");
                    cuda_check(cudaMemcpyAsync(
                        &status, d_status_, sizeof(status), cudaMemcpyDeviceToHost, stream_),
                        "read halo layer status");
                    cuda_check(cudaStreamSynchronize(stream_), "synchronize halo layer");
                    result.metrics.control_transfer_bytes +=
                        sizeof(output_count) + sizeof(status);
                }
                result.layer_offsets.push_back(output_count);
                frontier = d_output_coord_ + layer_start;
                frontier_arrival = want_arrival ? d_arrival_ + layer_start : nullptr;
                frontier_count = output_count - layer_start;
                layer_start = output_count;
            }
            while (result.layer_offsets.size() <
                   std::uint64_t(options.hops) + 1U) {
                result.layer_offsets.push_back(
                    std::min<unsigned long long>(output_count, requested_output));
            }
            cuda_check(cudaMemcpyAsync(
                &status, d_status_, sizeof(status), cudaMemcpyDeviceToHost, stream_),
                "read CUDA status");
            cuda_check(cudaMemcpyAsync(
                &seams, d_seam_count_, sizeof(seams), cudaMemcpyDeviceToHost, stream_),
                "read seam count");
            cuda_check(cudaMemcpyAsync(
                &macrochart_crossings, d_macrochart_crossing_count_,
                sizeof(macrochart_crossings), cudaMemcpyDeviceToHost, stream_),
                "read macrochart crossing count");
            cuda_check(cudaMemcpyAsync(
                &paired_direct, d_paired_direct_count_, sizeof(paired_direct),
                cudaMemcpyDeviceToHost, stream_), "read paired edge count");
            result.metrics.control_transfer_bytes +=
                sizeof(status) + sizeof(seams) + sizeof(macrochart_crossings) +
                sizeof(paired_direct);
        }

        cuda_check(cudaEventRecord(wavefront_done_, stream_), "record wavefront complete");
        result.metrics.unique_input_cells = unique_count;
        result.metrics.skipped_arrival_probes = skipped_references;

        if (output_count != 0U && output_count <= requested_output) {
            const int blocks = static_cast<int>((output_count + kThreads - 1U) / kThreads);
            encode_output_kernel<<<blocks, kThreads, 0, stream_>>>(
                d_output_coord_, d_output_path_, output_count, options.level);
            cuda_check(cudaGetLastError(), "launch encode_output_kernel");
            ++result.metrics.kernel_launches;
        }
        cuda_check(cudaEventRecord(encode_done_, stream_), "record output encoding complete");
        cuda_check(cudaEventRecord(stop_, stream_), "record pipeline stop");
        cuda_check(cudaEventSynchronize(stop_), "synchronize pipeline stop");
        float pipeline_ms = 0.0F;
        float input_workspace_ms = 0.0F;
        float fused_wavefront_ms = 0.0F;
        float output_encode_ms = 0.0F;
        cuda_check(cudaEventElapsedTime(&pipeline_ms, start_, stop_), "measure pipeline");
        cuda_check(cudaEventElapsedTime(&input_workspace_ms, start_, input_done_),
                   "measure input/workspace stage");
        cuda_check(cudaEventElapsedTime(&fused_wavefront_ms, input_done_, wavefront_done_),
                   "measure fused wavefront stage");
        cuda_check(cudaEventElapsedTime(&output_encode_ms, wavefront_done_, encode_done_),
                   "measure output encoding stage");

        // Remember what the table holds so that the next query can clear lazily.
        previous_unique_ = unique_count;
        previous_output_ = std::min<std::uint64_t>(output_count, requested_output);
        previous_hash_capacity_ = hash_capacity;
        previous_hash_mode_ = tuning.hash_mode;
        lazy_possible_ = status == 0;

        if (status == 1) throw std::runtime_error("BAFE GPU visited hash table exhausted");
        if (status == 2 || output_count > requested_output) {
            throw std::length_error("BAFE halo exceeded max_output_cells");
        }
        if (status == 3) {
            throw std::invalid_argument("Input contains a non-canonical or mixed-level BAFE index");
        }

        const auto download_start = std::chrono::steady_clock::now();
        double materialize_ms = 0.0;
        if (tuning.pinned_output) {
            if (output_count != 0U) {
                cuda_check(cudaMemcpyAsync(
                    h_pinned_, d_output_path_, output_count * sizeof(CellIndex),
                    cudaMemcpyDeviceToHost, stream_), "copy halo output to pinned host");
                cuda_check(cudaStreamSynchronize(stream_), "synchronize halo download");
            }
        } else {
            result.cells.resize(output_count);
            materialize_ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - download_start).count();
            if (output_count != 0U) {
                cuda_check(cudaMemcpyAsync(
                    result.cells.data(), d_output_path_, output_count * sizeof(CellIndex),
                    cudaMemcpyDeviceToHost, stream_), "copy halo output to host");
                cuda_check(cudaStreamSynchronize(stream_), "synchronize halo download");
            }
        }
        result.metrics.output_download_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - download_start).count() - materialize_ms;
        result.metrics.host_materialize_ms = materialize_ms;

        const auto sort_start = std::chrono::steady_clock::now();
        if (options.deterministic_layers && !tuning.pinned_output) {
            for (std::size_t layer = 0; layer + 1U < result.layer_offsets.size(); ++layer) {
                std::sort(
                    result.cells.begin() + static_cast<std::ptrdiff_t>(result.layer_offsets[layer]),
                    result.cells.begin() + static_cast<std::ptrdiff_t>(result.layer_offsets[layer + 1U]));
            }
        }
        result.metrics.deterministic_sort_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - sort_start).count();

        result.metrics.output_cells = output_count;
        result.metrics.root_edge_crossings = seams;
        result.metrics.macrochart_boundary_crossings = macrochart_crossings;
        result.metrics.paired_edge_direct_transforms = paired_direct;
        result.metrics.gpu_input_and_workspace_ms = input_workspace_ms;
        result.metrics.gpu_fused_index_hash_wavefront_ms = fused_wavefront_ms;
        result.metrics.gpu_output_encode_ms = output_encode_ms;
        result.metrics.device_pipeline_ms = pipeline_ms;
        result.metrics.device_to_host_bytes =
            output_count * sizeof(CellIndex) + result.metrics.control_transfer_bytes;
        result.metrics.end_to_end_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - wall_start).count();

        if (options.reuse_output) {
            // Reused buffer delivery: the caller reads the page locked host
            // buffer directly; no vector is allocated and nothing is copied.
            if (options.deterministic_layers) {
                for (std::size_t layer = 0; layer + 1U < result.layer_offsets.size(); ++layer) {
                    std::sort(h_pinned_ + result.layer_offsets[layer],
                              h_pinned_ + result.layer_offsets[layer + 1U]);
                }
            }
            result.view = h_pinned_;
            result.view_count = static_cast<std::size_t>(output_count);
        } else if (tuning.pinned_output) {
            // The result is complete in page locked host memory at this point;
            // the copy into the caller's vector is reported separately.  A wall
            // clock around the whole call (fresh container delivery) includes it.
            const auto copy_start = std::chrono::steady_clock::now();
            result.cells.assign(h_pinned_, h_pinned_ + output_count);
            if (options.deterministic_layers) {
                for (std::size_t layer = 0; layer + 1U < result.layer_offsets.size(); ++layer) {
                    std::sort(
                        result.cells.begin() + static_cast<std::ptrdiff_t>(result.layer_offsets[layer]),
                        result.cells.begin() + static_cast<std::ptrdiff_t>(result.layer_offsets[layer + 1U]));
                }
            }
            result.metrics.post_timing_copy_ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - copy_start).count();
        }
        return result;
    }

    DeviceInfo device_info() const override {
        return DeviceInfo{
            true,
            properties_.name,
            properties_.major,
            properties_.minor,
            static_cast<std::uint64_t>(properties_.totalGlobalMem)};
    }

private:
    static constexpr std::size_t kControlBytes = 64U + sizeof(PersistentState);

    void release_noexcept() noexcept {
        cudaSetDevice(ordinal_);
        cudaFree(d_input_);
        cudaFree(d_feature_);
        cudaFree(d_output_coord_);
        cudaFree(d_output_path_);
        cudaFree(d_hash_);
        cudaFree(d_arrival_);
        cudaFree(d_slots_);
        cudaFree(d_layer_offsets_);
        cudaFree(d_control_);
        if (h_pinned_) cudaFreeHost(h_pinned_);
        if (start_) cudaEventDestroy(start_);
        if (stop_) cudaEventDestroy(stop_);
        if (input_done_) cudaEventDestroy(input_done_);
        if (wavefront_done_) cudaEventDestroy(wavefront_done_);
        if (encode_done_) cudaEventDestroy(encode_done_);
        if (stream_) cudaStreamDestroy(stream_);
    }

    int ordinal_ = 0;
    cudaDeviceProp properties_{};
    cudaStream_t stream_ = nullptr;
    cudaEvent_t start_ = nullptr;
    cudaEvent_t stop_ = nullptr;
    cudaEvent_t input_done_ = nullptr;
    cudaEvent_t wavefront_done_ = nullptr;
    cudaEvent_t encode_done_ = nullptr;
    std::mutex mutex_;

    CellIndex* d_input_ = nullptr;
    std::uint64_t* d_feature_ = nullptr;
    std::uint64_t* d_output_coord_ = nullptr;
    CellIndex* d_output_path_ = nullptr;
    std::uint64_t* d_hash_ = nullptr;
    std::uint8_t* d_arrival_ = nullptr;
    std::uint32_t* d_slots_ = nullptr;
    unsigned long long* d_layer_offsets_ = nullptr;
    unsigned char* d_control_ = nullptr;
    unsigned long long* d_unique_count_ = nullptr;
    unsigned long long* d_output_count_ = nullptr;
    unsigned long long* d_seam_count_ = nullptr;
    unsigned long long* d_macrochart_crossing_count_ = nullptr;
    unsigned long long* d_paired_direct_count_ = nullptr;
    int* d_status_ = nullptr;
    PersistentState* d_persistent_state_ = nullptr;
    CellIndex* h_pinned_ = nullptr;

    std::uint64_t input_capacity_ = 0U;
    std::uint64_t feature_capacity_ = 0U;
    std::uint64_t output_capacity_ = 0U;
    std::uint64_t path_capacity_ = 0U;
    std::uint64_t hash_capacity_ = 0U;
    std::uint64_t arrival_capacity_ = 0U;
    std::uint64_t slot_capacity_ = 0U;
    std::uint64_t layer_capacity_ = 0U;
    std::uint64_t pinned_capacity_ = 0U;

    // Table bookkeeping for lazy clearing.
    bool table_dirty_ = false;
    bool lazy_possible_ = false;
    std::uint64_t dirty_span_ = 0U;
    std::uint64_t previous_unique_ = 0U;
    std::uint64_t previous_output_ = 0U;
    std::uint64_t previous_hash_capacity_ = 0U;
    int previous_hash_mode_ = 0;
};

}  // namespace

std::unique_ptr<HaloEngine> make_cuda_engine(int device_ordinal) {
    int count = 0;
    cuda_check(cudaGetDeviceCount(&count), "cudaGetDeviceCount");
    if (device_ordinal < 0 || device_ordinal >= count) {
        throw std::out_of_range("Requested CUDA device does not exist");
    }
    return std::make_unique<CudaHaloEngine>(device_ordinal);
}

}  // namespace bafe
