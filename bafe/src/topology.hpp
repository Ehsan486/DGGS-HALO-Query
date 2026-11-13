#pragma once

#include "atlas.hpp"
#include "internal.hpp"

#include <cstdint>

#if defined(__CUDACC__)
#define BAFE_TOPO_HD __host__ __device__
#else
#define BAFE_TOPO_HD
#endif

namespace bafe::detail {

BAFE_TOPO_HD inline Cell boundary_up(
    std::uint32_t face,
    std::uint32_t edge,
    std::uint32_t parameter,
    std::uint32_t n) noexcept {
    if (edge == 0U) return Cell{face, n - 1U - parameter, parameter, 0U};
    if (edge == 1U) return Cell{face, 0U, parameter, 0U};
    return Cell{face, parameter, 0U, 0U};
}

inline Cell cross_edge_host(
    const Cell& cell,
    std::uint32_t edge,
    std::uint32_t parameter,
    std::uint32_t n) noexcept {
    const Transition transition = kAtlas[cell.face * 3U + edge];
    const std::uint32_t mapped = transition.reverse
        ? n - 1U - parameter
        : parameter;
    return boundary_up(transition.dst_face, transition.dst_edge, mapped, n);
}

inline void neighbors_host(
    const Cell& cell,
    std::uint32_t level,
    Cell output[3],
    std::uint32_t* seam_count = nullptr) noexcept {
    const std::uint32_t n = 1U << level;
    if (cell.orientation == 1U) {
        output[0] = Cell{cell.face, cell.i, cell.j, 0U};
        output[1] = Cell{cell.face, cell.i + 1U, cell.j, 0U};
        output[2] = Cell{cell.face, cell.i, cell.j + 1U, 0U};
        return;
    }
    if (cell.i + cell.j + 1U < n) {
        output[0] = Cell{cell.face, cell.i, cell.j, 1U};
    } else {
        output[0] = cross_edge_host(cell, 0U, cell.j, n);
        if (seam_count) ++*seam_count;
    }
    if (cell.j > 0U) {
        output[1] = Cell{cell.face, cell.i, cell.j - 1U, 1U};
    } else {
        output[1] = cross_edge_host(cell, 2U, cell.i, n);
        if (seam_count) ++*seam_count;
    }
    if (cell.i > 0U) {
        output[2] = Cell{cell.face, cell.i - 1U, cell.j, 1U};
    } else {
        output[2] = cross_edge_host(cell, 1U, cell.j, n);
        if (seam_count) ++*seam_count;
    }
}

// Execution-only rhombic macrochart: canonical root faces (2p,2p+1)
// share edge 0.  Crossing that internal edge is a direct coordinate swap;
// the returned cells and public IDs remain the original triangles.
inline void neighbors_rhombic_host(
    const Cell& cell,
    std::uint32_t level,
    Cell output[3],
    std::uint32_t* seam_count = nullptr,
    std::uint32_t* macrochart_boundary_count = nullptr,
    std::uint32_t* paired_direct_count = nullptr) noexcept {
    const std::uint32_t n = 1U << level;
    if (cell.orientation == 1U) {
        output[0] = Cell{cell.face, cell.i, cell.j, 0U};
        output[1] = Cell{cell.face, cell.i + 1U, cell.j, 0U};
        output[2] = Cell{cell.face, cell.i, cell.j + 1U, 0U};
        return;
    }
    if (cell.i + cell.j + 1U < n) {
        output[0] = Cell{cell.face, cell.i, cell.j, 1U};
    } else {
        output[0] = Cell{cell.face ^ 1U, cell.j, cell.i, 0U};
        if (seam_count) ++*seam_count;
        if (paired_direct_count) ++*paired_direct_count;
    }
    if (cell.j > 0U) {
        output[1] = Cell{cell.face, cell.i, cell.j - 1U, 1U};
    } else {
        output[1] = cross_edge_host(cell, 2U, cell.i, n);
        if (seam_count) ++*seam_count;
    }
    if (cell.i > 0U) {
        output[2] = Cell{cell.face, cell.i - 1U, cell.j, 1U};
    } else {
        output[2] = cross_edge_host(cell, 1U, cell.j, n);
        if (seam_count) ++*seam_count;
    }
    if (macrochart_boundary_count) {
        for (std::uint32_t edge = 0U; edge < 3U; ++edge) {
            if ((output[edge].face >> 1U) != (cell.face >> 1U)) {
                ++*macrochart_boundary_count;
            }
        }
    }
}

}  // namespace bafe::detail

#undef BAFE_TOPO_HD
