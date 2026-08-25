# ZiguratIP

![Zigurat, the warrior](art/banner.svg)

**Zigurat Informational Platform** — an object-relational database server, a
programming language, and a web application server, built as one system in
C++11 with no third-party dependencies.

Write a table, a stored procedure and a web page in the same language, in the
same file, against the same transaction. The server compiles what you write to
native code and runs it inside the database process.

```parsi
TABLE human_resources::employees
BEGIN
    COLUMN id AS Long PRIMARY KEY;
    COLUMN name AS String UNIQUE KEY NOT NULL;
END

SEQUENCE human_resources::employees_id_sequence
BEGIN
    FROM 1;
    TO Long::MAX;
    STEP 1;
END

PROCEDURE human_resources::insert_employee(name AS String)
RETURNS Long
REQUIRES human_resources::employees
BEGIN
    DECLARE id AS Long = human_resources::employees_id_sequence::NEXT();
    INSERT INTO human_resources::employees VALUES (id, name);
    RETURN id;
END
```

---

## The three parts

| | | |
|---|---|---|
| **Zigurat** | the database | An MVCC storage engine with its own pager, B-tree indexes, five isolation levels and a binary wire protocol on port 2160. |
| **Parsi** | the language | A grammar-driven language covering DDL, DML, control flow and classes. Compiled to C++, then to a shared object the server loads. |
| **Zeytun** | the web server | Serves static files and Parsi pages over HTTP on port 2190. One request is one transaction. Sessions ride on a cookie. |

Around them sits everything they need, all written for this project: big
integers, a type system with SQL-style nullability, binary streams, an encoding
layer (base16/32/64, DER, ASN.1), cryptography (SHA-1/2, HMAC, AES, RSA, X.509),
a certificate authority, sockets, a thread pool, and a configuration parser.

About 50,000 lines across 14 libraries and 4 executables. The only vendored
code is zlib.

---

## Getting started

This section is the reference: what each piece is, where it lives, and what to
set. If you would rather paste ten commands and end up with the demo running,
skip to [From nothing to the demo](#from-nothing-to-the-demo-step-by-step) and
come back.

### Requirements

A C++11 compiler and GNU make. Nothing else. The server also invokes a C++
compiler at runtime to build Parsi objects, so one must stay on `PATH`.

### Build

```bash
git clone <your-fork-url> ZiguratIP
cd ZiguratIP
make
```

Everything installs into `home/`, which is both the install prefix and the
server's runtime home directory:

```
home/bin      ziguratip, parsi, ca, Test
home/lib      the 14 shared libraries
home/include  public headers
home/etc      configuration
home/etc/cert certificates and keys (SECURITY/CERTIFICATE_PATH)
home/http     static web assets
home/data     the page store (created on first run)
home/ld       compiled Parsi objects
home/catalog  schema catalogue
```

### Configure

Point the server at its home directory and library path:

```bash
export ZIGURATIP_HOME=$PWD/home
export DYLD_LIBRARY_PATH=$ZIGURATIP_HOME/lib    # LD_LIBRARY_PATH on Linux
```

Settings live in `home/etc/ziguratip.conf`, which documents every option the
server reads. The shipped values run the demo and the tutorials as they are:
the store is created on first use and kept between restarts, objects are
reloaded rather than cached so a recompile takes effect, and requests are
traced to stdout.

Two worth knowing about before anything busy: `TRACE_MODE` logs every request,
and `LIBRARY/CACHE_MODE: NONE` reloads each compiled object on every use.
Set the first to `FALSE` and the second to `GLOBAL` for a real deployment.

### Run

```bash
./home/bin/ziguratip
```

The server reports what it loaded, then listens on **2160** (binary protocol)
and **2190** (HTTP). Open <http://127.0.0.1:2190/> for the landing page.

### Verify

```bash
./Test/run-e2e.sh
```

Starts a server, runs the full suite against it, stops it again. 257 cases and a
keep-alive check covering every library, the Parsi grammar, and the storage
engine's ACID, isolation, concurrency and durability behaviour.

```bash
./Test/run-permissions-e2e.sh
```

Issues certificates granting different things, starts a secure server on its own
ports, and checks that each one reaches what it should and nothing else — over
both protocols, and with the switch off as well as on. Needs `demo/build.sh` to
have run.

```bash
./Test/run-reload-e2e.sh
```

Replaces a library under a running server and checks that it says so and stays
up. Also needs `demo/build.sh`.

> **Do not rebuild `home/lib` while a server is running.** A compiled object
> names the core libraries as dependencies and the loader resolves those names
> again every time one is opened, so a library replaced mid-run is loaded a
> second time and the object binds to that copy instead of the one the server is
> using. The server notices and refuses the request; restart it.

### Ask a program what it takes

All four accept `--help`, and none of them needs a working installation to
answer:

```bash
./home/bin/ziguratip --help   # the one argument, and every setting with its default
./home/bin/parsi --help       # what it compiles, where it puts it, what it reads
./home/bin/ca --help          # the eight instructions and their options
./home/bin/Test --help        # the filter, the environment, and the suites present
```

`ziguratip` and `parsi` take `--config=<file>` and nothing else: everything else
is configuration, and their help lists the search order for the file and the
keys it may contain. `ca` is the exception — it is a command-line tool proper,
with instructions and options.

---

## From nothing to the demo, step by step

Ten commands, no prior state. The demo is a small book catalogue: two tables
with indexes, sequences, procedures that write, and pages that read.

### 1. Build

```bash
git clone <your-fork-url> ZiguratIP
cd ZiguratIP
make
```

`make MODE=Release` optimises everything; `Core` and `Cryptography` are optimised
either way, because the big-integer arithmetic underneath RSA is seconds
unoptimised and a fraction of one at `-O3`. Expect 14 libraries and 4 programs
under `home/`.

### 2. Point the environment at it

```bash
export ZIGURATIP_HOME=$PWD/home
export DYLD_LIBRARY_PATH=$ZIGURATIP_HOME/lib    # LD_LIBRARY_PATH on Linux
```

Both are needed: the first is where the server looks for its configuration and
resolves every other path from, the second is how the programs find the shared
libraries. A C++ compiler must stay on `PATH` — Parsi objects are compiled at
run time, not only at build time.

### 3. Check the build

```bash
./Test/run-e2e.sh
```

Starts a server, runs all 257 cases against it, stops it again. Expect
`result: PASS`. If this fails, nothing below will work.

### 4. Compile the demo

```bash
./demo/build.sh
```

Five Parsi files become sixteen shared objects in `home/ld` and sixteen
catalogue entries in `home/catalog` — tables, sequences, procedures and pages.
The script starts nothing; it compiles with `parsi` and exits.

### 5. Start the server

```bash
./home/bin/ziguratip
```

It prints what it loaded, then listens on **2160** and **2190**. Leave it
running. It writes `home/data/hexmap` and `home/data/data` on first use and
keeps them between restarts.

### 6. Seed and browse

In a browser, in this order:

| | |
|---|---|
| <http://127.0.0.1:2190/setup.zt> | creates the authors and books rows |
| <http://127.0.0.1:2190/catalog.zt> | browses them |
| <http://127.0.0.1:2190/lookup.zt> | queries served from single-column indexes |
| <http://127.0.0.1:2190/bulk.zt> | loads 500 rows into a second table |
| <http://127.0.0.1:2190/report.zt> | queries those through a two-column index |

Each page is a compiled Parsi object, and each request is one transaction. With
`TRACE_MODE: TRUE` the server's terminal shows every one of them opening and
committing.

### 7. Call a procedure from a client

```bash
cat > count.cpp <<'EOF'
#include "connector.hpp"
#include "typelong.hpp"
#include <iostream>
using namespace Zigurat;

int main()
{
    Connector db;
    db.open("127.0.0.1", "2160", true, 10);
    db.call("demo::count_books");

    Long total(0);
    for (ResultType r = db.result(); r != ResultType::SUCCESSFUL_DONE; r = db.result()) {
        if (r == ResultType::CURSOR_OPEN)       db.columns();
        else if (r == ResultType::RETURN_VALUE) db.fetch(total);
    }
    db.commit();
    std::cout << total.value() << " books" << std::endl;
    db.close();
    return 0;
}
EOF

c++ -std=c++11 -I$ZIGURATIP_HOME/include count.cpp -o count \
    -L$ZIGURATIP_HOME/lib -lConnector -lCore -lStreamIO -lType -lSocketIO \
    -lCryptography -lEncoding -lConfiguration -lThreading -lLibrary -lCompression
./count
```

The same procedure the pages call, over the binary protocol. Drain the results
to `SUCCESSFUL_DONE` or the next request on that connection reads a byte that
already went past.

### 8. Write something of your own

```bash
cat > mine.parsi <<'EOF'
PROCEDURE demo::authors_from(where_from AS String)
RETURNS Long
REQUIRES demo::authors
BEGIN
    DECLARE found AS Long = 0;
    SELECT found = found + 1 FROM demo::authors WHERE country == where_from;
    RETURN found;
END
EOF

./home/bin/parsi mine.parsi
```

The parameter is not called `country`, because `WHERE country == country` would
compare the column with itself. `SELECT` counts by assigning rather than by
returning rows — see [SELECT](doc/select.md).

`CACHE_MODE: NONE` is the shipped default, so the server picks it up on the next
call with no restart. Call it exactly as in step 7.

### 9. Turn on certificates

Nothing so far has required one. Work **outside the repository** — a private key
does not belong in version control, and the shipped `home/etc/cert/dont-use-*`
files are a sample everybody who has cloned ZiguratIP also has:

```bash
mkdir -p ~/zigurat-pki && cd ~/zigurat-pki
cp $ZIGURATIP_HOME/etc/cert/issuer.conf authority.conf   # then edit COMMON_NAME etc.

CA=$ZIGURATIP_HOME/bin/ca
$CA keygen --signature=RSA-2048 --private=authority.key --public=authority.pub
$CA csr    --subject=authority.conf --subject-pik=authority.key --csr=authority.csr
$CA issue  --serial=1 --issuer=authority.conf --issuer-pik=authority.key \
           --csr=authority.csr --certificate=authority.crt
```

The issuer is itself, which is what makes it the root of your small world. Then
the same three steps for the server and for each client, with the authority as
the issuer and a fresh `--serial` each time — and, for a client, `ca issue
--permission=DEMO` to write what it may reach into its certificate. Point
`SECURITY/CERTIFICATE`, `PRIVATE_KEY` and `AUTHORITY` at the result, set
`SERVER/TLS_MODE` and `HTTP/TLS_MODE` to `TRUE`, and restart.
[doc/security.md](doc/security.md) does all of this properly, with the subject
configuration files spelled out.

### 10. Turn on permissions

```
SECURITY:
	PERMISSIONS_MODE: TRUE
```

Now a connection reaches only what its certificate names, and only if its
subject is registered:

```bash
./home/bin/ca put   --certificate=alice-demo.crt   # let a subject connect
./home/bin/ca users                                # who may connect
./home/bin/ca off   --subject-name="CN=alice"      # stop them, at once
```

Verify the whole of it:

```bash
./Test/run-permissions-e2e.sh
```

**Deploying it elsewhere.** `home` is the install prefix and the runtime
directory both, so copying it is the deployment — nothing outside it is needed
at run time. Set `TRACE_MODE: FALSE` and `LIBRARY/CACHE_MODE: GLOBAL` for
anything busy, and remember that caching means a recompiled object is ignored
until the server restarts. [doc/tutorial.md](doc/tutorial.md) covers a real
installation, including putting Zeytun behind a reverse proxy.

---

## Your first application

The section above gets the demo running. This one is the same ground with the
reasoning attached — what a page is, why a sequence is declared rather than
implied, and who owns a transaction — built up one piece at a time. A larger
worked example with indexes and bulk loading lives in
[`demo/`](demo/README.md).

### 1. A page

Zeytun serves any URL ending in `.zt` from a compiled Parsi page. Write
`hello.parsi`:

```parsi
PAGE hello
BEGIN
PUBLIC:
    OVERRIDE FUNCTION PAGE_LOAD() RETURNS Void
    BEGIN
        ECHO '<h1>Hello Zigurat</h1>';
    END
END
```

Compile it:

```bash
./home/bin/parsi hello.parsi
```

`parsi` tokenizes the source, parses it against the grammar in
`home/etc/patterns.conf`, generates C++, and builds `lib_HELLO_.so` into
`home/ld`. With the server running, visit <http://127.0.0.1:2190/hello.zt>:

```
$ curl -i http://127.0.0.1:2190/hello.zt
HTTP/1.1 200 OK
Server: Zeytun/0.0 (ZiguratIP; Darwin)
Content-Length: 22

<h1>Hello Zigurat</h1>
```

### 2. A table and a procedure

Sequences are declared, not implied by a primary key. All three objects can
live in one file, and `parsi` compiles them in order:

```parsi
TABLE demo::visitors
BEGIN
    COLUMN id AS Long PRIMARY KEY;
    COLUMN name AS String NOT NULL;
END

SEQUENCE demo::visitors_id_sequence
BEGIN
    FROM 1;
    TO Long::MAX;
    STEP 1;
END

PROCEDURE demo::add_visitor(name AS String)
RETURNS Long
REQUIRES demo::visitors, demo::visitors_id_sequence
BEGIN
    DECLARE id AS Long = demo::visitors_id_sequence::NEXT();
    INSERT INTO demo::visitors VALUES (id, name);
    RETURN id;
END
```

A procedure that inserts returns the key it inserted. The sequence is consumed
inside the procedure, so the caller has no other way to learn it, and both the
web side and a connector client usually want it.

Each object becomes a catalogue entry under `home/catalog` and a shared object
under `home/ld`, which the server loads on demand. Anything named in `REQUIRES`
is linked against, so declaration order matters.

### 3. Talking to the server from C++

The `Connector` class speaks the binary protocol on port 2160. Compile the
procedure first — with `parsi`, or by sending the same source through
`Connector::compile`:

```parsi
PROCEDURE demo::add_visitor(name AS String)
RETURNS Long
REQUIRES demo::visitors, demo::visitors_id_sequence
BEGIN
    DECLARE id AS Long = demo::visitors_id_sequence::NEXT();
    INSERT INTO demo::visitors VALUES (id, name);
    RETURN id;
END
```

**The transaction belongs to the connection, and the client decides its fate.**
A connection holds one worker thread for its whole life, and the transaction is
that thread's, so every `call` made down one connection is part of the same
transaction: `Connector::commit()` makes all of them stand and
`Connector::rollback()` discards all of them. Closing without either discards
the work. `Connector::auto_commit(true)` commits after every `call` instead, and
a procedure may still end with `TRANSACTION COMMIT;` to commit its own work —
but then the client can no longer roll it back.

Then the client calls it. Arguments go out after `call`, and the results come
back as a sequence the caller drains:

```cpp
#include "connector.hpp"
#include "typelong.hpp"
#include "typestring.hpp"
#include <iostream>
using namespace Zigurat;

int main()
{
    Connector db;
    db.open("127.0.0.1", "2160", true, 10);

    db.call("demo::add_visitor");
    db.write_string(String("pitarugi"));

    Long id(0);
    for (ResultType r = db.result(); r != ResultType::SUCCESSFUL_DONE; r = db.result()) {
        if (r == ResultType::CURSOR_OPEN)       db.columns();
        else if (r == ResultType::RETURN_VALUE) db.fetch(id);
    }
    std::cout << "inserted visitor " << id.value() << std::endl;

    db.close();
    return 0;
}
```

```bash
c++ -std=c++11 -I$ZIGURATIP_HOME/include client.cpp -o client \
    -L$ZIGURATIP_HOME/lib -lConnector -lCore -lStreamIO -lSocketIO \
    -lType -lConfiguration -lCryptography -lEncoding
```

Each connection opens a transaction, and `commit()` and `rollback()` bound the
statements the client itself issues.

### 4. Sessions on the web side

Zeytun makes one HTTP request one transaction: it opens a transaction before
the page runs, commits when the page returns cleanly, and rolls back otherwise.
Sessions are separate and outlive the request, carried by the `ZIPSESSID`
cookie:

```parsi
PAGE counter
REQUIRES Session
BEGIN
PUBLIC:
    OVERRIDE FUNCTION PAGE_LOAD() RETURNS Void
    BEGIN
        session::initialize(request, response);
        ECHO 'visits: ', session::get<Int>('visits');
    END
END
```

The session store is shared across worker threads and swept for idle entries
(`HTTP/SESSION_TIMEOUT`, 1800 seconds by default). `Session` itself is written
in C++, in `HTTP/session.cpp` (the Parsi original was removed: it
does not currently compile.

---

## The Parsi language

Parsi is case-insensitive, and every name is a path: `domain::subdomain::name`.
The domains are namespaces and are optional — `employees` and
`human_resources::employees` are both valid names. The same path is what a
certificate grants access to, and what `REQUIRES` refers to.

### What you can declare

| | |
|---|---|
| [`TABLE`](doc/table.md) | Columns with `PRIMARY KEY`, `UNIQUE KEY`, `INDEX` and `NOT NULL`. Each key builds a B-tree in the storage engine. |
| [`SEQUENCE`](doc/sequence.md) | `FROM`, `TO`, `STEP`. Declared explicitly — a primary key does not imply one. |
| [`PROCEDURE`](doc/procedure.md) | Typed parameters, `RETURNS`, `REQUIRES`. The unit a client calls and the unit a permission names. |
| [`CLASS`](doc/class.md) | Fields, methods, constructors and destructors, inheritance, `VIRTUAL` and `OVERRIDE`. |
| [`TYPE`](doc/type.md) | An alias for another type. |
| [`ENUM`](doc/enum.md) | A named set of values. |
| [`PAGE`](doc/page.md) | A class Zeytun serves at a URL. Overrides `PAGE_LOAD()`; sees `request`, `response` and `session`. |

`TABLE`, `SEQUENCE` and `PROCEDURE` are **named objects**: a permission can name
one, and a caller has to hold it. `CLASS`, `TYPE`, `ENUM` and `PAGE` are not —
nobody is granted a page, so what is checked is whatever it reaches.

### Statements

[`DECLARE`](doc/declare.md) · [`SET`](doc/set.md) · [`ECHO`](doc/echo.md) ·
[`IF`](doc/if.md) · [`DO … WHILE` and `WHILE`](doc/do.md) ·
[`TRY`](doc/try.md) · `THROW` · `RETURN` · [`CALL`](doc/call.md) ·
[`DEFAULT`](doc/default.md)

Data manipulation is part of the language rather than strings handed to a
driver: [`SELECT`](doc/select.md) · [`INSERT`](doc/insert.md) ·
[`UPDATE`](doc/update.md) · [`DELETE`](doc/delete.md) ·
[`TRUNCATE`](doc/truncate.md) · [`TRANSACTION`](doc/transaction.md)

`SELECT` is a **cursor, not a result set**. Everything between `SELECT` and
`FROM` runs once per row, and there is nothing to iterate afterwards. An item
written `variable = expression` assigns instead of emitting, which is how a
`SELECT` counts or totals:

```parsi
DECLARE rows AS Long = 0;
SELECT rows = rows + 1 FROM demo::books WHERE year == 2026;
```

### Types

Every type is nullable, the way a SQL column is, and carries that as state
rather than as a separate flag: `Long id(0); id.is_null()`.

| | |
|---|---|
| `Bool` | `TRUE` or `FALSE` |
| `Char` `Byte` `UByte` | 8 bits, signed and unsigned |
| `Short` `UShort` | at least 16 bits |
| `Int` `UInt` | at least 32 bits |
| `Long` `ULong` | at least 64 bits |
| `Float` `Double` `Real` | 32, 64 and 80-bit floating point |
| `Timestamp` | a point in time |
| `String` `Text` | up to 255 and 65,535 characters |
| `Vector<T>` | up to 4,294,967,295 elements |
| `Object` | the type of `NULL`, and not a type you declare |

Full reference in [doc/datatypes.md](doc/datatypes.md); operators and precedence
in [doc/expression.md](doc/expression.md).

### Reaching C++

`INCLUDE` and `LINK` at the top of a file put a header and a library on the
generated object's compile and link lines, so a Parsi object can use any C++
library on the machine. [doc/cpp.md](doc/cpp.md) covers the syntax that reaches
through.

---

## Compiled objects

Parsi is not interpreted and there is no plan cache. Every object becomes a
shared library, and the server `dlopen`s it.

```
source.parsi
   │  Tokenizer          home/etc/patterns.conf — the grammar, read at runtime
   │  Parser                  so the language is data, not a generated parser
   ▼
home/tmp/_NAME_.cpp + .hpp        generated C++, one file per object
   │  c++ -fPIC -c
   │  c++ -shared
   ▼
home/ld/lib_NAME_.so             plus home/catalog/_NAME_.conf
```

Each library exports a small, fixed interface:

| | |
|---|---|
| `call` | a procedure: reads its arguments off the connection, writes its results back |
| `new_page` / `delete_page` | a page: constructs it against the request and response |
| `links` | the link flags anything requiring this object also needs |
| `objects` | the named objects this one lets a caller reach |

`objects` is what makes permissions enforceable without a grants table: the
answer travels inside the code it describes and cannot drift from it. A library
built before it existed is refused rather than waved through.

The catalogue entry beside it records the name, its path, the guard macro, the
hash key and what it `REQUIRES`, which is how the compiler resolves a dependency
without reading anyone's source again.

Three ways in: `home/bin/parsi file.parsi` offline, `Connector::compile` over the
binary protocol, or the compiler page at `/compiler.zt`. All three run the same
compiler in the same process shape.

> `LIBRARY/CACHE_MODE` decides what happens next. `NONE` re-opens the object on
> every use, so a recompile takes effect immediately — what you want while
> developing. `GLOBAL` and `LOCAL` cache it, which is faster and means a
> recompiled object is ignored until the server restarts.

---

## Transactions and concurrency

Both servers are a thread pool in front of a socket, and both bind what a
connection is doing to the thread serving it — the client stream, the peer's
identity, and the transaction all live in thread-local state that is unbound
again on the way out.

**Zigurat: the transaction belongs to the connection.** One connection holds one
worker thread for its whole life, so every `call` down that connection is part
of one transaction. The client ends it — `commit()` makes all of them stand,
`rollback()` discards all of them, closing without either discards the work.
`auto_commit(true)` commits after each call instead, and a procedure can end with
`TRANSACTION COMMIT;` to commit its own — though then the client can no longer
roll it back.

**Zeytun: one request is one transaction.** It opens one before the page runs,
commits when the page returns cleanly and rolls back otherwise. Sessions are
separate and outlive the request.

`SERVER/POOL_SIZE` and `HTTP/POOL_SIZE` are therefore the number of concurrent
transactions and concurrent requests, five each by default.

---

## The binary protocol

Port 2160. A client sends a function name, then whatever that function reads;
the server answers with a stream of tagged results the client drains.
[`Connector`](doc/connector.md) is the C++ client, and there is nothing in it a
different language could not reimplement.

| function | |
|---|---|
| `echo` | round-trips a string; the cheapest liveness check there is |
| `call` | runs a compiled procedure |
| `compile` | compiles Parsi source sent inline |
| `commit` / `rollback` | ends the connection's transaction |
| `auto_commit` | commit after every call, or not |
| `isolate` | sets the isolation level for this connection |
| `dba_pagefiles` / `dba_pointers` | the storage engine's own view of itself |
| `dba_attach_watcher` / `dba_detach_watcher` | streams engine events to the client |
| `close` | ends the conversation |

Every answer is a sequence of `ResultType` bytes, and a client that stops reading
early desynchronises the connection:

| | |
|---|---|
| `SUCCESSFUL_DONE` | the end of one answer |
| `CURSOR_OPEN` `CURSOR_FETCH` `CURSOR_CLOSE` | a `SELECT`, one row at a time |
| `RETURN_VALUE` | what the procedure returned |
| `EXCEPTION_THROWN` | a message, and then the connection ends |

---

## Security

Both ports can require every client to present a certificate the configured
authority issued, and both ends prove themselves: `SERVER/TLS_MODE` and
`HTTP/TLS_MODE`. The transport is ZiguratIP's own TLS 1.2 with RSA key
transport — see [Status](#status) for what that is and is not worth.

On top of that sits an access model with **no server-side grant table**:

- **What a client may reach is written into its certificate.** `ca issue
  --permission=DEMO` grants a schema, `--permission=DEMO::AUTHORS` one object,
  `*` everything. A certificate naming none reaches nothing. One subject can hold
  several certificates granting different things, and which one it presents is
  what decides.
- **Who may connect at all is a directory.** `SECURITY/USERS_PATH` holds one file
  per subject; a subject with no file is refused during the handshake, whichever
  certificate it presents. Removing the file withdraws access at once, and there
  is nothing else to undo.
- **Checked in three places:** the handshake, invocation (`call`, and a page by
  what it requires), and declaration — so a caller allowed one schema cannot
  compile a procedure that reads another and then legitimately call it.

`SECURITY/PERMISSIONS_MODE` is the one switch for all of it, off by default,
because half-enforced would be worse than either.

The `ca` tool does the rest: `keygen`, `csr`, `issue`, `pikval`, `pukval`, and
`put`, `off` and `users` for the directory. Its certificates and requests are
OpenSSL-compatible. [doc/security.md](doc/security.md) walks through it from an
empty directory.

---

## Documentation

Full reference in [`doc`](doc/README.md):

- [**Tutorial**](doc/tutorial.md) — building, deploying, configuring, securing,
  putting Zeytun behind a proxy, and running the samples ·
  [Installation](doc/installation.md) ·
  [Configuration](doc/configuration.md) ·
  [Server security](doc/security.md) ·
  [Connector](doc/connector.md) ·
  [C++ API](doc/cpp.md)
- [Language overview](doc/parsi.md) ·
  [Data types](doc/datatypes.md) ·
  [Expressions](doc/expression.md)
- [TABLE](doc/table.md) ·
  [PROCEDURE](doc/procedure.md) ·
  [CLASS](doc/class.md) ·
  [SEQUENCE](doc/sequence.md)
- [SELECT](doc/select.md) ·
  [INSERT](doc/insert.md) ·
  [UPDATE](doc/update.md) ·
  [DELETE](doc/delete.md) ·
  [TRUNCATE](doc/truncate.md) ·
  [TRANSACTION](doc/transaction.md)
- [PAGE](doc/page.md) ·
  [Request](doc/request.md) ·
  [Response](doc/response.md) ·
  [Session](doc/session.md)

---

## How it fits together

```
                    Parsi source (.parsi)
                            |
                     Tokenizer -> Parser              patterns.conf
                            |                        (the grammar,
                     Compiler -> C++                  loaded at runtime)
                            |
                   c++ -shared -> home/ld/lib_NAME_.so
                            |
   ┌────────────────────────┴────────────────────────┐
   │                                                 │
Zigurat (2160)                                  Zeytun (2190)
binary protocol                                 HTTP
   │                                                 │
   └──────────────────► MVCCS ◄──────────────────────┘
                   page store + B-tree indexes
                    home/data/{hexmap,data}
```

### The storage engine

`MVCCS` keeps two files: `hexmap`, a chunk allocation map, and `data`, the pages
themselves. Rows are addressed by SHA-1 key and indexed by a templated B-tree.
Every row carries a control record with online and offline state, a row lock and
timestamps, which is what gives the five isolation levels:

| Level | Behaviour |
| --- | --- |
| `READ-UNCOMMITTED` | sees uncommitted rows |
| `READ-COMMITTED` | waits for the writer, then reads only what committed |
| `REPEATABLE-READ` | takes shared locks and retries on conflict |
| `SNAPSHOT` | reads the store as of the transaction's start time |
| `SERIALIZABLE` | one transaction at a time |

Rows are never physically reclaimed. A delete sets a flag and a rollback marks
the row deleted, so the store keeps its history and can be read back at an
earlier point in time.

---

## Project layout

```
Core/           big integers, arrays, polynomials, utility
StreamIO/       binary streams, custom streambufs, typed serialisation
Type/           Bool, Int, Long, String, Text, Timestamp, Vector, ...
Encoding/       base16/32/32hex/64/64url, CTE, DER, ASN.1
Compression/    zlib wrapper
Cryptography/   SHA-1/2, HMAC, HKDF, AES, RSA, X.509
Configuration/  configuration files and command line arguments
Threading/      thread pool
SocketIO/       sockets, TCP/IPC streams, TLS records
Connector/      client library for the binary protocol
HTTP/           request, response, server, session
MVCCS/          the storage engine
Compiler/       tokenizer, parser, Parsi to C++ compiler
Library/        shared object loader and pool
System/         built-in catalogue objects, written in Parsi
ca/             X.509 certificate authority tool
parsi/          standalone Parsi compiler
ziguratip/      the server
Test/           test suite
doc/            language and API reference
home/           install prefix and runtime home
zlib/           vendored zlib
```

Each module is an independent make target producing one shared library; the
top-level `Makefile` builds them in dependency order. `System` is the exception:
its sources are Parsi, compiled by `parsi` rather than by `make`, and it is not
part of the default build (see [Status](#status)).

Headers are `.hpp` and implementations `.cpp`, so nothing in the tree is
mistaken for C. The headers the compiler generates for Parsi objects follow the
same convention. Vendored zlib is C and keeps its own `.h` names.

### What each library carries

Nothing here wraps a third-party equivalent; each was written for this project,
which is why the list is as long as it is.

| | |
|---|---|
| `Core` | `BigInt` arbitrary-precision arithmetic (what RSA rests on), arrays, polynomials, filesystem and string utilities |
| `StreamIO` | `binarystream` and `textstream` with typed reads and writes, custom `streambuf`s, byte-order handling |
| `Type` | the nullable value types, `Vector`, and the type descriptor byte the wire protocol uses |
| `Encoding` | base16/32/32hex/64/64url, CTE, DER and ASN.1 |
| `Compression` | DEFLATE over vendored zlib |
| `Cryptography` | SHA-1/2, HMAC, HKDF, AES, RSA, X.509 certificates and requests |
| `Configuration` | the indentation-based configuration format, and command-line arguments |
| `Threading` | the thread pool both servers run on |
| `SocketIO` | sockets, TCP and IPC streams, and the TLS 1.2 record layer |
| `Connector` | the client for the binary protocol |
| `HTTP` | request, response, the server, sessions and MIME types |
| `MVCCS` | the storage engine: pager, B-trees, transactions, isolation, `Globals` |
| `Compiler` | tokenizer, the pattern-driven parser, and Parsi to C++ |
| `Library` | the shared-object loader and the pool that caches handles |

The build order in the top-level `Makefile` is the dependency order:
`Core → StreamIO → Type → Library → Encoding → Compression → Cryptography →
Configuration → Threading → SocketIO → Connector → HTTP → MVCCS → Compiler`,
then the four programs.

### The four programs

| | |
|---|---|
| `ziguratip` | the server: both protocols, the storage engine, and the compiler |
| `parsi` | the compiler on its own, for compiling objects without a server |
| `ca` | the certificate authority, and the registry of who may connect |
| `Test` | the suite; `Test/run-*.sh` start a server around it |

All four take `--help`.

---

## Working on ZiguratIP

```bash
make                  # everything, Debug
make MODE=Release     # optimised
make -C MVCCS         # one module
make clean            # and "make clean -C MVCCS" for one
```

A module's `Makefile` is four variables and one include:

```make
PROJECT := MVCCS
TYPE    := Library          # or Executable
HEADERS := memory.hpp globals.hpp ...     # copied to home/include
SOURCES := memory.cpp globals.cpp ...     # compiled into home/obj
LIBS    := -lCore -lCryptography -lStreamIO
include ../Makefile.global
```

`Makefile.global` does the rest: it copies the headers, generates a
`<Module>-<platform>-<compiler>.depend`, and links either one shared library
into `home/lib` or one program into `home/bin`. Adding a module is a directory
with a `Makefile` in that shape and a line in `PROJECTS`. `home/include` is what
`-I$ZIGURATIP_HOME/include` gives a client, so a header that is not in `HEADERS`
is private to its module.

Tests are `ZTEST(Suite, name)` blocks in `Test/test_*.cpp`, registered at load
time, so a new file needs nothing but a line in `Test/Makefile`. `Test <filter>`
runs one suite or one case; `Test --help` lists what is there.

Three scripts do the things a unit test cannot see, because they need two
processes:

| | |
|---|---|
| `Test/run-e2e.sh` | the suite against a live server, plus keep-alive and a missing page |
| `Test/run-permissions-e2e.sh` | certificates granting different things, over both protocols |
| `Test/run-reload-e2e.sh` | a library replaced under a running server |

The last two need `demo/build.sh` to have run. All three start their own server
and stop it again.

---

## Status

The server runs, serves both protocols, compiles and executes Parsi, and the
suite passes. Some things are known to be incomplete:

- **TLS is TLS 1.2, verified against OpenSSL, but only with RSA key exchange.**
  Both servers can require every client to hold a certificate the configured
  authority issued, and the connector can present one — see
  [doc/security.md](doc/security.md). `openssl s_client` completes a mutually
  authenticated handshake against it and agrees the chain: `Cipher is
  AES256-SHA256`, `Verify return code: 0 (ok)`. What is missing is everything
  modern — no elliptic curve key exchange, no AEAD suites, no session
  resumption, no renegotiation — and browsers dropped static RSA key exchange
  years ago, so `HTTP/TLS_MODE: TRUE` is reachable from OpenSSL-based clients
  but not from a browser; put a reverse proxy in front if you need one. The
  cryptography underneath is ZiguratIP's own and has had no adversarial review,
  and the MAC comparison is not constant time. A closed-network measure, not
  transport security against a capable attacker.

- **The CA encodes one extension, and no others.** Certificates and requests are
  OpenSSL-compatible — `openssl verify` accepts a chain issued by `ca`, and
  `openssl req -verify` accepts its signing requests. `ca issue --permission`
  writes what its holder may reach into a private extension, which makes the
  certificate v3; without it the certificate is v1, which is what RFC 5280 says
  a certificate with no extensions is. The standard extensions are still
  missing: no `basicConstraints`, `keyUsage` or subject alternative name. Such a
  certificate is fine as an explicitly trusted anchor and is refused by anything
  that wants a real chain.
- **`keygen` does not force the modulus width.** `RSA-2048` produces a modulus
  of 2047 or 2048 bits depending on the primes it lands on, because the top bits
  of *p* and *q* are not fixed. Interoperable either way, just not the width that
  was asked for.
- **The system catalogue objects do not build.** `System` holds the
  built-in objects written in Parsi itself — `Session`, `Connector`, the RPC
  console and the memory viewer among them — but 9 of the 12 are written
  against the pre-migration C++ API and no longer compile. `System` is not in
  the top-level `Makefile` for that reason. Until they are ported,
  `REQUIRES Session` has nothing to link against; the C++ `Session` in
  `HTTP` is complete and callable from generated pages in the meantime.
- **`ZLib::compress` only implements DEFLATE**; the `ZLIB` and `GZIP` wrappers
  throw.
## Licence

[GNU General Public License v3](LICENSE). You may use, study, share and modify
this software; anything you distribute that is derived from it has to carry the
same licence and offer its source.

Note that the vendored `zlib/` keeps its own, more permissive licence — see
`zlib/README`.
