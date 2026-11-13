#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include "geodesic.h"

namespace ellipsoid {
struct Point { double latitude, longitude; }; // geodetic degrees, WGS84
struct InverseResult { double distance_m, azimuth1_deg, azimuth2_deg; };
struct Segment { Point a, b; };
struct Options {
    double tolerance_m = 0.001;
    // Engineering allowance, not an interval arithmetic certificate.
    double numerical_guard_m = 0.00001;
    std::uint64_t max_evaluations = 1000000;
};
struct BoundaryResult {
    double lower_m = 0, upper_m = 0;
    Point closest_sample{};
    std::size_t segment_index = 0;
    std::uint64_t evaluations = 0;
    bool converged = false;
};
class WGS84 {
public:
    WGS84();
    InverseResult inverse(Point a, Point b) const;
    Point direct(Point a, double azimuth_deg, double distance_m) const;
    const geod_geodesic& native() const { return model_; }
private:
    geod_geodesic model_{};
};
// Immutable prepared source. All edges are shortest WGS84 geodesic segments.
// Distance is to the boundary, NOT to a filled polygon's interior.
class Boundary {
public:
    explicit Boundary(const std::vector<Segment>& segments);
    BoundaryResult distance(Point query, Options options = {}) const;
    std::size_t size() const { return arcs_.size(); }
private:
    struct Arc { Segment endpoints; geod_geodesicline line; double length; };
    WGS84 earth_;
    std::vector<Arc> arcs_;
};
enum class Relation { inside, outside, unresolved };
// For a QUERY POINT, not a complete DGGS cell.
Relation classify_point(const BoundaryResult& result, double distance_m);
} // namespace ellipsoid
