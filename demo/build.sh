#!/bin/sh
# Compiles the demo objects in order. Each step depends on the one before it,
# because REQUIRES links against objects that must already exist.
set -e

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)

export ZIGURATIP_HOME="$ROOT/home"
export DYLD_LIBRARY_PATH="$ROOT/home/lib:$DYLD_LIBRARY_PATH"
export LD_LIBRARY_PATH="$ROOT/home/lib:$LD_LIBRARY_PATH"

if [ ! -x "$ROOT/home/bin/parsi" ]; then
  echo "build ZiguratIP first: make -C $ROOT" >&2
  exit 1
fi

for step in "$HERE"/0*.parsi; do
  echo "==> $(basename "$step")"
  # No pipe here: piping into tail would hide a non-zero exit from parsi and
  # the build would carry on as if the step had succeeded.
  if ! "$ROOT/home/bin/parsi" "$step" > "$HERE/.build.log" 2>&1; then
    echo "failed:" >&2
    tail -5 "$HERE/.build.log" >&2
    rm -f "$HERE/.build.log"
    exit 1
  fi
  tail -1 "$HERE/.build.log"
done
rm -f "$HERE/.build.log"

echo
echo "Compiled into $ZIGURATIP_HOME/ld:"
ls "$ZIGURATIP_HOME"/ld/*.so 2>/dev/null | sed 's|.*/|  |'
echo
echo "Now start the server and visit http://127.0.0.1:2190/setup.zt"
