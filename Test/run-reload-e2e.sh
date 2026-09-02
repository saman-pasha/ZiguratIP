#!/bin/sh
# What happens when the libraries change underneath a running server.
#
#   Test/run-reload-e2e.sh
#
# A compiled object names the core libraries as dependencies, and the loader
# resolves those names again every time one is opened. Rebuild home/lib while a
# server is running -- a make, an install, a package upgrade -- and the file
# behind ../home/lib/libMVCCS.so is no longer the one the server mapped at
# startup, so the loader maps it a second time and binds the object to that.
#
# Nothing complains. The object loads, every symbol resolves, and then it writes
# its rows through a second storage engine whose client stream was never set,
# which is a null pointer, and the server dies inside the user's procedure. It
# took a stray "make" during another test run to find, and one crash report in
# twelve to notice.
#
# So: start a server, replace a library under it, and ask for a procedure. The
# server has to say what is wrong and stay up.
#
# The window is narrow, which is why this needs a server of its own: once an
# object has been opened successfully the loader remembers where its
# dependencies came from, and a replacement after that is deduplicated. The
# swap has to land before the first call.

set -e

HERE=$(cd "$(dirname "$0")" && pwd)
TRUNK=$(cd "$HERE/.." && pwd)
HOME_DIR="$TRUNK/home"

if [ ! -x "$HOME_DIR/bin/ziguratip" ]; then
  echo "build first: make -C $TRUNK" >&2
  exit 1
fi

if [ ! -f "$HOME_DIR/ld/lib_DEMO::COUNT_BOOKS_.so" ]; then
  echo "compile the demo first: $TRUNK/demo/build.sh" >&2
  exit 1
fi

# ---------------------------------------------------------------------------
# THIS CANNOT HAPPEN UNDER GLIBC, so the test does not pretend to run there.
#
# glibc's loader resolves a DT_NEEDED entry by NAME against the objects it has
# already loaded, before it looks at the filesystem at all. The core libraries
# carry no SONAME and never have, so the name is the bare `libMVCCS.so' in both
# the server and the object -- and the object's dependency binds to the copy the
# server already has mapped. Replacing the file changes nothing for a process
# holding it.
#
# Measured rather than assumed, on gcc 15 / glibc:
#
#   before the call   lib_DEMO::COUNT_BOOKS_.so is not mapped   (window open)
#   after the call    it is mapped                              (dlopen ran)
#   libMVCCS mapped   one, the original, shown "(deleted)"      (no second copy)
#
# So the server answering OK is CORRECT here, and asserting a refusal is
# asserting dyld's behaviour on a loader that does not have it. macOS is where
# this was written and where it means something: dyld's dedup is by install_name
# and the swapped file arrives with a different one.
#
# What is skipped is the TEST, not the protection. ziguratip/shared.cpp still
# compares zigurat_runtime_instance() through every object it opens, which is
# right on both platforms -- an object can be bound to a second copy by other
# routes, an RUNPATH pointing elsewhere being the obvious one. It is that check
# that is unexercised on Linux, and nothing here should read as though it were.
if [ "$(uname -s)" = "Linux" ]; then
  echo "reload: SKIPPED on Linux -- glibc binds the object's libMVCCS by name to"
  echo "        the copy already mapped, so no second copy exists to detect and"
  echo "        the refusal this asserts cannot happen. See the note in this"
  echo "        script. The check in ziguratip/shared.cpp is unexercised here."
  exit 0
fi

export ZIGURATIP_HOME="$HOME_DIR"
export DYLD_LIBRARY_PATH="$HOME_DIR/lib:$DYLD_LIBRARY_PATH"
export LD_LIBRARY_PATH="$HOME_DIR/lib:$LD_LIBRARY_PATH"

WORK=$(mktemp -d "${TMPDIR:-/tmp}/ziguratip-reload.XXXXXX")
LOG="$WORK/server.log"

# Ports of its own, so this neither disturbs nor is disturbed by a server
# somebody is already running.
ZPORT=2164
HPORT=2194

FAILED=0
check() {
  if [ "$2" = "$3" ]; then
    echo "  $1: ok"
  else
    echo "  $1: FAILED (expected '$3', got '$2')"
    FAILED=$((FAILED + 1))
  fi
}

# The library is put back whatever happens, because a half-finished run must not
# leave the tree in the state it was testing.
LIBRARY="$HOME_DIR/lib/libMVCCS.so"
cp "$LIBRARY" "$WORK/libMVCCS.original"

restore() {
  if [ -f "$WORK/libMVCCS.original" ]; then
    cp "$WORK/libMVCCS.original" "$WORK/libMVCCS.restored"
    mv "$WORK/libMVCCS.restored" "$LIBRARY"
  fi
}

stop_server() {
  if [ -n "$SERVER_PID" ]; then
    kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
  fi
}

trap 'stop_server; restore; rm -rf "$WORK"' EXIT INT TERM

port_free() {
  python3 - "$1" <<'FREE' 2>/dev/null
import socket, sys
try:
    socket.create_connection(("127.0.0.1", int(sys.argv[1])), timeout=1).close()
    sys.exit(1)
except OSError:
    sys.exit(0)
FREE
}

for port in "$ZPORT" "$HPORT"; do
  if ! port_free "$port"; then
    echo "port $port is already answering; stop whatever is on it first" >&2
    exit 1
  fi
done

sed -e "s/^	PORT: 2160/	PORT: $ZPORT/" \
    -e "s/^	PORT: 2190/	PORT: $HPORT/" \
    "$HOME_DIR/etc/ziguratip.conf" > "$WORK/ziguratip.conf"

echo "building the probe"
PROBE="$WORK/e2e-probe"
c++ -Wall -std=c++17 -I"$HOME_DIR/include" -L"$HOME_DIR/lib" -o "$PROBE" \
    "$HERE/e2e-probe.cpp" \
    -lConnector -lCore -lStreamIO -lType -lSocketIO -lCryptography \
    -lEncoding -lConfiguration -lThreading -lLibrary -lCompression

echo "starting ziguratip on $ZPORT (log: $LOG)"
"$HOME_DIR/bin/ziguratip" --config="$WORK/ziguratip.conf" > "$LOG" 2>&1 &
SERVER_PID=$!

i=0
READY=0
while [ $i -lt 200 ]; do
  if ! kill -0 "$SERVER_PID" 2>/dev/null; then break; fi
  if ! port_free "$ZPORT"; then READY=1; break; fi
  sleep 0.1
  i=$((i + 1))
done

if [ "$READY" -ne 1 ]; then
  echo "server did not come up:" >&2
  tail -20 "$LOG" >&2
  exit 1
fi

echo
echo "replacing home/lib/libMVCCS.so under the running server"

# A byte-for-byte copy, moved into place: the content is irrelevant, what
# matters is that the path now leads to a different file from the one the
# server mapped. That is what a linker writing its output does.
cp "$LIBRARY" "$WORK/libMVCCS.swapped"
mv "$WORK/libMVCCS.swapped" "$LIBRARY"

echo
echo "calling a procedure with the libraries changed"
ANSWER=$("$PROBE" - - - 127.0.0.1 "$ZPORT" call DEMO::COUNT_BOOKS 2>/dev/null || true)
case "$ANSWER" in
  REFUSED*second\ copy*restart*) check "says what is wrong, and to restart" "yes" "yes" ;;
  *)                             check "says what is wrong, and to restart" "$ANSWER" "REFUSED ... second copy ... restart" ;;
esac

if kill -0 "$SERVER_PID" 2>/dev/null; then
  check "server is still up" "yes" "yes"
else
  check "server is still up" "it died" "yes"
fi

# Putting the file back does not mend the running server, and should not appear
# to: it is still holding the copy it mapped at startup, and every object it
# opens now is bound to another one. The remedy the message gives is the only
# remedy there is, so check that it is a real one.
echo
echo "putting the library back, without restarting"
restore

ANSWER=$("$PROBE" - - - 127.0.0.1 "$ZPORT" call DEMO::COUNT_BOOKS 2>/dev/null || true)
case "$ANSWER" in
  REFUSED*restart*) check "the running server still refuses" "yes" "yes" ;;
  *)                check "the running server still refuses" "$ANSWER" "REFUSED ... restart" ;;
esac

echo
echo "restarting, which is what it asked for"
stop_server
"$HOME_DIR/bin/ziguratip" --config="$WORK/ziguratip.conf" > "$WORK/restarted.log" 2>&1 &
SERVER_PID=$!

i=0
READY=0
while [ $i -lt 200 ]; do
  if ! kill -0 "$SERVER_PID" 2>/dev/null; then break; fi
  if ! port_free "$ZPORT"; then READY=1; break; fi
  sleep 0.1
  i=$((i + 1))
done

if [ "$READY" -ne 1 ]; then
  echo "the restarted server did not come up:" >&2
  tail -20 "$WORK/restarted.log" >&2
  exit 1
fi

ANSWER=$("$PROBE" - - - 127.0.0.1 "$ZPORT" call DEMO::COUNT_BOOKS 2>/dev/null || true)
case "$ANSWER" in
  OK*) check "the restarted server answers" "ok" "ok" ;;
  *)   check "the restarted server answers" "$ANSWER" "OK" ;;
esac

echo
if [ "$FAILED" -eq 0 ]; then
  echo "reload: PASS"
else
  echo "reload: $FAILED FAILED"
fi
exit $FAILED
