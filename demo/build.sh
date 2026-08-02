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
  "$ROOT/home/bin/parsi" "$step" | tail -1
done

echo
echo "Compiled into $ZIGURATIP_HOME/ld:"
ls "$ZIGURATIP_HOME"/ld/*.so 2>/dev/null | sed 's|.*/|  |'
echo
echo "Now start the server and visit http://127.0.0.1:2190/setup.zt"
