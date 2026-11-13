// Numerical settings study of the WGS84 buffer queries (revision 2026-11-13,
// second pass, Reviewer 2 comment 3).
//
// The published buffer program fixes a 10 micrometre allowance inside
// coverage.hpp (it enters the cell radius, the radius of every source
// hierarchy node, the radius of every source arc and the node lower bound)
// and takes the cell classification tolerance from --tolerance-m (0.1 mm).
// This program repeats the complete published pipeline (source cover, halo
// replay at depths 1,2,4,..., whole ring stopping test, footprint
// classification of every visited cell) with the CPU halo backend for every
// combination of allowance and classification tolerance, saves the source
// cover and the returned cells of every run, and compares every run with the
// reference setting (allowance 10 micrometres, tolerance 0.1 mm).
//
// coverage_settings.hpp is a generated copy of the unchanged file
// Scalable_Atlas_Ellipsoidal_Buffer/src/coverage.hpp in which the literal
// 1e-5 is replaced by the variable coverage::g_allowance
// (see build_buffer_settings.sh).  The original file is not modified.
//
// The study measures the sensitivity of the returned sets to the two
// settings.  It does not certify the floating point arithmetic.
#include "coverage_settings.hpp"
#include "bafe/bafe.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#ifdef _OPENMP
#include <omp.h>
#endif
using Id = bafe::CellIndex;
template <class Fn> void parallel(std::size_t n, Fn fn) {
    std::exception_ptr error;
    #pragma omp parallel for schedule(dynamic, 8)
    for (std::int64_t i = 0; i < static_cast<std::int64_t>(n); ++i) {
        try { fn(std::size_t(i)); } catch (...) {
            #pragma omp critical
            { if (!error) error = std::current_exception(); }
        }
    }
    if (error) std::rethrow_exception(error);
}
struct Query {
    bool ok = false; std::string error;
    std::vector<Id> seeds, visited, accepted;
    std::vector<coverage::CellResult> classified;
    unsigned hops = 0; double stopping_lower = 0; std::size_t last_ring = 0;
    std::size_t outside = 0, confirmed = 0, unresolved = 0;
    double min_out = INFINITY, min_in = INFINITY;
    Id min_out_cell = 0, min_in_cell = 0; double min_out_lower = 0, min_in_upper = 0;
};
Query run(const std::vector<ellipsoid::Segment>& segments, unsigned level, double distance,
          double allowance, double tolerance, std::uint64_t capacity, unsigned max_hops) {
    Query q;
    try {
        coverage::g_allowance = allowance;
        coverage::Feature source(segments);            // node and arc radii contain the allowance
        q.seeds = coverage::cover(source, level, 0, capacity);
        if (q.seeds.empty()) throw std::runtime_error("source cover empty");
        auto engine = bafe::make_cpu_engine();
        bafe::HaloResult halo;
        unsigned hops = 1;
        for (;;) {
            bafe::QueryOptions options; options.level = level; options.hops = hops; options.max_output_cells = capacity;
            halo = engine->halo(q.seeds, options);
            auto start = halo.layer_offsets[hops - 1], end = halo.layer_offsets[hops];
            std::vector<double> bounds(end - start);
            parallel(bounds.size(), [&](std::size_t i) {
                bounds[i] = source.bounds(coverage::patch(halo.cells[start + i], level)).lower; });
            q.stopping_lower = bounds.empty() ? INFINITY : *std::min_element(bounds.begin(), bounds.end());
            q.last_ring = bounds.size();
            if (q.stopping_lower > distance) break;
            if (hops >= max_hops) throw std::runtime_error("maximum hops reached without a separating ring");
            hops = std::min(max_hops, hops * 2);
        }
        q.hops = hops;
        q.visited = q.seeds;
        q.visited.insert(q.visited.end(), halo.cells.begin(), halo.cells.end());
        q.classified.resize(q.visited.size());
        parallel(q.visited.size(), [&](std::size_t i) {
            q.classified[i] = coverage::classify(source, coverage::patch(q.visited[i], level), distance, tolerance); });
        for (std::size_t i = 0; i < q.visited.size(); ++i) {
            const auto& c = q.classified[i];
            if (c.status == coverage::Status::outside) {
                ++q.outside; double m = c.lower - distance;
                if (m < q.min_out) { q.min_out = m; q.min_out_cell = q.visited[i]; q.min_out_lower = c.lower; }
            } else {
                q.accepted.push_back(q.visited[i]);
                if (c.status == coverage::Status::intersects) {
                    ++q.confirmed; double m = distance - c.upper;
                    if (m < q.min_in) { q.min_in = m; q.min_in_cell = q.visited[i]; q.min_in_upper = c.upper; }
                } else ++q.unresolved;
            }
        }
        std::sort(q.accepted.begin(), q.accepted.end());
        q.ok = true;
    } catch (const std::exception& e) { q.ok = false; q.error = e.what(); }
    return q;
}
std::vector<Id> minus(const std::vector<Id>& a, const std::vector<Id>& b) {
    std::vector<Id> d; std::set_difference(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(d)); return d;
}
std::vector<double> list(const std::string& s) {
    std::vector<double> v; std::string t = s; std::replace(t.begin(), t.end(), ',', ' '); std::istringstream in(t);
    double x; while (in >> x) v.push_back(x); return v;
}
std::string tag(double x) { std::ostringstream o; o << x; return o.str(); }
int main(int argc, char** argv) try {
    std::map<std::string, std::string> args;
    for (int i = 1; i + 1 < argc; i += 2) args[argv[i]] = argv[i + 1];
    auto get = [&](std::string key, std::string fallback) { auto i = args.find(key); return i == args.end() ? fallback : i->second; };
    const std::string name = get("--name", "case");
    const unsigned level = std::stoul(get("--level", "12"));
    double distance = std::stod(get("--distance-m", "1000"));
    const std::uint64_t capacity = std::stoull(get("--max-cells", "200000"));
    const auto allowances = list(get("--allowances", "0.000001,0.00001,0.0001"));
    const auto tolerances = list(get("--tolerances", "0.0001,0.001,0.01"));
    const double ref_allowance = 1e-5, ref_tolerance = 1e-4;
    const std::string out_dir = get("--out-dir", "settings") + "/" + name;
    std::filesystem::create_directories(out_dir);
    #ifdef _OPENMP
    omp_set_num_threads(int(std::stoul(get("--threads", "2"))));
    #endif
    using namespace bafe::geo;
    std::vector<ellipsoid::Segment> segments; std::string source_kind;
    if (args.count("--point")) {
        std::string s = args["--point"]; std::replace(s.begin(), s.end(), ',', ' '); std::istringstream in(s);
        ellipsoid::Point p; in >> p.latitude >> p.longitude; segments.push_back({p, p}); source_kind = "point";
    } else if (args.count("--base-vertex")) {
        // A point source placed exactly on a vertex of the base mesh with the requested valence (5 or 6).
        const unsigned want = std::stoul(args["--base-vertex"]); int found = -1;
        for (unsigned v = 0; v < kVertices.size() && found < 0; ++v) {
            unsigned valence = 0; for (auto f : kFaces) for (auto c : f) if (c == v) ++valence;
            if (valence == want) found = int(v);
        }
        if (found < 0) throw std::runtime_error("no base vertex with that valence");
        auto p = coverage::geographic(kVertices[found]); segments.push_back({p, p});
        source_kind = "point on base mesh vertex " + std::to_string(found) + " (valence " + std::to_string(want) + ")";
    } else if (args.count("--base-edge")) {
        // A point source placed on the midpoint of the edge between corners 0 and 1 of the given base face.
        const unsigned f = std::stoul(args["--base-edge"]);
        auto p = coverage::geographic(mul(add(kVertices[kFaces[f][0]], kVertices[kFaces[f][1]]), .5));
        segments.push_back({p, p}); source_kind = "point on midpoint of a base mesh edge of face " + std::to_string(f);
    } else {
        std::ifstream in(get("--input", "data/nose_hill_segments.csv"));
        if (!in) throw std::runtime_error("cannot open input segment file");
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#' || line.rfind("lat", 0) == 0) continue;
            std::replace(line.begin(), line.end(), ',', ' '); std::istringstream row(line); ellipsoid::Segment s;
            row >> s.a.latitude >> s.a.longitude >> s.b.latitude >> s.b.longitude; segments.push_back(s);
        }
        source_kind = "segments file";
    }
    // Optional construction of a near threshold query: move d so that the closest excluded
    // (or confirmed) cell of the nominal reference query lies offset metres from the threshold.
    std::string near = get("--near", ""); const double offset = std::stod(get("--offset-m", "0.000002"));
    double nominal = distance; Id near_cell = 0;
    if (!near.empty()) {
        Query q0 = run(segments, level, distance, ref_allowance, ref_tolerance, capacity, 256);
        if (!q0.ok) throw std::runtime_error("nominal query failed: " + q0.error);
        if (near == "excluded") { distance = q0.min_out_lower - offset; near_cell = q0.min_out_cell; }
        else if (near == "confirmed") { distance = q0.min_in_upper + offset; near_cell = q0.min_in_cell; }
        else throw std::runtime_error("--near must be excluded or confirmed");
    }
    {
        std::ofstream in(out_dir + "/inputs.txt"); in << std::setprecision(17);
        in << "name=" << name << "\nlevel=" << level << "\ndistance_m=" << distance << "\nnominal_distance_m=" << nominal
           << "\nnear=" << (near.empty() ? "none" : near) << "\nnear_offset_m=" << (near.empty() ? 0. : offset)
           << "\nnear_cell=" << near_cell << "\nsource=" << source_kind << "\nsegments=" << segments.size() << "\n";
        for (std::size_t i = 0; i < segments.size() && i < 3; ++i)
            in << "segment" << i << "=" << segments[i].a.latitude << "," << segments[i].a.longitude << ","
               << segments[i].b.latitude << "," << segments[i].b.longitude << "\n";
        if (args.count("--input")) in << "input_file=" << std::filesystem::path(args["--input"]).filename().string() << "\n";
        in << "max_cells=" << capacity << "\nreference_allowance_m=" << ref_allowance << "\nreference_tolerance_m=" << ref_tolerance << "\n";
    }
    const Query ref = run(segments, level, distance, ref_allowance, ref_tolerance, capacity, 256);
    std::ofstream changes(out_dir + "/changed_cells.csv");
    changes << "allowance_m,tolerance_m,public_index,change_against_reference,status,lower_m,upper_m\n" << std::setprecision(17);
    std::cout << std::setprecision(17);
    if (get("--header", "1") == "1")
        std::cout << "name,level,distance_m,nominal_distance_m,source,segments,allowance_m,tolerance_m,run_status,"
                     "source_cover_cells,source_cover_added,source_cover_removed,tested_cells,returned_cells,confirmed_cells,"
                     "unresolved_retained_cells,excluded_cells,separating_ring,last_ring_cells,stopping_margin_m,"
                     "smallest_margin_excluded_m,smallest_margin_confirmed_m,added_against_reference,removed_against_reference,error\n";
    for (double a : allowances) for (double t : tolerances) {
        const Query q = run(segments, level, distance, a, t, capacity, 256);
        const std::string stem = out_dir + "/a" + tag(a) + "_t" + tag(t);
        std::cout << name << ',' << level << ',' << distance << ',' << nominal << ',' << '"' << source_kind << '"' << ',' << segments.size()
                  << ',' << a << ',' << t << ',';
        if (!q.ok) { std::cout << "failure,,,,,,,,,,,,,,,,\"" << q.error << "\"\n"; continue; }
        { std::ofstream s(stem + "_source_cover.csv"); s << "public_index\n"; for (auto id : q.seeds) s << id << '\n'; }
        { std::ofstream s(stem + "_returned.csv"); s << "public_index,status,lower_m,upper_m\n" << std::setprecision(17);
          for (std::size_t i = 0; i < q.visited.size(); ++i) if (q.classified[i].status != coverage::Status::outside)
              s << q.visited[i] << ',' << (q.classified[i].status == coverage::Status::unresolved ? "unresolved_retained" : "confirmed")
                << ',' << q.classified[i].lower << ',' << q.classified[i].upper << '\n'; }
        std::vector<Id> added, removed, seed_added, seed_removed;
        if (ref.ok) {
            added = minus(q.accepted, ref.accepted); removed = minus(ref.accepted, q.accepted);
            auto sa = q.seeds, sb = ref.seeds; std::sort(sa.begin(), sa.end()); std::sort(sb.begin(), sb.end());
            seed_added = minus(sa, sb); seed_removed = minus(sb, sa);
            std::map<Id, std::size_t> where; for (std::size_t i = 0; i < q.visited.size(); ++i) where[q.visited[i]] = i;
            std::map<Id, std::size_t> where_ref; for (std::size_t i = 0; i < ref.visited.size(); ++i) where_ref[ref.visited[i]] = i;
            for (auto id : added) { const auto& c = q.classified[where[id]];
                changes << a << ',' << t << ',' << id << ",added," << (c.status == coverage::Status::unresolved ? "unresolved_retained" : "confirmed") << ',' << c.lower << ',' << c.upper << '\n'; }
            for (auto id : removed) { auto it = where.find(id);
                if (it != where.end()) { const auto& c = q.classified[it->second]; changes << a << ',' << t << ',' << id << ",removed,outside," << c.lower << ',' << c.upper << '\n'; }
                else changes << a << ',' << t << ',' << id << ",removed,not_visited,,\n"; }
        }
        std::cout << "ok," << q.seeds.size() << ',' << seed_added.size() << ',' << seed_removed.size() << ',' << q.visited.size() << ','
                  << q.accepted.size() << ',' << q.confirmed << ',' << q.unresolved << ',' << q.outside << ',' << q.hops << ',' << q.last_ring << ','
                  << q.stopping_lower - distance << ',' << q.min_out << ',' << q.min_in << ',' << added.size() << ',' << removed.size() << ",\n";
    }
    return 0;
} catch (const std::exception& e) { std::cerr << "ERROR: " << e.what() << "\n"; return 1; }
