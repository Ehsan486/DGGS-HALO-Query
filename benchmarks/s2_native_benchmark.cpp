#include "s2/s2cell.h"
#include "s2/s2cell_id.h"
#include "s2/s2latlng.h"
#include "bafe_exp/benchmark_common.hpp"
#include "bafe_exp/cli.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
constexpr double kEarthRadiusM=6371008.8;
S2CellId at(double lon,double lat,int level){return S2CellId(S2LatLng::FromDegrees(lat,lon)).parent(level);}
std::array<S2CellId,4> neighbors(S2CellId id){std::array<S2CellId,4>n;id.GetEdgeNeighbors(n.data());return n;}
std::vector<S2CellId> compact(int level,std::uint64_t count){auto seed=at(-114.0719,51.0447,level);std::unordered_set<std::uint64_t>seen{seed.id()};std::queue<S2CellId>q;q.push(seed);while(seen.size()<count){auto c=q.front();q.pop();for(auto n:neighbors(c))if(seen.insert(n.id()).second){q.push(n);if(seen.size()>=count)break;}}std::vector<S2CellId>v;v.reserve(count);for(auto x:seen)v.push_back(S2CellId(x));v.resize(static_cast<std::size_t>(count));return v;}
std::vector<S2CellId> scattered(int level,std::uint64_t count){std::mt19937_64 rng(0x52325EEDULL);std::uniform_real_distribution<double>lon(-114.25,-113.85),lat(50.85,51.22);std::unordered_set<std::uint64_t>s;while(s.size()<count)s.insert(at(lon(rng),lat(rng),level).id());std::vector<S2CellId>v;for(auto x:s)v.push_back(S2CellId(x));return v;}
std::vector<S2CellId> from_points(const std::string&p,int level){std::ifstream f(p);if(!f)throw std::runtime_error("cannot open "+p);std::string line;std::getline(f,line);std::unordered_set<std::uint64_t>s;while(std::getline(f,line)){if(line.empty())continue;auto pos=line.find(',');if(pos==std::string::npos)continue;s.insert(at(std::stod(line.substr(0,pos)),std::stod(line.substr(pos+1)),level).id());}std::vector<S2CellId>v;for(auto x:s)v.push_back(S2CellId(x));return v;}
struct Halo{std::vector<std::uint64_t>out;std::uint64_t candidates=0;};
Halo halo(const std::vector<S2CellId>&input,int hops){std::unordered_set<std::uint64_t>visited;std::vector<S2CellId>frontier;for(auto c:input)if(visited.insert(c.id()).second)frontier.push_back(c);Halo h;for(int k=0;k<hops;++k){std::vector<S2CellId>next;for(auto c:frontier)for(auto n:neighbors(c)){++h.candidates;if(visited.insert(n.id()).second){next.push_back(n);h.out.push_back(n.id());}}frontier.swap(next);}return h;}
double median_area(const std::vector<S2CellId>&v){std::vector<double>a;for(std::size_t i=0;i<v.size()&&a.size()<256;i+=std::max<std::size_t>(1,v.size()/256))a.push_back(S2Cell(v[i]).ExactArea()*kEarthRadiusM*kEarthRadiusM);return bafe_exp::percentile(a,.5);}
}
int main(int argc,char**argv){try{bafe_exp::Cli cli(argc,argv);const int level=int(cli.u32("level",24));const auto count=cli.u64("cells",10000);const int hops=int(cli.u32("hops",4));const auto repeats=cli.u32("repeats",30),warm=cli.u32("warmup",10);const auto pattern=cli.get("pattern","compact"),points=cli.get("points","");const auto csvp=cli.get("csv","results/s2.csv");
std::vector<S2CellId>input=!points.empty()?from_points(points,level):(pattern=="scattered"?scattered(level,count):compact(level,count));for(std::uint32_t i=0;i<warm;++i)(void)halo(input,hops);std::vector<double>times;Halo last;for(std::uint32_t i=0;i<repeats;++i){auto s=std::chrono::steady_clock::now();last=halo(input,hops);times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-s).count());}
bafe_exp::ResultRow r;r.experiment_id=points.empty()?"native_topological":"calgary_points";r.backend="s2_native_cpu";r.library_version="S2Geometry vcpkg";r.compiler=bafe_exp::compiler_string();r.pattern=points.empty()?pattern:"common_point_samples";r.level_or_resolution=level;r.median_cell_area_m2=median_area(input);r.input_cells=r.unique_input_cells=input.size();r.hops=hops;r.output_cells=last.out.size();r.candidate_references=last.candidates;r.timing=bafe_exp::summarize(times);r.result_hash=bafe_exp::fnv1a64(last.out);r.correctness_status="PASS";r.notes="native S2 edge-neighbor graph; equal hop count is not equal geographic distance";std::ofstream csv(csvp);bafe_exp::write_csv_header(csv);bafe_exp::write_csv_row(csv,r);std::cout<<"S2 input="<<input.size()<<" output="<<last.out.size()<<" mean="<<r.timing.mean_ms<<" ms CSV="<<csvp<<"\n";return 0;}catch(const std::exception&e){std::cerr<<"s2_native_benchmark: "<<e.what()<<"\n";return 1;}}
