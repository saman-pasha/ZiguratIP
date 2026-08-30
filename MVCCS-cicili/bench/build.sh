#!/bin/sh
# Build and run the two engine benchmarks against home/lib/libMVCCS.so --
# the same compile line as consumer-test in ../build.sh, so a benchmark is
# a consumer like any other. Run from anywhere; the engine must have been
# built first (sh ../build.sh, or make at the workspace root).
#
#   sh bench/build.sh            # both, at 7000 rows
#   sh bench/build.sh 20000      # another N
#   STORE_MAP=1 sh bench/build.sh  # the same over a memory-mapped store
#
# Every number in ../README.md's "Insertion time, measured" came from here.
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
. "$ROOT/tools/cc/env.sh"
LIBDIR="$ROOT/home/lib"
INCDIR="$ROOT/home/include"
N=${1:-7000}
set -e
for b in insert delete composite; do
  "$CXX" -O3 -std=gnu++17 -Wno-deprecated-declarations "$HERE/$b-$( [ $b = composite ] && echo check || echo bench ).cpp" \
    -o "$HERE/${b}_$( [ $b = composite ] && echo check || echo bench )" -I"$HERE/.." -I"$INCDIR" \
    -L"$LIBDIR" -lMVCCS -lCore -lStreamIO -lpthread -Wl,-rpath,"$LIBDIR"
done
echo "== insert_bench $N"
"$HERE/insert_bench" "$N"
echo "== composite_check $N/7 pairs, then past 1024 pages"
"$HERE/composite_check" 3 $(( N / 7 ))
"$HERE/composite_check" 3 4000 | tail -1
for mode in equal equal2 equal3; do
  echo "== delete_bench $N $mode"
  "$HERE/delete_bench" "$N" "$mode" | grep -v ' done, '
done
