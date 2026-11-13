#pragma once
#include "wgs84_distance.hpp"
#include "bafe_exp/geo.hpp"
#include <queue>
#include <numeric>
#include <memory>

namespace coverage {
inline double g_allowance = 0.00001; // settings study: stands for the literal 1e-5 of coverage.hpp
using bafe::geo::Vec3;
using namespace bafe::geo;
using ellipsoid::Point;
struct Bounds { double lower, upper; };
inline Point geographic(Vec3 v) {
    return {std::atan2(v.z,std::hypot(v.x,v.y))*180/kPi,
            std::atan2(v.y,v.x)*180/kPi};
}
// Geodetic latitude is the latitude parameter of the original spherical DGGS.
// Cell edges are NOT replaced by ellipsoidal geodesic chords.
struct Patch { std::array<Vec3,3> vertices; double base_height; };
inline Patch patch(bafe::CellIndex id,unsigned r) {
    auto c=bafe::detail::path_to_cell_scalar(id,r);
    auto f=kFaces[c.face];
    Vec3 a=kVertices[f[0]],b=kVertices[f[1]],d=kVertices[f[2]];
    auto normal=normalize(cross(sub(b,a),sub(d,a)));
    double n=std::ldexp(1.,int(r));
    auto at=[&](double i,double j){return add(a,add(mul(sub(b,a),i/n),mul(sub(d,a),j/n)));};
    if(c.orientation==0) return {{{at(c.i,c.j),at(c.i+1.,c.j),at(c.i,c.j+1.)}},std::abs(dot(normal,a))};
    return {{{at(c.i+1.,c.j),at(c.i,c.j+1.),at(c.i+1.,c.j+1.)}},std::abs(dot(normal,a))};
}
inline Vec3 centre(const Patch& p) {return mul(add(add(p.vertices[0],p.vertices[1]),p.vertices[2]),1./3);}
inline double radius(const Patch& p) {
    auto c=centre(p); double m=0;
    for(auto v:p.vertices) m=std::max(m,norm(sub(v,c)));
    // max(M(phi),N(phi))=a^2/b < 6,400,000 m on WGS84.
    // Radial normalization has Lipschitz factor <= 1 / base plane height.
    // The 10 micrometre allowance is engineering error control, not interval certification.
    return 6400000.*m/p.base_height+g_allowance;
}
inline std::array<Patch,4> split(const Patch& p) {
    auto a=p.vertices[0],b=p.vertices[1],c=p.vertices[2];
    auto ab=mul(add(a,b),.5),bc=mul(add(b,c),.5),ca=mul(add(c,a),.5);
    return {Patch{{a,ab,ca},p.base_height},Patch{{ab,b,bc},p.base_height},
            Patch{{ca,bc,c},p.base_height},Patch{{ab,bc,ca},p.base_height}};
}

// Immutable geodesic segment hierarchy. Bounding balls use triangle inequality
// on WGS84, rather than projected bounding boxes.
class Feature {
    struct Arc { Point midpoint; double radius; std::unique_ptr<ellipsoid::Boundary> oracle; };
    struct Node { Point centre; double radius; int left=-1,right=-1,arc=-1; };
    ellipsoid::WGS84 earth;
    std::vector<Arc> arcs;
    std::vector<Node> nodes;
    int build(std::vector<int>& ids,int first,int last) {
        int me=int(nodes.size()); nodes.emplace_back();
        if(last-first==1) {
            const auto& a=arcs[ids[first]];
            nodes[me]={a.midpoint,a.radius,-1,-1,ids[first]}; return me;
        }
        // The sort only accelerates the search; correctness uses metric bounds.
        double minlon=180,maxlon=-180,minlat=90,maxlat=-90;
        for(int k=first;k<last;++k) {
            auto p=arcs[ids[k]].midpoint;
            minlon=std::min(minlon,p.longitude);maxlon=std::max(maxlon,p.longitude);
            minlat=std::min(minlat,p.latitude);maxlat=std::max(maxlat,p.latitude);
        }
        bool longitude=maxlon-minlon>maxlat-minlat; int middle=(first+last)/2;
        std::nth_element(ids.begin()+first,ids.begin()+middle,ids.begin()+last,[&](int a,int b){
            return longitude?arcs[a].midpoint.longitude<arcs[b].midpoint.longitude:
                             arcs[a].midpoint.latitude<arcs[b].midpoint.latitude;});
        Point c=arcs[ids[middle]].midpoint; double rad=0;
        for(int k=first;k<last;++k) rad=std::max(rad,earth.inverse(c,arcs[ids[k]].midpoint).distance_m+arcs[ids[k]].radius);
        int left=build(ids,first,middle),right=build(ids,middle,last);
        nodes[me]={c,rad+g_allowance,left,right,-1}; return me;
    }
public:
    explicit Feature(const std::vector<ellipsoid::Segment>& segments) {
        if(segments.empty())throw std::invalid_argument("Empty source geometry");
        for(auto s:segments) {
            auto inv=earth.inverse(s.a,s.b);
            Point mid=earth.direct(s.a,inv.azimuth1_deg,inv.distance_m/2);
            arcs.push_back({mid,inv.distance_m/2+g_allowance,
                std::make_unique<ellipsoid::Boundary>(std::vector<ellipsoid::Segment>{s})});
        }
        std::vector<int> ids(arcs.size());std::iota(ids.begin(),ids.end(),0);build(ids,0,int(ids.size()));
    }
    Bounds distance(Point q,double tolerance=.001) const {
        using Item=std::pair<double,int>;
        std::priority_queue<Item,std::vector<Item>,std::greater<Item>> pending;
        auto lower=[&](int n){return std::max(0.,earth.inverse(q,nodes[n].centre).distance_m-nodes[n].radius-g_allowance);};
        pending.push({lower(0),0});double hi=INFINITY,lo=INFINITY;
        while(!pending.empty()) {
            auto [l,n]=pending.top();pending.pop();
            if(l>hi)continue;
            const auto& node=nodes[n];
            if(node.arc>=0) {
                ellipsoid::Options options;options.tolerance_m=std::max(.001,tolerance);
                auto d=arcs[node.arc].oracle->distance(q,options);
                if(!d.converged)throw std::runtime_error("Segment distance budget exhausted");
                hi=std::min(hi,d.upper_m);lo=std::min(lo,d.lower_m);
            } else {
                pending.push({lower(node.left),node.left});pending.push({lower(node.right),node.right});
            }
        }
        if(!std::isfinite(hi)||lo>hi)throw std::runtime_error("Invalid feature distance interval");
        return {lo,hi};
    }
    Bounds bounds(const Patch& p)const {
        double rho=radius(p);auto d=distance(geographic(centre(p)),std::max(.001,rho*.01));
        return {std::max(0.,d.lower-rho),d.upper};
    }
};
enum class Status { intersects, outside, unresolved };
struct CellResult {Status status; double lower,upper; unsigned evaluations;};
inline CellResult classify(const Feature& source,const Patch& cell,double distance,
                           double tolerance=.01,unsigned budget=4096) {
    struct Entry {Patch p;double lower;bool operator<(const Entry&b)const{return lower>b.lower;}};
    std::priority_queue<Entry> queue;
    auto b=source.bounds(cell);queue.push({cell,b.lower});double hi=b.upper;unsigned evaluated=1;
    for(;;) {
        double lo=queue.top().lower;
        if(hi<=distance)return {Status::intersects,lo,hi,evaluated};
        if(lo>distance)return {Status::outside,lo,hi,evaluated};
        if(hi-lo<=tolerance||evaluated+4>budget)return {Status::unresolved,lo,hi,evaluated};
        auto top=queue.top();queue.pop();
        for(auto child:split(top.p)) {
            auto cb=source.bounds(child);hi=std::min(hi,cb.upper);
            queue.push({child,std::max(top.lower,cb.lower)});++evaluated;
        }
    }
}
inline std::vector<bafe::CellIndex> cover(const Feature& source,unsigned level,
                                        double distance,std::uint64_t budget) {
    std::vector<bafe::CellIndex> pending,result;
    for(unsigned f=0;f<60;++f)pending.push_back(bafe::root(f));
    std::uint64_t examined=0;
    while(!pending.empty()) {
        auto id=pending.back();pending.pop_back();auto r=bafe::level(id);
        if(++examined>budget*64)throw std::runtime_error("Hierarchy node budget exhausted; no complete result");
        if(source.bounds(patch(id,r)).lower>distance)continue;
        if(r==level) {
            if(result.size()>=budget)throw std::runtime_error("Cell capacity exhausted; no complete result");
            result.push_back(id);
        } else for(unsigned d=0;d<4;++d)pending.push_back(bafe::child(id,d));
    }
    std::sort(result.begin(),result.end());return result;
}
} // namespace coverage
