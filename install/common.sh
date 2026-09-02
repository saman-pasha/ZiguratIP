# The part of the install that is the same on every OS. Sourced by
# install-linux.sh and install-macos.sh AFTER they have set HERE, ROOT,
# OS, LIBVAR, SHIMS and LOG and installed their packages. Not a program.
#
# What it does, in order: find or clone the Cicili checkout; give SBCL the
# four Lisp systems Cicili is built from; build ZiguratIP in Release and
# check the ARTIFACTS rather than make's exit code, because the top-level
# make steps over a failed project; and print the three exports a shell
# needs afterwards.
CICILI=${CICILI:-$(cd "$ROOT/.." && pwd)/cicili}
QL=${QUICKLISP_HOME:-$HOME/quicklisp}
step() { printf '== %s\n' "$*"; }
say()  { printf '   %s\n' "$*"; }
die()  { printf 'INSTALL RED: %s\n' "$*" >&2; exit 1; }

# Is the C++ compiler the wrappers in tools/cc will call good for C++17?
# The argument is the clang major that is enough on this OS: 16 on Linux,
# where tools/cc/cxx passes --gcc-install-dir and older clangs reject it;
# 10 on macOS, where the flag is never passed. g++ 7 speaks C++17.
cxx_ok() {
  cxx=${CICILI_CXX:-clang++}
  case "$cxx" in
    *clang*) v=$("$cxx" --version 2>/dev/null | grep -oE 'version [0-9]+' | grep -oE '[0-9]+' | head -1)
             [ "${v:-0}" -ge "$1" ] ;;
    *)       v=$("$cxx" -dumpversion 2>/dev/null | cut -d. -f1); [ "${v:-0}" -ge 7 ] ;;
  esac
}

checkout_cicili() {
  step "the Cicili checkout"
  if [ -f "$CICILI/cicili.lisp" ]; then
    say "CICILI=$CICILI"
  else
    say "no Cicili at $CICILI -- cloning it there"
    git clone -q https://github.com/saman-pasha/cicili.git "$CICILI"
  fi
}

lisp_side() {
  step "the Lisp systems Cicili is built from"
  if [ ! -f "$QL/setup.lisp" ]; then
    say "installing Quicklisp into $QL"
    curl -fsSL -o /tmp/quicklisp.lisp https://beta.quicklisp.org/quicklisp.lisp \
      || die "cannot reach beta.quicklisp.org -- install Quicklisp by hand into $QL and re-run"
    sbcl --non-interactive --load /tmp/quicklisp.lisp \
         --eval "(quicklisp-quickstart:install :path \"$QL/\")" >/dev/null
  fi
  sbcl --non-interactive --load "$QL/setup.lisp" \
       --eval '(ql:quickload (list :str :cl-ppcre) :silent t)' >/dev/null
  say "str and cl-ppcre: present (Quicklisp)"
  mkdir -p "$HOME/common-lisp"
  cp -R "$SHIMS/sha1" "$SHIMS/base64" "$HOME/common-lisp/"
  if [ -L "$HOME/common-lisp/cicili" ] || [ ! -e "$HOME/common-lisp/cicili" ]; then
    ln -sfn "$CICILI" "$HOME/common-lisp/cicili"
    say "sha1 and base64 shims, and cicili -> $CICILI: in $HOME/common-lisp"
  else
    say "sha1 and base64 shims in $HOME/common-lisp; $HOME/common-lisp/cicili is a directory of its own and stays"
  fi
  sbcl --non-interactive --load "$QL/setup.lisp" --eval '(ql:quickload "cicili" :silent t)' >/dev/null 2>&1 \
    || die "SBCL cannot load the cicili system -- run sbcl and (ql:quickload \"cicili\") to see why"
  say "SBCL loads cicili"
}

build_ziguratip() {
  step "building ZiguratIP (Release) with $(${CICILI_CXX:-clang++} --version | head -1)"
  # a dependency file written by a build that had no compiler has no object
  # rules and is never regenerated; drop it and let make remake it
  for d in "$ROOT"/*/*.depend; do
    [ -f "$d" ] && ! grep -q '\.o:' "$d" && rm -f "$d"
  done
  mkdir -p "$ROOT/home/data" "$ROOT/home/ld" "$ROOT/home/catalog" "$ROOT/home/log" \
           "$ROOT/home/tmp" "$ROOT/home/obj" "$ROOT/home/lib" "$ROOT/home/bin"
  ( cd "$ROOT" && CICILI="$CICILI" make MODE=Release ) > "$LOG" 2>&1 || true
  missing=""
  for lib in Core StreamIO Type Library Encoding Compression Cryptography Configuration \
             Threading SocketIO Connector HTTP MVCCS Compiler; do
    [ -f "$ROOT/home/lib/lib$lib.so" ] || missing="$missing lib$lib.so"
  done
  for b in parsi parsic ziguratip; do
    [ -x "$ROOT/home/bin/$b" ] || missing="$missing bin/$b"
  done
  if [ -n "$missing" ]; then
    grep -nE 'error:|cannot find -l|library .* not found|Unhandled' "$LOG" | head -12 | sed 's/^/   /'
    die "ZiguratIP incomplete, missing:$missing  (whole log: $LOG)"
  fi
  say "$(ls "$ROOT"/home/lib/*.so | wc -l | tr -d ' ') libraries, $(ls "$ROOT/home/bin" | wc -l | tr -d ' ') executables in $ROOT/home"
}

exports_hint() {
  step "installed. Put these in your shell profile:"
  echo "   export CICILI=$CICILI"
  echo "   export ZIGURATIP_HOME=$ROOT/home"
  echo "   export $LIBVAR=\$ZIGURATIP_HOME/lib"
  say "check it: cd $ROOT && sh Test/run-e2e.sh   (starts a server on ports 2160/2190, runs every case, stops it)"
}
