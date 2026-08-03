# ZiguratIP

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

Starts a server, runs the full suite against it, stops it again. 213 cases
covering every library, the Parsi grammar, and the storage engine's ACID,
isolation, concurrency and durability behaviour.

---

## Your first application

A worked example with tables, indexes, sequences, procedures and pages lives in
[`demo/`](demo/README.md) — build it with `demo/build.sh` and browse it at
<http://127.0.0.1:2190/setup.zt>. The rest of this section covers the same
ground in miniature.

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
`Connector::compile` — and note the explicit commit:

```parsi
PROCEDURE demo::add_visitor(name AS String)
RETURNS Long
REQUIRES demo::visitors, demo::visitors_id_sequence
BEGIN
    DECLARE id AS Long = demo::visitors_id_sequence::NEXT();
    INSERT INTO demo::visitors VALUES (id, name);
    TRANSACTION COMMIT;
    RETURN id;
END
```

**With `TRANSACTION/MODE: NON-AUTOCOMMIT` — the shipped default — a procedure
called over the binary protocol has to commit its own work with
`TRANSACTION COMMIT;`.** Nothing commits it afterwards: the connection closing
discards it, and `Connector::commit()` does not pick it up. The alternative is
`Connector::auto_commit(true)`, which makes the server commit after every
`call`.

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
in Parsi, in `System/session.sql` — but see [Status](#status): that file
does not currently compile.

---

## Documentation

Full reference in [`doc`](doc/README.md):

- [Installation](doc/installation.md) ·
  [Configuration](doc/configuration.md) ·
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

---

## Status

The server runs, serves both protocols, compiles and executes Parsi, and the
suite passes. Some things are known to be incomplete:

- **TLS** is drafted but not finished; `tlsclient.cpp` and `tlsserver.cpp` are
  empty. Both servers are plaintext.
- **The CA issues X.509 v1 only.** Certificates and requests are
  OpenSSL-compatible — `openssl verify` accepts a chain issued by `ca`, and
  `openssl req -verify` accepts its signing requests — but there is no extension
  encoding yet, so no `basicConstraints`, `keyUsage` or subject alternative
  name. RFC 5280 has a certificate without extensions be v1, which is what these
  are, and the `[0] EXPLICIT Version` field belongs to v3 and is correctly
  absent. A v1 certificate is fine as an explicitly trusted anchor and is
  refused by anything that wants a real chain.
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
- **`Connector::commit()` does not commit a `call`'s work.** The transaction is
  thread-local to the worker, and what the procedure wrote is not part of what
  the commit sees. Either end the procedure with `TRANSACTION COMMIT;` or turn
  on `Connector::auto_commit(true)`; both are verified to persist.
- **`ZLib::compress` only implements DEFLATE**; the `ZLIB` and `GZIP` wrappers
  throw.
- **`nbostream` and `hbostream` are identical** — the "network byte order"
  stream does not actually swap, so the wire format is host-endian.

The CA material under `home/etc/ca` is a sample, named `dont-use-*` for the
obvious reason: its private key is in this repository and is therefore public.
Generate your own with `ca keygen` for anything real — see
[home/etc/ca/README.md](home/etc/ca/README.md).

---

## Licence

[GNU General Public License v3](LICENSE). You may use, study, share and modify
this software; anything you distribute that is derived from it has to carry the
same licence and offer its source.

Note that the vendored `zlib/` keeps its own, more permissive licence — see
`zlib/README`.
