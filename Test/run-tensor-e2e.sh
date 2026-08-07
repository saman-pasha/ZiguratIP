#!/bin/sh
# A tensor through RPC: Vector<Float> out, an answer about it back.
#
#   Test/run-tensor-e2e.sh
#
# WHAT IT COVERS, and why it is its own script. Everything else that packs a
# Vector does it inside one process. This is the only path where one is written
# to a socket by a client and read off it by the server, which is where every
# defect in Vector's packing lived -- and they lived there undisturbed because
# a template member is only checked when something uses it, and nothing did.
#
# The values are WHOLE NUMBERS. An integer under 2^24 is exact in a float, so
# what comes back is a statement about the transport and nothing else. The
# first version of this sent i/1000 and expected the sum of i after the far
# side multiplied by a thousand; it answered 306554 where 306936 was expected,
# because i/1000 is not exact and the product truncates just below. The tensor
# had crossed perfectly and the arithmetic was wrong.
#
# BEFORE THE FIX this did not fail politely. lib_DEMO::TENSOR_SUM_.so carried
# an undefined Vector<Float>::operator[] -- a shared object links with those
# and resolves them when it is loaded -- so the server died on the call with
# `undefined symbol', taking the connection with it.

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

echo "compiling the procedure"
"$HOME_DIR/bin/parsi" "$HERE/rpc/tensor.parsi" > "$WORK/compile.log" 2>&1 || {
  echo "FAIL parsi could not compile demo::tensor_sum:"; tail -15 "$WORK/compile.log"; exit 1; }

echo "building the probe"
c++ -Wall -std=c++11 -I"$HOME_DIR/include" -L"$HOME_DIR/lib" -o "$WORK/probe" \
    "$HERE/e2e-probe.cpp" \
    -lConnector -lCore -lStreamIO -lType -lSocketIO -lCryptography \
    -lEncoding -lConfiguration -lThreading -lLibrary -lCompression

# An undefined symbol in the object is the pre-fix failure, and it is visible
# without running anything. Checked here so a regression is named rather than
# arriving as a dead server.
if nm -D --undefined-only "$HOME_DIR/ld/lib_DEMO::TENSOR_SUM_.so" 2>/dev/null | grep -q "VectorINS_5FloatEEix"; then
  check "the object has no undefined Vector accessor" "undefined operator[]" "none"
else
  check "the object has no undefined Vector accessor" "none" "none"
fi

LOG="$HOME_DIR/log/tensor-e2e-server.log"
mkdir -p "$HOME_DIR/log" "$HOME_DIR/data"
"$HOME_DIR/bin/ziguratip" > "$LOG" 2>&1 &
SERVER_PID=$!
trap 'kill "$SERVER_PID" 2>/dev/null; rm -rf "$WORK"' EXIT

i=0
while [ $i -lt 600 ]; do grep -q "Zeytun is ready" "$LOG" 2>/dev/null && break; i=$((i + 1)); sleep 0.1; done
grep -q "Zeytun is ready" "$LOG" || { echo "FAIL the server never started"; tail -5 "$LOG"; exit 1; }

# 0 and 1 are the edges -- an empty vector is not a null one, and a single
# element is the case where a length and a count are easy to confuse. 784 is an
# MNIST digit, which is the size this was wanted for.
for n in 0 1 2 784 4096; do
  EXPECT=$(python3 -c "print(sum(range($n)))")
  OUT=$("$WORK/probe" - - - 127.0.0.1 2160 tensor "$n" 2>&1 || true)
  GOT=$(echo "$OUT" | sed -n 's/.*server answered \([0-9-]*\),.*/\1/p')
  check "$n floats sum to $EXPECT" "$GOT" "$EXPECT"
done

kill -0 "$SERVER_PID" 2>/dev/null && check "the server survived" "yes" "yes" \
                                  || check "the server survived" "it died" "yes"

echo
echo "cases    : $((pass + fail)) run, $fail failed"
if [ "$fail" -eq 0 ]; then echo "result   : PASS"; exit 0; else echo "result   : FAIL"; exit 1; fi
