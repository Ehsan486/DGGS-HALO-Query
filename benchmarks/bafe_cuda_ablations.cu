#include "bafe/bafe.hpp"
#include "atlas.hpp"
#include "internal.hpp"
#include "topology.hpp"
#include "bafe_exp/bafe_workloads.hpp"
#include "bafe_exp/benchmark_common.hpp"
#include "bafe_exp/cli.hpp"
#include "bafe_exp/geo.hpp"

#include <cuda_runtime.h>
#include <thrust/copy.h>
#include <thrust/device_vector.h>
#include <thrust/execution_policy.h>
#include <thrust/set_operations.h>
#include <thrust/sort.h>
#include <thrust/unique.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
using bafe::detail::Cell;
using bafe::detail::Transition;
constexpr int kThreads=256;
__constant__ Transition d_atlas_ablation[180];

void check(cudaError_t e,const char*what){if(e!=cudaSuccess)throw std::runtime_error(std::string(what)+": "+cudaGetErrorString(e));}

__device__ Cell boundary_up(std::uint32_t face,std::uint32_t edge,std::uint32_t t,std::uint32_t n){
    if(edge==0U)return Cell{face,n-1U-t,t,0U};if(edge==1U)return Cell{face,0U,t,0U};return Cell{face,t,0U,0U};
}
__device__ Cell cross_edge(const Cell&c,std::uint32_t edge,std::uint32_t t,std::uint32_t n){auto tr=d_atlas_ablation[c.face*3U+edge];auto mt=tr.reverse?n-1U-t:t;return boundary_up(tr.dst_face,tr.dst_edge,mt,n);}
__device__ void neighbors(const Cell&c,std::uint32_t level,bool rhombic,Cell out[3],std::uint32_t&seams,std::uint32_t&internal){
    seams=0;internal=0;const std::uint32_t n=1U<<level;
    if(c.orientation==1U){out[0]={c.face,c.i,c.j,0};out[1]={c.face,c.i+1,c.j,0};out[2]={c.face,c.i,c.j+1,0};return;}
    if(c.i+c.j+1U<n)out[0]={c.face,c.i,c.j,1};
    else if(rhombic){out[0]={c.face^1U,c.j,c.i,0};++internal;}
    else {out[0]=cross_edge(c,0U,c.j,n);++seams;}
    if(c.j>0U)out[1]={c.face,c.i,c.j-1,1};else{out[1]=cross_edge(c,2U,c.i,n);++seams;}
    if(c.i>0U)out[2]={c.face,c.i-1,c.j,1};else{out[2]=cross_edge(c,1U,c.j,n);++seams;}
}
__global__ void neighbor_kernel(const std::uint64_t*frontier,std::uint64_t n,std::uint32_t level,bool rhombic,
                                std::uint64_t*candidates,unsigned long long*seams,unsigned long long*internal){
    const std::uint64_t idx=std::uint64_t(blockIdx.x)*blockDim.x+threadIdx.x;if(idx>=n)return;
    Cell ns[3];std::uint32_t s=0,in=0;neighbors(bafe::detail::decode_coord(frontier[idx]),level,rhombic,ns,s,in);
    candidates[3*idx+0]=bafe::detail::encode_coord(ns[0]);candidates[3*idx+1]=bafe::detail::encode_coord(ns[1]);candidates[3*idx+2]=bafe::detail::encode_coord(ns[2]);
    if(s)atomicAdd(seams,(unsigned long long)s);if(in)atomicAdd(internal,(unsigned long long)in);
}

std::vector<std::uint64_t> to_coord(const std::vector<bafe::CellIndex>&input,std::uint32_t level){std::vector<std::uint64_t>v;v.reserve(input.size());for(auto id:input)v.push_back(bafe::detail::encode_coord(bafe::detail::path_to_cell_scalar(id,level)));return v;}
std::vector<std::uint64_t> to_path(const std::vector<std::uint64_t>&coord,std::uint32_t level){std::vector<std::uint64_t>v;v.reserve(coord.size());for(auto id:coord)v.push_back(bafe::detail::cell_to_path_scalar(bafe::detail::decode_coord(id),level));return v;}

struct CustomRun{std::vector<std::uint64_t>out;double ms=0;std::uint64_t candidates=0,seams=0,internal=0,h2d=0,d2h=0,peak=0;};

CustomRun run_sort(const std::vector<bafe::CellIndex>&input,std::uint32_t level,std::uint32_t hops,bool rhombic){
    auto started=std::chrono::steady_clock::now();auto host=to_coord(input,level);
    thrust::device_vector<std::uint64_t>frontier(host.begin(),host.end());thrust::sort(frontier.begin(),frontier.end());frontier.erase(thrust::unique(frontier.begin(),frontier.end()),frontier.end());
    thrust::device_vector<std::uint64_t>visited=frontier,allout;CustomRun r;r.h2d=host.size()*8ULL;
    unsigned long long *d_seams=nullptr,*d_internal=nullptr;check(cudaMalloc(&d_seams,sizeof(*d_seams)),"cudaMalloc seams");check(cudaMalloc(&d_internal,sizeof(*d_internal)),"cudaMalloc internal");
    for(std::uint32_t h=0;h<hops;++h){
        const std::uint64_t n=frontier.size();if(n==0)break;r.candidates+=3ULL*n;
        thrust::device_vector<std::uint64_t>cand(static_cast<std::size_t>(3*n));check(cudaMemset(d_seams,0,sizeof(*d_seams)),"memset seams");check(cudaMemset(d_internal,0,sizeof(*d_internal)),"memset internal");
        neighbor_kernel<<<int((n+kThreads-1)/kThreads),kThreads>>>(thrust::raw_pointer_cast(frontier.data()),n,level,rhombic,thrust::raw_pointer_cast(cand.data()),d_seams,d_internal);check(cudaGetLastError(),"neighbor kernel");
        thrust::sort(cand.begin(),cand.end());cand.erase(thrust::unique(cand.begin(),cand.end()),cand.end());
        thrust::device_vector<std::uint64_t>next(cand.size());auto next_end=thrust::set_difference(cand.begin(),cand.end(),visited.begin(),visited.end(),next.begin());next.erase(next_end,next.end());
        const auto old=allout.size();allout.resize(old+next.size());thrust::copy(next.begin(),next.end(),allout.begin()+old);
        thrust::device_vector<std::uint64_t>merged(visited.size()+next.size());auto mend=thrust::set_union(visited.begin(),visited.end(),next.begin(),next.end(),merged.begin());merged.erase(mend,merged.end());visited.swap(merged);frontier.swap(next);
        unsigned long long hs=0,hi=0;check(cudaMemcpy(&hs,d_seams,sizeof(hs),cudaMemcpyDeviceToHost),"copy seams");check(cudaMemcpy(&hi,d_internal,sizeof(hi),cudaMemcpyDeviceToHost),"copy internal");r.seams+=hs;r.internal+=hi;
        r.peak=std::max<std::uint64_t>(r.peak,8ULL*(frontier.capacity()+visited.capacity()+allout.capacity()+cand.capacity()));
    }
    std::vector<std::uint64_t>coord(allout.size());thrust::copy(allout.begin(),allout.end(),coord.begin());r.out=to_path(coord,level);r.d2h=coord.size()*8ULL;
    check(cudaFree(d_seams),"free seams");check(cudaFree(d_internal),"free internal");check(cudaDeviceSynchronize(),"sync sort");r.ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();return r;
}

CustomRun run_host_managed(const std::vector<bafe::CellIndex>&input,std::uint32_t level,std::uint32_t hops){
    auto started=std::chrono::steady_clock::now();auto coords=to_coord(input,level);std::unordered_set<std::uint64_t>visited(coords.begin(),coords.end());std::vector<std::uint64_t>frontier(visited.begin(),visited.end());CustomRun r;
    unsigned long long *d_seams=nullptr,*d_internal=nullptr;check(cudaMalloc(&d_seams,sizeof(*d_seams)),"malloc seams");check(cudaMalloc(&d_internal,sizeof(*d_internal)),"malloc internal");
    for(std::uint32_t h=0;h<hops;++h){if(frontier.empty())break;const std::uint64_t n=frontier.size();r.candidates+=3*n;r.h2d+=n*8ULL;
        std::uint64_t *d_frontier=nullptr,*d_candidates=nullptr;check(cudaMalloc(&d_frontier,n*8ULL),"malloc frontier");check(cudaMalloc(&d_candidates,3*n*8ULL),"malloc candidates");check(cudaMemcpy(d_frontier,frontier.data(),n*8ULL,cudaMemcpyHostToDevice),"copy frontier");check(cudaMemset(d_seams,0,sizeof(*d_seams)),"memset seams");check(cudaMemset(d_internal,0,sizeof(*d_internal)),"memset internal");
        neighbor_kernel<<<int((n+kThreads-1)/kThreads),kThreads>>>(d_frontier,n,level,false,d_candidates,d_seams,d_internal);check(cudaGetLastError(),"neighbor kernel");
        std::vector<std::uint64_t>cand(3*n);check(cudaMemcpy(cand.data(),d_candidates,cand.size()*8ULL,cudaMemcpyDeviceToHost),"copy candidates");r.d2h+=cand.size()*8ULL;unsigned long long hs=0;check(cudaMemcpy(&hs,d_seams,sizeof(hs),cudaMemcpyDeviceToHost),"copy seams");r.seams+=hs;
        std::vector<std::uint64_t>next;next.reserve(cand.size()/2);for(auto c:cand)if(visited.insert(c).second){next.push_back(c);r.out.push_back(bafe::detail::cell_to_path_scalar(bafe::detail::decode_coord(c),level));}frontier.swap(next);
        r.peak=std::max<std::uint64_t>(r.peak,(n+3*n)*8ULL);cudaFree(d_frontier);cudaFree(d_candidates);
    }
    cudaFree(d_seams);cudaFree(d_internal);r.ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();return r;
}

bafe_exp::ResultRow custom_row(const std::string&backend,const CustomRun&r,const std::vector<bafe::CellIndex>&input,std::uint32_t level,std::uint32_t hops,const std::string&pattern,const std::vector<double>&times){
    bafe_exp::ResultRow row;row.experiment_id="cuda_frontier_ablation";row.backend=backend;row.library_version=bafe::kLibraryVersion;row.compiler=bafe_exp::compiler_string();row.pattern=pattern;row.level_or_resolution=level;row.median_cell_area_m2=bafe::geo::global_average_cell_area_m2(level);row.input_cells=row.unique_input_cells=input.size();row.hops=hops;row.output_cells=r.out.size();row.candidate_references=r.candidates;row.seam_crossings=r.seams;row.peak_memory_bytes=r.peak;row.h2d_bytes=r.h2d;row.d2h_bytes=r.d2h;row.timing=bafe_exp::summarize(times);row.result_hash=bafe_exp::fnv1a64(r.out);row.correctness_status="UNVERIFIED";row.notes="internal_pair_crossings="+std::to_string(r.internal);return row;
}
}

int main(int argc,char**argv){try{
    bafe_exp::Cli cli(argc,argv);const auto level=cli.u32("level",28),hops=cli.u32("hops",4);const auto count=cli.u64("cells",100000);const auto repeats=cli.u32("repeats",20),warm=cli.u32("warmup",3);const auto pattern=cli.get("pattern","compact"),mode=cli.get("mode","all"),csvp=cli.get("csv","results/cuda_ablations.csv");
    auto input=pattern=="scattered"?bafe_exp::bafe_scattered(level,count):bafe_exp::bafe_compact(level,count,pattern=="seam");check(cudaMemcpyToSymbol(d_atlas_ablation,bafe::detail::kAtlas,sizeof(bafe::detail::kAtlas)),"copy atlas");
    // Output ordering is irrelevant to the set-valued halo API.  Keep layer
    // sorting outside the timed region for every backend, then compare sorted
    // copies after timing so the ablation measures traversal architecture.
    bafe::QueryOptions q{level,hops,0,false};auto cpu=bafe::make_cpu_engine();auto expected=cpu->halo(input,q).cells;std::sort(expected.begin(),expected.end());std::vector<bafe_exp::ResultRow>rows;
    auto add_custom=[&](const std::string&name,bool sort,bool rhombic){for(std::uint32_t i=0;i<warm;++i)(void)(sort?run_sort(input,level,hops,rhombic):run_host_managed(input,level,hops));std::vector<double>times;CustomRun last;for(std::uint32_t i=0;i<repeats;++i){last=sort?run_sort(input,level,hops,rhombic):run_host_managed(input,level,hops);times.push_back(last.ms);}auto actual=last.out;std::sort(actual.begin(),actual.end());if(actual!=expected)throw std::runtime_error(name+" output mismatch");auto row=custom_row(name,last,input,level,hops,pattern,times);row.correctness_status="PASS";rows.push_back(row);};
    if(mode=="host"||mode=="all")add_custom("cuda_neighbors_host_sets",false,false);
    if(mode=="sort"||mode=="all")add_custom("cuda_sort_unique_triangle",true,false);
    if(mode=="rhombus"||mode=="all")add_custom("cuda_sort_unique_rhombic_macrochart",true,true);
    if(mode=="hash"||mode=="all"){
        auto gpu=bafe::make_cuda_engine();for(std::uint32_t i=0;i<warm;++i)(void)gpu->halo(input,q);std::vector<double>times;bafe::HaloResult last;for(std::uint32_t i=0;i<repeats;++i){last=gpu->halo(input,q);times.push_back(last.metrics.end_to_end_ms);}auto actual=last.cells;std::sort(actual.begin(),actual.end());if(actual!=expected)throw std::runtime_error("hash output mismatch");bafe_exp::ResultRow r;r.experiment_id="cuda_frontier_ablation";r.backend="cuda_hash_device_resident";r.library_version=bafe::kLibraryVersion;r.compiler=bafe_exp::compiler_string();r.pattern=pattern;r.level_or_resolution=level;r.median_cell_area_m2=bafe::geo::global_average_cell_area_m2(level);r.input_cells=input.size();r.unique_input_cells=last.metrics.unique_input_cells;r.hops=hops;r.output_cells=last.cells.size();r.candidate_references=last.metrics.touched_neighbor_references;r.seam_crossings=last.metrics.root_edge_crossings;r.peak_memory_bytes=last.metrics.device_peak_bytes;r.h2d_bytes=last.metrics.host_to_device_bytes;r.d2h_bytes=last.metrics.device_to_host_bytes;r.timing=bafe_exp::summarize(times);r.result_hash=bafe_exp::fnv1a64(last.cells);r.correctness_status="PASS";r.notes="open-addressing device hash";rows.push_back(r);}
    std::ofstream csv(csvp);bafe_exp::write_csv_header(csv);for(auto&r:rows)bafe_exp::write_csv_row(csv,r);for(auto&r:rows)std::cout<<r.backend<<" mean="<<r.timing.mean_ms<<" ms output="<<r.output_cells<<" PASS\n";std::cout<<"CSV: "<<csvp<<"\n";return 0;
}catch(const std::exception&e){std::cerr<<"bafe_cuda_ablations: "<<e.what()<<"\n";return 1;}}
