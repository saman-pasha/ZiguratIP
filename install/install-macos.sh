#!/bin/sh
# ZiguratIP on macOS, from a clone to a built home/:
#
#   sh install/install-macos.sh
#
#   NO_PACKAGES=1 ...                          Homebrew already has everything
#   CICILI=/path/to/cicili ...                 a Cicili checkout elsewhere
#                                              (default ../cicili, cloned if absent)
#
# Apple's clang comes with the Xcode command line tools and speaks C++17;
# Homebrew supplies sbcl, GNU libtool (as glibtool, which Cicili links
# through) and OpenSSL 3, which Cryptography and SocketIO link. Idempotent.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/.." && pwd)
OS=macos; LIBVAR=DYLD_LIBRARY_PATH; SHIMS=$HERE/lisp; LOG=${LOG:-/tmp/ziguratip-install.log}
. "$HERE/common.sh"

step "Xcode command line tools"
xcode-select -p >/dev/null 2>&1 || die "run: xcode-select --install   (Apple's clang, make and git)"
cxx_ok 10 || die "${CICILI_CXX:-clang++} does not speak C++17"
say "$(${CICILI_CXX:-clang++} --version | head -1)"

if [ "${NO_PACKAGES:-0}" != 1 ]; then
  step "Homebrew packages"
  command -v brew >/dev/null 2>&1 || die "Homebrew is not installed: https://brew.sh"
  brew list --formula sbcl libtool openssl@3 >/dev/null 2>&1 || brew install sbcl libtool openssl@3
  say "sbcl libtool openssl@3"
fi
for t in make git curl sbcl glibtool python3; do command -v $t >/dev/null 2>&1 || die "$t is not on PATH"; done
BREW=$(brew --prefix 2>/dev/null || echo /usr/local)
[ -f "$BREW/include/openssl/ssl.h" ] || say "warning: no openssl/ssl.h under $BREW/include -- try: brew link openssl@3"

checkout_cicili
lisp_side
build_ziguratip
exports_hint
