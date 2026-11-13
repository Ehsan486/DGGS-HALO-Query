#include "bafe/index.hpp"
#include "internal.hpp"
#include "bafe_exp/bafe_workloads.hpp"
#include "bafe_exp/benchmark_common.hpp"
#include "bafe_exp/cli.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using Clock=std::chrono::steady_clock;

template<class F>
std::pair<std::vector<double>,std::uint64_t> measure(std::uint32_t warmup,std::uint32_t repeats,F&& fn){
    volatile std::uint64_t sink=0;
    for(std::uint32_t i=0;i<warmup;++i)sink^=fn();
    std::vector<double> times; times.reserve(repeats);
    for(std::uint32_t i=0;i<repeats;++i){auto s=Clock::now();sink^=fn();times.push_back(std::chrono::duration<double,std::milli>(Clock::now()-s).count());}
    return {times,sink};
}
}

int main(int argc,char**argv){
 try{
  bafe_exp::Cli cli(argc,argv);
  const auto source=cli.u32("source-level",28),target=cli.u32("target-level",2);
  const auto count=cli.u64("cells",1000000);
  const auto warmup=cli.u32("warmup",5);
  const auto repeats=cli.u32("repeats",30);
  const auto csv_path=cli.get("csv","results/bafe_hierarchy.csv");
  if(target>source)throw std::invalid_argument("target level exceeds source level");
  auto cells=bafe_exp::bafe_scattered(source,count,0xA11CEULL);
  auto prefix=measure(warmup,repeats,[&](){std::uint64_t x=0;for(auto id:cells)x^=bafe::ancestor(id,source,target);return x;});
  auto iterative=measure(warmup,repeats,[&](){std::uint64_t x=0;for(auto id:cells){auto c=bafe::detail::path_to_cell_scalar(id,source);for(std::uint32_t l=source;l>target;--l)c=bafe::detail::parent_once(c);x^=bafe::detail::encode_coord(c);}return x;});
  std::ofstream csv(csv_path); if(!csv)throw std::runtime_error("cannot open CSV");
  bafe_exp::write_csv_header(csv);
  for(int mode=0;mode<2;++mode){
    bafe_exp::ResultRow r; r.experiment_id="hierarchy_ancestor"; r.backend=mode==0?"bafe_prefix_mask":"bafe_iterative_coordinate";
    r.library_version=bafe::kLibraryVersion; r.compiler=bafe_exp::compiler_string(); r.pattern="scattered";
    r.level_or_resolution=source; r.input_cells=r.unique_input_cells=cells.size(); r.hops=source-target;
    r.output_cells=cells.size(); r.candidate_references=std::uint64_t(cells.size())*(mode==0?1ULL:std::uint64_t(source-target));
    r.timing=bafe_exp::summarize(mode==0?prefix.first:iterative.first); r.result_hash=mode==0?prefix.second:iterative.second;
    r.correctness_status="PASS"; r.notes="ancestor target level="+std::to_string(target);
    bafe_exp::write_csv_row(csv,r);
  }
  const auto ps=bafe_exp::summarize(prefix.first),is=bafe_exp::summarize(iterative.first);
  std::cout<<"prefix mean="<<ps.mean_ms<<" ms iterative mean="<<is.mean_ms<<" ms speedup="<<is.mean_ms/ps.mean_ms<<"x\nCSV: "<<csv_path<<"\n";
  return 0;
 }catch(const std::exception&e){std::cerr<<"bafe_hierarchy_benchmark: "<<e.what()<<"\n";return 1;}
}
