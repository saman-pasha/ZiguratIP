#!/bin/sh
# ZiguratIP on Debian, Ubuntu or Fedora, from a clone to a built home/:
#
#   sh install/install-linux.sh
#
#   NO_PACKAGES=1 ...                          no root, or apt already done
#   CICILI=/path/to/cicili ...                 a Cicili checkout elsewhere
#                                              (default ../cicili, cloned if absent)
#   CICILI_CC=... CICILI_CXX=... ...           a particular clang (clang-18, a path);
#                                              every build here is clang, and a
#                                              compiler that is not is refused
#   sudo sh install/install-linux.sh           the packages as root, then the rest as
#                                              the user who called sudo, in that
#                                              user's own home
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
    $SUDO apt-get -qq install -y build-essential make git curl ca-certificates gnupg sbcl libssl-dev zlib1g-dev python3 >/dev/null
    say "build-essential make git curl gnupg sbcl libssl-dev zlib1g-dev python3"
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
        *) die "CICILI_CXX=${CICILI_CXX} is not clang -- every build here is clang" ;;
      esac
    fi
  elif command -v dnf >/dev/null 2>&1; then
    # ---- Fedora, and the Red Hat family with EPEL for sbcl --------------
    # Fedora's clang is 17 or newer, so it is taken as is. gcc-c++ is here
    # for libstdc++'s headers and runtime, which clang compiles and links
    # against; nothing is compiled by gcc. redhat-rpm-config provides the
    # hardened-cc1 specs file that home/etc/ziguratip-RedHat.conf names in
    # its CPP_FLAGS.
    $SUDO dnf -q install -y gcc gcc-c++ make git curl ca-certificates gnupg2 clang sbcl openssl-devel zlib-devel python3 redhat-rpm-config >/dev/null
    say "gcc gcc-c++ make git curl gnupg2 clang sbcl openssl-devel zlib-devel python3 redhat-rpm-config"
    cxx_ok 16 || case "${CICILI_CXX:-clang++}" in
      *clang*) die "this clang is older than 16 and tools/cc/cxx needs --gcc-install-dir; dnf install a newer clang" ;;
      *) die "CICILI_CXX=${CICILI_CXX} is not clang -- every build here is clang" ;;
    esac
  else
    say "neither apt-get nor dnf here -- needed: clang 16+ with libstdc++'s headers,"
    say "make, git, curl, gnupg, sbcl, GNU libtool, the OpenSSL, zlib headers, python3. Checking for them:"
  fi
fi

# THE REST IS THE CALLING USER'S. Quicklisp and the ~/common-lisp tree are
# found through $HOME -- SBCL's (user-homedir-pathname), ASDF's search of
# ~/common-lisp -- and under `sudo sh install/install-linux.sh' $HOME is
# /root: both would go to root's home, owned by root, where the user's own
# sbcl never looks, and the build beside them would be root's too. So the
# packages go in as root, and everything after them runs again as the user
# who called sudo, in that user's own home (-H), with NO_PACKAGES=1 and this
# run's options. A root login with no sudo (a container, Colab) is in its
# own home already and goes on. cocolog's install-linux.sh does the same.
if [ "$(id -u)" = 0 ] && [ -n "${SUDO_USER:-}" ] && [ "$SUDO_USER" != root ]; then
  say "packages done as root; the rest as $SUDO_USER, in $(getent passwd "$SUDO_USER" | cut -d: -f6)"
  set -- NO_PACKAGES=1
  for v in CICILI QUICKLISP_HOME CICILI_CC CICILI_CXX LOG; do
    if eval "[ -n \"\${$v+set}\" ]"; then eval "set -- \"\$@\" \"$v=\$$v\""; fi
  done
  exec sudo -u "$SUDO_USER" -H env "$@" sh "$HERE/install-linux.sh"
fi

cxx_ok 16 || die "no clang 16+ for tools/cc: ${CICILI_CXX:-clang++}"
for t in make git curl sbcl python3; do command -v $t >/dev/null 2>&1 || die "$t is not on PATH"; done
say "compiler: $(${CICILI_CXX:-clang++} --version | head -1)"
[ -f /usr/include/openssl/ssl.h ] || [ -n "$(ls /usr/include/*/openssl/ssl.h 2>/dev/null)" ] \
  || say "warning: no OpenSSL headers under /usr/include -- Cryptography and SocketIO need libssl-dev"
log_ok "$LOG"

checkout_cicili
lisp_side
build_ziguratip
exports_hint
