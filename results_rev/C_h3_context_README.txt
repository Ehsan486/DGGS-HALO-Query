C_h3_context.csv  (2026-11-12)
Context comparison of the Pentakis halo (serial flat table, compact index) with the union of H3 gridDisk calls.
Seed points: calgary_building_representative_points.csv (100,000 points). One CPU thread, wall clock around the
complete call, 5 warmup + 30 timed calls, discovered set sized to twice the realized keys ("tight") in both programs.
Machine: Intel Core i7 12700K, inside the Linux virtual machine of the Claude desktop app (Ubuntu 22.04, GCC 11.4, -O3).
The Pentakis serial time in this virtual machine (714 ms at level 28) equals the native Windows time of the earlier
sessions (715 ms), so the virtual machine does not change the single thread result materially.
H3 version 4.5.0 built from the tagged source (github.com/uber/h3).
Programs: benchmarks/bafe_representation_ablation.cpp --only-compact, benchmarks/h3_grid_disk_union.cpp.
