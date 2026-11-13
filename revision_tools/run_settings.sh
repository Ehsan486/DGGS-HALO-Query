#!/bin/sh
# Numerical settings study: allowance {1, 10, 100 micrometres} x classification tolerance {0.1, 1, 10 mm}.
# Usage: sh run_settings.sh <path to Scalable_Atlas_Ellipsoidal_Buffer> <path to buffer_settings binary> <output directory>
B="$1"; X="$2"; OUT="$3"; mkdir -p "$OUT"; S="$OUT/G_numerical_settings.csv"; : > "$S"; : > "$OUT/run_log.txt"; first=1
run() { n=$1; shift; "$X" --name $n --out-dir "$OUT/cells" --header $first "$@" >> "$S" 2>> "$OUT/run_log.txt" || echo "$n: program error" >> "$OUT/run_log.txt"; first=0; }
NH="--input $B/data/nose_hill_segments.csv"; PT="--point 51.125,-114.115"
# the seven published cases
run nose_hill_L12 --level 12 --distance-m 1000 $NH
run nose_hill_L14 --level 14 --distance-m 1000 $NH
run nose_hill_L16 --level 16 --distance-m 1000 $NH
run point_L20 --level 20 --distance-m 10 $PT
run point_L28 --level 28 --distance-m 0.1 $PT
run dateline_L12 --level 12 --distance-m 1000 --point 0,179.999
run polar_L12 --level 12 --distance-m 1000 --point 89.999,0
# point sources placed on base mesh vertices (valence 5 and 6) and on a base mesh edge
for L in 12:1000 20:10 28:0.1; do l=${L%%:*}; d=${L##*:}
  run vertex5_L$l --level $l --distance-m $d --base-vertex 5
  run vertex6_L$l --level $l --distance-m $d --base-vertex 6
  run edge_L$l --level $l --distance-m $d --base-edge 0
done
# near threshold queries: the distance threshold is moved to 2 micrometres on either side of the bound that
# decided the closest excluded cell (or the closest confirmed cell) of the nominal query.
# plus2um: that cell keeps a margin of 2 micrometres; minus2um: the threshold has passed its bound by 2 micrometres.
near() { base=$1; shift; for k in excluded confirmed; do
  run near_${k}_plus2um_$base --near $k --offset-m 0.000002 "$@"
  run near_${k}_minus2um_$base --near $k --offset-m -0.000002 "$@"; done; }
near point_L20 --level 20 --distance-m 10 $PT
near nose_hill_L14 --level 14 --distance-m 1000 $NH
near point_L28 --level 28 --distance-m 0.1 $PT
echo done > "$OUT/done.txt"
