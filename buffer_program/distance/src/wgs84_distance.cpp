#include "wgs84_distance.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <stdexcept>

namespace ellipsoid {
namespace {
void validate(Point p) {
    if (!std::isfinite(p.latitude) || !std::isfinite(p.longitude) ||
        std::abs(p.latitude)>90 || std::abs(p.longitude)>180)
        throw std::invalid_argument("Expected finite geodetic degrees: lat [-90,90], lon [-180,180].");
}
struct Interval {
    std::size_t arc;
    double start, end, d_start, d_end, lower;
    bool operator<(const Interval& other) const { return lower>other.lower; }
};
double lower_bound(double da, double db, double length, double guard) {
    // Distance to a fixed point is 1-Lipschitz along arc length.
    // The endpoint distance cones give this lower bound on the whole interval.
    return std::max(0., std::max({(da+db-length)*.5,da-length,db-length})-guard);
}
}
WGS84::WGS84() { geod_init(&model_,6378137.,1./298.257223563); }
InverseResult WGS84::inverse(Point a, Point b) const {
    validate(a); validate(b);
    InverseResult r{};
    geod_inverse(&model_,a.latitude,a.longitude,b.latitude,b.longitude,
                 &r.distance_m,&r.azimuth1_deg,&r.azimuth2_deg);
    if (!std::isfinite(r.distance_m)) throw std::runtime_error("Geodesic inverse failed.");
    return r;
}
Point WGS84::direct(Point a,double azimuth,double distance) const {
    validate(a);
    if (!std::isfinite(azimuth)||!std::isfinite(distance)||distance<0)
        throw std::invalid_argument("Invalid direct geodesic arguments.");
    Point b{};
    geod_direct(&model_,a.latitude,a.longitude,azimuth,distance,
                &b.latitude,&b.longitude,nullptr);
    return b;
}
Boundary::Boundary(const std::vector<Segment>& segments) {
    if (segments.empty()) throw std::invalid_argument("Boundary must contain at least one segment.");
    arcs_.reserve(segments.size());
    for (const auto& s:segments) {
        validate(s.a); validate(s.b);
        Arc arc{}; arc.endpoints=s;
        geod_inverseline(&arc.line,&earth_.native(),s.a.latitude,s.a.longitude,
                        s.b.latitude,s.b.longitude,0);
        arc.length=arc.line.s13;
        if (!std::isfinite(arc.length)) throw std::runtime_error("Invalid geodesic segment.");
        arcs_.push_back(arc);
    }
}
BoundaryResult Boundary::distance(Point query,Options options) const {
    validate(query);
    if (!std::isfinite(options.tolerance_m)||!std::isfinite(options.numerical_guard_m)||
        options.numerical_guard_m<1e-6||options.tolerance_m<=2*options.numerical_guard_m)
        throw std::invalid_argument("Tolerance must exceed twice the guard; guard must be >= 1 micrometre.");
    if (options.max_evaluations/2<arcs_.size())
        throw std::invalid_argument("Evaluation budget too small even to inspect all boundary segments.");
    BoundaryResult result{};
    double best=std::numeric_limits<double>::infinity();
    auto evaluate=[&](Point p,std::size_t id) {
        const double d=earth_.inverse(query,p).distance_m;
        ++result.evaluations;
        if(d<best) { best=d; result.closest_sample=p; result.segment_index=id; }
        return d;
    };
    std::priority_queue<Interval> work;
    for (std::size_t k=0;k<arcs_.size();++k) {
        const auto& a=arcs_[k];
        const double da=evaluate(a.endpoints.a,k),db=evaluate(a.endpoints.b,k);
        work.push({k,0,a.length,da,db,lower_bound(da,db,a.length,options.numerical_guard_m)});
    }
    for (;;) {
        result.upper_m=best+options.numerical_guard_m;
        result.lower_m=work.top().lower;
        if(result.lower_m>result.upper_m)
            throw std::runtime_error("Numerical bounds are inconsistent; no distance result accepted.");
        result.converged=result.upper_m-result.lower_m<=options.tolerance_m;
        if (result.converged||result.evaluations>=options.max_evaluations) return result;
        auto x=work.top(); work.pop();
        const double middle=x.start+(x.end-x.start)*.5;
        if (middle==x.start||middle==x.end) {
            // Explicit unresolved status, never a false convergence claim.
            work.push(x); return result;
        }
        Point p{};
        geod_position(&arcs_[x.arc].line,middle,&p.latitude,&p.longitude,nullptr);
        const double dm=evaluate(p,x.arc);
        const double left=std::max(x.lower,lower_bound(x.d_start,dm,middle-x.start,options.numerical_guard_m));
        const double right=std::max(x.lower,lower_bound(dm,x.d_end,x.end-middle,options.numerical_guard_m));
        work.push({x.arc,x.start,middle,x.d_start,dm,left});
        work.push({x.arc,middle,x.end,dm,x.d_end,right});
    }
}
Relation classify_point(const BoundaryResult& result,double distance) {
    if (!std::isfinite(distance)||distance<0) throw std::invalid_argument("Invalid threshold.");
    if (result.upper_m<=distance) return Relation::inside;
    if (result.lower_m>distance) return Relation::outside;
    return Relation::unresolved;
}
} // namespace ellipsoid
