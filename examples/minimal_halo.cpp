// Minimal use of the halo query: the rings around the cell that contains one
// point.  Build target: halo_example.  Usage: halo_example [level] [hops]
#include "bafe/bafe.hpp"
#include "bafe_exp/geo.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    const std::uint32_t level = argc > 1 ? std::uint32_t(std::atoi(argv[1])) : 12U;
    const std::uint32_t hops = argc > 2 ? std::uint32_t(std::atoi(argv[2])) : 4U;

    // One seed cell: the cell of level `level` that contains a point in Calgary
    // (longitude, latitude in degrees).  Cells enter and leave a query as 64 bit
    // hierarchical indices.
    const std::vector<bafe::CellIndex> seeds = {
        bafe::geo::lonlat_to_cell(-114.115, 51.125, level)};

    bafe::QueryOptions options;
    options.level = level;
    options.hops = hops;            // number of rings
    options.max_output_cells = 0;   // 0 selects the automatic output budget

    // make_cpu_engine(): serial reference.  make_atomic_cpu_engine(): multithreaded.
    // make_cuda_engine(): GPU (only when the project is built with CUDA).
    auto engine = bafe::make_cpu_engine();
    const bafe::HaloResult result = engine->halo(seeds, options);

    // layer_offsets has hops + 1 entries; ring k is cells[layer_offsets[k-1] .. layer_offsets[k]).
    std::cout << "level " << level << ", " << hops << " rings, " << result.size() << " cells\n";
    for (std::uint32_t k = 1; k <= hops; ++k) {
        std::cout << "  ring " << k << ": "
                  << result.layer_offsets[k] - result.layer_offsets[k - 1] << " cells\n";
    }
    return 0;
}
