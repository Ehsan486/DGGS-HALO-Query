#!/bin/sh
# Builds buffer_settings against the WGS84 buffer sources (CPU only).
#   sh build_buffer_settings.sh /path/to/Scalable_Atlas_Ellipsoidal_Buffer /path/to/output_binary
# coverage_settings.hpp is generated here from the unchanged src/coverage.hpp:
# the literal 1e-5 (the 10 micrometre allowance, four occurrences) becomes the variable g_allowance.
set -e
B="$1"; OUT="${2:-./buffer_settings}"; HERE="$(cd "$(dirname "$0")" && pwd)"; GEN="$(dirname "$OUT")/gen_settings"
mkdir -p "$GEN"
n=$(grep -o "1e-5" "$B/src/coverage.hpp" | wc -l)
[ "$n" = "4" ] || { echo "expected four occurrences of 1e-5 in coverage.hpp, found $n"; exit 1; }
sed -e 's/1e-5/g_allowance/g' "$B/src/coverage.hpp" \
  | sed -e 's/^namespace coverage {$/namespace coverage {\ninline double g_allowance = 0.00001; \/\/ settings study: stands for the literal 1e-5 of coverage.hpp/' \
  > "$GEN/coverage_settings.hpp"
m=$(grep -o "g_allowance" "$GEN/coverage_settings.hpp" | wc -l)
[ "$m" = "5" ] || { echo "replacement check failed ($m)"; exit 1; }
gcc -O3 -fno-fast-math -c "$B/distance/vendor/geodesic.c" -I"$B/distance/vendor" -o "$GEN/geodesic.o"
g++ -O3 -fno-fast-math -std=c++17 -fopenmp -DBAFE_HAS_OPENMP=1 -DBAFE_HAS_CUDA=0 \
  -I"$GEN" -I"$B/src" -I"$B/include" -I"$B/bafe/include" -I"$B/bafe/src" -I"$B/distance/include" -I"$B/distance/vendor" \
  "$HERE/buffer_settings.cpp" "$B/distance/src/wgs84_distance.cpp" "$B/bafe/src/factory.cpp" "$B/bafe/src/halo_cpu.cpp" \
  "$B/bafe/src/halo_cuda_stub.cpp" "$GEN/geodesic.o" -lm -o "$OUT"
