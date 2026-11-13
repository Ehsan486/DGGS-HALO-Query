#include "bafe/bafe.hpp"
#include "bafe_exp/bafe_workloads.hpp"
#include "bafe_exp/benchmark_common.hpp"
#include "bafe_exp/cli.hpp"
#include "bafe_exp/geo.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

double median_area(const std::vector<bafe::CellIndex>& cells,std::uint32_t level){
    std::vector<double> areas;
    const std::size_t stride=std::max<std::size_t>(1,cells.size()/256);
    for(std::size_t i=0;i<cells.size()&&areas.size()<256;i+=stride) areas.push_back(bafe::geo::cell_area_m2(cells[i],level));
    return bafe_exp::percentile(areas,0.5);
}

struct RunOut { bafe_exp::ResultRow row; std::vector<std::uint64_t> cells; };

RunOut run(const std::string& name,bafe::HaloEngine& engine,const std::vector<bafe::CellIndex>& input,
           std::uint32_t level,std::uint32_t hops,std::uint32_t warmup,std::uint32_t repeats,std::uint64_t max_output,
           double area,const std::string& pattern){
    bafe::QueryOptions q; q.level=level; q.hops=hops; q.max_output_cells=max_output; q.deterministic_layers=false;
    for(std::uint32_t i=0;i<warmup;++i)(void)engine.halo(input,q);
    std::vector<double> samples; samples.reserve(repeats); bafe::HaloResult latest;
    for(std::uint32_t i=0;i<repeats;++i){latest=engine.halo(input,q);samples.push_back(latest.metrics.end_to_end_ms);}    
    bafe_exp::ResultRow r;
    r.experiment_id=name; r.backend=latest.metrics.backend; r.library_version=bafe::kLibraryVersion;
    r.compiler=bafe_exp::compiler_string(); r.pattern=pattern; r.level_or_resolution=level;
    r.median_cell_area_m2=area; r.input_cells=input.size(); r.unique_input_cells=latest.metrics.unique_input_cells;
    r.hops=hops; r.output_cells=latest.cells.size(); r.candidate_references=latest.metrics.touched_neighbor_references;
    r.seam_crossings=latest.metrics.root_edge_crossings; r.peak_memory_bytes=latest.metrics.device_peak_bytes;
    r.h2d_bytes=latest.metrics.host_to_device_bytes; r.d2h_bytes=latest.metrics.device_to_host_bytes;
    r.timing=bafe_exp::summarize(samples); r.result_hash=bafe_exp::fnv1a64(latest.cells); r.correctness_status="SELF";
    r.notes="single-resolution exact halo; complete materialized output";
    return {r,std::vector<std::uint64_t>(latest.cells.begin(),latest.cells.end())};
}

}

int main(int argc,char**argv){
  try{
    bafe_exp::Cli cli(argc,argv);
    const auto level=cli.u32("level",28); const auto count=cli.u64("cells",100000);
    const auto hops=cli.u32("hops",4); const auto warmup=cli.u32("warmup",20); const auto repeats=cli.u32("repeats",100);
    const auto max_output=cli.u64("max-output",0); const auto pattern=cli.get("pattern","compact");
    const auto backend=cli.get("backend","both"); const auto csv_path=cli.get("csv","results/bafe_synthetic.csv");
    std::vector<bafe::CellIndex> input;
    if(pattern=="compact") input=bafe_exp::bafe_compact(level,count,false);
    else if(pattern=="seam") input=bafe_exp::bafe_compact(level,count,true);
    else if(pattern=="scattered") input=bafe_exp::bafe_scattered(level,count);
    else throw std::invalid_argument("pattern must be compact, scattered, or seam");
    const double area=median_area(input,level);
    std::vector<RunOut> outputs;
    if(backend=="cpu"||backend=="both") { auto e=bafe::make_cpu_engine(); outputs.push_back(run("bafe_synthetic",*e,input,level,hops,warmup,repeats,max_output,area,pattern)); }
    if(backend=="gpu"||backend=="both") { auto e=bafe::make_cuda_engine(); outputs.push_back(run("bafe_synthetic",*e,input,level,hops,warmup,repeats,max_output,area,pattern)); }
    if(outputs.size()==2){
      auto a=outputs[0].cells,b=outputs[1].cells; std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());
      if(a!=b) throw std::runtime_error("BAFE CPU/GPU output mismatch");
      outputs[0].row.correctness_status=outputs[1].row.correctness_status="PASS";
    }
    std::ofstream csv(csv_path); if(!csv) throw std::runtime_error("cannot open "+csv_path);
    bafe_exp::write_csv_header(csv); for(const auto&o:outputs)bafe_exp::write_csv_row(csv,o.row);
    for(const auto&o:outputs) std::cout<<o.row.backend<<" mean="<<o.row.timing.mean_ms<<" ms p95="<<o.row.timing.p95_ms<<" output="<<o.row.output_cells<<" hash="<<o.row.result_hash<<" status="<<o.row.correctness_status<<"\n";
    std::cout<<"CSV: "<<csv_path<<"\n";
    return 0;
  }catch(const std::exception&e){std::cerr<<"bafe_synthetic_benchmark: "<<e.what()<<"\n";return 1;}
}
