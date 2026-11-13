// Decision margins of the WGS84 buffer queries (revision 2026-11, Reviewer 2
// comment 3).  The program repeats the published buffer pipeline of
// Scalable_Atlas_Ellipsoidal_Buffer/src/main.cpp with the CPU halo backend
// (source cover, halo replay at depths 1,2,4,..., whole ring stopping test,
// footprint classification of every visited cell) and records, for every
// tested cell, the margin by which it was decided:
//   excluded cell:   lower bound - d   (the lower bound already contains the
//                                       10 micrometre allowance)
//   confirmed cell:  d - upper bound
// It then repeats the complete query at d + delta and d - delta and counts the
// cells whose membership in the returned set changes.
// Build: see build_buffer_margins.sh.  No timing is taken here.
#include "coverage.hpp"
#include "bafe/bafe.hpp"
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
    std::vector<Id> visited;
    std::vector<coverage::CellResult> classified;
    std::vector<Id> accepted;
    unsigned hops = 0;
    double stopping_lower = 0;
    std::size_t seeds = 0, last_ring = 0;
};
Query run(const coverage::Feature& source, unsigned level, double distance, double tolerance,
          std::uint64_t capacity, unsigned max_hops) {
    Query q;
    auto seeds = coverage::cover(source, level, 0, capacity);
    q.seeds = seeds.size();
    auto engine = bafe::make_cpu_engine();
    bafe::HaloResult halo;
    unsigned hops = 1;
    for (;;) {
        bafe::QueryOptions options; options.level = level; options.hops = hops; options.max_output_cells = capacity;
        halo = engine->halo(seeds, options);
        auto start = halo.layer_offsets[hops - 1], end = halo.layer_offsets[hops];
        std::vector<double> bounds(end - start);
        parallel(bounds.size(), [&](std::size_t i) {
            bounds[i] = source.bounds(coverage::patch(halo.cells[start + i], level)).lower; });
        q.stopping_lower = bounds.empty() ? INFINITY : *std::min_element(bounds.begin(), bounds.end());
        q.last_ring = bounds.size();
        if (q.stopping_lower > distance) break;
        if (hops >= max_hops) throw std::runtime_error("maximum hops reached");
        hops = std::min(max_hops, hops * 2);
    }
    q.hops = hops;
    q.visited = seeds;
    q.visited.insert(q.visited.end(), halo.cells.begin(), halo.cells.end());
    q.classified.resize(q.visited.size());
    parallel(q.visited.size(), [&](std::size_t i) {
        q.classified[i] = coverage::classify(source, coverage::patch(q.visited[i], level), distance, tolerance); });
    for (std::size_t i = 0; i < q.visited.size(); ++i)
        if (q.classified[i].status != coverage::Status::outside) q.accepted.push_back(q.visited[i]);
    std::sort(q.accepted.begin(), q.accepted.end());
    return q;
}
std::size_t symmetric_difference(const std::vector<Id>& a, const std::vector<Id>& b) {
    std::vector<Id> d;
    std::set_symmetric_difference(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(d));
    return d.size();
}
int main(int argc, char** argv) try {
    std::map<std::string, std::string> args;
    for (int i = 1; i + 1 < argc; i += 2) args[argv[i]] = argv[i + 1];
    auto get = [&](std::string key, std::string fallback) { auto i = args.find(key); return i == args.end() ? fallback : i->second; };
    const std::string name = get("--name", "case");
    const unsigned level = std::stoul(get("--level", "12"));
    const double distance = std::stod(get("--distance-m", "1000"));
    const double tolerance = std::stod(get("--tolerance-m", "0.0001"));
    const double delta = std::stod(get("--delta-m", "0.001"));
    const std::uint64_t capacity = std::stoull(get("--max-cells", "200000"));
    #ifdef _OPENMP
    omp_set_num_threads(int(std::stoul(get("--threads", "2"))));
    #endif
    std::vector<ellipsoid::Segment> segments;
    if (args.count("--point")) {
        std::string s = args["--point"]; std::replace(s.begin(), s.end(), ',', ' '); std::istringstream in(s);
        ellipsoid::Point p; in >> p.latitude >> p.longitude; segments.push_back({p, p});
    } else {
        std::ifstream in(get("--input", "data/nose_hill_segments.csv"));
        if (!in) throw std::runtime_error("cannot open input segment file");
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#' || line.rfind("lat", 0) == 0) continue;
            std::replace(line.begin(), line.end(), ',', ' '); std::istringstream row(line); ellipsoid::Segment s;
            row >> s.a.latitude >> s.a.longitude >> s.b.latitude >> s.b.longitude; segments.push_back(s);
        }
    }
    coverage::Feature source(segments);
    const Query base = run(source, level, distance, tolerance, capacity, 256);
    const Query plus = run(source, level, distance + delta, tolerance, capacity, 256);
    const Query minus = run(source, level, distance - delta, tolerance, capacity, 256);

    std::size_t outside = 0, inside = 0, unresolved = 0;
    double min_out = INFINITY, min_in = INFINITY;
    const double thresholds[4] = {1e-5, 1e-3, 1e-2, 1.0};
    std::size_t below[4] = {0, 0, 0, 0};
    std::ofstream cells(get("--cells-csv", name + "_margins.csv"));
    cells << "public_index,status,lower_m,upper_m,margin_m,evaluations\n" << std::setprecision(17);
    for (std::size_t i = 0; i < base.visited.size(); ++i) {
        const auto& c = base.classified[i];
        double margin = 0;
        const char* status = "unresolved";
        if (c.status == coverage::Status::outside) { margin = c.lower - distance; ++outside; min_out = std::min(min_out, margin); status = "outside"; }
        else if (c.status == coverage::Status::intersects) { margin = distance - c.upper; ++inside; min_in = std::min(min_in, margin); status = "intersects"; }
        else { ++unresolved; }
        for (int k = 0; k < 4; ++k) if (margin < thresholds[k]) ++below[k];
        cells << base.visited[i] << ',' << status << ',' << c.lower << ',' << c.upper << ',' << margin << ',' << c.evaluations << '\n';
    }
    std::cout << std::setprecision(10)
              << "name,level,distance_m,segments,seed_cells,tested_cells,returned_cells,excluded_cells,unresolved_cells,"
                 "separating_ring,last_ring_cells,stopping_margin_m,smallest_margin_excluded_m,smallest_margin_confirmed_m,"
                 "below_10um,below_1mm,below_1cm,below_1m,delta_m,returned_plus,changed_plus,returned_minus,changed_minus\n"
              << name << ',' << level << ',' << distance << ',' << segments.size() << ',' << base.seeds << ','
              << base.visited.size() << ',' << base.accepted.size() << ',' << outside << ',' << unresolved << ','
              << base.hops << ',' << base.last_ring << ',' << base.stopping_lower - distance << ','
              << min_out << ',' << min_in << ',' << below[0] << ',' << below[1] << ',' << below[2] << ',' << below[3] << ','
              << delta << ',' << plus.accepted.size() << ',' << symmetric_difference(base.accepted, plus.accepted) << ','
              << minus.accepted.size() << ',' << symmetric_difference(base.accepted, minus.accepted) << '\n';
    return 0;
} catch (const std::exception& e) { std::cerr << "ERROR: " << e.what() << "\n"; return 1; }
