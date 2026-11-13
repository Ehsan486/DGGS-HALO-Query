#include "coverage.hpp"
#include "bafe/bafe.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <map>
#include <atomic>
#ifdef _OPENMP
#include <omp.h>
#endif
using Clock=std::chrono::steady_clock;
double elapsed(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
using Id=bafe::CellIndex;
template<class Fn> void parallel(std::size_t n,Fn fn) {
    std::exception_ptr error;
    #pragma omp parallel for schedule(dynamic,8)
    for(std::int64_t i=0;i<static_cast<std::int64_t>(n);++i) {
        try{fn(std::size_t(i));}catch(...) {
            #pragma omp critical
            {if(!error)error=std::current_exception();}
        }
    }
    if(error)std::rethrow_exception(error);
}
void require(bool b,const char* m){if(!b)throw std::runtime_error(m);}
void self_test() {
    ellipsoid::WGS84 earth;
    std::vector<ellipsoid::Segment> segments={{{51.,-114.},{51.01,-114.01}},{{51.02,-114.02},{51.025,-114.015}}};
    coverage::Feature feature(segments);ellipsoid::Boundary reference(segments);
    for(auto p:std::vector<ellipsoid::Point>{{51.,-114.},{51.04,-114.03},{-51.,65.},{89.9,179.9}}) {
        auto a=feature.distance(p);auto b=reference.distance(p);
        require(a.lower<=b.upper_m&&b.lower_m<=a.upper,"Segment hierarchy differs from unpruned oracle");
    }
    // A radius bound must include every sampled point of the actual curved footprint.
    for(unsigned f=0;f<60;++f)for(unsigned r: {0U,2U,14U,28U}) {
        Id id=bafe::root(f);for(unsigned l=0;l<r;++l)id=bafe::child(id,(l+f)%4);
        auto p=coverage::patch(id,r);auto centre=coverage::geographic(coverage::centre(p));
        for(int i=0;i<=10;++i)for(int j=0;j<=10-i;++j) {
            auto v=bafe::geo::add(bafe::geo::mul(p.vertices[0],(10.-i-j)/10.),
                bafe::geo::add(bafe::geo::mul(p.vertices[1],i/10.),bafe::geo::mul(p.vertices[2],j/10.)));
            require(earth.inverse(centre,coverage::geographic(v)).distance_m<=coverage::radius(p),"Cell radius violated");
        }
    }
    for(auto point:std::vector<ellipsoid::Point>{{51.,-114.},{0.,179.99},{89.999,0.}}) {
        coverage::Feature src({{point,point}});
        for(unsigned r=0;r<=3;++r) {
            auto pruned=coverage::cover(src,r,10000,100000);
            std::vector<Id> all;for(unsigned f=0;f<60;++f)all.push_back(bafe::root(f));
            for(unsigned l=0;l<r;++l){std::vector<Id> next;for(auto id:all)for(unsigned d=0;d<4;++d)next.push_back(bafe::child(id,d));all=std::move(next);}
            std::vector<Id> expected;for(auto id:all)if(src.bounds(coverage::patch(id,r)).lower<=10000)expected.push_back(id);
            std::sort(expected.begin(),expected.end());require(pruned==expected,"Hierarchical pruning omitted a candidate");
        }
    }
    bool failed=false;try{coverage::cover(feature,28,1000,1);}catch(const std::runtime_error&){failed=true;}
    require(failed,"Capacity exhaustion must fail explicitly");
    std::cout<<"PASS: segment hierarchy, 15840 footprint samples, 12 exhaustive coarse covers, capacity error\n";
}
int main(int argc,char**argv)try {
    auto total_start=Clock::now();std::map<std::string,std::string> args;
    for(int i=1;i<argc;++i) {
        std::string key=argv[i];if(key=="--self-test"){self_test();return 0;}
        if(key=="--verify"){args[key]="1";continue;}
        if(i+1>=argc)throw std::invalid_argument("Missing option value");args[key]=argv[++i];
    }
    auto get=[&](std::string key,std::string fallback){auto i=args.find(key);return i==args.end()?fallback:i->second;};
    unsigned level=std::stoul(get("--level","12")),max_hops=std::stoul(get("--max-hops","256"));
    double distance=std::stod(get("--distance-m","1000")),tolerance=std::stod(get("--tolerance-m","0.0001"));
    std::uint64_t capacity=std::stoull(get("--max-cells","1000000"));
    unsigned threads=std::stoul(get("--threads","8"));
    require(level<=28&&std::isfinite(distance)&&distance>=0&&std::isfinite(tolerance)&&tolerance>=.0001&&capacity>0&&max_hops>0&&threads>0,"Invalid query options");
    #ifdef _OPENMP
    omp_set_num_threads(int(threads));
    #endif
    std::map<std::string,double> times;
    std::vector<ellipsoid::Segment> segments;
    auto tick=Clock::now();
    if(args.count("--point")) {
        std::string s=args["--point"];std::replace(s.begin(),s.end(),',',' ');std::istringstream in(s);
        ellipsoid::Point p;require(bool(in>>p.latitude>>p.longitude),"Point must be latitude,longitude");segments.push_back({p,p});
    } else {
        std::ifstream in(get("--input","data/nose_hill_segments.csv"));require(bool(in),"Cannot open input segment file");
        std::string line;while(std::getline(in,line)) {
            if(line.empty()||line[0]=='#'||line.rfind("lat",0)==0)continue;
            std::replace(line.begin(),line.end(),',',' ');std::istringstream row(line);ellipsoid::Segment s;
            require(bool(row>>s.a.latitude>>s.a.longitude>>s.b.latitude>>s.b.longitude),"Malformed segment row");segments.push_back(s);
        }
    }
    times["read_ms"]=elapsed(tick);tick=Clock::now();coverage::Feature source(segments);times["feature_prepare_ms"]=elapsed(tick);
    tick=Clock::now();auto seeds=coverage::cover(source,level,0,capacity);times["source_cover_ms"]=elapsed(tick);
    require(!seeds.empty(),"Source cover unexpectedly empty");
    tick=Clock::now();std::string backend=get("--backend","cuda");require(backend=="cpu"||backend=="cuda","Backend must be cpu or cuda");
    auto engine=backend=="cuda"?bafe::make_cuda_engine():bafe::make_cpu_engine();times["backend_initialization_ms"]=elapsed(tick);
    bafe::HaloResult halo;unsigned hops=1,replays=0;double stopping_lower=0;
    bafe::Metrics metrics;
    for(;;) {
        bafe::QueryOptions options;options.level=level;options.hops=hops;options.max_output_cells=capacity;
        tick=Clock::now();halo=engine->halo(seeds,options);times["halo_calls_wall_ms"]+=elapsed(tick);++replays;
        const auto& m=halo.metrics;
        times["gpu_upload_ms"]+=m.gpu_upload_ms;times["gpu_workspace_clear_ms"]+=m.gpu_workspace_clear_ms;
        times["gpu_decode_seed_hash_ms"]+=m.gpu_decode_seed_hash_ms;times["gpu_halo_expand_hash_ms"]+=m.gpu_halo_expand_hash_ms;
        times["gpu_output_encode_ms"]+=m.gpu_output_encode_ms;times["gpu_download_ms"]+=m.gpu_download_ms;
        metrics=m;tick=Clock::now();
        require(halo.layer_offsets.size()==std::size_t(hops)+1,"Unexpected halo layer offsets");
        auto start=halo.layer_offsets[hops-1],end=halo.layer_offsets[hops];
        std::vector<double> bounds(end-start);
        parallel(bounds.size(),[&](std::size_t i){bounds[i]=source.bounds(coverage::patch(halo.cells[start+i],level)).lower;});
        stopping_lower=bounds.empty()?INFINITY:*std::min_element(bounds.begin(),bounds.end());
        times["stopping_distance_ms"]+=elapsed(tick);
        if(stopping_lower>distance)break;
        if(hops>=max_hops)throw std::runtime_error("Maximum hops reached without a complete separating ring; no result accepted");
        hops=std::min(max_hops,hops*2);
    }
    std::vector<Id> visited=seeds;visited.insert(visited.end(),halo.cells.begin(),halo.cells.end());
    tick=Clock::now();std::vector<coverage::CellResult> classified(visited.size());
    parallel(visited.size(),[&](std::size_t i){classified[i]=coverage::classify(source,coverage::patch(visited[i],level),distance,tolerance);});
    times["cell_footprint_filter_ms"]=elapsed(tick);
    std::vector<Id> accepted;std::size_t unresolved=0;double max_uncertain_width=0;
    for(std::size_t i=0;i<visited.size();++i)if(classified[i].status!=coverage::Status::outside) {
        accepted.push_back(visited[i]);if(classified[i].status==coverage::Status::unresolved) {
            ++unresolved;max_uncertain_width=std::max(max_uncertain_width,classified[i].upper-classified[i].lower);
        }
    }
    std::sort(accepted.begin(),accepted.end());
    times["query_total_ms"]=elapsed(total_start);
    bool verified=false;
    if(args.count("--verify")) {
        tick=Clock::now();auto cpu=bafe::make_cpu_engine();bafe::QueryOptions options;
        options.level=level;options.hops=hops;options.max_output_cells=capacity;auto ref=cpu->halo(seeds,options);
        require(ref.layer_offsets==halo.layer_offsets,"CPU/GPU layer sizes differ");
        for(unsigned k=0;k<hops;++k) {
            std::vector<Id>a(ref.cells.begin()+ref.layer_offsets[k],ref.cells.begin()+ref.layer_offsets[k+1]);
            std::vector<Id>b(halo.cells.begin()+halo.layer_offsets[k],halo.cells.begin()+halo.layer_offsets[k+1]);
            std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());require(a==b,"CPU/GPU cell layers differ");
        }
        // Alternative spatial enumeration never uses halo adjacency or the ACM.
        auto spatial=coverage::cover(source,level,distance,capacity);std::vector<char>keep(spatial.size());
        parallel(spatial.size(),[&](std::size_t i){keep[i]=coverage::classify(source,coverage::patch(spatial[i],level),distance,tolerance).status!=coverage::Status::outside;});
        std::vector<Id> expected;for(std::size_t i=0;i<spatial.size();++i)if(keep[i])expected.push_back(spatial[i]);
        require(expected==accepted,"Spatial hierarchy and halo buffer outputs differ");verified=true;times["validation_ms"]=elapsed(tick);
    }
    std::string prefix=get("--output","results/buffer");std::filesystem::path path(prefix);
    if(path.has_parent_path())std::filesystem::create_directories(path.parent_path());
    tick=Clock::now();std::ofstream out(prefix+"_cells.csv");require(bool(out),"Cannot open output");
    out<<"public_index,status,distance_lower_m,distance_upper_m,latitude,longitude\n"<<std::setprecision(17);
    for(std::size_t i=0;i<visited.size();++i)if(classified[i].status!=coverage::Status::outside) {
        auto p=coverage::geographic(coverage::centre(coverage::patch(visited[i],level)));
        out<<visited[i]<<','<<(classified[i].status==coverage::Status::unresolved?"unresolved_retained":"intersection_confirmed")
           <<','<<classified[i].lower<<','<<classified[i].upper<<','<<p.latitude<<','<<p.longitude<<'\n';
    }
    out.close();require(bool(out),"Failed writing complete cell output");times["output_file_ms"]=elapsed(tick);
    std::ofstream report(prefix+"_report.json");require(bool(report),"Cannot write report");
    report<<std::setprecision(17)<<"{\n  \"schema\": \"ellipsoidal-boundary-buffer-1\",\n"
       <<"  \"source_semantics\": \"union of shortest WGS84 geodesic segments; not polygon interior\",\n"
       <<"  \"numerical_contract\": \"double precision with engineering guards, not formal interval certification\",\n"
       <<"  \"backend\": \""<<backend<<"\",\n  \"device\": \""<<metrics.device<<"\",\n"
       <<"  \"level\": "<<level<<",\n  \"distance_m\": "<<distance<<",\n  \"threads\": "<<threads<<",\n"
       <<"  \"cell_tolerance_m\": "<<tolerance<<",\n  \"engineering_guard_m\": 0.00001,\n"
       <<"  \"segments\": "<<segments.size()<<",\n  \"seed_cells\": "<<seeds.size()<<",\n"
       <<"  \"visited_cells\": "<<visited.size()<<",\n  \"output_cells\": "<<accepted.size()<<",\n"
       <<"  \"unresolved_retained\": "<<unresolved<<",\n  \"max_unresolved_width_m\": "<<max_uncertain_width<<",\n"
       <<"  \"separating_ring\": "<<hops<<",\n  \"halo_calls\": "<<replays<<",\n"
       <<"  \"stopping_lower_m\": "<<(std::isfinite(stopping_lower)?std::to_string(stopping_lower):"null")<<",\n"
       <<"  \"last_call_device_peak_bytes\": "<<metrics.device_peak_bytes<<",\n"
       <<"  \"validated\": "<<(verified?"true":"false")<<",\n  \"timing_ms\": {\n";
    bool first=true;for(auto [key,value]:times){if(!first)report<<",\n";first=false;report<<"    \""<<key<<"\": "<<value;}
    report<<"\n  }\n}\n";report.close();require(bool(report),"Report write failed");
    std::cout<<"level="<<level<<" source="<<seeds.size()<<" visited="<<visited.size()<<" output="<<accepted.size()
       <<" unresolved="<<unresolved<<" K="<<hops<<" query_ms="<<times["query_total_ms"]<<" validated="<<verified<<'\n';
    std::cout<<"Report: "<<prefix<<"_report.json\n";return 0;
} catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<"\n";return 1;}
