#pragma once

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace bafe_exp {

inline std::string trim_copy(std::string s) {
    auto not_space=[](unsigned char c){return !std::isspace(c);};
    s.erase(s.begin(),std::find_if(s.begin(),s.end(),not_space));
    s.erase(std::find_if(s.rbegin(),s.rend(),not_space).base(),s.end());
    return s;
}

inline std::vector<std::pair<double,double>> read_lonlat_csv(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot open point CSV: "+path);
    std::string line;
    if (!std::getline(f,line)) return {};
    std::vector<std::pair<double,double>> out;
    while (std::getline(f,line)) {
        line=trim_copy(line);
        if (line.empty()) continue;
        const auto comma=line.find(',');
        if (comma==std::string::npos) continue;
        const double lon=std::stod(trim_copy(line.substr(0,comma)));
        const double lat=std::stod(trim_copy(line.substr(comma+1)));
        out.emplace_back(lon,lat);
    }
    return out;
}

} // namespace bafe_exp
