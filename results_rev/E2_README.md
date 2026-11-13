# E2: distance perturbation of one micrometre (13 November 2026)

`E_buffer_decision_margins.csv` (12 November) repeated every published buffer query at d + 1 mm and d - 1 mm.
`E2_buffer_perturbation_1um.csv` repeats the same seven queries at d + 1 micrometre and d - 1 micrometre
(`buffer_margins --delta-m 0.000001`, same binary, GCC 11.4, Linux). The columns are the same.
The per cell margin files of the base queries are byte identical to the files in `E_cells`.
Outcome: no cell changes in any of the seven queries at +/- 1 micrometre. At +/- 1 mm the level 28 query
changes by 9 and 6 cells (its cells are about 2 cm wide); the other six queries do not change.
Command: `revision_tools/run_delta1um.sh <buffer project> <buffer_margins binary> <output directory>`.
