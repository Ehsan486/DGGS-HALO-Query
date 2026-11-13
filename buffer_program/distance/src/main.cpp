#include "wgs84_distance.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#ifdef _OPENMP
#include <omp.h>
#endif
using Clock=std::chrono::steady_clock;
double elapsed(Clock::time_point a,Clock::time_point b) {
    return std::chrono::duration<double,std::milli>(b-a).count();
}
double number(const std::string& s) {
    std::size_t used=0; double n=std::stod(s,&used);
    if(used!=s.size()) throw std::invalid_argument("Invalid numeric value: "+s);
    return n;
}
std::vector<std::vector<double>> csv(const std::string& path,const std::string& header,std::size_t cols) {
    std::ifstream in(path); if(!in) throw std::runtime_error("Cannot read "+path);
    std::string line; std::getline(in,line);
    if(!line.empty()&&line.back()=='\r') line.pop_back();
    if(line!=header) throw std::runtime_error("Unexpected CSV header in "+path+"; expected "+header);
    std::vector<std::vector<double>> rows;
    while(std::getline(in,line)) {
        if(!line.empty()&&line.back()=='\r') line.pop_back();
        if(line.empty()) continue;
        std::stringstream stream(line); std::string item; std::vector<double> row;
        while(std::getline(stream,item,',')) row.push_back(number(item));
        if(row.size()!=cols) throw std::runtime_error("Invalid CSV row in "+path);
        rows.push_back(std::move(row));
    }
    return rows;
}
int main(int argc,char** argv) try {
    std::cout<<std::setprecision(17);
    if(argc==6&&std::string(argv[1])=="point") {
        ellipsoid::WGS84 earth;
        auto r=earth.inverse({number(argv[2]),number(argv[3])},{number(argv[4]),number(argv[5])});
        std::cout<<"{\"model\":\"WGS84\",\"distance_m\":"<<r.distance_m
                 <<",\"azimuth1_deg\":"<<r.azimuth1_deg<<",\"azimuth2_deg\":"<<r.azimuth2_deg<<"}\n";
        return 0;
    }
    if(argc<5||std::string(argv[1])!="boundary"||argc>8) {
        std::cerr<<"Usage:\n wgs84_distance point lat1 lon1 lat2 lon2\n"
                 <<" wgs84_distance boundary segments.csv queries.csv output.csv [tolerance_m=0.001] [threads=0] [max_evaluations=1000000]\n";
        return 2;
    }
    const auto start=Clock::now();
    auto edges=csv(argv[2],"lat1,lon1,lat2,lon2",4);
    auto points=csv(argv[3],"latitude,longitude",2);
    if(points.empty()) throw std::runtime_error("Query file is empty.");
    auto read=Clock::now();
    std::vector<ellipsoid::Segment> segments;
    for(const auto& r:edges) segments.push_back({{r[0],r[1]},{r[2],r[3]}});
    ellipsoid::Boundary boundary(segments);
    ellipsoid::Options options;
    if(argc>5) options.tolerance_m=number(argv[5]);
    int threads=argc>6?std::stoi(argv[6]):0;
    if(threads<0) throw std::invalid_argument("Thread count must be nonnegative.");
    if(argc>7) options.max_evaluations=std::stoull(argv[7]);
#ifdef _OPENMP
    if(threads==0) threads=omp_get_max_threads();
    threads=std::min(threads,static_cast<int>(std::min<std::size_t>(points.size(),1000000)));
#else
    if(threads>1) throw std::runtime_error("This build does not support OpenMP.");
    threads=1;
#endif
    auto prepared=Clock::now();
    std::vector<ellipsoid::BoundaryResult> results(points.size());
    std::vector<std::string> errors(points.size());
#ifdef _OPENMP
#pragma omp parallel for num_threads(threads) schedule(dynamic,1)
#endif
    for(long long k=0;k<static_cast<long long>(points.size());++k) {
        try {results[k]=boundary.distance({points[k][0],points[k][1]},options);}
        catch(const std::exception& e) {errors[k]=e.what();}
    }
    auto computed=Clock::now();
    for(const auto& error:errors) if(!error.empty()) throw std::runtime_error(error);
    std::ofstream out(argv[4]); if(!out) throw std::runtime_error("Cannot create output CSV.");
    out<<std::setprecision(17)<<"latitude,longitude,lower_m,upper_m,closest_latitude,closest_longitude,segment_index,evaluations,converged\n";
    std::size_t failed=0; std::uint64_t evaluations=0;
    for(std::size_t k=0;k<points.size();++k) {
        const auto& r=results[k]; failed+=!r.converged; evaluations+=r.evaluations;
        out<<points[k][0]<<','<<points[k][1]<<','<<r.lower_m<<','<<r.upper_m<<','
           <<r.closest_sample.latitude<<','<<r.closest_sample.longitude<<','<<r.segment_index<<','
           <<r.evaluations<<','<<(r.converged?1:0)<<'\n';
    }
    out.close(); if(!out) throw std::runtime_error("Failed writing result CSV.");
    auto end=Clock::now();
    std::cout<<"{\"model\":\"WGS84\",\"operation\":\"point_to_geodesic_boundary\",\"threads\":"<<threads
             <<",\"segments\":"<<segments.size()<<",\"queries\":"<<points.size()<<",\"tolerance_m\":"<<options.tolerance_m
             <<",\"read_ms\":"<<elapsed(start,read)<<",\"prepare_ms\":"<<elapsed(read,prepared)
             <<",\"distance_ms\":"<<elapsed(prepared,computed)<<",\"write_ms\":"<<elapsed(computed,end)
             <<",\"total_ms\":"<<elapsed(start,end)<<",\"evaluations\":"<<evaluations
             <<",\"unresolved_queries\":"<<failed<<",\"interval_certified\":false,\"gpu_used\":false}\n";
    return failed?3:0;
} catch(const std::exception& e) {std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}
