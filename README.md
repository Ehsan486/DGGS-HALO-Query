# DGGS-HALO-Query

Exact halo queries on a triangular discrete global grid system (DGGS), with CPU and GPU (CUDA) implementations.

A **halo query** takes a set of seed cells and a number of steps `K`, and returns every cell that can be reached within `K` steps across cell edges, grouped into rings `H_1 ... H_K`. The grid is the Pentakis dodecahedron (60 triangular base faces) refined to level 28. Adjacent cells are computed from integer arithmetic and 180 small records for the base mesh edges; no adjacency table is stored.

This repository contains the source code, the tests, the data preparation scripts, and the measurement records of the paper

> E. Moradi and F. F. Samavati, "From Topological Distance to Spatial Buffers: Efficient Halo Queries on Triangular DGGS", submitted to the *ISPRS International Journal of Geo-Information* (under review, 2026).

It is research code. It was built and measured on one workstation (see "Tested configurations").

## Contents

| Folder | Content |
|---|---|
| `bafe/` | The halo library (`bafe` is the internal name of the library). `include/bafe/` is the public interface, `src/` the implementations, `tests/` the correctness tests. |
| `include/bafe_exp/` | Helper headers for the experiment programs (geometry, point files, command line, CSV output). |
| `examples/` | A minimal program that calls the library. |
| `benchmarks/` | The measurement and validation programs. |
| `scripts/` | `build.ps1` (build and test on Windows) and `run_revision.ps1` (repeats the halo measurements of the paper). |
| `analysis/` | Dataset download and preparation, and the scripts that turn records into tables and figures. |
| `datasets/` | Checksums of the downloaded City of Calgary files and the prepared point files that were measured. |
| `buffer_program/` | A separate program for buffer queries on the WGS84 ellipsoid, with its inputs and saved results. It contains its own, earlier copy of the halo library. |
| `revision_tools/` | Programs for the numerical sensitivity study of the buffer queries. |
| `results_rev/` | The measurement records that the paper reports. |
| `paper_scripts/` | Scripts for one table and three drawings of the paper. |
| `docs/` | `MANIFEST.md` (which record and program is behind each table and figure), `ENVIRONMENT_AND_LICENSES.md`, `CHANGES.md`. |

## Implementations

All implementations return the same rings. The interface is `bafe::HaloEngine` in `bafe/include/bafe/bafe.hpp`.

| Function | Implementation | File |
|---|---|---|
| `make_cpu_engine()` | Serial reference with a standard container | `bafe/src/halo_cpu.cpp` |
| `make_atomic_cpu_engine(1)` | Serial, flat hash table (the "serial CPU implementation" of the paper) | `bafe/src/halo_cpu_atomic.cpp` |
| `make_atomic_cpu_engine(n)` | Multithreaded, flat hash table with compare and swap | `bafe/src/halo_cpu_atomic.cpp` |
| `make_parallel_cpu_engine(n)` | Earlier multithreaded version | `bafe/src/halo_cpu_parallel.cpp` |
| `make_cuda_engine()` | GPU | `bafe/src/halo_cuda.cu` |

The 180 base mesh edge records are in `bafe/src/atlas.hpp`; the adjacency arithmetic is in `bafe/src/topology.hpp`.

## Requirements

- C++17 compiler and CMake 3.24 or newer.
- For the GPU implementation: an NVIDIA GPU and the CUDA toolkit.
- OpenMP for the multithreaded CPU implementations (optional).
- Python 3 with the packages in `analysis/requirements.txt`, only for data preparation and figures.

## Build

Windows with CUDA (Visual Studio C++ build tools, CMake, Ninja, CUDA toolkit):

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

This configures the project into `build\`, compiles the main programs, and runs the correctness tests and the example. Add `-All` to compile every program, `-CpuOnly` to build without CUDA, and `-CudaArch 86` (for example) for another GPU generation.

Without CUDA, on Linux or any other system:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBAFE_ENABLE_CUDA=OFF
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/halo_example
```

In a build without CUDA, `./build/bafe_global_validation --no-gpu` validates the CPU implementations on the 40 global cases.

CMake options: `BAFE_ENABLE_CUDA` (default `ON`), `BAFE_ENABLE_H3` and `BAFE_ENABLE_S2` (default `OFF`; they need installed H3 and S2 CMake packages).

## Minimal use

```cpp
#include "bafe/bafe.hpp"
#include "bafe_exp/geo.hpp"

std::vector<bafe::CellIndex> seeds = { bafe::geo::lonlat_to_cell(-114.115, 51.125, 12) };

bafe::QueryOptions options;
options.level = 12;   // level of the seed cells and of the result
options.hops  = 4;    // number of rings

auto engine = bafe::make_cpu_engine();          // or make_atomic_cpu_engine(), make_cuda_engine()
bafe::HaloResult result = engine->halo(seeds, options);

// ring k is result.cells[ result.layer_offsets[k-1] .. result.layer_offsets[k] )
```

The complete program is `examples/minimal_halo.cpp`. Cells are 64 bit hierarchical indices (`bafe/include/bafe/index.hpp`). If the result does not fit into the output capacity, the query reports an error and returns no partial result.

## Repeating the measurements of the paper

The records that the paper reports are in `results_rev/`. `docs/MANIFEST.md` lists, for every table and figure, the record and the program behind it.

| Record | Content | Program | How to run |
|---|---|---|---|
| `A_session*_matrix.csv`, `A_session*_calls.csv` | Timing of 81 configurations, three sessions | `benchmarks/bafe_realworld_halo.cpp` | `scripts\run_revision.ps1 -Stage A` |
| `B_representation.csv` | Representation study on one CPU thread | `benchmarks/bafe_representation_ablation.cpp` | `-Stage B` |
| `C_h3_context.csv` | H3 grid disk union as context | `benchmarks/h3_grid_disk_union.cpp` | build with `-DBAFE_ENABLE_H3=ON`; see `results_rev/C_h3_context_README.txt` |
| `D_controlled_variants.csv` | Variants of the GPU implementation | `benchmarks/bafe_realworld_halo.cpp` | `-Stage D` |
| `E_*`, `E2_*` | Decision margins and distance changes of the buffer queries | `revision_tools/buffer_margins.cpp` | `revision_tools/run_margins.sh`, `run_delta1um.sh` |
| `F_global_validation.csv` | Ring by ring validation, 40 global cases | `benchmarks/bafe_global_validation.cpp` | `-Stage F` |
| `G_numerical_settings*` | Numerical settings study of the buffer queries | `revision_tools/buffer_settings.cpp` | `revision_tools/run_settings.sh` |
| `buffer_program/results/` | Complete buffer queries on WGS84 | `buffer_program/` | `buffer_program/scripts/run_experiments.py` |

Halo measurements (Windows, CUDA). The script builds the project and runs the stages; it writes to `results_local\` and does not change `results_rev\`:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_revision.ps1            # everything
powershell -ExecutionPolicy Bypass -File .\scripts\run_revision.ps1 -Quick     # short test run
python analysis\analyze_revision.py results_local
python analysis\plot_revision_results.py results_local
```

Times depend on the machine. The returned cells do not.

Numerical sensitivity of the buffer queries (GCC with OpenMP, CPU only):

```sh
sh revision_tools/build_buffer_margins.sh  buffer_program ./buffer_margins
sh revision_tools/run_margins.sh           buffer_program ./buffer_margins  out_margins
sh revision_tools/run_delta1um.sh          buffer_program ./buffer_margins  out_1um
sh revision_tools/build_buffer_settings.sh buffer_program ./buffer_settings
sh revision_tools/run_settings.sh          buffer_program ./buffer_settings out_settings
```

Other programs in `benchmarks/` (`bafe_synthetic_benchmark`, `bafe_hierarchy_benchmark`, `bafe_dual_index_ablation`, `bafe_rhombus_cpu_ablation`, `bafe_geographic_mapper`, `bafe_canada_regression`, `bafe_calgary_clearance`, `bafe_spatial_semantics`, `bafe_cuda_ablations`, and the remaining H3 and S2 programs) belong to earlier experiments. They are kept for completeness. The paper does not report their results, some of them need input files that are not part of this repository, and the S2 programs were not rebuilt for this release.

## Data

The point sets come from three open datasets of the City of Calgary (Buildings, Street Centreline, City Boundary). `datasets/download_lock.json` records the download of 20 July 2026 with the SHA-256 checksum of every file. The raw files (383 MB) are not in the repository:

```sh
python analysis/download_calgary_data.py      # writes datasets/raw/
python analysis/prepare_real_world_layers.py  # writes datasets/processed/real_world_layers/
```

The prepared point files that were measured are in `datasets/processed/real_world_layers/`, with their checksums in `metadata.json`. The City of Calgary datasets are provided under the City of Calgary Open Data Terms (https://data.calgary.ca/d/Open-Data-Terms/u45n-7awa). The datasets may have changed since the download; the checksums identify the version that was used.

## Tested configurations

| Configuration | What was run |
|---|---|
| Windows 11, Visual C++ 19.51, CUDA toolkit 13.3, Intel Core i7 12700K, NVIDIA GeForce RTX 4070 | All measurements of the paper |
| Linux, GCC 11.4, without CUDA | Build of all CPU programs, the correctness tests, the example, the H3 programs with H3 4.5.0, and the numerical sensitivity programs |

No other compiler, GPU, or operating system was tested. The scripts for Windows assume Ninja and the Visual Studio C++ build tools.

## Scope and limits

- One base mesh (Pentakis dodecahedron), levels 0 to 28.
- The GPU implementation was faster than the CPU implementations for large halos and slower for small ones; the paper gives the measurements and a selection rule.
- Buffer queries on the WGS84 ellipsoid use double precision bounds with an engineering allowance. The bounds are not certified in floating point arithmetic; the paper reports a sensitivity study.
- `buffer_program/` contains the version of the GPU implementation with which its results were measured, which is older than the one in `bafe/`.
- Some run logs and JSON reports in `results_rev/` and `buffer_program/results/` contain absolute paths of the measuring machine. They are records and were left unchanged.

## Citation

cff-version: 1.2.0
message: "If you use this software, please cite the paper given under preferred-citation."
title: "DGGS-HALO-Query: exact halo queries on a triangular discrete global grid system"
type: software
version: "1.2.0"
authors:
  - family-names: Moradi
    given-names: Ehsan
    affiliation: "Department of Computer Science, University of Calgary"
  - family-names: Samavati
    given-names: "Faramarz F."
    affiliation: "Department of Computer Science, University of Calgary"
repository-code: "https://github.com/Ehsan486/DGGS-HALO-Query"
preferred-citation:
  type: article
  title: "From Topological Distance to Spatial Buffers: Efficient Halo Queries on Triangular DGGS"
  authors:
    - family-names: Moradi
      given-names: Ehsan
    - family-names: Samavati
      given-names: "Faramarz F."
  journal: "ISPRS International Journal of Geo-Information"
  year: 2026
  status: submitted

## License

No license has been specified yet. Until a license file is added, all rights are reserved by the authors. Third party components keep their own licenses, which are listed in `docs/ENVIRONMENT_AND_LICENSES.md`.
