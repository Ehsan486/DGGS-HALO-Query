#include "bafe/index.hpp"
#include "bafe_exp/cli.hpp"
#include "bafe_exp/geo.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

int main(int argc,char**argv){
 try{
  bafe_exp::Cli cli(argc,argv);const auto input_path=cli.get("input");const auto output_path=cli.get("output","bafe_cells.txt");const auto level=cli.u32("level",20);
  if(input_path.empty())throw std::invalid_argument("--input points.csv is required");
  std::ifstream in(input_path);if(!in)throw std::runtime_error("cannot open "+input_path);
  std::string line;std::getline(in,line); // header expected: lon,lat or longitude,latitude
  std::unordered_set<bafe::CellIndex> ids;ids.reserve(100000);
  std::uint64_t rows=0,failed=0;
  while(std::getline(in,line)){
    if(line.empty())continue;std::stringstream ss(line);std::string a,b;std::getline(ss,a,',');std::getline(ss,b,',');
    try{const double lon=std::stod(a),lat=std::stod(b);ids.insert(bafe::geo::lonlat_to_cell(lon,lat,level));++rows;}catch(...){++failed;}
  }
  std::vector<bafe::CellIndex> sorted(ids.begin(),ids.end());std::sort(sorted.begin(),sorted.end());
  std::ofstream out(output_path);if(!out)throw std::runtime_error("cannot open "+output_path);for(auto id:sorted)out<<id<<'\n';
  std::cout<<"mapped_rows="<<rows<<" unique_cells="<<sorted.size()<<" failed_rows="<<failed<<" level="<<level<<" output="<<output_path<<"\n";
  return failed?2:0;
 }catch(const std::exception&e){std::cerr<<"bafe_geographic_mapper: "<<e.what()<<"\n";return 1;}
}
