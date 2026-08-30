#!/bin/sh
# Build and run the two engine benchmarks against home/lib/libMVCCS.so --
# the same compile line as consumer-test in ../build.sh, so a benchmark is
# a consumer like any other. Run from anywhere; the engine must have been
# built first (sh ../build.sh, or make at the workspace root).
#
#   sh bench/build.sh            # both, at 7000 rows
#   sh bench/build.sh 20000      # another N
#
# Every number in ../README.md's "Insertion time, measured" came from here.
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
. "$ROOT/tools/cc/env.sh"
LIBDIR="$ROOT/home/lib"
INCDIR="$ROOT/home/include"
N=${1:-7000}
set -e
for b in insert delete; do
  "$CXX" -O3 -std=gnu++17 -Wno-deprecated-declarations "$HERE/$b-bench.cpp" \
    -o "$HERE/${b}_bench" -I"$HERE/.." -I"$INCDIR" \
    -L"$LIBDIR" -lMVCCS -lCore -lStreamIO -lpthread -Wl,-rpath,"$LIBDIR"
done
echo "== insert_bench $N"
"$HERE/insert_bench" "$N"
for mode in equal equal2 equal3; do
  echo "== delete_bench $N $mode"
  "$HERE/delete_bench" "$N" "$mode" | grep -v ' done, '
done
