#include "wgs84_distance.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace ellipsoid;
void require(bool condition,const std::string& msg) { if(!condition) throw std::runtime_error(msg); }
void near(double a,double b,double tolerance,const std::string& msg) {
    require(std::abs(a-b)<=tolerance,msg);
}
void contains(const BoundaryResult& r,double expected,double tolerance,const std::string& msg) {
    require(r.converged,msg+": did not converge");
    require(r.lower_m<=expected+1e-7&&expected<=r.upper_m+1e-7,msg+": bounds miss oracle");
    require(r.upper_m-r.lower_m<=tolerance,msg+": bracket exceeds tolerance");
}
int main() try {
    WGS84 g;
    near(g.inverse({0,0},{0,1}).distance_m,111319.49079327357,1e-6,"equatorial degree");
    near(g.inverse({0,0},{1,0}).distance_m,110574.38855779878,1e-6,"meridional degree");
    near(g.inverse({0,0},{0,180}).distance_m,20003931.458625447,1e-6,"antipodal");
    near(g.inverse({90,0},{90,123}).distance_m,0,1e-9,"same pole");
    auto p=g.direct({51.1,-114.1},72.,10000.);
    near(g.inverse({51.1,-114.1},p).distance_m,10000.,1e-6,"direct inverse");
    bool invalid=false;
    try {g.inverse({91,0},{0,0});} catch(const std::invalid_argument&) {invalid=true;}
    require(invalid,"latitude validation");
    invalid=false;
    try {g.inverse({0,std::numeric_limits<double>::quiet_NaN()},{0,0});}
    catch(const std::invalid_argument&) {invalid=true;}
    require(invalid,"NaN validation");
    Boundary equator({{{0,-1},{0,1}}});
    Options opts; opts.tolerance_m=.001;
    auto middle=equator.distance({1,0},opts);
    contains(middle,110574.38855779878,.001,"interior minimum");
    require(middle.upper_m<g.inverse({1,0},{0,1}).distance_m-10000,"not endpoint sampling");
    contains(equator.distance({0,0},opts),0,.001,"point on segment");
    contains(equator.distance({0,2},opts),111319.49079327357,.001,"endpoint minimum");
    Boundary dateline({{{0,179},{0,-179}}});
    contains(dateline.distance({1,180},opts),110574.38855779878,.001,"antimeridian segment");
    Boundary repeated({{{51,-114},{51,-114}}});
    contains(repeated.distance({51.001,-114},opts),g.inverse({51.001,-114},{51,-114}).distance_m,.001,"zero length");
    auto end=g.direct({88,40},45,50000);
    auto inside=g.direct({88,40},45,23765);
    Boundary polar({{{88,40},end}});
    contains(polar.distance(inside,opts),0,.001,"polar segment");
    Boundary multi({{{40,20},{40,21}},{{0,-1},{0,1}}});
    contains(multi.distance({1,0},opts),110574.38855779878,.001,"disconnected components");
    Options limited=opts; limited.max_evaluations=2;
    auto unresolved=equator.distance({1,0},limited);
    require(!unresolved.converged,"budget exhaustion must not claim convergence");
    require(classify_point(middle,110575)==Relation::inside,"inside threshold");
    require(classify_point(middle,110574)==Relation::outside,"outside threshold");
    require(classify_point(middle,(middle.lower_m+middle.upper_m)/2)==Relation::unresolved,"threshold uncertainty");
    std::cout<<"PASS: known geodesics, antipodes, poles, invalid inputs, complete segment minima, dateline, degeneracy, components, budget exhaustion, threshold uncertainty.\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
