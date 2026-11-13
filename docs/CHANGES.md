# Changes

## Version 1.2.0 (November 2026, revision of the paper)

Library
- `QueryOptions::reuse_output`: the caller can read the result from memory that the implementation keeps between calls (`HaloResult::data()`, `size()`).
- `bafe/src/halo_cpu_atomic.cpp`: flat hash table with compare and swap; used for the serial (one thread) and the multithreaded CPU measurements.
- `bafe/src/halo_cuda.cu`: switches for the four mechanisms that the paper measures (`QueryOptions::cuda_tuning`) and the reused output delivery.

Measurement programs
- `benchmarks/bafe_realworld_halo.cpp`: wall clock around the complete call (`--timer wall`), two delivery scopes (`--delivery fresh|reused|both`), every timed call saved (`--samples-csv`), several implementations in one process.
- `benchmarks/bafe_representation_ablation.cpp` (new): compact index, hierarchical index converted at every expansion, two BIM style variants written by the authors, and a stored neighbor table, on one CPU thread.
- `benchmarks/bafe_global_validation.cpp` (new): 40 global cases, every ring of every implementation against the serial reference, and the behavior after a capacity error.
- `benchmarks/h3_grid_disk_union.cpp` (new): union of H3 grid disk calls, as context.
- `benchmarks/process_control.cpp` (new): the timing programs request the high priority class and disable the throttling of execution speed for their own process.
- `revision_tools/` (new): decision margins, distance changes and numerical settings study of the WGS84 buffer queries.

## Differences between this repository and the folder in which the measurements were made

The halo library, the measurement programs and the records are the measured files. For publication the following was changed:
- Code for a separate study of three dimensional queries, which the paper does not use, was left out together with its build targets.
- The CMake options `BAFE_ENABLE_H3` and `BAFE_ENABLE_S2` are `OFF` by default, and a build target for `h3_grid_disk_union` was added.
- `scripts/run_revision.ps1` takes its dataset folder from `datasets/`, writes to `results_local/` and builds into `build/` by default. The stages and their commands are unchanged.
- `scripts/build.ps1` and `examples/minimal_halo.cpp` were added.
- `revision_tools/plot_wgs84_buffer_figure.py` and `revision_tools/run_margins.sh` take their folders as arguments.
