#include "bafe/bafe.hpp"
#include "bafe_exp/cli.hpp"
#include "bafe_exp/geo.hpp"
#include "bafe_exp/point_csv.hpp"
#include "internal.hpp"
#include "topology.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace {

std::vector<bafe::CellIndex> map_points(
    const std::vector<std::pair<double, double>>& points,
    std::uint32_t level,
    std::uint64_t maximum) {
    const std::size_t n = maximum == 0U
        ? points.size()
        : std::min<std::size_t>(points.size(), maximum);
    std::unordered_set<bafe::CellIndex> unique;
    unique.reserve(n * 2U + 16U);
    for (std::size_t sample = 0; sample < n; ++sample) {
        const std::size_t index = n == points.size()
            ? sample
            : sample * points.size() / n;
        unique.insert(bafe::geo::lonlat_to_cell(
            points[index].first, points[index].second, level));
    }
    return {unique.begin(), unique.end()};
}

double percentile(std::vector<double> values, double q) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const double position = q * static_cast<double>(values.size() - 1U);
    const auto lo = static_cast<std::size_t>(std::floor(position));
    const auto hi = static_cast<std::size_t>(std::ceil(position));
    const double t = position - static_cast<double>(lo);
    return values[lo] * (1.0 - t) + values[hi] * t;
}

std::vector<double> neighbor_distances(
    const std::vector<bafe::CellIndex>& cells,
    std::uint32_t level,
    std::size_t maximum_cells) {
    std::vector<double> result;
    if (cells.empty()) return result;
    const std::size_t samples = std::min(maximum_cells, cells.size());
    result.reserve(samples * 3U);
    for (std::size_t sample = 0; sample < samples; ++sample) {
        const std::size_t index = sample * cells.size() / samples;
        const auto source = bafe::detail::path_to_cell_scalar(cells[index], level);
        const auto a = bafe::geo::cell_centroid(source, level);
        bafe::detail::Cell neighbors[3];
        bafe::detail::neighbors_host(source, level, neighbors, nullptr);
        for (const auto& neighbor : neighbors) {
            const auto b = bafe::geo::cell_centroid(neighbor, level);
            // atan2(||a x b||, a dot b) remains accurate for centimetre-scale
            // level-28 edges, where acos(dot) can round small angles to zero.
            const double angle = std::atan2(
                bafe::geo::norm(bafe::geo::cross(a, b)),
                std::clamp(bafe::geo::dot(a, b), -1.0, 1.0));
            result.push_back(bafe::geo::kEarthRadiusM * angle);
        }
    }
    return result;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        bafe_exp::Cli cli(argc, argv);
        const auto input_path = cli.get("input");
        if (input_path.empty()) throw std::invalid_argument("--input points.csv is required");
        const auto scenario = cli.get("scenario", "real_world");
        const auto output_path = cli.get("csv", "spatial_semantics.csv");
        const auto maximum = cli.u64("max-points", 100000U);
        const auto sample_cells = static_cast<std::size_t>(cli.u64("sample-cells", 2048U));
        const auto levels_text = cli.get("levels", "20,28");

        std::vector<std::uint32_t> levels;
        std::size_t start = 0U;
        while (start < levels_text.size()) {
            const auto end = levels_text.find(',', start);
            levels.push_back(static_cast<std::uint32_t>(std::stoul(
                levels_text.substr(start, end - start))));
            if (end == std::string::npos) break;
            start = end + 1U;
        }

        const auto points = bafe_exp::read_lonlat_csv(input_path);
        const bool write_header = !std::filesystem::exists(output_path)
            || std::filesystem::file_size(output_path) == 0U;
        std::ofstream output(output_path, std::ios::app);
        if (!output) throw std::runtime_error("cannot open " + output_path);
        if (write_header) {
            output << "scenario,level,unique_input_cells,sampled_edges,"
                      "neighbor_p05_m,neighbor_median_m,neighbor_mean_m,neighbor_p95_m,"
                      "reach_1hop_m,reach_4hop_m,reach_8hop_m\n";
        }

        output << std::setprecision(12);
        for (const auto level : levels) {
            const auto cells = map_points(points, level, maximum);
            const auto distances = neighbor_distances(cells, level, sample_cells);
            if (distances.empty()) throw std::runtime_error("no neighbor distances produced");
            double sum = 0.0;
            for (const double distance : distances) sum += distance;
            const double median = percentile(distances, 0.5);
            output << scenario << ',' << level << ',' << cells.size() << ','
                   << distances.size() << ',' << percentile(distances, 0.05) << ','
                   << median << ',' << sum / distances.size() << ','
                   << percentile(distances, 0.95) << ',' << median << ','
                   << 4.0 * median << ',' << 8.0 * median << '\n';
            std::cout << scenario << " L" << level << ": median one-hop="
                      << median << " m, four-hop=" << 4.0 * median
                      << " m, eight-hop=" << 8.0 * median << " m\n";
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "bafe_spatial_semantics: " << error.what() << '\n';
        return 1;
    }
}
