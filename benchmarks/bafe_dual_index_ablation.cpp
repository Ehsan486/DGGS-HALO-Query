#include "bafe/bafe.hpp"
#include "internal.hpp"
#include "topology.hpp"
#include "bafe_exp/bafe_workloads.hpp"
#include "bafe_exp/benchmark_common.hpp"
#include "bafe_exp/cli.hpp"
#include "bafe_exp/geo.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace {
using Clock=std::chrono::steady_clock;
struct PrefixRun{std::vector<bafe::CellIndex>out;std::uint64_t candidates=0,seams=0;};
PrefixRun prefix_only(const std::vector<bafe::CellIndex>&input,std::uint32_t level,std::uint32_t hops){
    std::unordered_set<bafe::CellIndex>visited(input.begin(),input.end());std::vector<bafe::CellIndex>frontier(visited.begin(),visited.end());PrefixRun r;
    for(std::uint32_t h=0;h<hops;++h){std::vector<bafe::CellIndex>next;next.reserve(frontier.size()*2+16);r.candidates+=3ULL*frontier.size();for(auto id:frontier){auto c=bafe::detail::path_to_cell_scalar(id,level);bafe::detail::Cell ns[3];std::uint32_t seams=0;bafe::detail::neighbors_host(c,level,ns,&seams);r.seams+=seams;for(const auto&n:ns){auto p=bafe::detail::cell_to_path_scalar(n,level);if(visited.insert(p).second){next.push_back(p);r.out.push_back(p);}}}frontier.swap(next);}return r;
}
template<class F>std::pair<std::vector<double>,decltype(std::declval<F>()())> measure(std::uint32_t warm,std::uint32_t repeats,F fn){for(std::uint32_t i=0;i<warm;++i)(void)fn();std::vector<double>t;decltype(fn())last;for(std::uint32_t i=0;i<repeats;++i){auto s=Clock::now();last=fn();t.push_back(std::chrono::duration<double,std::milli>(Clock::now()-s).count());}return {t,std::move(last)};}
}
int main(int argc,char**argv){try{
 bafe_exp::Cli cli(argc,argv);const auto level=cli.u32("level",28),hops=cli.u32("hops",4);const auto count=cli.u64("cells",100000);const auto warm=cli.u32("warmup",5),repeats=cli.u32("repeats",30);const auto pattern=cli.get("pattern","compact"),csvp=cli.get("csv","results/dual_index_ablation.csv");
 auto input=pattern=="scattered"?bafe_exp::bafe_scattered(level,count):bafe_exp::bafe_compact(level,count,pattern=="seam");auto a=measure(warm,repeats,[&](){return prefix_only(input,level,hops);});auto cpu=bafe::make_cpu_engine();bafe::QueryOptions q{level,hops,0,false};auto b=measure(warm,repeats,[&](){return cpu->halo(input,q);});auto ao=a.second.out,bo=b.second.cells;std::sort(ao.begin(),ao.end());std::sort(bo.begin(),bo.end());if(ao!=bo)throw std::runtime_error("prefix-only and dual-index output mismatch");
 std::ofstream csv(csvp);if(!csv)throw std::runtime_error("cannot open "+csvp);bafe_exp::write_csv_header(csv);
 bafe_exp::ResultRow r1;r1.experiment_id="dual_index_ablation";r1.backend="cpu_prefix_only_decode_each_expansion";r1.library_version=bafe::kLibraryVersion;r1.compiler=bafe_exp::compiler_string();r1.pattern=pattern;r1.level_or_resolution=level;r1.median_cell_area_m2=bafe::geo::global_average_cell_area_m2(level);r1.input_cells=r1.unique_input_cells=input.size();r1.hops=hops;r1.output_cells=a.second.out.size();r1.candidate_references=a.second.candidates;r1.seam_crossings=a.second.seams;r1.timing=bafe_exp::summarize(a.first);r1.result_hash=bafe_exp::fnv1a64(a.second.out);r1.correctness_status="PASS";r1.notes="public prefix retained in frontier; decode and encode for every expanded candidate";bafe_exp::write_csv_row(csv,r1);
 bafe_exp::ResultRow r2;r2.experiment_id="dual_index_ablation";r2.backend="cpu_dual_index_persistent_coordinates";r2.library_version=bafe::kLibraryVersion;r2.compiler=bafe_exp::compiler_string();r2.pattern=pattern;r2.level_or_resolution=level;r2.median_cell_area_m2=r1.median_cell_area_m2;r2.input_cells=input.size();r2.unique_input_cells=b.second.metrics.unique_input_cells;r2.hops=hops;r2.output_cells=b.second.cells.size();r2.candidate_references=b.second.metrics.touched_neighbor_references;r2.seam_crossings=b.second.metrics.root_edge_crossings;r2.timing=bafe_exp::summarize(b.first);r2.result_hash=bafe_exp::fnv1a64(b.second.cells);r2.correctness_status="PASS";r2.notes="convert input once; retain fixed-level coordinate IDs through all hops";bafe_exp::write_csv_row(csv,r2);
 std::cout<<"PASS prefix="<<r1.timing.mean_ms<<" ms dual="<<r2.timing.mean_ms<<" ms speedup="<<r1.timing.mean_ms/r2.timing.mean_ms<<"x\nCSV: "<<csvp<<"\n";return 0;
}catch(const std::exception&e){std::cerr<<"bafe_dual_index_ablation: "<<e.what()<<"\n";return 1;}}
