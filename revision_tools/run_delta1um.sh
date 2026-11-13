#!/bin/sh
# Repeats the seven published buffer queries at d, d + 1 micrometre and d - 1 micrometre.
# Usage: sh run_delta1um.sh <path to Scalable_Atlas_Ellipsoidal_Buffer> <path to buffer_margins binary> <output directory>
B="$1"; X="$2"; OUT="$3"; mkdir -p "$OUT"
run() { n=$1; shift; s=$(date +%s.%N); "$X" --name $n --delta-m 0.000001 --cells-csv "$OUT"/m_$n.csv "$@" > "$OUT"/s_$n.txt 2>&1; e=$(date +%s.%N); echo "$n $(echo "$e - $s" | bc)" >> "$OUT"/times.txt; }
run nose_hill_L12 --level 12 --distance-m 1000 --input $B/data/nose_hill_segments.csv
run nose_hill_L14 --level 14 --distance-m 1000 --input $B/data/nose_hill_segments.csv
run point_L20 --level 20 --distance-m 10 --point 51.125,-114.115
run point_L28 --level 28 --distance-m 0.1 --point 51.125,-114.115
run dateline_L12 --level 12 --distance-m 1000 --point 0,179.999
run polar_L12 --level 12 --distance-m 1000 --point 89.999,0
run nose_hill_L16 --level 16 --distance-m 1000 --input $B/data/nose_hill_segments.csv
echo done > "$OUT"/done.txt
