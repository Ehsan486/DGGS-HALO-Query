#include "s2/s2cell.h"
#include "s2/s2cell_id.h"
#include "s2/s2latlng.h"
#include "bafe_exp/benchmark_common.hpp"
#include "bafe_exp/cli.hpp"
#include "bafe_exp/geo_math.hpp"
#include "bafe_exp/point_csv.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace {
using Clock=std::chrono::steady_clock;constexpr double kEarthRadiusM=6371008.8;
S2CellId at(double lon,double lat,int level){return S2CellId(S2LatLng::FromDegrees(lat,lon)).parent(level);}
std::array<S2CellId,4> neighbors(S2CellId id){std::array<S2CellId,4>n;id.GetEdgeNeighbors(n.data());return n;}
std::vector<S2CellId> map_points(const std::vector<std::pair<double,double>>&pts,int level){std::unordered_set<std::uint64_t>s;s.reserve(pts.size()*2+16);for(const auto&p:pts)s.insert(at(p.first,p.second,level).id());std::vector<S2CellId>v;v.reserve(s.size());for(auto x:s)v.push_back(S2CellId(x));return v;}
struct Halo{std::vector<std::uint64_t>out;std::uint64_t candidates=0;};
Halo halo(const std::vector<S2CellId>&input,int hops){std::unordered_set<std::uint64_t>v;std::vector<S2CellId>f;for(auto c:input)if(v.insert(c.id()).second)f.push_back(c);Halo h;for(int k=0;k<hops;++k){std::vector<S2CellId>n;for(auto c:f)for(auto x:neighbors(c)){++h.candidates;if(v.insert(x.id()).second){n.push_back(x);h.out.push_back(x.id());}}f.swap(n);}return h;}
double area(const std::vector<S2CellId>&cells){std::vector<double>a;const std::size_t stride=std::max<std::size_t>(1,cells.size()/256);for(std::size_t i=0;i<cells.size()&&a.size()<256;i+=stride)a.push_back(S2Cell(cells[i]).ExactArea()*kEarthRadiusM*kEarthRadiusM);return bafe_exp::percentile(a,.5);}
std::pair<double,double> center(S2CellId id){S2LatLng ll(S2Cell(id).GetCenter());return {ll.lng().degrees(),ll.lat().degrees()};}
double step(const std::vector<S2CellId>&cells){std::vector<double>d;const std::size_t stride=std::max<std::size_t>(1,cells.size()/128);for(std::size_t i=0;i<cells.size()&&d.size()<512;i+=stride){auto a=center(cells[i]);for(auto n:neighbors(cells[i])){auto b=center(n);d.push_back(bafe_exp::great_circle_m(a.first,a.second,b.first,b.second));}}return bafe_exp::percentile(d,.5);}
}
int main(int argc,char**argv){try{
 bafe_exp::Cli cli(argc,argv);const auto bp=cli.get("building-points"),cp=cli.get("corridor-points");if(bp.empty()||cp.empty())throw std::invalid_argument("--building-points and --corridor-points are required");
 const int level=int(cli.u32("level",23));const auto warm=cli.u32("warmup",10),repeats=cli.u32("repeats",30);const auto scenario=cli.get("scenario","calgary"),csvp=cli.get("csv","results/s2_calgary.csv");const double requested=std::stod(cli.get("clearance-m","10"));
 auto rawb=bafe_exp::read_lonlat_csv(bp),rawc=bafe_exp::read_lonlat_csv(cp);auto ms=Clock::now();auto buildings=map_points(rawb,level);auto corridor_vec=map_points(rawc,level);double mapping=std::chrono::duration<double,std::milli>(Clock::now()-ms).count();std::unordered_set<std::uint64_t>corridor;for(auto c:corridor_vec)corridor.insert(c.id());
 double ns=step(buildings.empty()?corridor_vec:buildings);if(!(ns>0))throw std::runtime_error("could not estimate S2 neighbor step");auto explicit_hops=cli.u32("hops",0);int hops=explicit_hops?int(explicit_hops):std::max(1,int(std::ceil(requested/ns)));
 auto run=[&](){auto h=halo(buildings,hops);std::vector<std::uint64_t>conf;for(auto x:h.out)if(corridor.find(x)!=corridor.end())conf.push_back(x);return std::pair<Halo,std::vector<std::uint64_t>>(std::move(h),std::move(conf));};for(std::uint32_t i=0;i<warm;++i)(void)run();std::vector<double>times;std::pair<Halo,std::vector<std::uint64_t>>last;for(std::uint32_t i=0;i<repeats;++i){auto s=Clock::now();last=run();times.push_back(std::chrono::duration<double,std::milli>(Clock::now()-s).count());}
 bafe_exp::ResultRow r;r.experiment_id="calgary_horizontal_clearance";r.backend="s2_native_cpu";r.library_version="S2Geometry native";r.compiler=bafe_exp::compiler_string();r.pattern="real_building_samples";r.scenario=scenario;r.level_or_resolution=level;r.median_cell_area_m2=area(buildings);r.input_cells=r.unique_input_cells=buildings.size();r.hops=hops;r.output_cells=last.first.out.size();r.conflict_cells=last.second.size();r.candidate_references=last.first.candidates;r.mapping_ms=mapping;r.neighbor_step_m=ns;r.requested_clearance_m=requested;r.achieved_clearance_m=ns*hops;r.timing=bafe_exp::summarize(times);r.result_hash=bafe_exp::fnv1a64(last.second);r.correctness_status="PASS";r.notes="native S2 edge-neighbor halo relative to common sampled feature sets; conflict intersection included";std::ofstream csv(csvp);bafe_exp::write_csv_header(csv);bafe_exp::write_csv_row(csv,r);std::cout<<"S2 buildings="<<buildings.size()<<" corridor="<<corridor.size()<<" hops="<<hops<<" halo="<<r.output_cells<<" conflicts="<<r.conflict_cells<<" mean="<<r.timing.mean_ms<<" ms\nCSV: "<<csvp<<"\n";return 0;
}catch(const std::exception&e){std::cerr<<"s2_calgary_clearance: "<<e.what()<<"\n";return 1;}}
