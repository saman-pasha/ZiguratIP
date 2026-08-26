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
sbcl --script cicili.lisp "$HERE/mvccs.cicili"
if [ -f "$HERE/schema-test.cicili" ]; then
  sbcl --script cicili.lisp "$HERE/schema-test.cicili"
fi

# ---- the engine as ONE shared library: libMVCCS2.so ------------------
# engine.cicili expands the engine exactly once and adds the engine_*
# wrappers; consumers compile with plain g++ against engine.hpp.
sbcl --script cicili.lisp "$HERE/engine.cicili"
g++ -shared "$HERE/.libs/engine.o" -o "$LIBDIR/libMVCCS2.so" \
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
for name in ('Pointer','BaseTable'):
    e, h = block(emitted, name), block(header, name)
    h = h and h.replace('Zigurat::binarystream','binarystream')
    if e != h:
        print('engine.hpp DRIFTED from engine.cpp on struct', name); bad = 1
sys.exit(bad)
PY

# and the keystone consumer: plain g++, engine.hpp, libMVCCS2.so only
g++ -g -O0 "$HERE/consumer-test.cpp" -o "$HERE/consumer_test" -I"$HERE" \
  -L"$LIBDIR" -lMVCCS2 -lCore -lStreamIO -lpthread -Wl,-rpath,"$LIBDIR"
"$HERE/consumer_test"
