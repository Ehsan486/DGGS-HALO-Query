#pragma once

#include <cstdint>
#include <stdexcept>
#include <utility>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace bafe {

// BAFE's public self-describing hierarchical index.
//
//   bit  63      : reserved and required to be zero
//   bits 62..57 : Pentakis root face [0, 59]
//   bits 56..0  : base-4 refinement path, then one level sentinel, then zeros
//
// Descendants share a contiguous prefix.  Parent/ancestor queries therefore
// require one mask rather than repeatedly applying a geometric parent rule.
using CellIndex = std::uint64_t;

constexpr std::uint32_t kRootFaceCount = 60;
constexpr std::uint32_t kMaxLevel = 28;
constexpr std::uint32_t kFaceShift = 57;
constexpr std::uint32_t kRootSentinelBit = 56;
constexpr const char* kIndexSchema = "BAFE-PREFIX-1";

inline std::uint32_t face(CellIndex id) noexcept {
    return static_cast<std::uint32_t>((id >> kFaceShift) & 0x3fULL);
}

inline CellIndex root(std::uint32_t root_face) {
    if (root_face >= kRootFaceCount) {
        throw std::out_of_range("BAFE root face must be in [0, 59]");
    }
    return (CellIndex(root_face) << kFaceShift) |
           (CellIndex(1) << kRootSentinelBit);
}

inline std::uint32_t level(CellIndex id) {
    if ((id >> 63U) != 0U || face(id) >= kRootFaceCount) {
        throw std::invalid_argument("Invalid BAFE cell index");
    }
    const CellIndex payload = id & ((CellIndex(1) << kFaceShift) - 1U);
    if (payload == 0U) throw std::invalid_argument("BAFE index has no level sentinel");
#if defined(_MSC_VER)
    unsigned long sentinel = 0;
#if defined(_M_X64)
    _BitScanForward64(&sentinel, payload);
#else
    const std::uint32_t lo = static_cast<std::uint32_t>(payload);
    if (lo != 0U) _BitScanForward(&sentinel, lo);
    else {
        _BitScanForward(&sentinel, static_cast<std::uint32_t>(payload >> 32U));
        sentinel += 32U;
    }
#endif
    const std::uint32_t s = static_cast<std::uint32_t>(sentinel);
#else
    const std::uint32_t s = static_cast<std::uint32_t>(__builtin_ctzll(payload));
#endif
    if (s > kRootSentinelBit || (s & 1U) != 0U) {
        throw std::invalid_argument("Invalid BAFE level sentinel position");
    }
    return (kRootSentinelBit - s) / 2U;
}

inline bool is_canonical(CellIndex id) noexcept {
    try {
        (void)level(id);
        return true;
    } catch (...) {
        return false;
    }
}

inline bool is_canonical(CellIndex id, std::uint32_t expected_level) noexcept {
    try {
        return level(id) == expected_level;
    } catch (...) {
        return false;
    }
}

inline std::uint32_t child_digit(
    CellIndex id,
    std::uint32_t level,
    std::uint32_t child_level) {
    if (!is_canonical(id, level) || child_level == 0U || child_level > level) {
        throw std::invalid_argument("Invalid BAFE level or cell index");
    }
    const std::uint32_t shift = kFaceShift - 2U * child_level;
    return static_cast<std::uint32_t>((id >> shift) & 3U);
}

inline std::uint32_t child_digit(CellIndex id, std::uint32_t child_level) {
    return child_digit(id, level(id), child_level);
}

inline CellIndex child(
    CellIndex id,
    std::uint32_t level,
    std::uint32_t digit) {
    if (!is_canonical(id, level) || level >= kMaxLevel || digit >= 4U) {
        throw std::invalid_argument("Invalid BAFE child request");
    }
    const std::uint32_t sentinel = kRootSentinelBit - 2U * level;
    return (id & ~(CellIndex(1) << sentinel)) |
           (CellIndex(digit) << (sentinel - 1U)) |
           (CellIndex(1) << (sentinel - 2U));
}

inline CellIndex child(CellIndex id, std::uint32_t digit) {
    return child(id, level(id), digit);
}

inline CellIndex ancestor(
    CellIndex id,
    std::uint32_t source_level,
    std::uint32_t target_level) {
    if (!is_canonical(id, source_level) || target_level > source_level) {
        throw std::invalid_argument("Invalid BAFE ancestor request");
    }
    const std::uint32_t sentinel = kRootSentinelBit - 2U * target_level;
    const CellIndex clear_mask = (CellIndex(1) << (sentinel + 1U)) - 1U;
    return (id & ~clear_mask) | (CellIndex(1) << sentinel);
}

inline CellIndex ancestor(CellIndex id, std::uint32_t target_level) {
    return ancestor(id, level(id), target_level);
}

inline CellIndex parent(CellIndex id, std::uint32_t level) {
    if (level == 0U) throw std::invalid_argument("A level-0 cell has no parent");
    return ancestor(id, level, level - 1U);
}


inline CellIndex parent(CellIndex id) {
    const std::uint32_t source_level = level(id);
    return parent(id, source_level);
}

struct DescendantSpan {
    CellIndex first;
    CellIndex last;
    CellIndex stride;
    std::uint64_t count;
    std::uint32_t target_level;
};

// Canonical descendants have a fixed target-level sentinel and are separated
// by stride; integers between first and last are not necessarily valid IDs.
inline DescendantSpan descendants(
    CellIndex id,
    std::uint32_t source_level,
    std::uint32_t target_level) {
    if (!is_canonical(id, source_level) || target_level < source_level ||
        target_level > kMaxLevel) {
        throw std::invalid_argument("Invalid BAFE descendant span");
    }
    const std::uint32_t delta = target_level - source_level;
    const std::uint32_t target_sentinel = kRootSentinelBit - 2U * target_level;
    const std::uint32_t source_sentinel = kRootSentinelBit - 2U * source_level;
    const CellIndex prefix_without_sentinel =
        id & ~(CellIndex(1) << source_sentinel);
    const CellIndex first =
        prefix_without_sentinel | (CellIndex(1) << target_sentinel);
    const CellIndex count = CellIndex(1) << (2U * delta);
    const CellIndex stride = CellIndex(1) << (target_sentinel + 1U);
    return DescendantSpan{
        first,
        first + (count - 1U) * stride,
        stride,
        count,
        target_level};
}

inline DescendantSpan descendants(CellIndex id, std::uint32_t target_level) {
    return descendants(id, level(id), target_level);
}

// Inclusive numeric range of canonical target-level keys sharing this prefix.
inline std::pair<CellIndex, CellIndex> descendant_range(
    CellIndex id,
    std::uint32_t source_level,
    std::uint32_t target_level) {
    if (!is_canonical(id, source_level) || target_level < source_level ||
        target_level > kMaxLevel) {
        throw std::invalid_argument("Invalid BAFE descendant range");
    }
    const CellIndex prefix = ancestor(id, source_level, source_level);
    const std::uint32_t free_bits = 2U * (target_level - source_level);
    if (free_bits == 0U) return {prefix, prefix};
    const std::uint32_t target_sentinel = kRootSentinelBit - 2U * target_level;
    const std::uint32_t source_sentinel = kRootSentinelBit - 2U * source_level;
    const CellIndex prefix_without_sentinel = prefix & ~(CellIndex(1) << source_sentinel);
    const CellIndex minimum = prefix_without_sentinel | (CellIndex(1) << target_sentinel);
    const CellIndex suffix = ((CellIndex(1) << free_bits) - 1U)
        << (target_sentinel + 1U);
    return {minimum, minimum | suffix};
}

// Migration helpers for the project's earlier fixed-level layout:
// face:6 | i:28 | j:28 | orientation:1.  The level remains an argument.
inline std::uint64_t export_fixed_level_id(CellIndex id, std::uint32_t source_level) {
    if (!is_canonical(id, source_level)) {
        throw std::invalid_argument("Invalid BAFE index for fixed-level export");
    }
    std::uint32_t i = 0U, j = 0U, orientation = 0U;
    for (std::uint32_t depth = 1U; depth <= source_level; ++depth) {
        const std::uint32_t digit = child_digit(id, source_level, depth);
        i <<= 1U;
        j <<= 1U;
        if (orientation == 0U) {
            if (digit == 1U) ++i;
            else if (digit == 2U) ++j;
            else if (digit == 3U) orientation = 1U;
        } else {
            if (digit == 0U) ++i;
            else if (digit == 1U) ++j;
            else if (digit == 2U) { ++i; ++j; }
            else { ++i; ++j; orientation = 0U; }
        }
    }
    return (CellIndex(face(id)) << 57U) |
           (CellIndex(i) << 29U) |
           (CellIndex(j) << 1U) |
           orientation;
}

inline CellIndex import_fixed_level_id(std::uint64_t fixed, std::uint32_t source_level) {
    if (source_level > kMaxLevel || (fixed >> 63U) != 0U) {
        throw std::invalid_argument("Invalid fixed-level Pentakis ID");
    }
    const std::uint32_t root_face = static_cast<std::uint32_t>((fixed >> 57U) & 0x3fULL);
    std::uint32_t i = static_cast<std::uint32_t>((fixed >> 29U) & ((1ULL << 28U) - 1U));
    std::uint32_t j = static_cast<std::uint32_t>((fixed >> 1U) & ((1ULL << 28U) - 1U));
    std::uint32_t orientation = static_cast<std::uint32_t>(fixed & 1U);
    const std::uint64_t n = std::uint64_t(1) << source_level;
    const std::uint64_t sum = std::uint64_t(i) + j;
    if (root_face >= kRootFaceCount ||
        (orientation == 0U ? sum >= n : sum + 1U >= n)) {
        throw std::invalid_argument("Fixed-level ID is outside the triangular chart");
    }

    CellIndex id = CellIndex(root_face) << kFaceShift;
    for (std::uint32_t depth = source_level; depth > 0U; --depth) {
        const std::uint32_t parity = ((i & 1U) << 1U) | (j & 1U);
        std::uint32_t digit;
        if (orientation == 0U) {
            digit = parity == 0U ? 0U : parity == 2U ? 1U :
                    parity == 1U ? 2U : 3U;
            orientation = ((i & 1U) == 0U || (j & 1U) == 0U) ? 0U : 1U;
        } else {
            digit = parity == 2U ? 0U : parity == 1U ? 1U :
                    parity == 3U ? 2U : 3U;
            orientation = ((i & 1U) == 0U && (j & 1U) == 0U) ? 0U : 1U;
        }
        id |= CellIndex(digit) << (kFaceShift - 2U * depth);
        i >>= 1U;
        j >>= 1U;
    }
    return id | (CellIndex(1) << (kRootSentinelBit - 2U * source_level));
}

// Original project layout used before level-28 support:
// level:5 | face:6 | i:26 | j:26 | orientation:1.
inline CellIndex import_legacy_id(std::uint64_t legacy) {
    const std::uint32_t source_level = static_cast<std::uint32_t>((legacy >> 59U) & 0x1fULL);
    if (source_level > 26U) throw std::invalid_argument("Legacy ID level exceeds 26");
    const std::uint32_t root_face = static_cast<std::uint32_t>((legacy >> 53U) & 0x3fULL);
    const std::uint32_t i = static_cast<std::uint32_t>((legacy >> 27U) & ((1ULL << 26U) - 1U));
    const std::uint32_t j = static_cast<std::uint32_t>((legacy >> 1U) & ((1ULL << 26U) - 1U));
    const std::uint64_t fixed = (CellIndex(root_face) << 57U) |
        (CellIndex(i) << 29U) | (CellIndex(j) << 1U) | (legacy & 1U);
    return import_fixed_level_id(fixed, source_level);
}

inline std::uint64_t export_legacy_id(CellIndex id, std::uint32_t source_level) {
    if (source_level > 26U) {
        throw std::invalid_argument("The legacy ID schema supports levels only through 26");
    }
    const std::uint64_t fixed = export_fixed_level_id(id, source_level);
    const std::uint64_t root_face = (fixed >> 57U) & 0x3fULL;
    const std::uint64_t i = (fixed >> 29U) & ((1ULL << 28U) - 1U);
    const std::uint64_t j = (fixed >> 1U) & ((1ULL << 28U) - 1U);
    return (CellIndex(source_level) << 59U) | (root_face << 53U) |
           (i << 27U) | (j << 1U) | (fixed & 1U);
}

}  // namespace bafe
