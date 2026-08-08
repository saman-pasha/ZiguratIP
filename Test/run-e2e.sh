#!/bin/sh
# Starts a ziguratip server, runs the whole test suite against it (which
# includes the live Connector round trip), then stops the server again.
#
#   Test/run-e2e.sh            run every suite
#   Test/run-e2e.sh Connector  run one suite
#
# The suite keeps its own stores under /tmp, so it neither reads nor disturbs
# home/data.

set -e

HERE=$(cd "$(dirname "$0")" && pwd)
TRUNK=$(cd "$HERE/.." && pwd)
HOME_DIR="$TRUNK/home"

if [ ! -x "$HOME_DIR/bin/ziguratip" ]; then
  echo "build first: make -C $TRUNK" >&2
  exit 1
fi

export ZIGURATIP_HOME="$HOME_DIR"
export DYLD_LIBRARY_PATH="$HOME_DIR/lib:$DYLD_LIBRARY_PATH"
export LD_LIBRARY_PATH="$HOME_DIR/lib:$LD_LIBRARY_PATH"

LOG="$HOME_DIR/log/e2e-server.log"
mkdir -p "$HOME_DIR/log" "$HOME_DIR/data"

echo "starting ziguratip (log: $LOG)"
"$HOME_DIR/bin/ziguratip" > "$LOG" 2>&1 &
SERVER_PID=$!

stop_server() {
  kill "$SERVER_PID" 2>/dev/null || true
  wait "$SERVER_PID" 2>/dev/null || true

  # And wait for the ports to actually clear. The suite leaves connections
  # behind it, and until the kernel has finished with those the next run of this
  # script cannot bind -- which looked like the server failing to start.
  i=0
  while [ $i -lt 100 ]; do
    if python3 - <<'FREE' 2>/dev/null
import socket, sys
for port in (2160, 2190):
    try:
        socket.create_connection(("127.0.0.1", port), timeout=1).close()
        sys.exit(1)          # still answering
    except OSError:
        pass
FREE
    then
      break
    fi
    sleep 0.1
    i=$((i + 1))
  done
}
trap stop_server EXIT INT TERM

# Wait until both ports actually accept a connection.
#
# Watching the log for "Zeytun is ready" is not enough: the server prints that
# and then calls run(), which is what binds and listens, so the message arrives
# before there is anything to connect to. Tests that started on the strength of
# it raced the listener, and intermittently found nothing there.
READY=0
i=0
while [ $i -lt 200 ]; do
  if ! kill -0 "$SERVER_PID" 2>/dev/null; then break; fi
  if python3 - <<'LISTENING' 2>/dev/null
import socket, sys
for port in (2160, 2190):
    try:
        socket.create_connection(("127.0.0.1", port), timeout=1).close()
    except OSError:
        sys.exit(1)
LISTENING
  then
    READY=1
    break
  fi
  sleep 0.1
  i=$((i + 1))
done

if [ "$READY" -ne 1 ]; then
  echo "server did not come up:" >&2
  tail -20 "$LOG" >&2
  exit 1
fi

echo "server up, running tests"
echo

"$HOME_DIR/bin/Test" "$@"
STATUS=$?

# Zeytun's keep-alive, against the live server. The unit suites cannot see this:
# it only shows when a second request goes down a connection the first one used,
# and the server used to answer that one 400 because it read the request line as
# a header. Everything a proxy does rests on it.
echo
echo "checking keep-alive on one connection"
if python3 - <<'KEEPALIVE'
import socket, sys
try:
    s = socket.create_connection(("127.0.0.1", 2190), timeout=10)
    f = s.makefile("rb")
    for n in range(3):
        s.sendall(b"GET / HTTP/1.1\r\nHost: 127.0.0.1:2190\r\n\r\n")
        status = f.readline().decode().strip()
        if not status.startswith("HTTP/1.1 200"):
            print("  request %d on the same connection: %r" % (n + 1, status))
            sys.exit(1)
        length = 0
        while True:
            line = f.readline().decode().strip()
            if line == "":
                break
            if line.lower().startswith("content-length:"):
                length = int(line.split(":", 1)[1])
        f.read(length)
    s.close()
except Exception as error:
    print("  %s: %s" % (type(error).__name__, error))
    sys.exit(1)
KEEPALIVE
then
  echo "  three requests on one connection: ok"
else
  echo "  keep-alive FAILED"
  STATUS=1
fi

# A page that is not there is a 404, and the body says nothing about the
# filesystem. LibraryLoader throws rather than returning null when dlopen fails,
# so this used to answer 200 with the full path it had tried.
echo
echo "checking a missing page"
MISSING_STATUS=$(curl -s -o "$HOME_DIR/log/missing-body.txt" -w '%{http_code}' \
                 "http://127.0.0.1:2190/no-such-page-here.zt" 2>/dev/null || echo "000")
if [ "$MISSING_STATUS" = "404" ] && ! grep -q "$HOME_DIR" "$HOME_DIR/log/missing-body.txt" 2>/dev/null; then
  echo "  404, and no server path in the body: ok"
else
  echo "  missing page FAILED (status $MISSING_STATUS)"
  STATUS=1
fi
rm -f "$HOME_DIR/log/missing-body.txt"

# A SELECT's columns are escaped on the way out, and its literals are not.
#
# This is checked in the GENERATED C++ rather than over HTTP because the demo's
# own data has nothing dangerous in it -- a page could render every row wrongly
# and still look right. What can be asserted is the shape: demo::lookup lists
# both literal markup and columns, so its cursor must show both forms.
#
# It is here because the unit test for this covers Utility::escape_html and
# echo_escaped in isolation, which both passed the whole time the SELECT cursor
# was writing columns straight to the stream. Proven with a row whose title was
# <script>alert(1)</script>: the ECHO page escaped it and the SELECT page did
# not, and doc/page.md had always named a column as the case escaping is for.
echo
echo "checking a SELECT escapes its columns"
LOOKUP_CPP="$HOME_DIR/tmp/_LOOKUP_.cpp"
if [ ! -f "$LOOKUP_CPP" ]; then
  echo "  SKIP: $LOOKUP_CPP not built"
elif grep -q 'Zigurat::Utility::echo_escaped(\*Globals::echo_stream(), BOOKS' "$LOOKUP_CPP" \
  && grep -q '\*Globals::echo_stream() << R"ZIP0ML0S0(<li>' "$LOOKUP_CPP"; then
  echo "  columns escaped, literals left alone: ok"
else
  echo "  SELECT escaping FAILED -- columns are reaching the page unescaped"
  grep -n 'echo_stream' "$LOOKUP_CPP" | head -6
  STATUS=1
fi

echo
echo "server transcript:"
sed -n '/Transaction Opened/,$p' "$LOG" | head -20

exit $STATUS
