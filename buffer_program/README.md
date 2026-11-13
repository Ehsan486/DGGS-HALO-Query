# WGS84 shoreline buffers on the Pentakis DGGS

This module accepts a complete collection of boundary segments, builds a conservative source cell cover, expands complete topological rings with the existing CUDA or CPU implementation, and tests the curved footprints of the visited cells against the WGS84 distance request. It returns public hierarchical indices. Levels 0 through 28 are supported, subject to explicit capacity limits.

## Run the tested example

From this folder, after `build_windows.ps1`:

```powershell
.\build_cuda\Release\atlas_buffer.exe --input data/nose_hill_segments.csv --backend cuda --level 14 --distance-m 1000 --output results/my_buffer --verify
```

The file already contains all 1,482 supplied boundary segments. To prepare another GeoJSON polygon, line or point:

```powershell
python scripts/prepare_source.py your_source.geojson data/your_segments.csv
```

No simplification or ring concatenation is performed. Polygon rings, including holes and separate components, contribute boundary curves. **Polygon interiors do not become source geometry.** This implements distance from a shoreline, on both sides. A filled lake buffer requires a separately validated filled geometry cover; do not silently interpret this output as that different operation. Agricultural land selection is a subsequent spatial intersection.

Small level 28 example:

```powershell
.\build_cuda\Release\atlas_buffer.exe --point 51.125,-114.115 --level 28 --distance-m 0.1 --backend cuda --output results/point28 --verify
```

## Output and timing

`*_cells.csv` stores exact 64 bit public indices as decimal text, status, cell distance bounds and a display centre. Read indices as strings or uint64, never floating point. `*_report.json` stores source preparation, source covering, backend initialization, GPU upload, clearing, decoding and seed insertion, expansion and hashing, encoding, download, whole ring distance tests, cell footprint filtering, validation, and output writing separately.

`query_total_ms` includes a fresh backend initialization but excludes validation and file writing. Add `output_file_ms` for the recorded query plus cell file cost. The JSON report write itself is not timed. GPU event timings are totals over all replay calls. The reported device memory is the final call's retained workspace, not total process RAM. Hashing inside traversal is not falsely reported as a separately isolated measurement.

The controller tries depths 1, 2, 4, ... and replays complete queries until every cell of the last ring has a distance lower bound larger than the requested distance. Within each CUDA halo call the iterations stay on the device. Geometric tests run on the CPU, with eight OpenMP workers by default; the complete spatial workflow is **hybrid**, not fully GPU resident. `--backend cpu` is often faster for small one time buffers. No speedup against BIM is claimed.

## Precision and complete coverage

Source edges are explicitly interpreted as shortest WGS84 geodesics. This differs from the longitude and latitude linear edge interpretation of generic GeoJSON. The supplied vertices are preserved. The ellipsoid uses a = 6,378,137 m and f = 1/298.257223563; GeographicLib solves direct and inverse distances using Karney's algorithms.

The original spherical DGGS is mapped to WGS84 using its longitude and latitude as geodetic coordinates. Cell edges are the resulting curved footprints, **not** substituted ellipsoidal geodesic chords. A metric radius bound follows from max(M,N) = a²/b < 6,400,000 m and the radial projection derivative bound. All source segments are searched with geodesic interval subdivision and metric bounding balls. Final cell filtering subdivides the planar triangle and bounds the complete curved patches; it does not test centroids alone.

Default cell classification tolerance is 0.0001 m, with a 0.00001 m engineering allowance. Unresolved cells are retained and labelled, never discarded. A cell classified outside has a lower bound beyond the request; an accepted resolved cell has a sample upper bound within it. A zero unresolved count means every returned cell was resolved under these numerical bounds, **not a formal interval arithmetic proof**. The implementation uses ordinary double precision and engineering allowances. It is not an unconditional mathematical certificate, and it cannot correct inaccuracies or missing components in the input dataset. These limitations also appear in the paper.

`--max-cells` bounds source count and each halo output separately; `--max-hops` limits replay depth. Capacity exhaustion or an incomplete stopping test fails with a nonzero exit code and does not accept a partial new result. Existing files from earlier runs are not deleted. Use unique output prefixes and check exit codes.

## Reproduce the evidence

```powershell
python scripts/run_experiments.py
python scripts/validate_sample_coverage.py
python scripts/plot_results.py
```

Optional Python packages for validation and plots: numpy, pyproj, matplotlib. All numerical query processing itself is native C++ and CUDA. Raw reports and output sets are included. Three repetitions per case are separate process runs, not steady state GPU benchmarks.

The independent point locator uses a geometric solve without the ACM. Additional validation compares CPU and GPU rings, and compares final sets with a spatial hierarchy search that does not use adjacency. These checks share the source geometry and distance model, and the pyproj check uses the same geodesic algorithm family; they are not advertised as fully independent proofs.

See `results/FIGURE_DESCRIPTION.md` for the exact meaning of the supplied scientific figure. GeographicLib's license and pinned source attribution are retained in `distance/vendor/`. Author approval of a license for the project specific sources is still needed before public redistribution.
