#!/bin/sh
# Decision margins of the seven published buffer queries, and the complete query at d + 1 mm and d - 1 mm.
# Usage: sh run_margins.sh <path to buffer_program> <path to buffer_margins binary> <output directory>
# (The run of 12 November 2026 used the same commands with absolute paths of the measuring machine.)
B="$1"; X="$2"; OUT="$3"; mkdir -p "$OUT"
run() { n=$1; shift; "$X" --name $n --cells-csv "$OUT"/m_$n.csv "$@" > "$OUT"/s_$n.txt 2>&1; }
run nose_hill_L12 --level 12 --distance-m 1000 --input $B/data/nose_hill_segments.csv
run nose_hill_L14 --level 14 --distance-m 1000 --input $B/data/nose_hill_segments.csv
run nose_hill_L16 --level 16 --distance-m 1000 --input $B/data/nose_hill_segments.csv
run point_L20 --level 20 --distance-m 10 --point 51.125,-114.115
run point_L28 --level 28 --distance-m 0.1 --point 51.125,-114.115
run dateline_L12 --level 12 --distance-m 1000 --point 0,179.999
run polar_L12 --level 12 --distance-m 1000 --point 89.999,0
echo done > "$OUT"/done.txt
