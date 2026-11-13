#include "bafe/bafe.hpp"
#include "bafe/index.hpp"
#include "bafe_exp/benchmark_common.hpp"
#include "bafe_exp/cli.hpp"
#include "bafe_exp/geo.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

std::vector<std::uint64_t> read_ids(const std::string&p){std::ifstream f(p);if(!f)throw std::runtime_error("cannot open "+p);std::vector<std::uint64_t>v;std::uint64_t x;while(f>>x)v.push_back(x);return v;}

int main(int argc,char**argv){
 try{
  bafe_exp::Cli cli(argc,argv);const auto input_path=cli.get("input","datasets/processed/canada_feature_cell_ids.txt");const auto expected_path=cli.get("expected","datasets/processed/canada_expected_4hop_halo_ids.txt");
  const auto backend=cli.get("backend","both");
  const auto repeats=cli.u32("repeats",30);
  const auto csv_path=cli.get("csv","results/canada_regression.csv");
  auto legacy=read_ids(input_path),expected_legacy=read_ids(expected_path);std::vector<bafe::CellIndex>input,expected;input.reserve(legacy.size());expected.reserve(expected_legacy.size());
  for(auto x:legacy)input.push_back(bafe::import_legacy_id(x));for(auto x:expected_legacy)expected.push_back(bafe::import_legacy_id(x));std::sort(expected.begin(),expected.end());
  std::vector<bafe_exp::ResultRow>rows;
  auto run=[&](const std::string&name,std::unique_ptr<bafe::HaloEngine>e){bafe::QueryOptions q{7,4,100000,true};for(int i=0;i<5;++i)(void)e->halo(input,q);std::vector<double>t; bafe::HaloResult r;
    for(std::uint32_t i=0;i<repeats;++i){r=e->halo(input,q);t.push_back(r.metrics.end_to_end_ms);}auto actual=r.cells;std::sort(actual.begin(),actual.end());if(actual!=expected)throw std::runtime_error(name+" mismatch");
    bafe_exp::ResultRow row;row.experiment_id="canada_regression";row.backend=r.metrics.backend;row.library_version=bafe::kLibraryVersion;row.compiler=bafe_exp::compiler_string();row.pattern="actual_canada_cell_set";row.level_or_resolution=7;
    row.median_cell_area_m2=bafe::geo::global_average_cell_area_m2(7);row.input_cells=input.size();row.unique_input_cells=r.metrics.unique_input_cells;row.hops=4;row.output_cells=r.cells.size();row.candidate_references=r.metrics.touched_neighbor_references;row.seam_crossings=r.metrics.root_edge_crossings;row.timing=bafe_exp::summarize(t);row.result_hash=bafe_exp::fnv1a64(r.cells);row.correctness_status="PASS";row.notes="exact relative to supplied 20,504-cell feature set";rows.push_back(row);};
  if(backend=="cpu"||backend=="both")run("cpu",bafe::make_cpu_engine());if(backend=="gpu"||backend=="both")run("gpu",bafe::make_cuda_engine());
  std::ofstream csv(csv_path);bafe_exp::write_csv_header(csv);for(auto&r:rows)bafe_exp::write_csv_row(csv,r);std::cout<<"PASS output="<<expected.size()<<" CSV="<<csv_path<<"\n";return 0;
 }catch(const std::exception&e){std::cerr<<"bafe_canada_regression: "<<e.what()<<"\n";return 1;}
}
