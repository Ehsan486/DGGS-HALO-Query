#pragma once
#include "bafe/bafe.hpp"
#include "bafe/index.hpp"
#include "internal.hpp"
#include "topology.hpp"
#include "bafe_exp/geo.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>
#include <random>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace bafe_exp {

inline bafe::CellIndex bafe_seed(std::uint32_t level,bool seam=false) {
    auto id=bafe::root(seam?0U:17U);
    for(std::uint32_t d=0;d<level;++d) id=bafe::child(id,d,seam?0U:((d*3U+1U)&3U));
    return id;
}

inline std::vector<bafe::CellIndex> bafe_scattered(std::uint32_t level,std::uint64_t count,std::uint64_t seed=0xBAFE5EEDULL) {
    std::mt19937_64 rng(seed);
    std::unordered_set<bafe::CellIndex> unique;
    unique.reserve(static_cast<std::size_t>(count*1.4)+64);
    while(unique.size()<count) {
        auto id=bafe::root(static_cast<std::uint32_t>(rng()%60U));
        for(std::uint32_t d=0;d<level;++d) id=bafe::child(id,d,static_cast<std::uint32_t>(rng()&3U));
        unique.insert(id);
    }
    return {unique.begin(),unique.end()};
}

inline std::vector<bafe::CellIndex> bafe_compact(std::uint32_t level,std::uint64_t count,bool seam=false) {
    if(count==0) return {};
    const auto center=bafe_seed(level,seam);
    auto cpu=bafe::make_cpu_engine();
    std::uint32_t radius=std::max<std::uint32_t>(2U,static_cast<std::uint32_t>(std::ceil(std::sqrt(double(count)))));
    for(;;){
        bafe::QueryOptions q; q.level=level; q.hops=radius; q.max_output_cells=std::max<std::uint64_t>(count*4U,10000U);
        auto disk=cpu->halo(std::vector<bafe::CellIndex>{center},q);
        std::vector<bafe::CellIndex> cells{center}; cells.insert(cells.end(),disk.cells.begin(),disk.cells.end());
        if(cells.size()>=count){cells.resize(static_cast<std::size_t>(count));return cells;}
        radius=radius*3U/2U+1U;
    }
}

inline std::vector<bafe::CellIndex> bafe_from_lonlat_csv(const std::string& path,std::uint32_t level);

} // namespace bafe_exp
