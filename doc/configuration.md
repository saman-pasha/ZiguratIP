# Configuration

ziguratip searches for configuration file named ziguratip.conf as order, --config "path_to/ziguratip.conf" command line argument, $ZIGURATIP_HOME/etc/ziguratip.conf environment variable, $HOME/ZiguratIP/etc/ziguratip.conf folder in current user home directory, /etc/ZiguratIP/ziguratip.conf in UNIXs or %PROGRAMDATA%/ZiguratIP/ziguratip.conf in Windows.

## LOCALE

The locale facility includes internationalization support for character classification and string collation, numeric, monetary, and date/time formatting and parsing, and message retrieval. Locale settings control the behavior of stream I/O, and other components of the ZiguratIP project. [Available locales](locales.md).

## HOME_PATH

The path of the ZiguratIP home directory. If is not specified, ziguratip searches for home folder as order, $ZIGURATIP_HOME enviroment variable, $HOME/ZiguratIP folder in current user home directory.

## CATALOG_PATH

Explicitly defined the path of the ZiguratIP catalog directory.

## RESET_MODE

Boolean config for reset the whole data stored in ZiguratIP data directory at start of process.

## TRACE_MODE

Boolean config that enables tracing on ziguratip transactions and client calls.

## LIBRARY/GLOBAL_CACHE_MODE

Enables caching libraries globaly between sessions during each ziguratip process run.

## LIBRARY/LOCAL_CACHE_MODE

Enables caching libraries localy during each session life time.

## TRANSACTION/MODE

Two values: AUTOCOMMIT, NON-AUTOCOMMIT allowed for this config for enbaling a commit after each Remote Procedure Call.

## TRANSACTION/ISOLATION_LEVEL

Allowed values: READ-UNCOMMITTED, READ-COMMITTED, REPEATABLE-READ, SNAPSHOT, SERIALIZABLE specifies default SQL-92 transaction isolation level standard.

## PARSER/TRACE_MODE

Enables tracing parser Decision Tree and Abstract Syntax Tree during parsing [Parsi Server Side Programming Language](parsi.md) code.

## PARSER/PATTERNS_FILE

Explicitly specifies path to parser patterns file. As default is ZIGURATIP_HOME/etc/patterns.conf.

## COMPILER/CPP

Defines path to c++ compiler. Default value is c++.

## COMPILER/CPP_FLAGS

Defines flags for compiling libraries. Default value is -Wall -std=c++11 -fPIC.

## COMPILER/LD_FLAGS

Defines flags for building and linking libraries. Default value is -shared.

## COMPILER/INCLUDE_PATH

Defines path where the ZiguratIP headers are. Default value is ZIGURATIP_HOME/include.

## COMPILER/OBJ_PATH

Defines path where c++ object files xxx.o should stored. Default value is ZIGURATIP_HOME/obj.

## COMPILER/LIB_PATH

Defines path where the ZiguratIP libraries are. Default value is ZIGURATIP_HOME/lib.

## COMPILER/TMP_PATH

Define path to TEMP folder. Default value is ZIGURATIP_HOME/tmp.

## COMPILER/LD_PATH

Defines path where user headers and libraries should stored. Default value is ZIGURATIP_HOME/ld.

## COMPILER/TRACE_MODE

Enables tracing c++ header and implementation files and commands to produce them.

## MEMORY/BLOCK_SIZE

Specifies memory block size. NOTICE: max row size computes as BLOCK_SIZE - 96.

## SERVER/TYPE

Defines type of DBMS server, TCP or IPC (UNIXs only).

## SERVER/PATH

Defines path of IPC type server.

## SERVER/PORT

Defines port number of TCP type server. Default is 2160 like ZIGO.

## SERVER/BACKLOG

How many connections the kernel will hold waiting to be accepted. Beyond it a
client is refused at connect time.

## SERVER/POOL_SIZE

Worker threads, and so **the most clients that can be connected to the binary
port at once**.

This is not a throughput setting to be tuned later. A connection holds one of
these threads for its whole life — the transaction *is* the connection, see
[transaction.md](transaction.md) — so `POOL_SIZE` is a hard limit on how many
clients the server will talk to. Connections past it are not refused: they sit
in the accept queue with no greeting until a thread frees, which the client can
only see as its own socket timing out. Nothing appears in the server log.

So set it above the number of clients you expect to be connected at the same
time, not the number of requests per second. It ships as 32.

## SERVER/TIMEOUT

Defines timeout of idle or half-closed state connections.

## SERVER/TLS_MODE

Requires every client of the binary protocol to present a certificate the
SECURITY authority issued, and encrypts the connection. TCP only. A server told
to be secure that cannot read its certificate refuses to start rather than
listening in the clear. See [security.md](security.md).

## SECURITY/CERTIFICATE_PATH

Where the certificate files live. A bare file name in the settings below is
looked for here; an absolute path is taken as given. Defaults to
ZIGURATIP_HOME/etc/cert.

## SECURITY/CERTIFICATE

This installation's own certificate, presented by both servers and issued by
the authority below.

## SECURITY/PRIVATE_KEY

The key that certificate belongs to.

## SECURITY/PRIVATE_KEY_CIPHER

Its pass phrase, if the key file is encrypted. Empty otherwise.

## SECURITY/AUTHORITY

The certificate that must have signed a peer's, and so the whole of who may
connect. Both servers and the connector name the same one.

## SECURITY/PERMISSIONS_MODE

TRUE keys what a connection may reach on the certificate that opened it: the
subject has to be registered in USERS_PATH, and every table and procedure the
request touches has to be covered by a permission the issuer wrote into that
certificate. FALSE, the default, leaves the connection encrypted and
authenticated but enforces nothing beyond that. See
[security.md](security.md#permissions).

## SECURITY/USERS_PATH

The directory of subjects that may connect: one file per subject, named after
it. Managed with `ca put`, `ca off` and `ca users`. Defaults to
ZIGURATIP_HOME/etc/users. Read only when PERMISSIONS_MODE is TRUE.

## HTTP/PORT

Defines the port number of HTTP Server. Maybe 80 or 2190 like zigo.

## HTTP/BACKLOG

Defines limit the number of concurrent client connections or sessions.

## HTTP/TIMEOUT

Defines timeout of idle or half-closed state connections.

## HTTP/BLOCKING_MODE

Enables persistent connection or keep-alive.

## HTTP/ASYNCHRONOUS_MODE

Enables pipelining.

## HTTP/TLS_MODE

The same as SERVER/TLS_MODE, for Zeytun. Note that this is TLS 1.2 with static
RSA key exchange only, which browsers no longer accept: turning it on secures
Zeytun for OpenSSL-based clients and makes it unreachable from a browser. Put a
reverse proxy in front and leave this off if a browser has to reach it.

## HTTP/MAX_URL_LENGTH

Limits the maximum of characters allowed in URL. Default is 8,000.

## HTTP/MAX_HEADERS_LENGTH

Limits the maximum of characters allowed in headers sent by client. Default is 16,000.

## HTTP/MAX_CONTENT_LENGTH

Limits the maximum of characters allowed in data sent by client. Default is 2,000,000,000.

## HTTP/MIME_FILE

Explicitly specifies path to mime definitions file. As default is ZIGURATIP_HOME/etc/mime.conf.
