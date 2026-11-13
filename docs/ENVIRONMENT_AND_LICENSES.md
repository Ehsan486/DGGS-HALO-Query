# Environment, dependencies and licenses

## Measuring machine (halo timing, validation, GPU variants, representation study, buffer timing)

- Intel Core i7 12700K (8 performance cores, 4 efficiency cores, 20 logical processors), 64 GB memory
- NVIDIA GeForce RTX 4070, 12 GB. Driver 610.88 for the measurements of 12 November 2026 (`results_rev/environment_utf8.txt`); driver 595.79 for the buffer timing of 11 September 2026 (`buffer_program/results/experiment_manifest.json`)
- Windows 11 Pro, build 26200; power plan "High performance"
- Microsoft Visual C++ 19.51; CUDA toolkit 13.3 (nvcc V13.3.73); CMake with Ninja Multi-Config
- The timing programs request the high priority class and disable the throttling of execution speed for their own process (`benchmarks/process_control.cpp`). The placement of the threads on the cores was not measured.

## Linux virtual machine on the same workstation (H3 context, buffer decision margins, distance changes, numerical settings study)

- GCC 11.4, `-O3 -fno-fast-math`, OpenMP; 2 virtual processors; CPU implementation of the halo query only
- H3 4.5.0 for the H3 context measurement

## Third party components

| Component | Version | License | Where |
|---|---|---|---|
| GeographicLib geodesic routines in C (C. F. F. Karney) | as vendored | MIT/X11 | `buffer_program/distance/vendor` (`LICENSE.txt`, `SOURCE.md`) |
| H3 | 4.5.0 | Apache 2.0 | not included; https://github.com/uber/h3 |
| S2 Geometry | not used for the paper | Apache 2.0 | not included |
| CUDA toolkit | 13.3 | NVIDIA software license | not included |
| Python packages for data preparation and figures | geopandas >= 1.0, shapely >= 2.0, pyproj >= 3.6, pandas >= 2.0, requests >= 2.31, matplotlib >= 3.8, numpy >= 1.26 | licenses of the packages | `analysis/requirements.txt` |
| City of Calgary datasets: Buildings (uc4c-6kbd), Street Centreline (4dx8-rtm5), City Boundary (7t9h-2z9s) | downloaded 20 July 2026; checksums in `datasets/download_lock.json` | City of Calgary Open Data Terms (https://data.calgary.ca/d/Open-Data-Terms/u45n-7awa) | raw files not included; prepared point files in `datasets/processed/real_world_layers` |
| Nose Hill Park boundary (a parks record of the City of Calgary) | as included | City of Calgary Open Data Terms | `buffer_program/data` |

## License of the code of this repository

Not specified yet.
