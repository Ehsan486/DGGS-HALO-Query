#!/bin/sh
# Builds buffer_margins against the WGS84 buffer sources (CPU only).
#   sh build_buffer_margins.sh /path/to/Scalable_Atlas_Ellipsoidal_Buffer /path/to/output_binary
set -e
B="$1"; OUT="${2:-./buffer_margins}"; HERE="$(cd "$(dirname "$0")" && pwd)"
gcc -O3 -fno-fast-math -c "$B/distance/vendor/geodesic.c" -I"$B/distance/vendor" -o /tmp/geodesic.o
g++ -O3 -fno-fast-math -std=c++17 -fopenmp -DBAFE_HAS_OPENMP=1 -DBAFE_HAS_CUDA=0 \
  -I"$B/src" -I"$B/include" -I"$B/bafe/include" -I"$B/bafe/src" -I"$B/distance/include" -I"$B/distance/vendor" \
  "$HERE/buffer_margins.cpp" "$B/distance/src/wgs84_distance.cpp" "$B/bafe/src/factory.cpp" "$B/bafe/src/halo_cpu.cpp" \
  "$B/bafe/src/halo_cuda_stub.cpp" /tmp/geodesic.o -lm -o "$OUT"
