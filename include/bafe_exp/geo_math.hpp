#pragma once

#include <algorithm>
#include <cmath>
#include <utility>

namespace bafe_exp {
constexpr double kPiGeo=3.141592653589793238462643383279502884;
constexpr double kEarthRadiusMGeo=6371008.8;
inline double great_circle_m(double lon1,double lat1,double lon2,double lat2){
    const double p1=lat1*kPiGeo/180.0,p2=lat2*kPiGeo/180.0;
    const double dl=(lon2-lon1)*kPiGeo/180.0;
    const double c=std::clamp(std::sin(p1)*std::sin(p2)+std::cos(p1)*std::cos(p2)*std::cos(dl),-1.0,1.0);
    return kEarthRadiusMGeo*std::acos(c);
}
}
