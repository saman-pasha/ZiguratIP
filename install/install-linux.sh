#!/bin/sh
# ZiguratIP on Debian, Ubuntu or Fedora, from a clone to a built home/:
#
#   sh install/install-linux.sh
#
#   NO_PACKAGES=1 ...                          no root, or apt already done
#   CICILI=/path/to/cicili ...                 a Cicili checkout elsewhere
#                                              (default ../cicili, cloned if absent)
#   CICILI_CC=gcc CICILI_CXX=g++ ...           build with gcc; no clang needed
#
# Idempotent: run it again after a failure and it continues. The compiler
# comes first because a make without one leaves dependency files that
# poison the next make -- see common.sh. On a distribution without apt it
# names what to install and carries on if it is already there.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/.." && pwd)
OS=linux; LIBVAR=LD_LIBRARY_PATH; SHIMS=$HERE/lisp; LOG=${LOG:-/tmp/ziguratip-install.log}
. "$HERE/common.sh"

if [ "${NO_PACKAGES:-0}" != 1 ]; then
  step "packages"
  SUDO=""; [ "$(id -u)" = 0 ] || SUDO=sudo
  if command -v apt-get >/dev/null 2>&1; then
    # ---- Debian, Ubuntu ------------------------------------------------
    export DEBIAN_FRONTEND=noninteractive
    $SUDO apt-get -qq update
    $SUDO apt-get -qq install -y build-essential make git curl ca-certificates sbcl libssl-dev zlib1g-dev python3 >/dev/null
    say "build-essential make git curl sbcl libssl-dev zlib1g-dev python3"
    if ! cxx_ok 16; then
      case "${CICILI_CXX:-clang++}" in
        *clang*)
          # Ubuntu 22.04's apt has clang 14, and tools/cc/cxx passes
          # --gcc-install-dir, which exists from clang 16; Colab's image has
          # no clang at all. So: clang 18 from apt.llvm.org.
          say "clang++ 16+ not present -- installing clang 18 from apt.llvm.org"
          curl -fsSL -o /tmp/llvm.sh https://apt.llvm.org/llvm.sh || die "cannot reach apt.llvm.org"
          $SUDO bash /tmp/llvm.sh 18 >/dev/null
          for t in clang clang++; do
            $SUDO update-alternatives --install /usr/bin/$t $t /usr/bin/$t-18 100 >/dev/null
            $SUDO update-alternatives --set $t /usr/bin/$t-18 >/dev/null
          done ;;
        *) die "${CICILI_CXX} is too old for C++17" ;;
      esac
    fi
  elif command -v dnf >/dev/null 2>&1; then
    # ---- Fedora, and the Red Hat family with EPEL for sbcl --------------
    # Fedora's clang is 17 or newer, so it is taken as is; redhat-rpm-config
    # provides the hardened-cc1 specs file that home/etc/ziguratip-RedHat.conf
    # names in its CPP_FLAGS.
    $SUDO dnf -q install -y gcc gcc-c++ make git curl ca-certificates clang sbcl openssl-devel zlib-devel python3 redhat-rpm-config >/dev/null
    say "gcc gcc-c++ make git curl clang sbcl openssl-devel zlib-devel python3 redhat-rpm-config"
    cxx_ok 16 || case "${CICILI_CXX:-clang++}" in
      *clang*) die "this clang is older than 16 and tools/cc/cxx needs --gcc-install-dir; dnf install a newer clang, or CICILI_CC=gcc CICILI_CXX=g++" ;;
      *) die "${CICILI_CXX} is too old for C++17" ;;
    esac
  else
    say "neither apt-get nor dnf here -- needed: a C++17 compiler (clang 16+, or g++ 7+ with CICILI_CXX=g++),"
    say "make, git, curl, sbcl, GNU libtool, the OpenSSL, zlib headers, python3. Checking for them:"
  fi
fi
cxx_ok 16 || die "no C++17 compiler for tools/cc: ${CICILI_CXX:-clang++} (clang 16+, or CICILI_CC=gcc CICILI_CXX=g++)"
for t in make git curl sbcl python3; do command -v $t >/dev/null 2>&1 || die "$t is not on PATH"; done
say "compiler: $(${CICILI_CXX:-clang++} --version | head -1)"
[ -f /usr/include/openssl/ssl.h ] || [ -n "$(ls /usr/include/*/openssl/ssl.h 2>/dev/null)" ] \
  || say "warning: no OpenSSL headers under /usr/include -- Cryptography and SocketIO need libssl-dev"

checkout_cicili
lisp_side
build_ziguratip
exports_hint
