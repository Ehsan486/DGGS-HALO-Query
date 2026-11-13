#include "bafe/index.hpp"
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
using bafe::detail::Cell;

inline void rhombic_neighbors(const Cell& c,std::uint32_t level,Cell out[3],std::uint32_t& external,std::uint32_t& internal){
    const std::uint32_t n=1U<<level;
    if(c.orientation==1U){out[0]={c.face,c.i,c.j,0};out[1]={c.face,c.i+1,c.j,0};out[2]={c.face,c.i,c.j+1,0};return;}
    if(c.i+c.j+1U<n) out[0]={c.face,c.i,c.j,1};
    else {out[0]={c.face^1U,c.j,c.i,0};++internal;}
    if(c.j>0) out[1]={c.face,c.i,c.j-1,1}; else {out[1]=bafe::detail::cross_edge_host(c,2U,c.i,n);++external;}
    if(c.i>0) out[2]={c.face,c.i-1,c.j,1}; else {out[2]=bafe::detail::cross_edge_host(c,1U,c.j,n);++external;}
}

struct Output {std::vector<std::uint64_t> cells;std::uint64_t candidates=0,seams=0,internal=0;double ms=0;};

Output bfs(const std::vector<bafe::CellIndex>& input,std::uint32_t level,std::uint32_t hops,bool rhombic){
    const auto start=std::chrono::steady_clock::now();
    std::unordered_set<std::uint64_t> visited; visited.reserve(input.size()*4+1024);
    std::vector<std::uint64_t> frontier;
    for(auto id:input){auto key=bafe::detail::encode_coord(bafe::detail::path_to_cell_scalar(id,level));if(visited.insert(key).second)frontier.push_back(key);}
    Output o;
    for(std::uint32_t h=0;h<hops;++h){
        std::vector<std::uint64_t> next; next.reserve(frontier.size()*2+16);
        for(auto key:frontier){Cell ns[3];std::uint32_t ext=0,in=0;auto c=bafe::detail::decode_coord(key);
            if(rhombic)rhombic_neighbors(c,level,ns,ext,in);else bafe::detail::neighbors_host(c,level,ns,&ext);
            o.candidates+=3;o.seams+=ext;o.internal+=in;
            for(auto n:ns){auto nk=bafe::detail::encode_coord(n);if(visited.insert(nk).second){next.push_back(nk);o.cells.push_back(bafe::detail::cell_to_path_scalar(n,level));}}
        }
        frontier.swap(next);
    }
    o.ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    return o;
}
}

int main(int argc,char**argv){
 try{
  bafe_exp::Cli cli(argc,argv); const auto level=cli.u32("level",28),hops=cli.u32("hops",4);
  const auto count=cli.u64("cells",100000); const auto repeats=cli.u32("repeats",30);const auto pattern=cli.get("pattern","seam");
  const auto csv_path=cli.get("csv","results/bafe_rhombus_cpu.csv");
  auto input=pattern=="scattered"?bafe_exp::bafe_scattered(level,count):bafe_exp::bafe_compact(level,count,pattern=="seam");
  std::vector<double> tri_t,rho_t;Output tri,rho;
  for(std::uint32_t r=0;r<repeats;++r){tri=bfs(input,level,hops,false);rho=bfs(input,level,hops,true);tri_t.push_back(tri.ms);rho_t.push_back(rho.ms);}
  auto a=tri.cells,b=rho.cells;std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());if(a!=b)throw std::runtime_error("triangle/rhombic output mismatch");
  std::ofstream csv(csv_path);bafe_exp::write_csv_header(csv);
  for(int m=0;m<2;++m){const auto&o=m==0?tri:rho;bafe_exp::ResultRow row;row.experiment_id="surface_chart_ablation";row.backend=m==0?"triangle_atlas_cpu":"rhombic_macrochart_cpu";
    row.library_version=bafe::kLibraryVersion;row.compiler=bafe_exp::compiler_string();row.pattern=pattern;row.level_or_resolution=level;row.median_cell_area_m2=bafe::geo::global_average_cell_area_m2(level);
    row.input_cells=row.unique_input_cells=input.size();row.hops=hops;row.output_cells=o.cells.size();row.candidate_references=o.candidates;row.seam_crossings=o.seams;
    row.timing=bafe_exp::summarize(m==0?tri_t:rho_t);row.result_hash=bafe_exp::fnv1a64(o.cells);row.correctness_status="PASS";
    row.notes=m==0?"60 natural triangular charts":"30 paired execution macrocharts; internal paired-edge crossings="+std::to_string(o.internal);
    bafe_exp::write_csv_row(csv,row);}
  std::cout<<"PASS: identical output; triangle="<<bafe_exp::summarize(tri_t).mean_ms<<" ms rhombic="<<bafe_exp::summarize(rho_t).mean_ms<<" ms\nCSV: "<<csv_path<<"\n";return 0;
 }catch(const std::exception&e){std::cerr<<"bafe_rhombus_cpu_ablation: "<<e.what()<<"\n";return 1;}
}
