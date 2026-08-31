#!/bin/sh
# Transpile + compile + link the Cicili MVCCS. Run from anywhere.
# The transpiler runs from its own checkout (its builtins live there) and
# changes into each target's directory itself, so "./" imports resolve
# beside the target.
# NO ABSOLUTE PATH TO ANYBODY'S HOME. This file and everything it names
# used to spell one developer's checkout -- /home/user/... -- into
# .cicili includes, link flags, test sources and the drift check below,
# 51 times across this directory. The workspace therefore built on
# exactly one machine, and the first attempt to build it anywhere else
# died at `cannot find -lMVCCS' with the real cause fifty lines up. The
# headers this engine needs are published into ../home/include by every
# other project's `headers' target, and this one now includes them the
# way the rest of the repository does: by name, found relatively.
CICILI=${CICILI:-$HOME/cicili}
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
. "$ROOT/tools/cc/env.sh"
LIBDIR=$(cd "$HERE/.." && pwd)/home/lib
mkdir -p "$LIBDIR"
set -e
cd "$CICILI"
# LD_LIBRARY_PATH rather than an rpath: the rpath these two carried was
# an absolute path into one developer's home, and a RELATIVE rpath would
# resolve against whatever directory the caller happened to be in. A
# test binary this script runs itself does not need a promise that
# outlives the script.
LIB_ENV="$(cd "$HERE/.." && pwd)/home/lib"
LD_LIBRARY_PATH="$LIB_ENV${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LD_LIBRARY_PATH
sbcl --script cicili.lisp --release "$HERE/mvccs.cicili"
if [ -f "$HERE/schema-test.cicili" ]; then
  sbcl --script cicili.lisp --release "$HERE/schema-test.cicili"
fi

# The headers this engine includes live in ../home/include, and every
# project's `headers' target is what puts them there. The workspace's
# top-level `all: headers' does that before any project builds -- but
# THIS SCRIPT ALSO RUNS ON ITS OWN, and in a fresh clone home/include is
# empty. Found by building a relocated clone: the includes resolved on
# the development machine only because years of earlier builds had
# already filled that directory.
INCDIR=$(cd "$HERE/.." && pwd)/home/include
mkdir -p "$INCDIR"
if [ ! -f "$INCDIR/binarystream.hpp" ] || [ ! -f "$INCDIR/zexception.hpp" ]; then
  echo "== publishing the workspace headers first (home/include was empty)"
  make -C "$HERE/.." headers >/dev/null 2>&1 || true
fi
for h in zexception.hpp utility.hpp binarystream.hpp filestream.hpp \
         bufferstream.hpp textstream.hpp; do
  if [ ! -f "$INCDIR/$h" ]; then
    echo "   $h is not in $INCDIR and the engine includes it -- run 'make headers' at the workspace root" >&2
    exit 1
  fi
done
cp "$HERE/engine.hpp" "$HERE/engine-compat.hpp" "$INCDIR/"

# ---- the engine as ONE shared library: libMVCCS.so ------------------
# engine.cicili expands the engine exactly once and adds the engine_*
# wrappers; consumers compile with plain $CXX against engine.hpp.
sbcl --script cicili.lisp --release "$HERE/engine.cicili"
"$CXX" -shared "$HERE/.libs/engine.o" -o "$LIBDIR/libMVCCS.so" \
  -L"$LIBDIR" -lCore -lStreamIO -lpthread -Wl,-rpath,"$LIBDIR"

# THE HEADER MAY NOT DRIFT. engine.hpp copies Pointer and BaseTable
# verbatim from the emitted engine.cpp -- a generated table subclasses
# BaseTable, so the vtable must match to the byte. A drift is a build
# failure here, never a latent crash in a procedure object.
MVCCS_HERE="$HERE" python3 - <<'PY'
import io, re, sys
import os
here    = os.environ['MVCCS_HERE']
emitted = io.open(os.path.join(here, 'engine.cpp'), encoding='utf-8').read()
header  = io.open(os.path.join(here, 'engine.hpp'), encoding='utf-8').read()
def block(src, name):
    m = re.search(r'struct %s \{.*?\n\};' % name, src, re.S)
    return re.sub(r'\s+', ' ', m.group(0)) if m else None
bad = 0
for name in ('Pointer','BaseTable','BTreeIndex'):
    e, h = block(emitted, name), block(header, name)
    h = h and h.replace('Zigurat::binarystream','binarystream')
    if e != h:
        print('engine.hpp DRIFTED from engine.cpp on struct', name); bad = 1
sys.exit(bad)
PY

# and the keystone consumer: a plain C++ compiler, engine.hpp,
# libMVCCS.so only -- no Cicili, no engine sources
"$CXX" -O3 "$HERE/consumer-test.cpp" -o "$HERE/consumer_test" -I"$HERE" -I"$INCDIR" \
  -L"$LIBDIR" -lMVCCS -lCore -lStreamIO -lpthread -Wl,-rpath,"$LIBDIR"
"$HERE/consumer_test"

# the contention suite: the old Test/test_contention.cpp scenarios, run
# through engine-compat.hpp -- the exact surface a compiled object uses --
# with each "connection" a thread holding its own transaction
"$CXX" -O3 -std=c++17 "$HERE/contention-test.cpp" -o "$HERE/contention_test" \
  -I"$HERE" -I"$INCDIR" \
  -L"$LIBDIR" -lMVCCS -lCore -lStreamIO -lType -lCryptography -lpthread \
  -Wl,-rpath,"$LIBDIR"
"$HERE/contention_test"

# the carry-over acceptance: a store the OLD engine wrote, opened by the
# NEW one -- rows carry byte-identically, indexes rebuild. The old engine
# is retired, so the store is GOLDEN: the last one carryover-old.cpp ever
# wrote, checked in under golden/, handed to the reader as a scratch copy
# (the reader writes into it -- an update and an insert are part of the
# proof).
"$CXX" -O3 -std=c++17 "$HERE/carryover-new.cpp" -o "$HERE/carryover_new" \
  -I"$HERE" -I"$INCDIR" \
  -L"$LIBDIR" -lMVCCS -lCore -lStreamIO -lType -lpthread \
  -Wl,-rpath,"$LIBDIR"
# a checkout without the golden pair cannot prove carry-over -- "no golden
# here" and "the reader is wrong" are different findings, so it SKIPs
if [ -f "$HERE/golden/carryover-hexmap.bin" ]; then
  cp "$HERE/golden/carryover-hexmap.bin" /tmp/mvccs-carryover-hexmap.bin
  cp "$HERE/golden/carryover-data.bin"   /tmp/mvccs-carryover-data.bin
  LD_LIBRARY_PATH="$LIBDIR" "$HERE/carryover_new"
else
  echo "carry-over: SKIP (no golden store in this checkout)"
fi

# ageing is bounded: the slowdown a working store accumulates is real,
# the reclaim pass returns the cost and converges, steady churn+reclaim
# stays flat, and the store file plateaus. The structural checks are
# deterministic; the timing ratios carry 3x headroom on purpose.
"$CXX" -O3 -std=c++17 "$HERE/ageing-test.cpp" -o "$HERE/ageing_test" \
  -I"$HERE" -I"$INCDIR" \
  -L"$LIBDIR" -lMVCCS -lCore -lStreamIO -lpthread -Wl,-rpath,"$LIBDIR"
LD_LIBRARY_PATH="$LIBDIR" "$HERE/ageing_test"
