// Context measurement for the review (2026-11): union of H3 grid disks for
// the same seed points as the Pentakis halo, on one CPU thread.
//
// For every seed point the H3 cell of the requested resolution is found, the
// seed cells are deduplicated, gridDisk(origin, k) is evaluated for every seed
// cell, and the union of the disks is formed with the same open addressing set
// (same mixing function, load at most one half) as the serial flat table of
// the Pentakis implementation.  The returned set excludes the seed cells, as
// the halo does.  The timer is a wall clock around the complete call: seed
// cell indices in host memory to a new vector of result indices.  Point to
// cell assignment is outside the timer, as in the halo benchmark.
//
// H3 cells are hexagons with six neighbors, so this is a different operation
// from the triangular halo; the result gives the scale of an established
// library and is not a ranking.
#include "bafe_exp/h3_compat.hpp"
#include "bafe_exp/benchmark_common.hpp"
#include "bafe_exp/cli.hpp"
#include "bafe_exp/point_csv.hpp"

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
constexpr double kPi = 3.14159265358979323846;
constexpr std::uint64_t kEmpty = 0xffffffffffffffffULL;

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
private:
    std::vector<std::uint64_t> slots_;
    std::uint64_t mask_ = 0U;
};

struct Run {
    std::vector<std::uint64_t> out;
    std::uint64_t disk_cells = 0U;   // cells returned by all gridDisk calls
};

Run disk_union(const std::vector<H3Index>& seeds, int k, std::uint64_t capacity,
               FlatSet64& set, std::vector<H3Index>& disk) {
    Run r;
    set.reset(capacity);
    for (const H3Index seed : seeds) (void)set.insert(seed);
    for (const H3Index seed : seeds) {
        std::fill(disk.begin(), disk.end(), H3Index(0));
        if (gridDisk(seed, k, disk.data()) != E_SUCCESS) throw std::runtime_error("gridDisk failed");
        for (const H3Index cell : disk) {
            if (cell == 0) continue;   // unused entries near a pentagon
            ++r.disk_cells;
            if (set.insert(cell)) r.out.push_back(cell);
        }
    }
    return r;
}
}  // namespace

int main(int argc, char** argv) {
    try {
        bafe_exp::Cli cli(argc, argv);
        const int resolution = int(cli.u32("resolution", 14));
        const int k = int(cli.u32("k", 5));
        const auto maximum = cli.u64("max-points", 0);
        const auto warmup = cli.u32("warmup", 5);
        const auto repeats = cli.u32("repeats", 30);
        const auto points_path = cli.get("points", "");
        const auto table_mode = cli.get("table", "tight");
        const auto csv_path = cli.get("csv", "results/h3_grid_disk_union.csv");
        if (points_path.empty()) throw std::invalid_argument("--points points.csv is required");

        const auto points = bafe_exp::read_lonlat_csv(points_path);
        const std::size_t n = maximum == 0 ? points.size() : std::min<std::size_t>(points.size(), maximum);
        std::vector<H3Index> seeds;
        seeds.reserve(n);
        const auto map_start = Clock::now();
        for (std::size_t sample = 0; sample < n; ++sample) {
            const std::size_t index = n == points.size() ? sample : (sample * points.size()) / n;
            LatLng p{points[index].second * kPi / 180.0, points[index].first * kPi / 180.0};
            H3Index h = 0;
            if (latLngToCell(&p, resolution, &h) != E_SUCCESS) throw std::runtime_error("latLngToCell failed");
            seeds.push_back(h);
        }
        std::sort(seeds.begin(), seeds.end());
        seeds.erase(std::unique(seeds.begin(), seeds.end()), seeds.end());
        const double mapping_ms = std::chrono::duration<double, std::milli>(Clock::now() - map_start).count();

        int64_t disk_size = 0;
        if (maxGridDiskSize(k, &disk_size) != E_SUCCESS) throw std::runtime_error("maxGridDiskSize failed");
        std::vector<H3Index> disk(static_cast<std::size_t>(disk_size));
        FlatSet64 set;
        const std::uint64_t budget = std::uint64_t(seeds.size()) * std::uint64_t(disk_size - 1);
        std::uint64_t capacity = next_power_of_two(std::max<std::uint64_t>(1024U, 2U * (seeds.size() + budget)));
        if (table_mode == "tight") {
            const Run probe = disk_union(seeds, k, capacity, set, disk);
            capacity = next_power_of_two(std::max<std::uint64_t>(1024U, 2U * (seeds.size() + probe.out.size())));
        } else if (table_mode != "auto") {
            throw std::invalid_argument("--table must be tight or auto");
        }

        for (std::uint32_t i = 0; i < warmup; ++i) (void)disk_union(seeds, k, capacity, set, disk);
        std::vector<double> times;
        Run last;
        for (std::uint32_t i = 0; i < repeats; ++i) {
            const auto start = Clock::now();
            Run current = disk_union(seeds, k, capacity, set, disk);
            times.push_back(std::chrono::duration<double, std::milli>(Clock::now() - start).count());
            last = std::move(current);
        }
        double area = 0.0;
        if (!seeds.empty()) (void)cellAreaM2(seeds[seeds.size() / 2U], &area);

        bafe_exp::ResultRow row;
        row.experiment_id = "h3_grid_disk_union";
        row.backend = "h3_grid_disk_union_serial";
        row.library_version = "H3 " + std::to_string(H3_VERSION_MAJOR) + "." +
            std::to_string(H3_VERSION_MINOR) + "." + std::to_string(H3_VERSION_PATCH);
        row.compiler = bafe_exp::compiler_string();
        row.pattern = "point_samples";
        row.scenario = "calgary_points";
        row.level_or_resolution = static_cast<std::uint32_t>(resolution);
        row.median_cell_area_m2 = area;
        row.input_cells = n;
        row.unique_input_cells = seeds.size();
        row.hops = static_cast<std::uint32_t>(k);
        row.output_cells = last.out.size();
        row.candidate_references = last.disk_cells;
        row.mapping_ms = mapping_ms;
        row.timing = bafe_exp::summarize(times);
        row.result_hash = bafe_exp::fnv1a64(last.out);
        row.correctness_status = "SELF";
        row.notes = "union of gridDisk per seed; seeds excluded; table=" + table_mode;
        row.variant = "grid_disk_union";
        row.threads = 1U;
        row.timer = "wall";
        std::ofstream csv(csv_path);
        if (!csv) throw std::runtime_error("cannot open " + csv_path);
        bafe_exp::write_csv_header(csv);
        bafe_exp::write_csv_row(csv, row);
        std::cout << row.library_version << " resolution=" << resolution << " k=" << k << " seeds=" << seeds.size()
                  << " output=" << row.output_cells << " median_ms=" << row.timing.median_ms
                  << " ns_per_output_cell=" << 1e6 * row.timing.median_ms / double(std::max<std::uint64_t>(1, row.output_cells))
                  << " cell_area_m2=" << area << '\n' << "CSV: " << csv_path << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "h3_grid_disk_union: " << error.what() << '\n';
        return 1;
    }
}
