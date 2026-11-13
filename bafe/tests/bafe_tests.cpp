#include "bafe/bafe.hpp"
#include "internal.hpp"
#include "topology.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <stdexcept>
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace {

// Frozen oriented root faces from the canonical Pentakis mesh manifest.  This
// is intentionally independent of the transition table under test.
constexpr std::uint8_t kRootFaces[60][3] = {
    {0,20,19},{1,19,20},{0,12,20},{2,20,12},{0,19,13},{3,13,19},
    {0,15,12},{4,12,15},{0,13,15},{6,15,13},{1,20,21},{2,21,20},
    {1,23,19},{3,19,23},{1,21,22},{5,22,21},{1,22,23},{7,23,22},
    {2,12,18},{4,18,12},{2,16,21},{5,21,16},{2,18,16},{8,16,18},
    {3,25,13},{6,13,25},{3,23,24},{7,24,23},{3,24,25},{9,25,24},
    {4,15,14},{6,14,15},{4,17,18},{8,18,17},{4,14,17},{10,17,14},
    {5,28,22},{7,22,28},{5,16,29},{8,29,16},{5,29,28},{11,28,29},
    {6,25,26},{9,26,25},{6,26,14},{10,14,26},{7,30,24},{9,24,30},
    {7,28,30},{11,30,28},{8,17,27},{10,27,17},{8,27,29},{11,29,27},
    {9,31,26},{10,26,31},{9,30,31},{11,31,30},{10,31,27},{11,27,31}};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<bafe::CellIndex> sorted(std::vector<bafe::CellIndex> values) {
    std::sort(values.begin(), values.end());
    return values;
}

void enumerate_level(
    bafe::CellIndex prefix,
    std::uint32_t current,
    std::uint32_t target,
    std::vector<bafe::CellIndex>& output) {
    if (current == target) {
        output.push_back(prefix);
        return;
    }
    for (std::uint32_t digit = 0; digit < 4; ++digit) {
        enumerate_level(bafe::child(prefix, current, digit), current + 1U, target, output);
    }
}

void test_prefix_index() {
    const auto decode_chunks = bafe::detail::make_decode_chunks();
    const auto encode_chunks = bafe::detail::make_encode_chunks();
    for (std::uint32_t face = 0; face < bafe::kRootFaceCount; ++face) {
        const auto root = bafe::root(face);
        require(bafe::level(root) == 0U, "root level decode failed");
        require(bafe::face(root) == face, "root face decode failed");
        const auto span = bafe::descendants(root, 0U, 6U);
        require(span.count == 4096U && span.stride > 0U,
                "incorrect descendant span metadata");
        std::vector<bafe::CellIndex> cells;
        enumerate_level(root, 0U, 6U, cells);
        require(cells.size() == 4096U, "incorrect level-6 descendant count");
        for (const auto id : cells) {
            require(bafe::is_canonical(id, 6U), "non-canonical generated index");
            require(bafe::ancestor(id, 6U, 0U) == root, "O(1) root ancestor failed");
            auto cursor = id;
            for (std::uint32_t r = 6U; r > 0U; --r) cursor = bafe::parent(cursor, r);
            require(cursor == root, "prefix parent chain failed");
            const auto cell = bafe::detail::path_to_cell_scalar(id, 6U);
            require(bafe::detail::valid_cell(cell, 6U), "path decoded to invalid traversal cell");
            require(bafe::detail::cell_to_path_scalar(cell, 6U) == id,
                    "path/traversal round trip failed");
            require(bafe::detail::path_to_cell_chunked(
                        id, 6U, decode_chunks.data()) == cell,
                    "chunked path decode disagrees with scalar reference");
            require(bafe::detail::cell_to_path_chunked(
                        cell, 6U, encode_chunks.data()) == id,
                    "chunked path encode disagrees with scalar reference");
            const auto fixed = bafe::export_fixed_level_id(id, 6U);
            require(bafe::import_fixed_level_id(fixed, 6U) == id,
                    "legacy fixed-level ID migration failed");
            require(bafe::import_legacy_id(bafe::export_legacy_id(id, 6U)) == id,
                    "original legacy ID migration failed");
            require(id >= span.first && id <= span.last &&
                    (id - span.first) % span.stride == 0U,
                    "descendant is outside its canonical span");
        }
    }

    std::mt19937_64 random(0xBAFE2026ULL);
    for (std::uint32_t level = 0; level <= bafe::kMaxLevel; ++level) {
        for (int sample = 0; sample < 1000; ++sample) {
            auto id = bafe::root(static_cast<std::uint32_t>(random() % 60U));
            for (std::uint32_t r = 0; r < level; ++r) {
                id = bafe::child(id, r, static_cast<std::uint32_t>(random() & 3U));
            }
            require(bafe::level(id) == level, "random level decode failed");
            const auto traversal = bafe::detail::path_to_cell_scalar(id, level);
            require(bafe::detail::cell_to_path_scalar(traversal, level) == id,
                    "deep path/traversal round trip failed");
            require(bafe::detail::path_to_cell_chunked(
                        id, level, decode_chunks.data()) == traversal,
                    "deep chunked path decode failed");
            require(bafe::detail::cell_to_path_chunked(
                        traversal, level, encode_chunks.data()) == id,
                    "deep chunked path encode failed");
            require(bafe::import_fixed_level_id(
                        bafe::export_fixed_level_id(id, level), level) == id,
                    "deep legacy ID migration failed");
            if (level <= 26U) {
                require(bafe::import_legacy_id(bafe::export_legacy_id(id, level)) == id,
                        "deep original legacy ID migration failed");
            }
            const std::uint32_t target = level == 0U ? 0U :
                static_cast<std::uint32_t>(random() % (level + 1U));
            require(bafe::level(bafe::ancestor(id, level, target)) == target,
                    "random ancestor level failed");
        }
    }
}

void test_atlas_reciprocity() {
    auto directed_edge = [](std::uint32_t face, std::uint32_t edge) {
        const auto* vertices = kRootFaces[face];
        if (edge == 0U) return std::pair<std::uint8_t, std::uint8_t>{vertices[1], vertices[2]};
        if (edge == 1U) return std::pair<std::uint8_t, std::uint8_t>{vertices[0], vertices[2]};
        return std::pair<std::uint8_t, std::uint8_t>{vertices[0], vertices[1]};
    };

    // Generate every expected transition from the oriented coarse half-edge
    // manifest instead of trusting the hard-coded atlas to validate itself.
    for (std::uint32_t source = 0; source < 60U; ++source) {
        for (std::uint32_t edge = 0; edge < 3U; ++edge) {
            const auto endpoints = directed_edge(source, edge);
            std::uint32_t destination = 60U, destination_edge = 3U, reverse = 0U;
            for (std::uint32_t candidate = 0; candidate < 60U; ++candidate) {
                if (candidate == source) continue;
                for (std::uint32_t local_edge = 0; local_edge < 3U; ++local_edge) {
                    const auto other = directed_edge(candidate, local_edge);
                    if ((endpoints.first == other.first && endpoints.second == other.second) ||
                        (endpoints.first == other.second && endpoints.second == other.first)) {
                        destination = candidate;
                        destination_edge = local_edge;
                        reverse = endpoints.first == other.first ? 0U : 1U;
                    }
                }
            }
            require(destination < 60U, "coarse half-edge has no mate");
            const auto transition = bafe::detail::kAtlas[source * 3U + edge];
            require(transition.dst_face == destination &&
                    transition.dst_edge == destination_edge &&
                    transition.reverse == reverse,
                    "atlas disagrees with independent coarse half-edge oracle");
        }
    }

    for (std::uint32_t level = 0; level <= 5U; ++level) {
        std::vector<bafe::CellIndex> paths;
        for (std::uint32_t face = 0; face < 60U; ++face) {
            enumerate_level(bafe::root(face), 0U, level, paths);
        }
        for (const auto path : paths) {
            const auto cell = bafe::detail::path_to_cell_scalar(path, level);
            bafe::detail::Cell neighbors[3];
            bafe::detail::neighbors_host(cell, level, neighbors);
            bafe::detail::Cell rhombic_neighbors[3];
            bafe::detail::neighbors_rhombic_host(
                cell, level, rhombic_neighbors);
            require(neighbors[0] == rhombic_neighbors[0] &&
                    neighbors[1] == rhombic_neighbors[1] &&
                    neighbors[2] == rhombic_neighbors[2],
                    "rhombic macrochart changed an implicit neighbor");
            for (const auto& neighbor : neighbors) {
                require(bafe::detail::valid_cell(neighbor, level), "invalid implicit neighbor");
                bafe::detail::Cell reverse[3];
                bafe::detail::neighbors_host(neighbor, level, reverse);
                require(reverse[0] == cell || reverse[1] == cell || reverse[2] == cell,
                        "non-reciprocal atlas neighbor");
            }
        }
    }
}

struct ExplicitEdge {
    std::uint64_t a;
    std::uint64_t b;
    bool operator==(const ExplicitEdge& other) const noexcept {
        return a == other.a && b == other.b;
    }
};

struct ExplicitEdgeHash {
    std::size_t operator()(const ExplicitEdge& edge) const noexcept {
        std::uint64_t x = edge.a + 0x9e3779b97f4a7c15ULL;
        x ^= edge.b + 0x9e3779b97f4a7c15ULL + (x << 6U) + (x >> 2U);
        return static_cast<std::size_t>(x ^ (x >> 32U));
    }
};

std::uint64_t explicit_vertex_key(
    std::uint32_t face,
    std::uint32_t a,
    std::uint32_t b,
    std::uint32_t c,
    std::uint32_t n) {
    const auto* vertices = kRootFaces[face];
    if (a == n) return vertices[0];
    if (b == n) return vertices[1];
    if (c == n) return vertices[2];

    auto edge_key = [](std::uint32_t p, std::uint32_t q,
                       std::uint32_t weight_p, std::uint32_t weight_q) {
        const std::uint32_t low = std::min(p, q);
        const std::uint32_t high = std::max(p, q);
        const std::uint32_t weight_high = p == high ? weight_p : weight_q;
        return (1ULL << 62U) | (std::uint64_t(low) << 51U) |
               (std::uint64_t(high) << 40U) | weight_high;
    };
    if (c == 0U) return edge_key(vertices[0], vertices[1], a, b);
    if (b == 0U) return edge_key(vertices[0], vertices[2], a, c);
    if (a == 0U) return edge_key(vertices[1], vertices[2], b, c);

    return (2ULL << 62U) | (std::uint64_t(face) << 56U) |
           (std::uint64_t(a) << 28U) | b;
}

void test_independent_refined_edge_oracle() {
    constexpr std::uint32_t level = 6U;
    constexpr std::uint32_t n = 1U << level;
    struct Incident { bafe::CellIndex first = 0U; std::uint32_t count = 0U; };
    std::unordered_map<ExplicitEdge, Incident, ExplicitEdgeHash> edges;
    edges.reserve(800'000U);

    auto implicit_contains = [=](bafe::CellIndex source, bafe::CellIndex target) {
        const auto cell = bafe::detail::path_to_cell_scalar(source, level);
        bafe::detail::Cell neighbors[3];
        bafe::detail::neighbors_host(cell, level, neighbors);
        for (const auto& neighbor : neighbors) {
            if (bafe::detail::cell_to_path_scalar(neighbor, level) == target) return true;
        }
        return false;
    };

    for (std::uint32_t face = 0; face < 60U; ++face) {
        std::vector<bafe::CellIndex> paths;
        enumerate_level(bafe::root(face), 0U, level, paths);
        for (const auto path : paths) {
            const auto cell = bafe::detail::path_to_cell_scalar(path, level);
            std::uint32_t x[3], y[3];
            if (cell.orientation == 0U) {
                x[0] = cell.i;      y[0] = cell.j;
                x[1] = cell.i + 1;  y[1] = cell.j;
                x[2] = cell.i;      y[2] = cell.j + 1;
            } else {
                x[0] = cell.i + 1;  y[0] = cell.j;
                x[1] = cell.i;      y[1] = cell.j + 1;
                x[2] = cell.i + 1;  y[2] = cell.j + 1;
            }
            std::uint64_t vertices[3];
            for (std::uint32_t vertex = 0; vertex < 3U; ++vertex) {
                vertices[vertex] = explicit_vertex_key(
                    face, n - x[vertex] - y[vertex], x[vertex], y[vertex], n);
            }
            for (std::uint32_t edge = 0; edge < 3U; ++edge) {
                const std::uint64_t u = vertices[edge];
                const std::uint64_t v = vertices[(edge + 1U) % 3U];
                const ExplicitEdge key{std::min(u, v), std::max(u, v)};
                auto& incident = edges[key];
                if (incident.count == 0U) {
                    incident.first = path;
                    incident.count = 1U;
                } else if (incident.count == 1U) {
                    require(implicit_contains(incident.first, path) &&
                            implicit_contains(path, incident.first),
                            "implicit neighbor disagrees with explicit refined edge oracle");
                    incident.count = 2U;
                } else {
                    throw std::runtime_error("refined edge has more than two incident cells");
                }
            }
        }
    }
    for (const auto& entry : edges) {
        require(entry.second.count == 2U, "refined edge is not closed/manifold");
    }
    require(edges.size() == 90U * (1U << (2U * level)),
            "unexpected refined Pentakis edge count");
}

void compare_cpu_gpu_case(
    const std::vector<bafe::CellIndex>& feature,
    std::uint32_t level,
    std::uint32_t hops,
    bafe::CudaSchedule schedule = bafe::CudaSchedule::Auto) {
    bafe::QueryOptions options;
    options.level = level;
    options.hops = hops;
    options.deterministic_layers = true;
    options.max_output_cells = 2'000'000U;
    options.cuda_schedule = schedule;

    auto cpu = bafe::make_cpu_engine();
    auto parallel = bafe::make_parallel_cpu_engine();
    auto gpu = bafe::make_cuda_engine();
    const auto expected = cpu->halo(feature, options);
    const auto multicore = parallel->halo(feature, options);
    const auto actual = gpu->halo(feature, options);
    require(expected.layer_offsets == multicore.layer_offsets,
            "CPU/parallel-CPU layer sizes differ");
    for (std::size_t layer = 0; layer + 1U < expected.layer_offsets.size(); ++layer) {
        std::vector<bafe::CellIndex> expected_layer(
            expected.cells.begin() + static_cast<std::ptrdiff_t>(expected.layer_offsets[layer]),
            expected.cells.begin() + static_cast<std::ptrdiff_t>(expected.layer_offsets[layer + 1U]));
        std::vector<bafe::CellIndex> parallel_layer(
            multicore.cells.begin() + static_cast<std::ptrdiff_t>(multicore.layer_offsets[layer]),
            multicore.cells.begin() + static_cast<std::ptrdiff_t>(multicore.layer_offsets[layer + 1U]));
        require(sorted(std::move(expected_layer)) == sorted(std::move(parallel_layer)),
                "CPU/parallel-CPU exact layer sets differ");
    }
    require(expected.layer_offsets == actual.layer_offsets, "CPU/GPU layer sizes differ");
    for (std::size_t layer = 0; layer + 1U < expected.layer_offsets.size(); ++layer) {
        std::vector<bafe::CellIndex> expected_layer(
            expected.cells.begin() + static_cast<std::ptrdiff_t>(expected.layer_offsets[layer]),
            expected.cells.begin() + static_cast<std::ptrdiff_t>(expected.layer_offsets[layer + 1U]));
        std::vector<bafe::CellIndex> actual_layer(
            actual.cells.begin() + static_cast<std::ptrdiff_t>(actual.layer_offsets[layer]),
            actual.cells.begin() + static_cast<std::ptrdiff_t>(actual.layer_offsets[layer + 1U]));
        require(sorted(std::move(expected_layer)) == sorted(std::move(actual_layer)),
                "CPU/GPU exact layer sets differ");
    }
}

void compare_triangle_and_rhombic_modes(
    const std::vector<bafe::CellIndex>& feature,
    std::uint32_t level,
    std::uint32_t hops) {
    bafe::QueryOptions triangles;
    triangles.level = level;
    triangles.hops = hops;
    triangles.max_output_cells = 2'000'000U;
    triangles.deterministic_layers = true;
    bafe::QueryOptions rhombic = triangles;
    rhombic.chart_mode = bafe::ChartMode::RhombicMacrocharts;

    auto cpu = bafe::make_cpu_engine();
    auto gpu = bafe::make_cuda_engine();
    const auto cpu_triangles = cpu->halo(feature, triangles);
    const auto cpu_rhombic = cpu->halo(feature, rhombic);
    const auto gpu_triangles = gpu->halo(feature, triangles);
    const auto gpu_rhombic = gpu->halo(feature, rhombic);
    require(cpu_triangles.layer_offsets == cpu_rhombic.layer_offsets &&
            cpu_triangles.cells == cpu_rhombic.cells,
            "CPU rhombic macrocharts changed triangle halo semantics");
    require(gpu_triangles.layer_offsets == gpu_rhombic.layer_offsets &&
            gpu_triangles.cells == gpu_rhombic.cells,
            "CUDA rhombic macrocharts changed triangle halo semantics");
    require(cpu_triangles.cells == gpu_rhombic.cells,
            "CUDA rhombic macrochart result disagrees with CPU oracle");
    require(gpu_rhombic.metrics.paired_edge_direct_transforms > 0U,
            "rhombic seam test did not exercise a paired direct transform");
}

void test_edge_cases() {
    auto cpu = bafe::make_cpu_engine();
    auto gpu = bafe::make_cuda_engine();
    auto id = bafe::root(3U);
    for (std::uint32_t depth = 0; depth < 28U; ++depth) {
        id = bafe::child(id, depth, (depth + 2U) & 3U);
    }

    bafe::QueryOptions zero;
    zero.level = 28U;
    zero.hops = 0U;
    const std::vector<bafe::CellIndex> duplicates{id, id};
    const auto cpu_zero = cpu->halo(duplicates, zero);
    const auto gpu_zero = gpu->halo(duplicates, zero);
    require(cpu_zero.cells.empty() && gpu_zero.cells.empty(), "zero-hop result is not empty");
    require(cpu_zero.layer_offsets == gpu_zero.layer_offsets, "zero-hop offsets differ");
    require(cpu_zero.metrics.unique_input_cells == 1U &&
            gpu_zero.metrics.unique_input_cells == 1U,
            "zero-hop input deduplication differs");

    bool rejected = false;
    try {
        const bafe::CellIndex wrong_level = bafe::root(3U);
        (void)gpu->halo(std::vector<bafe::CellIndex>{wrong_level}, zero);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "CUDA accepted a mixed-level zero-hop ID");

    bafe::QueryOptions bounded;
    bounded.level = 28U;
    bounded.hops = 4U;
    bounded.max_output_cells = 29U;  // One isolated source has 30 cells through hop 4.
    bool overflowed = false;
    try {
        (void)gpu->halo(std::vector<bafe::CellIndex>{id}, bounded);
    } catch (const std::length_error&) {
        overflowed = true;
    }
    require(overflowed, "CUDA output-capacity overflow was not reported");

    bounded.max_output_cells = 30U;
    const auto recovered = gpu->halo(std::vector<bafe::CellIndex>{id}, bounded);
    require(recovered.cells.size() == 30U,
            "CUDA engine did not recover after a bounded-output failure");
}

void test_gpu_halo() {
#if BAFE_HAS_CUDA
    for (std::uint32_t resolution = 0U; resolution <= bafe::kMaxLevel; ++resolution) {
        auto resolution_seed = bafe::root((resolution * 7U + 3U) % 60U);
        for (std::uint32_t depth = 0; depth < resolution; ++depth) {
            resolution_seed = bafe::child(
                resolution_seed, depth, (resolution + depth * 3U) & 3U);
        }
        compare_cpu_gpu_case({resolution_seed}, resolution, 3U);
    }

    // Repeated digit zero is U(0,0), so this deliberately touches two root edges.
    auto seam_seed = bafe::root(0U);
    for (std::uint32_t r = 0; r < 28U; ++r) seam_seed = bafe::child(seam_seed, r, 0U);
    compare_cpu_gpu_case({seam_seed}, 28U, 12U);
    compare_cpu_gpu_case(
        {seam_seed}, 28U, 12U, bafe::CudaSchedule::HostDriven);
    const auto paired_edge_seed = bafe::detail::cell_to_path_scalar(
        bafe::detail::Cell{0U, (1U << 28U) - 1U, 0U, 0U}, 28U);
    compare_triangle_and_rhombic_modes({paired_edge_seed}, 28U, 12U);

    auto interior_seed = bafe::root(17U);
    for (std::uint32_t r = 0; r < 28U; ++r) {
        interior_seed = bafe::child(interior_seed, r, (r * 3U + 1U) & 3U);
    }
    auto cpu = bafe::make_cpu_engine();
    bafe::QueryOptions disk_options;
    disk_options.level = 28U;
    disk_options.hops = 20U;
    disk_options.max_output_cells = 100'000U;
    auto disk = cpu->halo(std::vector<bafe::CellIndex>{interior_seed}, disk_options);
    std::vector<bafe::CellIndex> feature{interior_seed};
    feature.insert(feature.end(), disk.cells.begin(), disk.cells.end());
    compare_cpu_gpu_case(feature, 28U, 4U);

    std::mt19937_64 random(1234567U);
    std::vector<bafe::CellIndex> scattered;
    for (int sample = 0; sample < 10'000; ++sample) {
        auto id = bafe::root(static_cast<std::uint32_t>(random() % 60U));
        for (std::uint32_t r = 0; r < 18U; ++r) {
            id = bafe::child(id, r, static_cast<std::uint32_t>(random() & 3U));
        }
        scattered.push_back(id);
    }
    compare_cpu_gpu_case(scattered, 18U, 2U);
    test_edge_cases();
#else
    std::cout << "SKIP CUDA halo tests (CPU-only build)\n";
#endif
}

}  // namespace

int main() {
    try {
        test_prefix_index();
        std::cout << "PASS prefix hierarchy and conversions\n";
        test_atlas_reciprocity();
        std::cout << "PASS independent coarse atlas oracle and exhaustive reciprocity through level 5\n";
        test_independent_refined_edge_oracle();
        std::cout << "PASS independent explicit refined-edge oracle at level 6\n";
        test_gpu_halo();
#if BAFE_HAS_CUDA
        std::cout << "PASS CPU/GPU exact halo equality including level-28 seams "
                     "and rhombic macrocharts\n";
#else
        std::cout << "PASS CPU-only halo validation\n";
#endif
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
