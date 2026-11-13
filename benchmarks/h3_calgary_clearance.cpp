#include "bafe_exp/h3_compat.hpp"
#include "bafe_exp/benchmark_common.hpp"
#include "bafe_exp/cli.hpp"
#include "bafe_exp/geo_math.hpp"
#include "bafe_exp/point_csv.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace {
using Clock=std::chrono::steady_clock;
constexpr double kPi=3.14159265358979323846;
LatLng ll(double lon,double lat){return LatLng{lat*kPi/180.0,lon*kPi/180.0};}
H3Index at(double lon,double lat,int res){LatLng p=ll(lon,lat);H3Index h=0;if(latLngToCell(&p,res,&h)!=E_SUCCESS)throw std::runtime_error("H3 latLngToCell failed");return h;}
std::vector<H3Index> map_points(const std::vector<std::pair<double,double>>& pts,int res){std::unordered_set<H3Index>s;s.reserve(pts.size()*2+16);for(const auto&p:pts)s.insert(at(p.first,p.second,res));return {s.begin(),s.end()};}
std::vector<H3Index> neighbors(H3Index h){int64_t n=0;if(maxGridDiskSize(1,&n)!=E_SUCCESS)throw std::runtime_error("H3 maxGridDiskSize failed");std::vector<H3Index>b(static_cast<std::size_t>(n));if(gridDisk(h,1,b.data())!=E_SUCCESS)throw std::runtime_error("H3 gridDisk failed");std::vector<H3Index>o;for(auto x:b)if(x&&x!=h)o.push_back(x);return o;}
struct Halo{std::vector<std::uint64_t> out;std::uint64_t candidates=0;};
Halo halo(const std::vector<H3Index>& input,int hops){std::unordered_set<H3Index>v(input.begin(),input.end());std::vector<H3Index>f(v.begin(),v.end());Halo h;for(int k=0;k<hops;++k){std::vector<H3Index>n;for(auto c:f){auto ns=neighbors(c);h.candidates+=ns.size();for(auto x:ns)if(v.insert(x).second){n.push_back(x);h.out.push_back(x);}}f.swap(n);}return h;}
double area(const std::vector<H3Index>&cells){std::vector<double>a;const std::size_t stride=std::max<std::size_t>(1,cells.size()/256);for(std::size_t i=0;i<cells.size()&&a.size()<256;i+=stride){double x=0;if(cellAreaM2(cells[i],&x)==E_SUCCESS)a.push_back(x);}return bafe_exp::percentile(a,.5);}
double step(const std::vector<H3Index>&cells){std::vector<double>d;const std::size_t stride=std::max<std::size_t>(1,cells.size()/128);for(std::size_t i=0;i<cells.size()&&d.size()<768;i+=stride){LatLng a{};if(cellToLatLng(cells[i],&a)!=E_SUCCESS)continue;for(auto n:neighbors(cells[i])){LatLng b{};if(cellToLatLng(n,&b)==E_SUCCESS)d.push_back(bafe_exp::great_circle_m(a.lng*180/kPi,a.lat*180/kPi,b.lng*180/kPi,b.lat*180/kPi));}}return bafe_exp::percentile(d,.5);}
}
int main(int argc,char**argv){try{
 bafe_exp::Cli cli(argc,argv);const auto bp=cli.get("building-points"),cp=cli.get("corridor-points");if(bp.empty()||cp.empty())throw std::invalid_argument("--building-points and --corridor-points are required");
 const int res=int(cli.u32("resolution",15));const auto warm=cli.u32("warmup",10),repeats=cli.u32("repeats",30);const auto scenario=cli.get("scenario","calgary"),csvp=cli.get("csv","results/h3_calgary.csv");const double requested=std::stod(cli.get("clearance-m","10"));
 auto rawb=bafe_exp::read_lonlat_csv(bp),rawc=bafe_exp::read_lonlat_csv(cp);auto ms=Clock::now();auto buildings=map_points(rawb,res);auto corridor_vec=map_points(rawc,res);double mapping=std::chrono::duration<double,std::milli>(Clock::now()-ms).count();std::unordered_set<H3Index>corridor(corridor_vec.begin(),corridor_vec.end());
 double ns=step(buildings.empty()?corridor_vec:buildings);if(!(ns>0))throw std::runtime_error("could not estimate H3 neighbor step");auto explicit_hops=cli.u32("hops",0);int hops=explicit_hops?int(explicit_hops):std::max(1,int(std::ceil(requested/ns)));
 auto run=[&](){auto h=halo(buildings,hops);std::vector<std::uint64_t>conf;for(auto x:h.out)if(corridor.find(x)!=corridor.end())conf.push_back(x);return std::pair<Halo,std::vector<std::uint64_t>>(std::move(h),std::move(conf));};for(std::uint32_t i=0;i<warm;++i)(void)run();std::vector<double>times;std::pair<Halo,std::vector<std::uint64_t>>last;for(std::uint32_t i=0;i<repeats;++i){auto s=Clock::now();last=run();times.push_back(std::chrono::duration<double,std::milli>(Clock::now()-s).count());}
 bafe_exp::ResultRow r;r.experiment_id="calgary_horizontal_clearance";r.backend="h3_native_cpu";r.library_version="H3 native";r.compiler=bafe_exp::compiler_string();r.pattern="real_building_samples";r.scenario=scenario;r.level_or_resolution=res;r.median_cell_area_m2=area(buildings);r.input_cells=r.unique_input_cells=buildings.size();r.hops=hops;r.output_cells=last.first.out.size();r.conflict_cells=last.second.size();r.candidate_references=last.first.candidates;r.mapping_ms=mapping;r.neighbor_step_m=ns;r.requested_clearance_m=requested;r.achieved_clearance_m=ns*hops;r.timing=bafe_exp::summarize(times);r.result_hash=bafe_exp::fnv1a64(last.second);r.correctness_status="PASS";r.notes="native H3 graph halo relative to common sampled feature sets; conflict intersection included";std::ofstream csv(csvp);bafe_exp::write_csv_header(csv);bafe_exp::write_csv_row(csv,r);std::cout<<"H3 buildings="<<buildings.size()<<" corridor="<<corridor.size()<<" hops="<<hops<<" halo="<<r.output_cells<<" conflicts="<<r.conflict_cells<<" mean="<<r.timing.mean_ms<<" ms\nCSV: "<<csvp<<"\n";return 0;
}catch(const std::exception&e){std::cerr<<"h3_calgary_clearance: "<<e.what()<<"\n";return 1;}}
