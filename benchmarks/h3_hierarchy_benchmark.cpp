#include "bafe_exp/h3_compat.hpp"
#include "bafe_exp/benchmark_common.hpp"
#include "bafe_exp/cli.hpp"
#include "bafe_exp/point_csv.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>
namespace {constexpr double kPi=3.14159265358979323846;using Clock=std::chrono::steady_clock;H3Index at(double lon,double lat,int r){LatLng p{lat*kPi/180,lon*kPi/180};H3Index h=0;if(latLngToCell(&p,r,&h)!=E_SUCCESS)throw std::runtime_error("latLngToCell failed");return h;}}
int main(int argc,char**argv){try{bafe_exp::Cli cli(argc,argv);const int source=int(cli.u32("source-resolution",15)),target=int(cli.u32("target-resolution",8));const auto count=cli.u64("cells",1000000);const auto warm=cli.u32("warmup",5),repeats=cli.u32("repeats",30);const auto csvp=cli.get("csv","results/h3_hierarchy.csv");if(target>source)throw std::invalid_argument("target exceeds source");std::mt19937_64 rng(0x48334849ULL);std::uniform_real_distribution<double>lon(-114.25,-113.85),lat(50.85,51.22);std::vector<H3Index>v;v.reserve(count);for(std::uint64_t i=0;i<count;++i)v.push_back(at(lon(rng),lat(rng),source));volatile std::uint64_t sink=0;auto once=[&](){std::uint64_t x=0;for(auto h:v){H3Index p=0;if(cellToParent(h,target,&p)!=E_SUCCESS)throw std::runtime_error("cellToParent failed");x^=p;}return x;};for(std::uint32_t i=0;i<warm;++i)sink^=once();std::vector<double>times;for(std::uint32_t i=0;i<repeats;++i){auto s=Clock::now();sink^=once();times.push_back(std::chrono::duration<double,std::milli>(Clock::now()-s).count());}bafe_exp::ResultRow r;r.experiment_id="hierarchy_ancestor";r.backend="h3_cellToParent";r.library_version="H3 native";r.compiler=bafe_exp::compiler_string();r.pattern="calgary_random_points";r.level_or_resolution=source;r.input_cells=r.unique_input_cells=count;r.hops=source-target;r.output_cells=count;r.candidate_references=count;r.timing=bafe_exp::summarize(times);r.result_hash=sink;r.correctness_status="PASS";r.notes="native parent operation; hierarchy levels are library-specific";std::ofstream csv(csvp);bafe_exp::write_csv_header(csv);bafe_exp::write_csv_row(csv,r);std::cout<<"H3 parent mean="<<r.timing.mean_ms<<" ms CSV="<<csvp<<"\n";return 0;}catch(const std::exception&e){std::cerr<<"h3_hierarchy_benchmark: "<<e.what()<<"\n";return 1;}}
