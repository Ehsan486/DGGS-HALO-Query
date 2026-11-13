#pragma once

#include "bafe/index.hpp"
#include "internal.hpp"
#include "bafe_exp/pentakis_geometry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace bafe::geo {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kEarthRadiusM = 6371008.8;

inline Vec3 add(Vec3 a, Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline Vec3 sub(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline Vec3 mul(Vec3 a, double s) { return {a.x*s,a.y*s,a.z*s}; }
inline double dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}
inline double norm(Vec3 a) { return std::sqrt(dot(a,a)); }
inline Vec3 normalize(Vec3 a) {
    const double n=norm(a);
    if (!(n>0.0)) throw std::invalid_argument("zero vector");
    return mul(a,1.0/n);
}

inline Vec3 lonlat_to_unit(double lon_deg, double lat_deg) {
    const double lon=lon_deg*kPi/180.0;
    const double lat=lat_deg*kPi/180.0;
    const double c=std::cos(lat);
    return {c*std::cos(lon), c*std::sin(lon), std::sin(lat)};
}

inline std::pair<double,double> unit_to_lonlat(Vec3 p) {
    p=normalize(p);
    return {std::atan2(p.y,p.x)*180.0/kPi,
            std::asin(std::clamp(p.z,-1.0,1.0))*180.0/kPi};
}

struct LocatedPoint {
    std::uint32_t face=0;
    std::array<double,3> bary{1.0,0.0,0.0};
    double margin=-std::numeric_limits<double>::infinity();
};

inline LocatedPoint locate_unit_point(Vec3 p) {
    p=normalize(p);
    LocatedPoint best;
    for (std::uint32_t f=0; f<60; ++f) {
        const auto &m=kFaceMaps[f];
        const double denominator=dot(p,m.normal);
        if (denominator<=1e-14) continue;
        const Vec3 q=mul(p,m.distance/denominator);
        const Vec3 r=sub(q,m.c);
        const double u=m.pinv[0]*r.x+m.pinv[1]*r.y+m.pinv[2]*r.z;
        const double v=m.pinv[3]*r.x+m.pinv[4]*r.y+m.pinv[5]*r.z;
        const std::array<double,3> bary{u,v,1.0-u-v};
        const double margin=std::min({bary[0],bary[1],bary[2]});
        if (margin>best.margin) best=LocatedPoint{f,bary,margin};
    }
    if (best.margin < -1e-7) throw std::runtime_error("Pentakis point-to-face mapping failed");
    return best;
}

inline detail::Cell lonlat_to_coord(double lon_deg, double lat_deg, std::uint32_t level) {
    if (level>kMaxLevel) throw std::out_of_range("level exceeds 28");
    const LocatedPoint loc=locate_unit_point(lonlat_to_unit(lon_deg,lat_deg));
    const double n=std::ldexp(1.0,static_cast<int>(level));
    constexpr double eps=1e-11;
    double x=std::clamp(loc.bary[1]*n,0.0,n-eps);
    double y=std::clamp(loc.bary[2]*n,0.0,n-eps);
    if (x+y>n) {
        const double s=n/(x+y);
        x=std::min(x*s,n-eps);
        y=std::min(y*s,n-eps);
    }
    const auto i=static_cast<std::uint32_t>(std::floor(x));
    const auto j=static_cast<std::uint32_t>(std::floor(y));
    const std::uint32_t o=((x-i)+(y-j)>1.0+1e-12)?1U:0U;
    detail::Cell c{loc.face,i,j,o};
    if (!detail::valid_cell(c,level)) throw std::runtime_error("numerically invalid Pentakis cell");
    return c;
}

inline CellIndex lonlat_to_cell(double lon_deg, double lat_deg, std::uint32_t level) {
    return detail::cell_to_path_scalar(lonlat_to_coord(lon_deg,lat_deg,level),level);
}

inline std::array<Vec3,3> cell_vertices(detail::Cell cell, std::uint32_t level) {
    const double n=std::ldexp(1.0,static_cast<int>(level));
    std::array<std::array<double,2>,3> xy{};
    if (cell.orientation==0U) {
        xy={{{double(cell.i),double(cell.j)}, {double(cell.i+1U),double(cell.j)}, {double(cell.i),double(cell.j+1U)}}};
    } else {
        xy={{{double(cell.i+1U),double(cell.j)}, {double(cell.i),double(cell.j+1U)}, {double(cell.i+1U),double(cell.j+1U)}}};
    }
    const auto tri=kFaces[cell.face];
    std::array<Vec3,3> out{};
    for (int k=0;k<3;++k) {
        const double x=xy[k][0]/n;
        const double y=xy[k][1]/n;
        const double b0=1.0-x-y;
        const Vec3 p=add(add(mul(kVertices[tri[0]],b0),mul(kVertices[tri[1]],x)),mul(kVertices[tri[2]],y));
        out[k]=normalize(p);
    }
    return out;
}

inline Vec3 cell_centroid(detail::Cell cell, std::uint32_t level) {
    const auto v=cell_vertices(cell,level);
    return normalize(add(add(v[0],v[1]),v[2]));
}

inline std::pair<double,double> cell_centroid_lonlat(CellIndex id, std::uint32_t level) {
    return unit_to_lonlat(cell_centroid(detail::path_to_cell_scalar(id,level),level));
}

inline double spherical_triangle_area_unit(Vec3 a, Vec3 b, Vec3 c) {
    // Stable formula: area = 2 atan2(|a b c|, 1+a.b+b.c+c.a).
    const double numerator=std::abs(dot(a,cross(b,c)));
    const double denominator=1.0+dot(a,b)+dot(b,c)+dot(c,a);
    return 2.0*std::atan2(numerator,denominator);
}

inline double cell_area_m2(CellIndex id, std::uint32_t level) {
    const auto c=detail::path_to_cell_scalar(id,level);
    const auto v=cell_vertices(c,level);
    return spherical_triangle_area_unit(v[0],v[1],v[2])*kEarthRadiusM*kEarthRadiusM;
}

inline double global_average_cell_area_m2(std::uint32_t level) {
    return 4.0*kPi*kEarthRadiusM*kEarthRadiusM/(60.0*std::pow(4.0,double(level)));
}

} // namespace bafe::geo
