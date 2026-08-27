# ONE COMPILER FOR EVERY LAYER. Sourced by the build scripts here, which
# set $ROOT to this repository first.
#
# CC and CXX become the two wrappers beside this file rather than `clang'
# and `clang++' outright, because ./cxx carries the --gcc-install-dir that
# Ubuntu makes necessary and nothing should have to remember it. tools/cc
# also goes on PATH, which is how the one step Cicili compiles itself --
# it names `gcc' and takes no override -- ends up with the same compiler
# as everything else. See ./README.
#
# `CICILI_CC=gcc CICILI_CXX=g++ sh ...build.sh' builds with gcc, and still
# agrees with itself: the wrappers read exactly those two.
CICILI_CC=${CICILI_CC:-clang}
CICILI_CXX=${CICILI_CXX:-clang++}
CC="$ROOT/tools/cc/cc"
CXX="$ROOT/tools/cc/cxx"
PATH="$ROOT/tools/cc:$PATH"
export CC CXX CICILI_CC CICILI_CXX PATH
