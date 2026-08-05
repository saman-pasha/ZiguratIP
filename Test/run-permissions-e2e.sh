#!/bin/sh
# Permissions, against a real server.
#
#   Test/run-permissions-e2e.sh
#
# Everything this checks happens between two processes: a subject is turned away
# during the handshake, and a caller is refused before the object it named is
# run. Neither can be seen from inside the server's own test binary, so this
# starts one, points a client at it with several different certificates, and
# compares the answers.
#
# It uses its own ports, its own certificates and its own users directory, so it
# neither needs nor disturbs a server already running. The compiled objects come
# from home/ld, which demo/build.sh fills.

set -e

HERE=$(cd "$(dirname "$0")" && pwd)
TRUNK=$(cd "$HERE/.." && pwd)
HOME_DIR="$TRUNK/home"

ZPORT=2161
HPORT=2191

if [ ! -x "$HOME_DIR/bin/ziguratip" ] || [ ! -x "$HOME_DIR/bin/ca" ]; then
  echo "build first: make -C $TRUNK" >&2
  exit 1
fi

if [ ! -f "$HOME_DIR/ld/lib_DEMO::SEED_.so" ]; then
  echo "compile the demo first: $TRUNK/demo/build.sh" >&2
  exit 1
fi

export ZIGURATIP_HOME="$HOME_DIR"
export DYLD_LIBRARY_PATH="$HOME_DIR/lib:$DYLD_LIBRARY_PATH"
export LD_LIBRARY_PATH="$HOME_DIR/lib:$LD_LIBRARY_PATH"

# Nothing else may be on these ports. A server of this script's that failed to
# bind would abort while an older one carried on answering, and every check
# below would then be measuring the wrong process -- which is exactly how a run
# of this reported that permissions were not enforced when they were.
port_free() {
  python3 - "$1" <<'FREE' 2>/dev/null
import socket, sys
try:
    socket.create_connection(("127.0.0.1", int(sys.argv[1])), timeout=1).close()
    sys.exit(1)
except OSError:
    pass
FREE
}

for port in $ZPORT $HPORT; do
  if ! port_free "$port"; then
    echo "something is already listening on $port; stop it first" >&2
    exit 1
  fi
done

WORK="$HOME_DIR/log/permissions-e2e"
rm -rf "$WORK"
mkdir -p "$WORK/cert" "$WORK/users"

LOG="$WORK/server.log"
STATUS=0

say() {
  echo "$1"
}

check() {
  # check <what> <expected> <got>
  if [ "$2" = "$3" ]; then
    say "  ok    $1"
  else
    say "  FAIL  $1"
    say "          want [$2]"
    say "          got  [$3]"
    STATUS=1
  fi
}

# The answer's first word: OK, or REFUSED. The rest is the server's message,
# which is checked separately where it matters.
verdict() {
  echo "$1" | awk '{print $1}'
}


# ---------------------------------------------------------------------------
# Certificates
#
# The shipped sample is self signed, so it is its own authority: certificates
# issued against its key validate against it. That makes every certificate here
# free -- no key generation, which is the slow part -- while still being exactly
# the arrangement a real deployment has.
# ---------------------------------------------------------------------------

AUTHORITY="$HOME_DIR/etc/cert/dont-use-certificate.crt"
KEY="$HOME_DIR/etc/cert/dont-use-private.key"
ISSUER="$HOME_DIR/etc/cert/issuer.conf"

issue() {
  # issue <name> <common-name> [permission ...]
  name=$1
  common=$2
  shift 2

  printf 'COUNTRY: US\nCOMMON_NAME: %s\n' "$common" > "$WORK/cert/$name.conf"

  "$HOME_DIR/bin/ca" csr --subject="$WORK/cert/$name.conf" --subject-pik="$KEY" \
      --hash=SHA-256 --encoding=DER --csr="$WORK/cert/$name.csr" > /dev/null

  set -- --serial=$SERIAL --issuer="$ISSUER" --issuer-pik="$KEY" \
         --csr="$WORK/cert/$name.csr" --hash=SHA-256 --encoding=DER \
         --certificate="$WORK/cert/$name.crt" "$@"
  "$HOME_DIR/bin/ca" issue "$@" > /dev/null

  SERIAL=$((SERIAL + 1))
}

SERIAL=100

echo "issuing certificates"
issue schema     demo-everything --permission=DEMO
issue one-table  demo-one-table  --permission=DEMO::AUTHORS
issue elsewhere  other-schema    --permission=BENCH::ITEM
issue nothing    no-permissions
issue stranger   never-registered --permission=DEMO

# The register: three of the four subjects may connect at all. "stranger" holds
# a perfectly good certificate granting DEMO and is deliberately left off it.
for who in schema one-table elsewhere nothing; do
  "$HOME_DIR/bin/ca" put --certificate="$WORK/cert/$who.crt" --users="$WORK/users" > /dev/null
done

REGISTERED=$("$HOME_DIR/bin/ca" users --users="$WORK/users" | grep -c 'CN=')
check "four subjects registered" "4" "$REGISTERED"


# ---------------------------------------------------------------------------
# The server
# ---------------------------------------------------------------------------

cat > "$WORK/ziguratip.conf" <<CONF
LOCALE: en_US.utf8
HOME_PATH: $HOME_DIR
RESET_MODE: FALSE
TRACE_MODE: TRUE

SECURITY:
	CERTIFICATE_PATH: $HOME_DIR/etc/cert/
	CERTIFICATE: dont-use-certificate.crt
	PRIVATE_KEY: dont-use-private.key
	AUTHORITY:   dont-use-certificate.crt
	PERMISSIONS_MODE: TRUE
	USERS_PATH: $WORK/users

MEMORY:
	PAGE_SIZE: 8192

TRANSACTION:
	MODE: NON-AUTOCOMMIT
	ISOLATION_LEVEL: READ-COMMITTED

LIBRARY:
	CACHE_MODE: NONE

PARSER:
	TRACE_MODE: FALSE

COMPILER:
	# The checks below declare procedures over the protocol, which is refused
	# by default -- compiling for a client runs a C++ compiler and a linker on
	# what that client sent. Allowed here because the server is this script's
	# own, on loopback, for the length of one run. Nothing shipped enables it.
	REMOTE_MODE: TRUE
	CPP:       c++
	CPP_FLAGS: -Wall -std=c++11 -fPIC
	LD_FLAGS:  -shared
	TRACE_MODE: FALSE

SERVER:
	TYPE: TCP
	PORT: $ZPORT
	BACKLOG: 5
	POOL_SIZE: 5
	BLOCKING_MODE: TRUE
	TIMEOUT: 60
	TLS_MODE: TRUE

HTTP:
	PORT: $HPORT
	BACKLOG: 5
	POOL_SIZE: 5
	BLOCKING_MODE: TRUE
	ASYNCHRONOUS_MODE: FALSE
	TIMEOUT: 60
	TLS_MODE: TRUE
	SESSION_TIMEOUT: 1800
	MAX_URL_LENGTH: 8000
	MAX_HEADERS_LENGTH: 16000
	MAX_CONTENT_LENGTH: 2000000000
CONF

echo "building the probe"
PROBE="$WORK/e2e-probe"
c++ -Wall -std=c++11 -I"$HOME_DIR/include" -L"$HOME_DIR/lib" -o "$PROBE" \
    "$HERE/e2e-probe.cpp" \
    -lConnector -lCore -lStreamIO -lType -lSocketIO -lCryptography \
    -lEncoding -lConfiguration -lThreading -lLibrary -lCompression

echo "starting ziguratip on $ZPORT (log: $LOG)"
"$HOME_DIR/bin/ziguratip" --config="$WORK/ziguratip.conf" > "$LOG" 2>&1 &
SERVER_PID=$!

stop_server() {
  kill "$SERVER_PID" 2>/dev/null || true
  wait "$SERVER_PID" 2>/dev/null || true
}
trap stop_server EXIT INT TERM

wait_ready() {
  # wait_ready <log>. The port answering is not enough on its own: it could be
  # somebody else's. The server this script started has to still be running too.
  i=0
  while [ $i -lt 200 ]; do
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
      echo "server exited while starting:" >&2
      tail -20 "$1" >&2
      return 1
    fi
    if ! port_free "$ZPORT"; then return 0; fi
    sleep 0.1
    i=$((i + 1))
  done

  echo "server did not come up:" >&2
  tail -20 "$1" >&2
  return 1
}

wait_ready "$LOG" || exit 1

probe() {
  # probe <who> <verb> [argument]
  who=$1
  shift
  "$PROBE" "$WORK/cert/$who.crt" "$KEY" "$AUTHORITY" 127.0.0.1 "$ZPORT" "$@" 2>/dev/null
}

page() {
  # page <who> <path>
  who=$1
  shift
  "$PROBE" "$WORK/cert/$who.crt" "$KEY" "$AUTHORITY" 127.0.0.1 "$HPORT" page "$@" 2>/dev/null
}

echo
echo "the register decides who may connect at all"

check "a registered subject connects" \
      "OK" "$(verdict "$(probe schema connect)")"

# Genuine, signed by the authority, granting DEMO -- and refused, because the
# subject is not on the register. This is the revocation the design has.
check "an unregistered subject is refused" \
      "REFUSED" "$(verdict "$(probe stranger connect)")"

echo
echo "permissions decide what a connection may reach"

check "DEMO reaches a procedure in DEMO" \
      "OK" "$(verdict "$(probe schema call demo::count_books)")"

# DEMO::AUTHORS is one table. DEMO::COUNT_BOOKS is a procedure, and nothing in
# the permission covers it, however much the two share a schema.
REFUSAL=$(probe one-table call demo::count_books)
check "one table does not reach a procedure" \
      "REFUSED" "$(verdict "$REFUSAL")"
check "and the refusal names the object" \
      "yes" "$(echo "$REFUSAL" | grep -q 'DEMO::COUNT_BOOKS' && echo yes || echo no)"
check "and the refusal names the subject" \
      "yes" "$(echo "$REFUSAL" | grep -q 'CN=demo-one-table' && echo yes || echo no)"

check "a permission for another schema reaches nothing here" \
      "REFUSED" "$(verdict "$(probe elsewhere call demo::count_books)")"

check "a certificate granting nothing reaches nothing" \
      "REFUSED" "$(verdict "$(probe nothing call demo::count_books)")"

echo
echo "a page is checked by what it requires, because nobody can be granted a page"

# /catalog.zt requires demo::books and demo::authors, so a certificate for the
# schema reaches it.
check "a schema holder loads the page" \
      "HTTP 200" "$(page schema /catalog.zt)"

# One table of the two it needs is not enough, and the answer is a refusal
# rather than a server error: the request was understood.
check "one of the two tables it needs is not enough" \
      "HTTP 403" "$(page one-table /catalog.zt)"

check "a permission for another schema does not load it" \
      "HTTP 403" "$(page elsewhere /catalog.zt)"

# And the register still comes first, before any page is looked at.
check "an unregistered subject cannot reach Zeytun either" \
      "REFUSED" "$(verdict "$(page stranger /catalog.zt)")"

echo
echo "declaring takes permission for what is declared and what it requires"

cat > "$WORK/inside.parsi" <<'PARSI'
PROCEDURE demo::e2e_inside()
RETURNS Void
REQUIRES demo::authors
BEGIN
    SELECT id FROM demo::authors;
END
PARSI

cat > "$WORK/reaching-out.parsi" <<'PARSI'
PROCEDURE demo::e2e_reaching_out()
RETURNS Void
REQUIRES demo::authors
BEGIN
    SELECT id FROM demo::authors;
END
PARSI

check "a schema holder may declare inside it" \
      "OK" "$(verdict "$(probe schema compile "$WORK/inside.parsi")")"

# The same source, from a certificate that holds one table of that schema: the
# procedure being declared is DEMO::E2E_REACHING_OUT, which DEMO::AUTHORS does
# not cover. Without this, anyone allowed one object could write a procedure
# that reads the rest and then call it entirely in order.
check "one table does not let you declare into the schema" \
      "REFUSED" "$(verdict "$(probe one-table compile "$WORK/reaching-out.parsi")")"

echo
echo "the switch turns it all off"

sed 's/PERMISSIONS_MODE: TRUE/PERMISSIONS_MODE: FALSE/' "$WORK/ziguratip.conf" > "$WORK/open.conf"

stop_server
trap - EXIT INT TERM

i=0
while [ $i -lt 100 ]; do
  if port_free "$ZPORT"; then break; fi
  sleep 0.1
  i=$((i + 1))
done

"$HOME_DIR/bin/ziguratip" --config="$WORK/open.conf" > "$WORK/open.log" 2>&1 &
SERVER_PID=$!
trap stop_server EXIT INT TERM

wait_ready "$WORK/open.log" || exit 1

# Same certificate, same call, and now nothing is keyed on either. The register
# is not read, so even the subject that was never on it gets in.
check "the same refused call now succeeds" \
      "OK" "$(verdict "$(probe one-table call demo::count_books)")"
check "and an unregistered subject connects" \
      "OK" "$(verdict "$(probe stranger connect)")"

# The compile checks above declared a procedure, and the refused one may have
# got as far as leaving something behind on an earlier run. Take both away, so
# a run of this leaves the installation as it found it.
for declared in DEMO::E2E_INSIDE DEMO::E2E_REACHING_OUT; do
  rm -f "$HOME_DIR/ld/lib_${declared}_.so" \
        "$HOME_DIR/ld/_${declared}_.hpp" \
        "$HOME_DIR/catalog/_${declared}_.conf" \
        "$HOME_DIR/obj/_${declared}_.o" \
        "$HOME_DIR/tmp/_${declared}_".*
done

echo
if [ "$STATUS" -eq 0 ]; then
  echo "permissions: PASS"
else
  echo "permissions: FAIL"
  echo
  echo "server transcript:"
  tail -30 "$LOG"
fi

exit $STATUS
