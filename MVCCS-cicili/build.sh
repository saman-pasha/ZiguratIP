#!/bin/sh
# Transpile + compile + link the Cicili MVCCS. Run from anywhere.
CICILI=${CICILI:-/home/user/cicili}
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$CICILI" || exit 1
sbcl --script cicili.lisp "$HERE/mvccs.cicili"
