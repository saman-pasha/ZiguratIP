#!/bin/sh
# Every binary answers --version with the number alone.
#
#   Test/run-version.sh
#
# WHAT IT PINS, AND WHAT IT DELIBERATELY DOES NOT. The SHAPE: a bare
# MAJOR.MINOR.PATCH on stdout, nothing else on stdout, status 0 -- and the
# SAME number from all four binaries, which is what says they read Core's
# one literal rather than each carrying a copy of it. NOT THE NUMBER: a
# case that named it would be a second place to edit on every bump, and the
# one somebody forgets. See "The version, and when it moves" in README.md.
#
# IT NEEDS NO SERVER AND NO CONFIGURATION FILE, and says so by asking with
# ZIGURATIP_HOME pointed at a directory that does not exist: what a build
# is must be answerable in a tree that is not set up yet.

set -e

HERE=$(cd "$(dirname "$0")" && pwd)
TRUNK=$(cd "$HERE/.." && pwd)
BIN="$TRUNK/home/bin"

export LD_LIBRARY_PATH="$TRUNK/home/lib:$LD_LIBRARY_PATH"
export DYLD_LIBRARY_PATH="$TRUNK/home/lib:$DYLD_LIBRARY_PATH"

pass=0
fail=0

check() {
  if [ "$2" = "$3" ]; then echo "ok   $1"; pass=$((pass + 1))
  else echo "FAIL $1: got '$2' want '$3'"; fail=$((fail + 1)); fi
}

first=""
for binary in ziguratip parsi parsic ca; do
  if [ ! -x "$BIN/$binary" ]; then
    check "$binary is built" "missing -- run make" "built"
    continue
  fi

  # a home that is not there: the answer may not depend on one. The || keeps
  # a non-zero status from ending the run under `set -e' -- it is a case
  # here, not an accident.
  out=$(ZIGURATIP_HOME=/nonexistent-for-the-version-check "$BIN/$binary" --version 2>/dev/null) && status=0 || status=$?

  check "$binary --version exits 0" "$status" "0"

  if printf '%s' "$out" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+$'; then
    check "$binary --version is a bare number, and it alone" "bare" "bare"
  else
    check "$binary --version is a bare number, and it alone" "$out" "bare"
  fi

  if [ -z "$first" ]; then first="$out"
  else check "$binary agrees with ziguratip" "$out" "$first"; fi
done

echo
echo "cases    : $((pass + fail)) run, $fail failed"
if [ "$fail" -eq 0 ]; then echo "result   : PASS"; exit 0; else echo "result   : FAIL"; exit 1; fi
