#include "bafe/bafe.hpp"

#include "atlas.hpp"
#include "internal.hpp"

#include <cooperative_groups.h>
#include <cuda_runtime.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
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
constexpr int kThreads = 256;

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

__device__ inline Cell cross_edge_device(
    const Cell& cell,
    std::uint32_t edge,
    std::uint32_t parameter,
    std::uint32_t n,
    bool rhombic_macrocharts,
    std::uint32_t& paired_direct) noexcept {
    if (rhombic_macrocharts && edge == 0U) {
        ++paired_direct;
        return Cell{cell.face ^ 1U, parameter, n - 1U - parameter, 0U};
    }
    const Transition transition = d_atlas[cell.face * 3U + edge];
    const std::uint32_t mapped = transition.reverse
        ? n - 1U - parameter
        : parameter;
    return boundary_up_device(
        transition.dst_face, transition.dst_edge, mapped, n);
}

__device__ inline void neighbors_device(
    const Cell& cell,
    std::uint32_t level,
    bool rhombic_macrocharts,
    Cell output[3],
    std::uint32_t& seams,
    std::uint32_t& macrochart_crossings,
    std::uint32_t& paired_direct) noexcept {
    const std::uint32_t n = 1U << level;
    seams = 0U;
    macrochart_crossings = 0U;
    paired_direct = 0U;
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
            cell, 0U, cell.j, n, rhombic_macrocharts, paired_direct);
        ++seams;
    }
    if (cell.j > 0U) {
        output[1] = Cell{cell.face, cell.i, cell.j - 1U, 1U};
    } else {
        output[1] = cross_edge_device(
            cell, 2U, cell.i, n, rhombic_macrocharts, paired_direct);
        ++seams;
    }
    if (cell.i > 0U) {
        output[2] = Cell{cell.face, cell.i - 1U, cell.j, 1U};
    } else {
        output[2] = cross_edge_device(
            cell, 1U, cell.j, n, rhombic_macrocharts, paired_direct);
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
// cannot accept the key.  The table is intentionally kept below 50% load.
__device__ inline int hash_insert(
    std::uint64_t* table,
    std::uint64_t capacity,
    std::uint64_t key) noexcept {
    const std::uint64_t mask = capacity - 1U;
    std::uint64_t position = mix64(key) & mask;
    for (std::uint64_t probe = 0; probe < capacity; ++probe) {
        auto* address = reinterpret_cast<unsigned long long*>(table + position);
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

__global__ void initialize_feature_kernel(
    const CellIndex* input,
    std::uint64_t input_count,
    std::uint32_t level,
    std::uint64_t* visited,
    std::uint64_t hash_capacity,
    std::uint64_t* unique_feature,
    unsigned long long* unique_count,
    int* status) {
    const std::uint64_t index = std::uint64_t(blockIdx.x) * blockDim.x + threadIdx.x;
    if (index >= input_count || atomicAdd(status, 0) != 0) return;
    const CellIndex path = input[index];
    if (!canonical_path(path, level)) {
        atomicCAS(status, 0, 3);
        return;
    }
    const std::uint64_t key = detail::encode_coord(path_to_cell(path, level));
    const int inserted = hash_insert(visited, hash_capacity, key);
    if (inserted < 0) {
        atomicCAS(status, 0, 1);
    } else if (inserted > 0) {
        const unsigned long long position = atomicAdd(unique_count, 1ULL);
        unique_feature[position] = key;
    }
}

__global__ void expand_frontier_kernel(
    const std::uint64_t* frontier,
    std::uint64_t frontier_count,
    std::uint32_t level,
    std::uint64_t* visited,
    std::uint64_t hash_capacity,
    std::uint64_t* output,
    std::uint64_t output_capacity,
    bool rhombic_macrocharts,
    unsigned long long* output_count,
    unsigned long long* seam_count,
    unsigned long long* macrochart_crossing_count,
    unsigned long long* paired_direct_count,
    int* status) {
    const std::uint64_t index = std::uint64_t(blockIdx.x) * blockDim.x + threadIdx.x;
    if (index >= frontier_count || atomicAdd(status, 0) != 0) return;

    Cell local[3];
    std::uint32_t seams = 0U;
    std::uint32_t macrochart_crossings = 0U;
    std::uint32_t paired_direct = 0U;
    neighbors_device(
        detail::decode_coord(frontier[index]), level, rhombic_macrocharts,
        local, seams, macrochart_crossings, paired_direct);
    if (seams != 0U) atomicAdd(seam_count, static_cast<unsigned long long>(seams));
    if (macrochart_crossings != 0U) {
        atomicAdd(macrochart_crossing_count,
                  static_cast<unsigned long long>(macrochart_crossings));
    }
    if (paired_direct != 0U) {
        atomicAdd(paired_direct_count,
                  static_cast<unsigned long long>(paired_direct));
    }

    #pragma unroll
    for (int edge = 0; edge < 3; ++edge) {
        const std::uint64_t key = detail::encode_coord(local[edge]);
        const int inserted = hash_insert(visited, hash_capacity, key);
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
    int status;
};

// Reserve one contiguous output range per warp rather than issuing one global
// atomicAdd for every discovered cell.  All lanes in the warp must call this
// helper with the same active_mask.
__device__ inline void warp_append(
    bool append,
    std::uint64_t key,
    std::uint64_t* output,
    std::uint64_t output_capacity,
    unsigned long long* counter,
    int* status,
    unsigned active_mask = 0xffffffffU) noexcept {
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
    const unsigned long long position = base + __popc(selected & lower_lanes);
    if (position < output_capacity) {
        output[position] = key;
    } else {
        atomicCAS(status, 0, 2);
    }
}

// Decode the public hierarchical index once, insert the compact coordinate
// into the persistent discovered set, and construct the unique seed frontier.
// The warp-aggregated append avoids one global counter atomic per seed.
__global__ void decode_seed_kernel(
    const CellIndex* input,
    std::uint64_t input_count,
    std::uint32_t level,
    std::uint64_t* visited,
    std::uint64_t hash_capacity,
    std::uint64_t* unique_feature,
    PersistentState* state) {
    const std::uint64_t index =
        std::uint64_t(blockIdx.x) * blockDim.x + threadIdx.x;
    bool inserted_new = false;
    std::uint64_t key = 0U;
    if (index < input_count && state->status == 0) {
        const CellIndex path = input[index];
        if (!canonical_path(path, level)) {
            atomicCAS(&state->status, 0, 3);
        } else {
            key = detail::encode_coord(path_to_cell(path, level));
            const int inserted = hash_insert(visited, hash_capacity, key);
            if (inserted < 0) atomicCAS(&state->status, 0, 1);
            inserted_new = inserted > 0;
        }
    }
    warp_append(
        inserted_new, key, unique_feature, input_count,
        &state->unique_count, &state->status, __activemask());
}

// Execute every requested ring in one cooperative launch.  The seed frontier
// has already been decoded and deduplicated by decode_seed_kernel, allowing
// that boundary cost and the repeated halo cost to be timed independently
// without transferring the frontier count through the host.
__global__ void persistent_expand_kernel(
    std::uint32_t level,
    std::uint32_t hops,
    std::uint64_t* visited,
    std::uint64_t hash_capacity,
    const std::uint64_t* unique_feature,
    std::uint64_t* output,
    std::uint64_t output_capacity,
    bool rhombic_macrocharts,
    unsigned long long* layer_offsets,
    PersistentState* state) {
    cg::grid_group grid = cg::this_grid();
    const std::uint64_t global_thread =
        std::uint64_t(blockIdx.x) * blockDim.x + threadIdx.x;
    const std::uint64_t grid_threads = std::uint64_t(gridDim.x) * blockDim.x;

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
        if (grid.thread_rank() == 0U && state->status == 0) {
            state->touched_references += 3ULL * frontier_count;
        }
        grid.sync();

        for (std::uint64_t base = 0U; base < frontier_count; base += grid_threads) {
            const std::uint64_t index = base + global_thread;
            Cell local[3]{};
            std::uint32_t seams = 0U;
            std::uint32_t macrochart_crossings = 0U;
            std::uint32_t paired_direct = 0U;
            const bool active = index < frontier_count && state->status == 0;
            if (active) {
                const std::uint64_t source_key = hop == 0U
                    ? unique_feature[index]
                    : output[layer_begin + index];
                neighbors_device(
                    detail::decode_coord(source_key), level, rhombic_macrocharts,
                    local, seams, macrochart_crossings, paired_direct);
            }

            const unsigned active_mask = __activemask();
            const unsigned warp_seams = __reduce_add_sync(active_mask, seams);
            const unsigned warp_macro = __reduce_add_sync(
                active_mask, macrochart_crossings);
            const unsigned warp_paired = __reduce_add_sync(
                active_mask, paired_direct);
            if ((threadIdx.x & 31U) == 0U) {
                if (warp_seams) atomicAdd(
                    &state->seam_count,
                    static_cast<unsigned long long>(warp_seams));
                if (warp_macro) atomicAdd(
                    &state->macrochart_crossing_count,
                    static_cast<unsigned long long>(warp_macro));
                if (warp_paired) atomicAdd(
                    &state->paired_direct_count,
                    static_cast<unsigned long long>(warp_paired));
            }

            #pragma unroll
            for (int edge = 0; edge < 3; ++edge) {
                std::uint64_t candidate_key = 0U;
                bool inserted_new = false;
                if (active) {
                    candidate_key = detail::encode_coord(local[edge]);
                    const int inserted = hash_insert(
                        visited, hash_capacity, candidate_key);
                    if (inserted < 0) atomicCAS(&state->status, 0, 1);
                    inserted_new = inserted > 0;
                }
                warp_append(
                    inserted_new, candidate_key, output, output_capacity,
                    &state->output_count, &state->status, active_mask);
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

// Grid-stride encoder whose count remains device resident.  A fixed number of
// blocks can be launched immediately after the cooperative wavefront without
// first downloading the discovered-cell count.
__global__ void encode_output_state_kernel(
    const std::uint64_t* coordinates,
    CellIndex* output,
    const PersistentState* state,
    std::uint32_t level) {
    const std::uint64_t count = state->output_count;
    const std::uint64_t stride = std::uint64_t(gridDim.x) * blockDim.x;
    for (std::uint64_t index =
             std::uint64_t(blockIdx.x) * blockDim.x + threadIdx.x;
         index < count;
         index += stride) {
        output[index] = cell_to_path(detail::decode_coord(coordinates[index]), level);
    }
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
void grow_device_buffer(T*& pointer, std::uint64_t& capacity, std::uint64_t required) {
    if (required <= capacity) return;
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
            for (auto& event : stage_events_) {
                cuda_check(cudaEventCreate(&event), "cudaEventCreate stage");
            }
            cuda_check(cudaMemcpyToSymbol(d_atlas, detail::kAtlas, sizeof(detail::kAtlas)),
                       "upload Pentakis atlas");
            const auto chunks = detail::make_decode_chunks();
            cuda_check(cudaMemcpyToSymbol(d_decode_chunks, chunks.data(), sizeof(chunks)),
                       "upload prefix decode LUT");
            const auto encode_chunks = detail::make_encode_chunks();
            cuda_check(cudaMemcpyToSymbol(
                           d_encode_chunks, encode_chunks.data(), sizeof(encode_chunks)),
                       "upload prefix encode LUT");
            cuda_check(cudaMalloc(reinterpret_cast<void**>(&d_unique_count_), sizeof(unsigned long long)),
                       "allocate unique counter");
            cuda_check(cudaMalloc(reinterpret_cast<void**>(&d_output_count_), sizeof(unsigned long long)),
                       "allocate output counter");
            cuda_check(cudaMalloc(reinterpret_cast<void**>(&d_seam_count_), sizeof(unsigned long long)),
                       "allocate seam counter");
            cuda_check(cudaMalloc(
                           reinterpret_cast<void**>(&d_macrochart_crossing_count_),
                           sizeof(unsigned long long)),
                       "allocate macrochart crossing counter");
            cuda_check(cudaMalloc(
                           reinterpret_cast<void**>(&d_paired_direct_count_),
                           sizeof(unsigned long long)),
                       "allocate paired edge counter");
            cuda_check(cudaMalloc(reinterpret_cast<void**>(&d_status_), sizeof(int)),
                       "allocate status");
            cuda_check(cudaMalloc(reinterpret_cast<void**>(&d_persistent_state_),
                                  sizeof(PersistentState)),
                       "allocate persistent state");
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
        if (total_keys > std::numeric_limits<std::uint64_t>::max() / 2U) {
            throw std::overflow_error("BAFE hash capacity overflow");
        }
        const std::uint64_t hash_capacity = next_power_of_two(std::max<std::uint64_t>(
            1024U, total_keys * 2U));

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
            (std::uint64_t(options.hops) + 1U > layer_capacity_
                ? static_cast<long double>(std::uint64_t(options.hops) + 1U) *
                    sizeof(unsigned long long) : 0.0L) +
            sizeof(PersistentState) + 64.0L;
        if (required_bytes > static_cast<long double>(free_bytes) * 0.85L) {
            throw std::length_error(
                "BAFE query workspace exceeds 85% of currently free GPU memory; "
                "set a realistic max_output_cells or split the query");
        }

        grow_device_buffer(d_input_, input_capacity_, input_count);
        grow_device_buffer(d_feature_, feature_capacity_, input_count);
        grow_device_buffer(d_output_coord_, output_capacity_, requested_output);
        grow_device_buffer(d_output_path_, path_capacity_, requested_output);
        grow_device_buffer(d_hash_, hash_capacity_, hash_capacity);
        grow_device_buffer(
            d_layer_offsets_, layer_capacity_, std::uint64_t(options.hops) + 1U);

        result.metrics.device_peak_bytes =
            input_capacity_ * sizeof(CellIndex) +
            feature_capacity_ * sizeof(std::uint64_t) +
            output_capacity_ * sizeof(std::uint64_t) +
            path_capacity_ * sizeof(CellIndex) +
            hash_capacity_ * sizeof(std::uint64_t) +
            layer_capacity_ * sizeof(unsigned long long) +
            sizeof(PersistentState) + 32U;
        std::uint64_t hash_value = hash_capacity;
        while (hash_value > 1U) {
            ++result.metrics.hash_capacity_log2;
            hash_value >>= 1U;
        }
        result.metrics.host_to_device_bytes = input_count * sizeof(CellIndex);

        cuda_check(cudaEventRecord(start_, stream_), "record pipeline start");
        cuda_check(cudaEventRecord(stage_events_[0], stream_), "record upload start");
        cuda_check(cudaMemcpyAsync(
            d_input_, input, input_count * sizeof(CellIndex),
            cudaMemcpyHostToDevice, stream_), "copy input to GPU");
        cuda_check(cudaEventRecord(stage_events_[1], stream_), "record upload end");
        cuda_check(cudaMemsetAsync(
            d_hash_, 0xff, hash_capacity * sizeof(std::uint64_t), stream_),
            "clear visited hash");
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
        cuda_check(cudaMemsetAsync(
                       d_persistent_state_, 0, sizeof(PersistentState), stream_),
                   "clear persistent state");
        cuda_check(cudaMemsetAsync(
                       d_layer_offsets_, 0,
                       (std::uint64_t(options.hops) + 1U) *
                           sizeof(unsigned long long),
                       stream_),
                   "clear device layer offsets");
        cuda_check(cudaEventRecord(
                       stage_events_[2], stream_),
                   "record workspace clear end");
        unsigned long long unique_count = 0U;
        unsigned long long output_count = 0U;
        unsigned long long seams = 0U;
        unsigned long long macrochart_crossings = 0U;
        unsigned long long paired_direct = 0U;
        int status = 0;
        const bool cooperative_supported = properties_.cooperativeLaunch != 0;
        if (options.cuda_schedule == CudaSchedule::CooperativePersistent &&
            !cooperative_supported) {
            throw std::runtime_error(
                "The selected CUDA device does not support cooperative grid launch");
        }
        const bool use_cooperative = cooperative_supported &&
            options.cuda_schedule != CudaSchedule::HostDriven;
        bool encoded_on_device = false;

        if (use_cooperative) {
            result.metrics.backend = "cuda_profiled_cooperative_hash_wavefront";
            const int input_blocks =
                static_cast<int>((input_count + kThreads - 1U) / kThreads);
            decode_seed_kernel<<<input_blocks, kThreads, 0, stream_>>>(
                d_input_, input_count, options.level, d_hash_, hash_capacity,
                d_feature_, d_persistent_state_);
            cuda_check(cudaGetLastError(), "launch decode_seed_kernel");
            ++result.metrics.kernel_launches;
            cuda_check(cudaEventRecord(
                           stage_events_[3], stream_),
                       "record seed decode end");

            int resident_blocks_per_sm = 0;
            cuda_check(cudaOccupancyMaxActiveBlocksPerMultiprocessor(
                           &resident_blocks_per_sm,
                           persistent_expand_kernel,
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

            std::uint64_t hash_capacity_argument = hash_capacity;
            std::uint64_t output_capacity_argument = requested_output;
            std::uint32_t level_argument = options.level;
            std::uint32_t hops_argument = options.hops;
            bool rhombic_macrocharts_argument =
                options.chart_mode == ChartMode::RhombicMacrocharts;
            void* arguments[] = {
                &level_argument,
                &hops_argument,
                &d_hash_,
                &hash_capacity_argument,
                &d_feature_,
                &d_output_coord_,
                &output_capacity_argument,
                &rhombic_macrocharts_argument,
                &d_layer_offsets_,
                &d_persistent_state_};
            cuda_check(cudaLaunchCooperativeKernel(
                           reinterpret_cast<const void*>(persistent_expand_kernel),
                           blocks,
                           kThreads,
                           arguments,
                           0,
                           stream_),
                       "launch cooperative persistent expansion kernel");
            ++result.metrics.kernel_launches;
            cuda_check(cudaEventRecord(
                           stage_events_[4], stream_),
                       "record halo expansion end");

            // The encoder reads the output count directly from device state.
            // A bounded grid-stride launch avoids a count transfer and remains
            // efficient even when the conservative output bound is large.
            const std::uint64_t encoder_work = std::max<std::uint64_t>(
                1U, std::min<std::uint64_t>(requested_output,
                    std::uint64_t(properties_.multiProcessorCount) *
                        kThreads * 8U));
            const int encoder_blocks = static_cast<int>(
                (encoder_work + kThreads - 1U) / kThreads);
            encode_output_state_kernel<<<encoder_blocks, kThreads, 0, stream_>>>(
                d_output_coord_, d_output_path_, d_persistent_state_, options.level);
            cuda_check(cudaGetLastError(), "launch encode_output_state_kernel");
            ++result.metrics.kernel_launches;
            encoded_on_device = true;
            cuda_check(cudaEventRecord(
                           stage_events_[5], stream_),
                       "record output encode end");

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
            result.metrics.macrochart_boundary_crossings =
                host_state.macrochart_crossing_count;
            result.metrics.paired_edge_direct_transforms =
                host_state.paired_direct_count;
            macrochart_crossings = host_state.macrochart_crossing_count;
            paired_direct = host_state.paired_direct_count;
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
                d_feature_, d_unique_count_, d_status_);
            cuda_check(cudaGetLastError(), "launch initialize_feature_kernel");
            ++result.metrics.kernel_launches;
            cuda_check(cudaEventRecord(
                           stage_events_[3], stream_),
                       "record host-driven seed decode end");

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
            for (std::uint32_t hop = 0; hop < options.hops && status == 0; ++hop) {
                if (frontier_count != 0U) {
                    result.metrics.touched_neighbor_references += 3ULL * frontier_count;
                    const int blocks =
                        static_cast<int>((frontier_count + kThreads - 1U) / kThreads);
                    expand_frontier_kernel<<<blocks, kThreads, 0, stream_>>>(
                        frontier, frontier_count, options.level, d_hash_, hash_capacity,
                        d_output_coord_, requested_output,
                        options.chart_mode == ChartMode::RhombicMacrocharts,
                        d_output_count_, d_seam_count_,
                        d_macrochart_crossing_count_, d_paired_direct_count_,
                        d_status_);
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
            cuda_check(cudaEventRecord(
                           stage_events_[4], stream_),
                       "record host-driven halo expansion end");
        }

        result.metrics.unique_input_cells = unique_count;

        if (!encoded_on_device && output_count != 0U && output_count <= requested_output) {
            const int blocks = static_cast<int>((output_count + kThreads - 1U) / kThreads);
            encode_output_kernel<<<blocks, kThreads, 0, stream_>>>(
                d_output_coord_, d_output_path_, output_count, options.level);
            cuda_check(cudaGetLastError(), "launch encode_output_kernel");
            ++result.metrics.kernel_launches;
        }
        if (!encoded_on_device) {
            cuda_check(cudaEventRecord(
                           stage_events_[5], stream_),
                       "record host-driven output encode end");
        }
        cuda_check(cudaEventRecord(stop_, stream_), "record pipeline stop");
        cuda_check(cudaEventSynchronize(stop_), "synchronize pipeline stop");
        float pipeline_ms = 0.0F;
        cuda_check(cudaEventElapsedTime(&pipeline_ms, start_, stop_), "measure pipeline");
        auto elapsed = [&](cudaEvent_t first, cudaEvent_t second, const char* label) {
            float value = 0.0F;
            cuda_check(cudaEventElapsedTime(&value, first, second), label);
            return static_cast<double>(value);
        };
        result.metrics.gpu_upload_ms = elapsed(
            stage_events_[0], stage_events_[1], "measure GPU upload");
        result.metrics.gpu_workspace_clear_ms = elapsed(
            stage_events_[1], stage_events_[2], "measure GPU workspace clear");
        result.metrics.gpu_decode_seed_hash_ms = elapsed(
            stage_events_[2], stage_events_[3], "measure GPU seed decode and hash");
        result.metrics.gpu_halo_expand_hash_ms = elapsed(
            stage_events_[3], stage_events_[4], "measure GPU halo expansion and hash");
        result.metrics.gpu_output_encode_ms = elapsed(
            stage_events_[4], stage_events_[5], "measure GPU output encoding");

        if (status == 1) throw std::runtime_error("BAFE GPU visited hash table exhausted");
        if (status == 2 || output_count > requested_output) {
            throw std::length_error("BAFE halo exceeded max_output_cells");
        }
        if (status == 3) {
            throw std::invalid_argument("Input contains a non-canonical or mixed-level BAFE index");
        }

        result.cells.resize(output_count);
        const auto download_start = std::chrono::steady_clock::now();
        if (output_count != 0U) {
            cuda_check(cudaMemcpyAsync(
                result.cells.data(), d_output_path_, output_count * sizeof(CellIndex),
                cudaMemcpyDeviceToHost, stream_), "copy halo output to host");
            cuda_check(cudaStreamSynchronize(stream_), "synchronize halo download");
        }
        result.metrics.gpu_download_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - download_start).count();

        if (options.deterministic_layers) {
            for (std::size_t layer = 0; layer + 1U < result.layer_offsets.size(); ++layer) {
                std::sort(
                    result.cells.begin() + static_cast<std::ptrdiff_t>(result.layer_offsets[layer]),
                    result.cells.begin() + static_cast<std::ptrdiff_t>(result.layer_offsets[layer + 1U]));
            }
        }

        result.metrics.output_cells = output_count;
        result.metrics.root_edge_crossings = seams;
        result.metrics.macrochart_boundary_crossings = macrochart_crossings;
        result.metrics.paired_edge_direct_transforms = paired_direct;
        result.metrics.device_pipeline_ms = pipeline_ms;
        result.metrics.device_to_host_bytes =
            output_count * sizeof(CellIndex) + result.metrics.control_transfer_bytes;
        result.metrics.end_to_end_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - wall_start).count();
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
    void release_noexcept() noexcept {
        cudaSetDevice(ordinal_);
        cudaFree(d_input_);
        cudaFree(d_feature_);
        cudaFree(d_output_coord_);
        cudaFree(d_output_path_);
        cudaFree(d_hash_);
        cudaFree(d_layer_offsets_);
        cudaFree(d_unique_count_);
        cudaFree(d_output_count_);
        cudaFree(d_seam_count_);
        cudaFree(d_macrochart_crossing_count_);
        cudaFree(d_paired_direct_count_);
        cudaFree(d_status_);
        cudaFree(d_persistent_state_);
        if (start_) cudaEventDestroy(start_);
        if (stop_) cudaEventDestroy(stop_);
        for (auto& event : stage_events_) {
            if (event) cudaEventDestroy(event);
        }
        if (stream_) cudaStreamDestroy(stream_);
    }

    int ordinal_ = 0;
    cudaDeviceProp properties_{};
    cudaStream_t stream_ = nullptr;
    cudaEvent_t start_ = nullptr;
    cudaEvent_t stop_ = nullptr;
    // upload start/end, workspace clear end, seed decode end, halo end,
    // output encoding end, and download end.
    cudaEvent_t stage_events_[7]{};
    std::mutex mutex_;

    CellIndex* d_input_ = nullptr;
    std::uint64_t* d_feature_ = nullptr;
    std::uint64_t* d_output_coord_ = nullptr;
    CellIndex* d_output_path_ = nullptr;
    std::uint64_t* d_hash_ = nullptr;
    unsigned long long* d_layer_offsets_ = nullptr;
    unsigned long long* d_unique_count_ = nullptr;
    unsigned long long* d_output_count_ = nullptr;
    unsigned long long* d_seam_count_ = nullptr;
    unsigned long long* d_macrochart_crossing_count_ = nullptr;
    unsigned long long* d_paired_direct_count_ = nullptr;
    int* d_status_ = nullptr;
    PersistentState* d_persistent_state_ = nullptr;

    std::uint64_t input_capacity_ = 0U;
    std::uint64_t feature_capacity_ = 0U;
    std::uint64_t output_capacity_ = 0U;
    std::uint64_t path_capacity_ = 0U;
    std::uint64_t hash_capacity_ = 0U;
    std::uint64_t layer_capacity_ = 0U;
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
