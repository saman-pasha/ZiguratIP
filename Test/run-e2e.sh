#!/bin/sh
# Starts a ziguratip server, runs the whole test suite against it (which
# includes the live Connector round trip), then stops the server again.
#
#   Test/run-e2e.sh            run every suite
#   Test/run-e2e.sh Connector  run one suite
#
# Note: the shipped etc/ziguratip.conf has RESET_MODE: TRUE, so starting the
# server truncates home/data/hexmap and home/data/data.

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
}
trap stop_server EXIT INT TERM

# Wait for the binary server to come up rather than guessing at a sleep.
READY=0
i=0
while [ $i -lt 100 ]; do
  if grep -q "Zeytun is ready" "$LOG" 2>/dev/null; then READY=1; break; fi
  if ! kill -0 "$SERVER_PID" 2>/dev/null; then break; fi
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

echo
echo "server transcript:"
sed -n '/Transaction Opened/,$p' "$LOG" | head -20

exit $STATUS
