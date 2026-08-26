#!/bin/sh
# Transpile + compile + link the Cicili MVCCS. Run from anywhere.
# The transpiler runs from its own checkout (its builtins live there) and
# changes into each target's directory itself, so "./" imports resolve
# beside the target.
CICILI=${CICILI:-/home/user/cicili}
HERE=$(cd "$(dirname "$0")" && pwd)
LIBDIR=$(cd "$HERE/../home/lib" && pwd)
set -e
cd "$CICILI"
sbcl --script cicili.lisp --release "$HERE/mvccs.cicili"
if [ -f "$HERE/schema-test.cicili" ]; then
  sbcl --script cicili.lisp --release "$HERE/schema-test.cicili"
fi

# ---- the engine as ONE shared library: libMVCCS.so ------------------
# engine.cicili expands the engine exactly once and adds the engine_*
# wrappers; consumers compile with plain g++ against engine.hpp.
sbcl --script cicili.lisp --release "$HERE/engine.cicili"
g++ -shared "$HERE/.libs/engine.o" -o "$LIBDIR/libMVCCS.so" \
  -L"$LIBDIR" -lCore -lStreamIO -lpthread -Wl,-rpath,"$LIBDIR"

# THE HEADER MAY NOT DRIFT. engine.hpp copies Pointer and BaseTable
# verbatim from the emitted engine.cpp -- a generated table subclasses
# BaseTable, so the vtable must match to the byte. A drift is a build
# failure here, never a latent crash in a procedure object.
python3 - <<'PY'
import io, re, sys
emitted = io.open(r'''/home/user/ZiguratIP/MVCCS-cicili/engine.cpp''',encoding='utf-8').read()
header  = io.open(r'''/home/user/ZiguratIP/MVCCS-cicili/engine.hpp''',encoding='utf-8').read()
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

# and the keystone consumer: plain g++, engine.hpp, libMVCCS.so only
g++ -O3 "$HERE/consumer-test.cpp" -o "$HERE/consumer_test" -I"$HERE" \
  -L"$LIBDIR" -lMVCCS -lCore -lStreamIO -lpthread -Wl,-rpath,"$LIBDIR"
"$HERE/consumer_test"

# the contention suite: the old Test/test_contention.cpp scenarios, run
# through engine-compat.hpp -- the exact surface a compiled object uses --
# with each "connection" a thread holding its own transaction
INCDIR=$(cd "$HERE/../home/include" && pwd)
g++ -O3 -std=c++17 "$HERE/contention-test.cpp" -o "$HERE/contention_test" \
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
g++ -O3 -std=c++17 "$HERE/carryover-new.cpp" -o "$HERE/carryover_new" \
  -I"$HERE" -I"$INCDIR" \
  -L"$LIBDIR" -lMVCCS -lCore -lStreamIO -lType -lpthread \
  -Wl,-rpath,"$LIBDIR"
cp "$HERE/golden/carryover-hexmap.bin" /tmp/mvccs-carryover-hexmap.bin
cp "$HERE/golden/carryover-data.bin"   /tmp/mvccs-carryover-data.bin
LD_LIBRARY_PATH="$LIBDIR" "$HERE/carryover_new"

# ageing is bounded: the slowdown a working store accumulates is real,
# the reclaim pass returns the cost and converges, steady churn+reclaim
# stays flat, and the store file plateaus. The structural checks are
# deterministic; the timing ratios carry 3x headroom on purpose.
g++ -O3 -std=c++17 "$HERE/ageing-test.cpp" -o "$HERE/ageing_test" \
  -I"$HERE" \
  -L"$LIBDIR" -lMVCCS -lCore -lStreamIO -lpthread -Wl,-rpath,"$LIBDIR"
LD_LIBRARY_PATH="$LIBDIR" "$HERE/ageing_test"
