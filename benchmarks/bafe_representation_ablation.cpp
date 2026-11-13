// Representation study on one CPU thread (revision 2026-11).
//
// Every variant runs the same serial ring construction with the same open
// addressing discovered set and returns the same public hierarchical indices.
// Only the way an adjacent cell is obtained, and therefore the key that is
// stored in the discovered set, differs:
//
//   compact       compact integer triangular index (f,i,j,o) in one 64 bit
//                 word; adjacency by arithmetic and the 3F atlas records;
//                 the public index is converted once at input and output
//                 (the representation of the paper)
//   hierarchical  the public hierarchical index is the key; every expansion
//                 decodes it, constructs the neighbors and encodes them again
//   bim_tuple     Barycentric Indexing Method style coordinates (a,b,c) with
//                 the face, stored as a two word key; one axis is decremented
//                 or incremented per move and a base mesh edge crossing
//                 rewrites the triple through the atlas record
//   bim_packed    the same (a,b,c) arithmetic with a one word key that stores
//                 two coordinates and the triangle type and reconstructs the
//                 third coordinate at every expansion
//   stored        a precomputed neighbor table with three 32 bit entries per
//                 dense cell number (only for levels where the table fits)
//
// The timer is a wall clock around the complete call: public seed indices in
// host memory to a new vector of public result indices.
#include "bafe/bafe.hpp"
#include "internal.hpp"
#include "topology.hpp"
#include "bafe_exp/bafe_workloads.hpp"
#include "bafe_exp/benchmark_common.hpp"
#include "bafe_exp/cli.hpp"
#include "bafe_exp/point_csv.hpp"
#include "bafe_exp/process_control.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
using bafe::CellIndex;
using bafe::detail::Cell;

inline std::uint64_t mix64(std::uint64_t value) noexcept {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

std::uint64_t next_power_of_two(std::uint64_t value) {
    std::uint64_t result = 1U;
    while (result < value) result <<= 1U;
    return result;
}

// Serial open addressing set with linear probing; same mixing function and
// load target (at most one half) as the flat table backends.
class FlatSet64 {
public:
    void reset(std::uint64_t capacity) {
        if (slots_.size() != capacity) slots_.assign(capacity, kEmpty);
        else std::fill(slots_.begin(), slots_.end(), kEmpty);
        mask_ = capacity - 1U;
    }
    bool insert(std::uint64_t key) noexcept {
        std::uint64_t position = mix64(key) & mask_;
        for (;;) {
            const std::uint64_t seen = slots_[position];
            if (seen == key) return false;
            if (seen == kEmpty) { slots_[position] = key; return true; }
            position = (position + 1U) & mask_;
        }
    }
    std::uint64_t bytes() const noexcept { return slots_.size() * sizeof(std::uint64_t); }
private:
    static constexpr std::uint64_t kEmpty = 0xffffffffffffffffULL;
    std::vector<std::uint64_t> slots_;
    std::uint64_t mask_ = 0U;
};

struct Key128 {
    std::uint64_t hi;  // face and a
    std::uint64_t lo;  // b and c
};
inline bool operator==(const Key128& x, const Key128& y) noexcept { return x.hi == y.hi && x.lo == y.lo; }

class FlatSet128 {
public:
    void reset(std::uint64_t capacity) {
        slots_.assign(capacity, Key128{kEmpty, kEmpty});
        mask_ = capacity - 1U;
    }
    bool insert(const Key128& key) noexcept {
        std::uint64_t position = mix64(key.hi ^ mix64(key.lo)) & mask_;
        for (;;) {
            const Key128 seen = slots_[position];
            if (seen == key) return false;
            if (seen.hi == kEmpty) { slots_[position] = key; return true; }
            position = (position + 1U) & mask_;
        }
    }
    std::uint64_t bytes() const noexcept { return slots_.size() * sizeof(Key128); }
private:
    static constexpr std::uint64_t kEmpty = 0xffffffffffffffffULL;
    std::vector<Key128> slots_;
    std::uint64_t mask_ = 0U;
};

struct Tables {
    std::array<bafe::detail::DecodeChunk, 512> decode = bafe::detail::make_decode_chunks();
    std::array<bafe::detail::EncodeChunk, 512> encode = bafe::detail::make_encode_chunks();
};

struct Run {
    std::vector<CellIndex> out;       // public indices of rings 1..K
    std::uint64_t references = 0U;    // three per expanded cell
    std::uint64_t set_bytes = 0U;
};

// ---------------------------------------------------------------- compact
Run run_compact(const std::vector<CellIndex>& input, std::uint32_t level, std::uint32_t hops,
                std::uint64_t capacity, const Tables& t, FlatSet64& set) {
    Run r;
    set.reset(capacity);
    std::vector<std::uint64_t> keys;
    keys.reserve(input.size());
    for (const CellIndex id : input) {
        const std::uint64_t key = bafe::detail::encode_coord(
            bafe::detail::path_to_cell_chunked(id, level, t.decode.data()));
        if (set.insert(key)) keys.push_back(key);
    }
    std::size_t begin = 0U;
    const std::size_t seeds = keys.size();
    for (std::uint32_t hop = 0; hop < hops; ++hop) {
        const std::size_t end = keys.size();
        r.references += 3ULL * (end - begin);
        for (std::size_t index = begin; index < end; ++index) {
            Cell local[3];
            bafe::detail::neighbors_host(bafe::detail::decode_coord(keys[index]), level, local);
            for (const Cell& neighbor : local) {
                const std::uint64_t candidate = bafe::detail::encode_coord(neighbor);
                if (set.insert(candidate)) keys.push_back(candidate);
            }
        }
        begin = end;
    }
    r.out.resize(keys.size() - seeds);
    for (std::size_t index = seeds; index < keys.size(); ++index) {
        r.out[index - seeds] = bafe::detail::cell_to_path_chunked(
            bafe::detail::decode_coord(keys[index]), level, t.encode.data());
    }
    r.set_bytes = set.bytes();
    return r;
}

// ----------------------------------------------------------- hierarchical
Run run_hierarchical(const std::vector<CellIndex>& input, std::uint32_t level, std::uint32_t hops,
                     std::uint64_t capacity, const Tables& t, FlatSet64& set) {
    Run r;
    set.reset(capacity);
    std::vector<std::uint64_t> keys;
    keys.reserve(input.size());
    for (const CellIndex id : input) {
        if (set.insert(id)) keys.push_back(id);
    }
    std::size_t begin = 0U;
    const std::size_t seeds = keys.size();
    for (std::uint32_t hop = 0; hop < hops; ++hop) {
        const std::size_t end = keys.size();
        r.references += 3ULL * (end - begin);
        for (std::size_t index = begin; index < end; ++index) {
            Cell local[3];
            bafe::detail::neighbors_host(
                bafe::detail::path_to_cell_chunked(keys[index], level, t.decode.data()), level, local);
            for (const Cell& neighbor : local) {
                const std::uint64_t candidate =
                    bafe::detail::cell_to_path_chunked(neighbor, level, t.encode.data());
                if (set.insert(candidate)) keys.push_back(candidate);
            }
        }
        begin = end;
    }
    r.out.assign(keys.begin() + static_cast<std::ptrdiff_t>(seeds), keys.end());
    r.set_bytes = set.bytes();
    return r;
}

// ------------------------------------------------------------------- BIM
// Coordinates (a,b,c) of a cell on one base mesh face with N = 2^level
// subdivisions: an upward cell has a+b+c = N-1 and a downward cell has
// a+b+c = N-2, where b and c are the two chart coordinates.  Crossing an edge
// inside the face decrements one axis of an upward cell or increments one axis
// of a downward cell.  When the axis of an upward cell is already zero the
// move crosses the base mesh edge opposite that axis.
struct Bim {
    std::uint32_t face, a, b, c;
};

inline Bim bim_from_cell(const Cell& cell, std::uint32_t n) noexcept {
    return Bim{cell.face, n - 1U - cell.orientation - cell.i - cell.j, cell.i, cell.j};
}
inline Cell cell_from_bim(const Bim& x, std::uint32_t n) noexcept {
    return Cell{x.face, x.b, x.c, (n - 1U) - (x.a + x.b + x.c)};
}
// Destination triple on the neighboring face: edge 0 is a = 0, edge 1 is
// b = 0, edge 2 is c = 0; t is the position along the destination edge.
inline Bim bim_boundary(std::uint32_t face, std::uint32_t edge, std::uint32_t t, std::uint32_t n) noexcept {
    if (edge == 0U) return Bim{face, 0U, n - 1U - t, t};
    if (edge == 1U) return Bim{face, n - 1U - t, 0U, t};
    return Bim{face, n - 1U - t, t, 0U};
}
inline Bim bim_cross(const Bim& x, std::uint32_t edge, std::uint32_t t, std::uint32_t n) noexcept {
    const bafe::detail::Transition transition = bafe::detail::kAtlas[x.face * 3U + edge];
    return bim_boundary(transition.dst_face, transition.dst_edge,
                        transition.reverse ? n - 1U - t : t, n);
}
inline void bim_neighbors(const Bim& x, std::uint32_t n, Bim out[3]) noexcept {
    if (x.a + x.b + x.c + 2U == n) {  // downward cell: increment one axis
        out[0] = Bim{x.face, x.a + 1U, x.b, x.c};
        out[1] = Bim{x.face, x.a, x.b + 1U, x.c};
        out[2] = Bim{x.face, x.a, x.b, x.c + 1U};
        return;
    }
    out[0] = x.a > 0U ? Bim{x.face, x.a - 1U, x.b, x.c} : bim_cross(x, 0U, x.c, n);
    out[1] = x.c > 0U ? Bim{x.face, x.a, x.b, x.c - 1U} : bim_cross(x, 2U, x.b, n);
    out[2] = x.b > 0U ? Bim{x.face, x.a, x.b - 1U, x.c} : bim_cross(x, 1U, x.c, n);
}

Run run_bim_tuple(const std::vector<CellIndex>& input, std::uint32_t level, std::uint32_t hops,
                  std::uint64_t capacity, const Tables& t, FlatSet128& set) {
    Run r;
    const std::uint32_t n = 1U << level;
    set.reset(capacity);
    std::vector<Key128> keys;
    keys.reserve(input.size());
    const auto pack = [](const Bim& x) noexcept {
        return Key128{(std::uint64_t(x.face) << 32U) | x.a, (std::uint64_t(x.b) << 32U) | x.c};
    };
    const auto unpack = [](const Key128& k) noexcept {
        return Bim{std::uint32_t(k.hi >> 32U), std::uint32_t(k.hi), std::uint32_t(k.lo >> 32U), std::uint32_t(k.lo)};
    };
    for (const CellIndex id : input) {
        const Key128 key = pack(bim_from_cell(
            bafe::detail::path_to_cell_chunked(id, level, t.decode.data()), n));
        if (set.insert(key)) keys.push_back(key);
    }
    std::size_t begin = 0U;
    const std::size_t seeds = keys.size();
    for (std::uint32_t hop = 0; hop < hops; ++hop) {
        const std::size_t end = keys.size();
        r.references += 3ULL * (end - begin);
        for (std::size_t index = begin; index < end; ++index) {
            Bim local[3];
            bim_neighbors(unpack(keys[index]), n, local);
            for (const Bim& neighbor : local) {
                const Key128 candidate = pack(neighbor);
                if (set.insert(candidate)) keys.push_back(candidate);
            }
        }
        begin = end;
    }
    r.out.resize(keys.size() - seeds);
    for (std::size_t index = seeds; index < keys.size(); ++index) {
        r.out[index - seeds] = bafe::detail::cell_to_path_chunked(
            cell_from_bim(unpack(keys[index]), n), level, t.encode.data());
    }
    r.set_bytes = set.bytes();
    return r;
}

Run run_bim_packed(const std::vector<CellIndex>& input, std::uint32_t level, std::uint32_t hops,
                   std::uint64_t capacity, const Tables& t, FlatSet64& set) {
    Run r;
    const std::uint32_t n = 1U << level;
    set.reset(capacity);
    std::vector<std::uint64_t> keys;
    keys.reserve(input.size());
    // One word: face, b, c and the triangle type; a is reconstructed.
    const auto pack = [n](const Bim& x) noexcept {
        const std::uint64_t type = (n - 1U) - (x.a + x.b + x.c);
        return (std::uint64_t(x.face) << 57U) | (std::uint64_t(x.b) << 29U) | (std::uint64_t(x.c) << 1U) | type;
    };
    const auto unpack = [n](std::uint64_t k) noexcept {
        const std::uint32_t b = std::uint32_t((k >> 29U) & bafe::detail::kCoordMask28);
        const std::uint32_t c = std::uint32_t((k >> 1U) & bafe::detail::kCoordMask28);
        const std::uint32_t type = std::uint32_t(k & 1ULL);
        return Bim{std::uint32_t((k >> 57U) & 0x3FULL), n - 1U - type - b - c, b, c};
    };
    for (const CellIndex id : input) {
        const std::uint64_t key = pack(bim_from_cell(
            bafe::detail::path_to_cell_chunked(id, level, t.decode.data()), n));
        if (set.insert(key)) keys.push_back(key);
    }
    std::size_t begin = 0U;
    const std::size_t seeds = keys.size();
    for (std::uint32_t hop = 0; hop < hops; ++hop) {
        const std::size_t end = keys.size();
        r.references += 3ULL * (end - begin);
        for (std::size_t index = begin; index < end; ++index) {
            Bim local[3];
            bim_neighbors(unpack(keys[index]), n, local);
            for (const Bim& neighbor : local) {
                const std::uint64_t candidate = pack(neighbor);
                if (set.insert(candidate)) keys.push_back(candidate);
            }
        }
        begin = end;
    }
    r.out.resize(keys.size() - seeds);
    for (std::size_t index = seeds; index < keys.size(); ++index) {
        r.out[index - seeds] = bafe::detail::cell_to_path_chunked(
            cell_from_bim(unpack(keys[index]), n), level, t.encode.data());
    }
    r.set_bytes = set.bytes();
    return r;
}

// ---------------------------------------------------------------- stored
// Dense cell number ((f*N + i)*N + j)*2 + o.  The table holds three neighbor
// numbers per dense number (entries of numbers that are not cells are unused).
struct NeighborTable {
    std::uint32_t level = 0U;
    std::uint32_t n = 0U;
    std::vector<std::uint32_t> next;
    double build_ms = 0.0;
    std::uint64_t cells = 0U;
};
inline std::uint64_t dense_of(const Cell& cell, std::uint32_t n) noexcept {
    return ((std::uint64_t(cell.face) * n + cell.i) * n + cell.j) * 2U + cell.orientation;
}
inline Cell cell_of_dense(std::uint64_t id, std::uint32_t n) noexcept {
    const std::uint32_t o = std::uint32_t(id & 1U);
    id >>= 1U;
    const std::uint32_t j = std::uint32_t(id % n);
    id /= n;
    return Cell{std::uint32_t(id / n), std::uint32_t(id % n), j, o};
}
NeighborTable build_table(std::uint32_t level) {
    NeighborTable table;
    table.level = level;
    table.n = 1U << level;
    const std::uint64_t ids = std::uint64_t(bafe::kRootFaceCount) * table.n * table.n * 2U;
    if (ids > 0xffffffffULL) throw std::invalid_argument("stored neighbor table supports levels up to 12");
    const auto start = Clock::now();
    table.next.assign(static_cast<std::size_t>(ids * 3U), 0xffffffffU);
    for (std::uint32_t face = 0; face < bafe::kRootFaceCount; ++face) {
        for (std::uint32_t i = 0; i < table.n; ++i) {
            for (std::uint32_t j = 0; i + j < table.n; ++j) {
                for (std::uint32_t o = 0; o < 2U; ++o) {
                    const Cell cell{face, i, j, o};
                    if (!bafe::detail::valid_cell(cell, level)) continue;
                    Cell local[3];
                    bafe::detail::neighbors_host(cell, level, local);
                    const std::uint64_t id = dense_of(cell, table.n);
                    for (std::uint32_t e = 0; e < 3U; ++e) {
                        table.next[static_cast<std::size_t>(id * 3U + e)] =
                            static_cast<std::uint32_t>(dense_of(local[e], table.n));
                    }
                    ++table.cells;
                }
            }
        }
    }
    table.build_ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    return table;
}

Run run_stored(const std::vector<CellIndex>& input, std::uint32_t level, std::uint32_t hops,
               std::uint64_t capacity, const Tables& t, FlatSet64& set, const NeighborTable& table) {
    Run r;
    set.reset(capacity);
    std::vector<std::uint64_t> keys;
    keys.reserve(input.size());
    for (const CellIndex id : input) {
        const std::uint64_t key = dense_of(
            bafe::detail::path_to_cell_chunked(id, level, t.decode.data()), table.n);
        if (set.insert(key)) keys.push_back(key);
    }
    std::size_t begin = 0U;
    const std::size_t seeds = keys.size();
    const std::uint32_t* next = table.next.data();
    for (std::uint32_t hop = 0; hop < hops; ++hop) {
        const std::size_t end = keys.size();
        r.references += 3ULL * (end - begin);
        for (std::size_t index = begin; index < end; ++index) {
            const std::uint32_t* row = next + keys[index] * 3U;
            for (std::uint32_t e = 0; e < 3U; ++e) {
                const std::uint64_t candidate = row[e];
                if (set.insert(candidate)) keys.push_back(candidate);
            }
        }
        begin = end;
    }
    r.out.resize(keys.size() - seeds);
    for (std::size_t index = seeds; index < keys.size(); ++index) {
        r.out[index - seeds] = bafe::detail::cell_to_path_chunked(
            cell_of_dense(keys[index], table.n), level, t.encode.data());
    }
    r.set_bytes = set.bytes();
    return r;
}

template <class F>
std::pair<std::vector<double>, Run> measure(std::uint32_t warmup, std::uint32_t repeats, F fn) {
    for (std::uint32_t i = 0; i < warmup; ++i) (void)fn();
    std::vector<double> times;
    Run last;
    for (std::uint32_t i = 0; i < repeats; ++i) {
        const auto start = Clock::now();
        Run current = fn();
        times.push_back(std::chrono::duration<double, std::milli>(Clock::now() - start).count());
        last = std::move(current);
    }
    return {times, std::move(last)};
}

std::vector<CellIndex> cells_from_points(const std::string& path, std::uint32_t level, std::uint64_t maximum) {
    const auto points = bafe_exp::read_lonlat_csv(path);
    const std::size_t n = maximum == 0 ? points.size() : std::min<std::size_t>(points.size(), maximum);
    std::vector<CellIndex> cells;
    cells.reserve(n);
    for (std::size_t sample = 0; sample < n; ++sample) {
        const std::size_t index = n == points.size() ? sample : (sample * points.size()) / n;
        cells.push_back(bafe::geo::lonlat_to_cell(points[index].first, points[index].second, level));
    }
    std::sort(cells.begin(), cells.end());
    cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
    return cells;
}
}  // namespace

int main(int argc, char** argv) {
    try {
        const std::string process_control = bafe_exp::stabilize_process();
        std::cout << process_control << "\n";
        bafe_exp::Cli cli(argc, argv);
        const auto level = cli.u32("level", 28);
        const auto hops = cli.u32("hops", 4);
        const auto count = cli.u64("cells", 100000);
        const auto warmup = cli.u32("warmup", 5);
        const auto repeats = cli.u32("repeats", 30);
        const auto pattern = cli.get("pattern", "compact");
        const auto points = cli.get("points", "");
        const auto table_mode = cli.get("table", "tight");   // tight: twice the realized keys; auto: output budget
        const auto csv_path = cli.get("csv", "results/representation_ablation.csv");
        // --only-compact measures the representation of the paper alone (used
        // for the context comparison with the H3 grid disk).
        const bool only_compact = cli.flag("only-compact");
        const bool with_stored = !only_compact && (cli.flag("stored") || level <= 10U);

        std::vector<CellIndex> input = !points.empty()
            ? cells_from_points(points, level, count)
            : (pattern == "scattered" ? bafe_exp::bafe_scattered(level, count)
                                      : bafe_exp::bafe_compact(level, count, pattern == "seam"));
        std::sort(input.begin(), input.end());

        const Tables tables;
        FlatSet64 set64;
        FlatSet128 set128;
        // Discovered set capacity, identical for every variant.
        std::uint64_t capacity = 0U;
        {
            const std::uint64_t budget = std::uint64_t(input.size()) * (3ULL * hops * (hops + 1ULL) / 2ULL);
            if (table_mode == "auto") {
                capacity = next_power_of_two(std::max<std::uint64_t>(1024U, 2U * (input.size() + budget)));
            } else if (table_mode == "tight") {
                const Run probe = run_compact(
                    input, level, hops,
                    next_power_of_two(std::max<std::uint64_t>(1024U, 2U * (input.size() + budget))), tables, set64);
                capacity = next_power_of_two(std::max<std::uint64_t>(1024U, 2U * (input.size() + probe.out.size())));
            } else {
                throw std::invalid_argument("--table must be tight or auto");
            }
        }

        struct Named { std::string name; std::vector<double> times; Run run; std::uint64_t static_bytes; std::string note; };
        std::vector<Named> results;
        const std::uint64_t atlas_bytes = sizeof(bafe::detail::kAtlas);
        {
            auto m = measure(warmup, repeats, [&]() { return run_compact(input, level, hops, capacity, tables, set64); });
            results.push_back({"compact", m.first, std::move(m.second), atlas_bytes, "compact integer triangular index; 3F atlas records"});
        }
        if (!only_compact) {
            auto m = measure(warmup, repeats, [&]() { return run_hierarchical(input, level, hops, capacity, tables, set64); });
            results.push_back({"hierarchical", m.first, std::move(m.second), atlas_bytes, "public hierarchical index converted at every expansion (table driven conversion)"});
        }
        if (!only_compact) {
            auto m = measure(warmup, repeats, [&]() { return run_bim_tuple(input, level, hops, capacity, tables, set128); });
            results.push_back({"bim_tuple", m.first, std::move(m.second), atlas_bytes, "BIM style (a,b,c) coordinates; two word key"});
        }
        if (!only_compact) {
            auto m = measure(warmup, repeats, [&]() { return run_bim_packed(input, level, hops, capacity, tables, set64); });
            results.push_back({"bim_packed", m.first, std::move(m.second), atlas_bytes, "BIM style (a,b,c) arithmetic; one word key with two coordinates and the type"});
        }
        if (with_stored) {
            const NeighborTable table = build_table(level);
            auto m = measure(warmup, repeats, [&]() { return run_stored(input, level, hops, capacity, tables, set64, table); });
            results.push_back({"stored", m.first, std::move(m.second),
                               std::uint64_t(table.next.size()) * sizeof(std::uint32_t),
                               "precomputed neighbor table; cells=" + std::to_string(table.cells) +
                               "; build_ms=" + std::to_string(table.build_ms)});
        }

        const std::uint64_t reference_hash = bafe_exp::fnv1a64(results.front().run.out);
        std::ofstream csv(csv_path);
        if (!csv) throw std::runtime_error("cannot open " + csv_path);
        bafe_exp::write_csv_header(csv);
        for (const auto& item : results) {
            const std::uint64_t hash = bafe_exp::fnv1a64(item.run.out);
            if (hash != reference_hash || item.run.out.size() != results.front().run.out.size()) {
                throw std::runtime_error("representation variant " + item.name + " returned a different cell set");
            }
            bafe_exp::ResultRow row;
            row.experiment_id = "representation_ablation";
            row.backend = "cpu_serial_flat_" + item.name;
            row.library_version = bafe::kLibraryVersion;
            row.compiler = bafe_exp::compiler_string();
            row.pattern = points.empty() ? pattern : "point_samples";
            row.scenario = points.empty() ? pattern : "calgary_points";
            row.level_or_resolution = level;
            row.input_cells = row.unique_input_cells = input.size();
            row.hops = hops;
            row.output_cells = item.run.out.size();
            row.candidate_references = item.run.references;
            row.peak_memory_bytes = item.static_bytes;   // static adjacency storage
            row.timing = bafe_exp::summarize(item.times);
            row.result_hash = hash;
            row.correctness_status = "PASS";
            row.notes = item.note + "; table=" + table_mode + "; set_bytes=" + std::to_string(item.run.set_bytes) + "; " + process_control;
            row.variant = item.name;
            row.threads = 1U;
            row.timer = "wall";
            bafe_exp::write_csv_row(csv, row);
            std::cout << item.name << " level=" << level << " input=" << input.size() << " hops=" << hops
                      << " output=" << row.output_cells << " median_ms=" << row.timing.median_ms
                      << " mean_ms=" << row.timing.mean_ms
                      << " ratio_to_compact=" << row.timing.median_ms / bafe_exp::percentile(results.front().times, 0.5)
                      << " adjacency_bytes=" << item.static_bytes << " status=PASS\n";
        }
        std::cout << "CSV: " << csv_path << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "bafe_representation_ablation: " << error.what() << '\n';
        return 1;
    }
}
