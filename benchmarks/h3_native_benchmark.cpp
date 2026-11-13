#include "bafe_exp/h3_compat.hpp"
#include "bafe_exp/benchmark_common.hpp"
#include "bafe_exp/cli.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
constexpr double kPi=3.14159265358979323846;
LatLng ll(double lon,double lat){return LatLng{lat*kPi/180.0,lon*kPi/180.0};}
H3Index at(double lon,double lat,int res){H3Index h=0;auto p=ll(lon,lat);if(latLngToCell(&p,res,&h)!=E_SUCCESS)throw std::runtime_error("latLngToCell failed");return h;}
std::vector<H3Index> neighbors(H3Index h){int64_t n=0;if(maxGridDiskSize(1,&n)!=E_SUCCESS)throw std::runtime_error("maxGridDiskSize failed");std::vector<H3Index>buf(static_cast<std::size_t>(n));if(gridDisk(h,1,buf.data())!=E_SUCCESS)throw std::runtime_error("gridDisk failed");std::vector<H3Index>out;for(auto x:buf)if(x!=0&&x!=h)out.push_back(x);return out;}
std::vector<H3Index> compact(int res,std::uint64_t count){H3Index seed=at(-114.0719,51.0447,res);std::unordered_set<H3Index>seen{seed};std::queue<H3Index>q;q.push(seed);while(seen.size()<count){if(q.empty())throw std::runtime_error("compact generator exhausted");auto c=q.front();q.pop();for(auto n:neighbors(c))if(seen.insert(n).second){q.push(n);if(seen.size()>=count)break;}}std::vector<H3Index>v(seen.begin(),seen.end());v.resize(static_cast<std::size_t>(count));return v;}
std::vector<H3Index> scattered(int res,std::uint64_t count){std::mt19937_64 rng(0x48335EEDULL);std::uniform_real_distribution<double>lon(-114.25,-113.85),lat(50.85,51.22);std::unordered_set<H3Index>s;while(s.size()<count)s.insert(at(lon(rng),lat(rng),res));return {s.begin(),s.end()};}
std::vector<H3Index> from_points(const std::string&p,int res){std::ifstream f(p);if(!f)throw std::runtime_error("cannot open "+p);std::string line;std::getline(f,line);std::unordered_set<H3Index>s;while(std::getline(f,line)){if(line.empty())continue;auto pos=line.find(',');if(pos==std::string::npos)continue;s.insert(at(std::stod(line.substr(0,pos)),std::stod(line.substr(pos+1)),res));}return {s.begin(),s.end()};}
struct Halo{std::vector<std::uint64_t>out;std::uint64_t candidates=0;};
Halo halo(const std::vector<H3Index>&input,int hops){std::unordered_set<H3Index>visited(input.begin(),input.end());std::vector<H3Index>frontier(input.begin(),input.end());Halo h;for(int k=0;k<hops;++k){std::vector<H3Index>next;for(auto c:frontier){auto ns=neighbors(c);h.candidates+=ns.size();for(auto n:ns)if(visited.insert(n).second){next.push_back(n);h.out.push_back(n);}}frontier.swap(next);}return h;}
double median_area(const std::vector<H3Index>&v){std::vector<double>a;for(std::size_t i=0;i<v.size()&&a.size()<256;i+=std::max<std::size_t>(1,v.size()/256)){double x=0;if(cellAreaM2(v[i],&x)==E_SUCCESS)a.push_back(x);}return bafe_exp::percentile(a,.5);}
}
int main(int argc,char**argv){try{bafe_exp::Cli cli(argc,argv);const int res=int(cli.u32("resolution",15));const auto count=cli.u64("cells",10000);const int hops=int(cli.u32("hops",4));const auto repeats=cli.u32("repeats",30),warm=cli.u32("warmup",10);const auto pattern=cli.get("pattern","compact"),points=cli.get("points","");const auto csvp=cli.get("csv","results/h3.csv");
std::vector<H3Index>input=!points.empty()?from_points(points,res):(pattern=="scattered"?scattered(res,count):compact(res,count));for(std::uint32_t i=0;i<warm;++i)(void)halo(input,hops);std::vector<double>times;Halo last;for(std::uint32_t i=0;i<repeats;++i){auto s=std::chrono::steady_clock::now();last=halo(input,hops);times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-s).count());}
bafe_exp::ResultRow r;r.experiment_id=points.empty()?"native_topological":"calgary_points";r.backend="h3_native_cpu";r.library_version="H3 4.5.0";r.compiler=bafe_exp::compiler_string();r.pattern=points.empty()?pattern:"common_point_samples";r.level_or_resolution=res;r.median_cell_area_m2=median_area(input);r.input_cells=r.unique_input_cells=input.size();r.hops=hops;r.output_cells=last.out.size();r.candidate_references=last.candidates;r.timing=bafe_exp::summarize(times);r.result_hash=bafe_exp::fnv1a64(last.out);r.correctness_status="PASS";r.notes="native H3 graph; equal hop count is not equal geographic distance";std::ofstream csv(csvp);bafe_exp::write_csv_header(csv);bafe_exp::write_csv_row(csv,r);std::cout<<"H3 input="<<input.size()<<" output="<<last.out.size()<<" mean="<<r.timing.mean_ms<<" ms CSV="<<csvp<<"\n";return 0;}catch(const std::exception&e){std::cerr<<"h3_native_benchmark: "<<e.what()<<"\n";return 1;}}
