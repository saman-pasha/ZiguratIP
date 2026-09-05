#!/bin/sh
# Every Parsi type is an index key, proved on the server path.
#
#   Test/run-keys-e2e.sh
#
# WHAT IT COVERS, and why it is its own script. The engine's own suites prove
# the key folds against defindex expansions (MVCCS-cicili/mvccs_test and
# schema_test). This is the other road to the same tree: a Parsi schema whose
# keys are a Double, a String and a Vector<Double>, compiled by parsi into
# generated C++ that reaches the engine through engine-compat.hpp's
# engine_key64 family -- the overloads nothing else in the tree instantiates,
# and a template member is only checked when something uses it. The procedure
# in Test/rpc/keys.parsi seeds the table and asks each index the question
# only that index can answer; the number it returns is compared here.
#
# The server is started by this script and stopped again; the table lives in
# home/data beside the demo's, and the procedure empties it first.

set -e

HERE=$(cd "$(dirname "$0")" && pwd)
TRUNK=$(cd "$HERE/.." && pwd)
HOME_DIR="$TRUNK/home"

if [ ! -x "$HOME_DIR/bin/ziguratip" ]; then
  echo "build first: make -C $TRUNK" >&2
  exit 1
fi

export ZIGURATIP_HOME="$HOME_DIR"
export LD_LIBRARY_PATH="$HOME_DIR/lib:$LD_LIBRARY_PATH"
export DYLD_LIBRARY_PATH="$HOME_DIR/lib:$DYLD_LIBRARY_PATH"

WORK=$(mktemp -d)
pass=0
fail=0

check() {
  if [ "$2" = "$3" ]; then echo "ok   $1"; pass=$((pass + 1))
  else echo "FAIL $1: got '$2' want '$3'"; fail=$((fail + 1)); fi
}

echo "compiling the table and the procedure"
"$HOME_DIR/bin/parsi" "$HERE/rpc/keys.parsi" > "$WORK/compile.log" 2>&1 || {
  echo "FAIL parsi could not compile demo::prices / demo::keys_check:"; tail -15 "$WORK/compile.log"; exit 1; }
check "the Double, String and Vector keys compiled" "compiled" "compiled"

# the generated C++ names the key types in full: a Vector key is
# BTreeIndex<..., VECTOR< DOUBLE >>, not BTreeIndex<..., VECTOR>
GEN="$HOME_DIR/ld/_DEMO::PRICES_.hpp"
if grep -q "BTreeIndex<[A-Za-z_:]*PRICES, VECTOR< DOUBLE >" "$GEN" 2>/dev/null; then
  check "the Vector key is declared by its whole type" "whole" "whole"
else
  check "the Vector key is declared by its whole type" "$(grep -o 'BTreeIndex<[A-Za-z_:]*PRICES, [^;]*' "$GEN" 2>/dev/null | head -1)" "whole"
fi

# and the procedure's WHEREs reach the INDEXES, not the table: a scan would
# answer the same counts, so the generated C++ is what says which road ran
PROC="$HOME_DIR/tmp/_DEMO::KEYS_CHECK_.cpp"
for want in "IDX_DEMO_PRICES_AMOUNT.cursor_less_than" \
            "IDX_DEMO_PRICES_AMOUNT.cursor_greater_than_equal" \
            "IDX_DEMO_PRICES_AMOUNT.cursor_equal" \
            "IDX_DEMO_PRICES_LABEL.cursor_equal" \
            "IDX_DEMO_PRICES_TAGS.cursor_equal"; do
  if grep -q "$want" "$PROC" 2>/dev/null; then check "the WHERE walks $want" "index" "index"
  else check "the WHERE walks $want" "scan" "index"; fi
done

echo "building the probe"
c++ -Wall -std=c++17 -I"$HOME_DIR/include" -L"$HOME_DIR/lib" -o "$WORK/probe" \
    "$HERE/e2e-probe.cpp" \
    -lConnector -lCore -lStreamIO -lType -lSocketIO -lCryptography \
    -lEncoding -lConfiguration -lThreading -lLibrary -lCompression

LOG="$HOME_DIR/log/keys-e2e-server.log"
mkdir -p "$HOME_DIR/log" "$HOME_DIR/data"
"$HOME_DIR/bin/ziguratip" > "$LOG" 2>&1 &
SERVER_PID=$!
trap 'kill "$SERVER_PID" 2>/dev/null; rm -rf "$WORK"' EXIT

i=0
while [ $i -lt 600 ]; do grep -q "Zeytun is ready" "$LOG" 2>/dev/null && break; i=$((i + 1)); sleep 0.1; done
grep -q "Zeytun is ready" "$LOG" || { echo "FAIL the server never started"; tail -5 "$LOG"; exit 1; }

# twice: the second run meets the rows the first left, deletes them, and must
# answer the same -- an index that lost an unmap would show here
for run in 1 2; do
  OUT=$("$WORK/probe" - - - 127.0.0.1 2160 callret demo::keys_check 2>&1 || true)
  GOT=$(echo "$OUT" | sed -n 's/^returned \([0-9-]*\).*/\1/p')
  check "run $run: two below zero, two at least 1.5, the two zeros one key, 'b' once, [1,2] thrice, [2,1] thrice, [1] never, the twin refused" \
        "$GOT" "10331222"
done

kill -0 "$SERVER_PID" 2>/dev/null && check "the server survived" "yes" "yes" \
                                  || check "the server survived" "it died" "yes"

echo
echo "cases    : $((pass + fail)) run, $fail failed"
if [ "$fail" -eq 0 ]; then echo "result   : PASS"; exit 0; else echo "result   : FAIL"; exit 1; fi
