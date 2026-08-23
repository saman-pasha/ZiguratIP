#!/bin/sh
# Transpile + compile + link the Cicili MVCCS. Run from anywhere.
# The transpiler runs from its own checkout (its builtins live there) and
# changes into each target's directory itself, so "./" imports resolve
# beside the target.
CICILI=${CICILI:-/home/user/cicili}
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$CICILI" || exit 1
sbcl --script cicili.lisp "$HERE/mvccs.cicili" || exit 1
if [ -f "$HERE/schema-test.cicili" ]; then
  sbcl --script cicili.lisp "$HERE/schema-test.cicili"
fi
