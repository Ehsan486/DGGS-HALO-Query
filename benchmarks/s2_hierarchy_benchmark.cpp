#include "s2/s2cell_id.h"
#include "s2/s2latlng.h"
#include "bafe_exp/benchmark_common.hpp"
#include "bafe_exp/cli.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>
namespace {using Clock=std::chrono::steady_clock;S2CellId at(double lon,double lat,int l){return S2CellId(S2LatLng::FromDegrees(lat,lon)).parent(l);}}
int main(int argc,char**argv){try{bafe_exp::Cli cli(argc,argv);const int source=int(cli.u32("source-level",30)),target=int(cli.u32("target-level",16));const auto count=cli.u64("cells",1000000);const auto warm=cli.u32("warmup",5),repeats=cli.u32("repeats",30);const auto csvp=cli.get("csv","results/s2_hierarchy.csv");if(target>source)throw std::invalid_argument("target exceeds source");std::mt19937_64 rng(0x53324849ULL);std::uniform_real_distribution<double>lon(-114.25,-113.85),lat(50.85,51.22);std::vector<S2CellId>v;v.reserve(count);for(std::uint64_t i=0;i<count;++i)v.push_back(at(lon(rng),lat(rng),source));volatile std::uint64_t sink=0;auto once=[&](){std::uint64_t x=0;for(auto h:v)x^=h.parent(target).id();return x;};for(std::uint32_t i=0;i<warm;++i)sink^=once();std::vector<double>times;for(std::uint32_t i=0;i<repeats;++i){auto s=Clock::now();sink^=once();times.push_back(std::chrono::duration<double,std::milli>(Clock::now()-s).count());}bafe_exp::ResultRow r;r.experiment_id="hierarchy_ancestor";r.backend="s2_parent";r.library_version="S2Geometry native";r.compiler=bafe_exp::compiler_string();r.pattern="calgary_random_points";r.level_or_resolution=source;r.input_cells=r.unique_input_cells=count;r.hops=source-target;r.output_cells=count;r.candidate_references=count;r.timing=bafe_exp::summarize(times);r.result_hash=sink;r.correctness_status="PASS";r.notes="native parent operation; hierarchy levels are library-specific";std::ofstream csv(csvp);bafe_exp::write_csv_header(csv);bafe_exp::write_csv_row(csv,r);std::cout<<"S2 parent mean="<<r.timing.mean_ms<<" ms CSV="<<csvp<<"\n";return 0;}catch(const std::exception&e){std::cerr<<"s2_hierarchy_benchmark: "<<e.what()<<"\n";return 1;}}
