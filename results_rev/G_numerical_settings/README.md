# Numerical settings study of the WGS84 buffer queries (13 November 2026)

Purpose: measure how the returned cell sets depend on two numerical settings of the published buffer program.
This is a sensitivity measurement. It does not certify the floating point arithmetic.

Settings
- allowance: 1, 10 (published) and 100 micrometres. In `coverage.hpp` the allowance is the literal `1e-5`;
  it enters the cell radius, the radius of every source hierarchy node, the radius of every source arc and
  the node lower bound (four places).
- classification tolerance (`--tolerance-m`): 0.1 mm (published), 1 mm and 10 mm. A cell whose distance
  interval is narrower than the tolerance and still contains the threshold is kept as "unresolved_retained".
- The tolerance of the point to segment distance solver (at least 1 mm, set inside `coverage.hpp`) was not varied.

Queries (28)
- the seven published queries;
- nine point sources placed exactly on base mesh vertices of valence 5 and 6 and on the midpoint of a base
  mesh edge, at levels 12 (1000 m), 20 (10 m) and 28 (0.1 m);
- twelve near threshold queries built from point_L20, nose_hill_L14 and point_L28: the distance threshold is
  moved to 2 micrometres on either side of the bound that decided the closest excluded cell or the closest
  confirmed cell of the nominal query.

Files
- `../G_numerical_settings.csv`: one row per run (28 queries x 9 settings = 252 runs).
- `cells/<query>/inputs.txt`: the inputs of the query (17 significant digits).
- `cells/<query>/a<allowance>_t<tolerance>_source_cover.csv`: public indices of the source cover.
- `cells/<query>/a<allowance>_t<tolerance>_returned.csv`: returned cells with status and distance interval.
- `cells/<query>/changed_cells.csv`: every cell added or removed against the reference setting
  (allowance 10 micrometres, tolerance 0.1 mm) of the same query.

Program and commands
- `revision_tools/buffer_settings.cpp`, built by `revision_tools/build_buffer_settings.sh`
  (GCC 11.4, Linux, -O3 -fno-fast-math, CPU halo backend). The build script generates
  `coverage_settings.hpp` from the unchanged `coverage.hpp`; a copy of the generated file is saved as
  `revision_tools/coverage_settings.generated.hpp`.
- `revision_tools/run_settings.sh <buffer project> <binary> <output directory>`.

Check of the program: with the reference setting, the returned cells of the seven published queries are
identical to the published CPU and GPU cell files in `Scalable_Atlas_Ellipsoidal_Buffer/results`.

Outcome
- no run failed; the source cover and the separating ring were the same in all nine settings of every query;
- no setting removed a cell that the reference setting returned;
- at the published tolerance of 0.1 mm the three allowances returned identical sets in all 28 queries;
- a tolerance of 1 mm added at most 3 cells and a tolerance of 10 mm at most 10 cells, all kept as unresolved,
  in the level 28 queries and in one level 20 query (vertex6_L20);
- at a tolerance of 1 mm two level 28 queries returned one cell fewer with the 1 micrometre allowance than
  with the 10 and 100 micrometre allowances.
