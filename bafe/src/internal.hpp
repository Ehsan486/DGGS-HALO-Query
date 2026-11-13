#pragma once

#include "bafe/index.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>

#if defined(__CUDACC__)
#define BAFE_HD __host__ __device__
#else
#define BAFE_HD
#endif

namespace bafe::detail {

constexpr std::uint64_t kCoordMask28 = (1ULL << 28U) - 1ULL;

struct Cell {
    std::uint32_t face;
    std::uint32_t i;
    std::uint32_t j;
    std::uint32_t orientation;
};

BAFE_HD inline bool operator==(const Cell& a, const Cell& b) noexcept {
    return a.face == b.face && a.i == b.i && a.j == b.j &&
           a.orientation == b.orientation;
}

BAFE_HD inline Cell child_once(const Cell& cell, std::uint32_t digit) noexcept {
    const std::uint32_t i = cell.i << 1U;
    const std::uint32_t j = cell.j << 1U;
    if (cell.orientation == 0U) {
        if (digit == 0U) return Cell{cell.face, i, j, 0U};
        if (digit == 1U) return Cell{cell.face, i + 1U, j, 0U};
        if (digit == 2U) return Cell{cell.face, i, j + 1U, 0U};
        return Cell{cell.face, i, j, 1U};
    }
    if (digit == 0U) return Cell{cell.face, i + 1U, j, 1U};
    if (digit == 1U) return Cell{cell.face, i, j + 1U, 1U};
    if (digit == 2U) return Cell{cell.face, i + 1U, j + 1U, 1U};
    return Cell{cell.face, i + 1U, j + 1U, 0U};
}

BAFE_HD inline Cell parent_once(const Cell& cell) noexcept {
    const bool even_i = (cell.i & 1U) == 0U;
    const bool even_j = (cell.j & 1U) == 0U;
    std::uint32_t orientation;
    if (cell.orientation == 0U) {
        orientation = (even_i || even_j) ? 0U : 1U;
    } else {
        orientation = (even_i && even_j) ? 0U : 1U;
    }
    return Cell{cell.face, cell.i >> 1U, cell.j >> 1U, orientation};
}

BAFE_HD inline std::uint32_t inverse_child_digit(const Cell& cell) noexcept {
    const std::uint32_t parity = ((cell.i & 1U) << 1U) | (cell.j & 1U);
    if (cell.orientation == 0U) {
        // parity 00, 10, 01, 11 -> digits 0, 1, 2, 3
        return parity == 0U ? 0U : parity == 2U ? 1U : parity == 1U ? 2U : 3U;
    }
    // parity 10, 01, 11, 00 -> digits 0, 1, 2, 3
    return parity == 2U ? 0U : parity == 1U ? 1U : parity == 3U ? 2U : 3U;
}

BAFE_HD inline std::uint64_t encode_coord(const Cell& cell) noexcept {
    return (std::uint64_t(cell.face) << 57U) |
           (std::uint64_t(cell.i) << 29U) |
           (std::uint64_t(cell.j) << 1U) |
           std::uint64_t(cell.orientation);
}

BAFE_HD inline Cell decode_coord(std::uint64_t id) noexcept {
    return Cell{
        std::uint32_t((id >> 57U) & 0x3FULL),
        std::uint32_t((id >> 29U) & kCoordMask28),
        std::uint32_t((id >> 1U) & kCoordMask28),
        std::uint32_t(id & 1ULL)};
}

BAFE_HD inline bool valid_cell(const Cell& cell, std::uint32_t level) noexcept {
    if (level > kMaxLevel || cell.face >= kRootFaceCount || cell.orientation > 1U) {
        return false;
    }
    const std::uint64_t n = std::uint64_t(1) << level;
    const std::uint64_t sum = std::uint64_t(cell.i) + cell.j;
    return cell.orientation == 0U ? sum < n : sum + 1ULL < n;
}

// Scalar reference conversion.  The CUDA implementation uses a four-level
// constant-memory chunk table for the hot input conversion path.
BAFE_HD inline Cell path_to_cell_scalar(CellIndex id, std::uint32_t level) noexcept {
    Cell cell{std::uint32_t((id >> kFaceShift) & 0x3fULL), 0U, 0U, 0U};
    for (std::uint32_t depth = 1U; depth <= level; ++depth) {
        const std::uint32_t shift = kFaceShift - 2U * depth;
        cell = child_once(cell, std::uint32_t((id >> shift) & 3ULL));
    }
    return cell;
}

BAFE_HD inline CellIndex cell_to_path_scalar(Cell cell, std::uint32_t level) noexcept {
    CellIndex id = CellIndex(cell.face) << kFaceShift;
    for (std::uint32_t depth = level; depth > 0U; --depth) {
        const std::uint32_t digit = inverse_child_digit(cell);
        const std::uint32_t shift = kFaceShift - 2U * depth;
        id |= CellIndex(digit) << shift;
        cell = parent_once(cell);
    }
    return id | (CellIndex(1) << (kRootSentinelBit - 2U * level));
}

struct DecodeChunk {
    std::uint8_t delta_i;
    std::uint8_t delta_j;
    std::uint8_t end_orientation;
    std::uint8_t unused;
};

// Reverse of DecodeChunk.  A fine cell is reduced by four hierarchy levels
// using one 512-entry lookup indexed by (orientation, low-i, low-j).  The
// packed digits use the same coarse-to-fine byte layout as DecodeChunk.
struct EncodeChunk {
    std::uint8_t packed_digits;
    std::uint8_t parent_orientation;
    std::uint8_t unused0;
    std::uint8_t unused1;
};

inline std::array<DecodeChunk, 512> make_decode_chunks() {
    std::array<DecodeChunk, 512> table{};
    for (std::uint32_t orientation = 0; orientation < 2; ++orientation) {
        for (std::uint32_t packed = 0; packed < 256; ++packed) {
            Cell cell{0U, 0U, 0U, orientation};
            for (std::uint32_t step = 0; step < 4; ++step) {
                const std::uint32_t shift = 6U - 2U * step;
                cell = child_once(cell, (packed >> shift) & 3U);
            }
            table[orientation * 256U + packed] = DecodeChunk{
                static_cast<std::uint8_t>(cell.i),
                static_cast<std::uint8_t>(cell.j),
                static_cast<std::uint8_t>(cell.orientation),
                0U};
        }
    }
    return table;
}

inline std::array<EncodeChunk, 512> make_encode_chunks() {
    std::array<EncodeChunk, 512> table{};
    for (std::uint32_t orientation = 0; orientation < 2U; ++orientation) {
        for (std::uint32_t low_i = 0; low_i < 16U; ++low_i) {
            for (std::uint32_t low_j = 0; low_j < 16U; ++low_j) {
                Cell cell{0U, low_i, low_j, orientation};
                std::uint32_t packed = 0U;
                for (std::uint32_t step = 0; step < 4U; ++step) {
                    packed |= inverse_child_digit(cell) << (2U * step);
                    cell = parent_once(cell);
                }
                const std::uint32_t index =
                    orientation * 256U + low_i * 16U + low_j;
                table[index] = EncodeChunk{
                    static_cast<std::uint8_t>(packed),
                    static_cast<std::uint8_t>(cell.orientation),
                    0U,
                    0U};
            }
        }
    }
    return table;
}

inline Cell path_to_cell_chunked(
    CellIndex id,
    std::uint32_t level,
    const DecodeChunk* chunks) noexcept {
    Cell cell{std::uint32_t((id >> kFaceShift) & 0x3fULL), 0U, 0U, 0U};
    std::uint32_t processed = 0U;
    while (level - processed >= 4U) {
        const std::uint32_t shift = kFaceShift - 2U * (processed + 4U);
        const std::uint32_t packed = std::uint32_t((id >> shift) & 0xffULL);
        const DecodeChunk entry = chunks[cell.orientation * 256U + packed];
        cell.i = (cell.i << 4U) | entry.delta_i;
        cell.j = (cell.j << 4U) | entry.delta_j;
        cell.orientation = entry.end_orientation;
        processed += 4U;
    }
    while (processed < level) {
        ++processed;
        const std::uint32_t shift = kFaceShift - 2U * processed;
        cell = child_once(cell, std::uint32_t((id >> shift) & 3ULL));
    }
    return cell;
}


inline CellIndex cell_to_path_chunked(
    Cell cell,
    std::uint32_t level,
    const EncodeChunk* chunks) noexcept {
    CellIndex id = CellIndex(cell.face) << kFaceShift;
    std::uint32_t remaining = level;
    while (remaining >= 4U) {
        const std::uint32_t index =
            cell.orientation * 256U + (cell.i & 15U) * 16U + (cell.j & 15U);
        const EncodeChunk entry = chunks[index];
        const std::uint32_t shift = kFaceShift - 2U * remaining;
        id |= CellIndex(entry.packed_digits) << shift;
        cell.i >>= 4U;
        cell.j >>= 4U;
        cell.orientation = entry.parent_orientation;
        remaining -= 4U;
    }
    while (remaining > 0U) {
        const std::uint32_t digit = inverse_child_digit(cell);
        id |= CellIndex(digit) << (kFaceShift - 2U * remaining);
        cell = parent_once(cell);
        --remaining;
    }
    return id | (CellIndex(1) << (kRootSentinelBit - 2U * level));
}

}  // namespace bafe::detail

#undef BAFE_HD
