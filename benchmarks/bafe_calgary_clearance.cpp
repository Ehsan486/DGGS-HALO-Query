#include "bafe/bafe.hpp"
#include "internal.hpp"
#include "topology.hpp"
#include "bafe_exp/benchmark_common.hpp"
#include "bafe_exp/cli.hpp"
#include "bafe_exp/geo.hpp"
#include "bafe_exp/geo_math.hpp"
#include "bafe_exp/point_csv.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace {
using Clock=std::chrono::steady_clock;

std::vector<bafe::CellIndex> map_points(const std::vector<std::pair<double,double>>& pts,std::uint32_t level){
    std::unordered_set<bafe::CellIndex> unique;
    unique.reserve(pts.size()*2+16);
    for(const auto& p:pts) unique.insert(bafe::geo::lonlat_to_cell(p.first,p.second,level));
    return {unique.begin(),unique.end()};
}

double neighbor_step(const std::vector<bafe::CellIndex>& cells,std::uint32_t level){
    if(cells.empty()) return 0.0;
    std::vector<double> distances;
    const std::size_t stride=std::max<std::size_t>(1,cells.size()/128);
    for(std::size_t k=0;k<cells.size()&&distances.size()<384;k+=stride){
        const auto c=bafe::detail::path_to_cell_scalar(cells[k],level);
        const auto a=bafe::geo::unit_to_lonlat(bafe::geo::cell_centroid(c,level));
        bafe::detail::Cell ns[3];
        bafe::detail::neighbors_host(c,level,ns,nullptr);
        for(const auto& n:ns){
            const auto b=bafe::geo::unit_to_lonlat(bafe::geo::cell_centroid(n,level));
            distances.push_back(bafe_exp::great_circle_m(a.first,a.second,b.first,b.second));
        }
    }
    return bafe_exp::percentile(distances,0.5);
}

double median_area(const std::vector<bafe::CellIndex>& cells,std::uint32_t level){
    std::vector<double> values;
    const std::size_t stride=std::max<std::size_t>(1,cells.size()/256);
    for(std::size_t i=0;i<cells.size()&&values.size()<256;i+=stride)values.push_back(bafe::geo::cell_area_m2(cells[i],level));
    return bafe_exp::percentile(values,0.5);
}

struct Run { bafe_exp::ResultRow row; std::vector<std::uint64_t> conflicts; std::vector<std::uint64_t> halo; };

Run execute(const std::string& label,bafe::HaloEngine& engine,const std::vector<bafe::CellIndex>& buildings,
            const std::unordered_set<bafe::CellIndex>& corridor,std::uint32_t level,std::uint32_t hops,
            std::uint32_t warmup,std::uint32_t repeats,double mapping_ms,double step,double requested,
            const std::string& scenario){
    bafe::QueryOptions q{level,hops,0,false};
    auto once=[&](){
        auto result=engine.halo(buildings,q);
        std::vector<std::uint64_t> conflicts;
        conflicts.reserve(std::min<std::size_t>(result.cells.size(),corridor.size()));
        for(auto id:result.cells) if(corridor.find(id)!=corridor.end()) conflicts.push_back(id);
        return std::pair<bafe::HaloResult,std::vector<std::uint64_t>>(std::move(result),std::move(conflicts));
    };
    for(std::uint32_t i=0;i<warmup;++i)(void)once();
    std::vector<double> times; times.reserve(repeats);
    std::pair<bafe::HaloResult,std::vector<std::uint64_t>> last;
    for(std::uint32_t i=0;i<repeats;++i){const auto s=Clock::now();last=once();times.push_back(std::chrono::duration<double,std::milli>(Clock::now()-s).count());}
    bafe_exp::ResultRow r;
    r.experiment_id="calgary_horizontal_clearance";r.backend=last.first.metrics.backend;r.library_version=bafe::kLibraryVersion;
    r.compiler=bafe_exp::compiler_string();r.pattern="real_building_samples";r.scenario=scenario;r.level_or_resolution=level;
    r.median_cell_area_m2=median_area(buildings,level);r.input_cells=buildings.size();r.unique_input_cells=last.first.metrics.unique_input_cells;
    r.hops=hops;r.output_cells=last.first.cells.size();r.conflict_cells=last.second.size();r.candidate_references=last.first.metrics.touched_neighbor_references;
    r.seam_crossings=last.first.metrics.root_edge_crossings;r.peak_memory_bytes=last.first.metrics.device_peak_bytes;
    r.h2d_bytes=last.first.metrics.host_to_device_bytes;r.d2h_bytes=last.first.metrics.device_to_host_bytes;
    r.mapping_ms=mapping_ms;r.neighbor_step_m=step;r.requested_clearance_m=requested;r.achieved_clearance_m=step*hops;
    r.timing=bafe_exp::summarize(times);r.result_hash=bafe_exp::fnv1a64(last.second);r.correctness_status="SELF";
    r.notes="exact graph halo relative to common sampled building/corridor cell sets; conflict intersection included in timing";
    return {r,{last.second.begin(),last.second.end()},{last.first.cells.begin(),last.first.cells.end()}};
}
}

int main(int argc,char**argv){
 try{
    bafe_exp::Cli cli(argc,argv);
    const auto building_path=cli.get("building-points");
    const auto corridor_path=cli.get("corridor-points");
    if(building_path.empty()||corridor_path.empty()) throw std::invalid_argument("--building-points and --corridor-points are required");
    const auto level=cli.u32("level",21);const auto warmup=cli.u32("warmup",10);const auto repeats=cli.u32("repeats",30);
    const auto backend=cli.get("backend","both");const auto scenario=cli.get("scenario","calgary");const auto csv_path=cli.get("csv","results/bafe_calgary.csv");
    const double requested=std::stod(cli.get("clearance-m","10"));
    const auto raw_buildings=bafe_exp::read_lonlat_csv(building_path);const auto raw_corridor=bafe_exp::read_lonlat_csv(corridor_path);
    const auto map_start=Clock::now();auto buildings=map_points(raw_buildings,level);auto corridor_vec=map_points(raw_corridor,level);
    const double mapping_ms=std::chrono::duration<double,std::milli>(Clock::now()-map_start).count();
    std::unordered_set<bafe::CellIndex> corridor(corridor_vec.begin(),corridor_vec.end());
    const double step=neighbor_step(buildings.empty()?corridor_vec:buildings,level);
    if(!(step>0.0)) throw std::runtime_error("could not estimate neighbor step");
    const auto explicit_hops=cli.u32("hops",0);const auto hops=explicit_hops?explicit_hops:std::max<std::uint32_t>(1,static_cast<std::uint32_t>(std::ceil(requested/step)));
    std::vector<Run> rows;
    if(backend=="cpu"||backend=="both"){auto e=bafe::make_cpu_engine();rows.push_back(execute("cpu",*e,buildings,corridor,level,hops,warmup,repeats,mapping_ms,step,requested,scenario));}
    if(backend=="gpu"||backend=="both"){auto e=bafe::make_cuda_engine();rows.push_back(execute("gpu",*e,buildings,corridor,level,hops,warmup,repeats,mapping_ms,step,requested,scenario));}
    if(rows.size()==2){auto a=rows[0].halo,b=rows[1].halo;std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());auto ca=rows[0].conflicts,cb=rows[1].conflicts;std::sort(ca.begin(),ca.end());std::sort(cb.begin(),cb.end());if(a!=b||ca!=cb)throw std::runtime_error("CPU/GPU Calgary result mismatch");rows[0].row.correctness_status=rows[1].row.correctness_status="PASS";}
    std::ofstream csv(csv_path);if(!csv)throw std::runtime_error("cannot open "+csv_path);bafe_exp::write_csv_header(csv);for(const auto& x:rows)bafe_exp::write_csv_row(csv,x.row);
    for(const auto& x:rows)std::cout<<x.row.backend<<" buildings="<<buildings.size()<<" corridor="<<corridor.size()<<" hops="<<hops<<" halo="<<x.row.output_cells<<" conflicts="<<x.row.conflict_cells<<" mean="<<x.row.timing.mean_ms<<" ms\n";
    std::cout<<"CSV: "<<csv_path<<"\n";return 0;
 }catch(const std::exception&e){std::cerr<<"bafe_calgary_clearance: "<<e.what()<<"\n";return 1;}
}
