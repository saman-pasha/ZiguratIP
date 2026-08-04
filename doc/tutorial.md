# Tutorial

From a fresh clone to a running, secured installation serving its own pages.

Everything here was run as written on a clean checkout. Where a command's output
matters it is shown.

- [1. What you are installing](#1-what-you-are-installing)
- [2. Building](#2-building)
- [3. Deploying](#3-deploying)
- [4. First run](#4-first-run)
- [5. Running the samples](#5-running-the-samples)
- [6. Configuring](#6-configuring)
- [7. Securing it with the CA](#7-securing-it-with-the-ca)
- [8. Putting Zeytun behind nginx or Apache](#8-putting-zeytun-behind-nginx-or-apache)
- [9. Where to go next](#9-where-to-go-next)

## 1. What you are installing

Three things in one process:

| | |
|---|---|
| **Zigurat** | the object-relational store, and a binary protocol on port 2160 |
| **Parsi** | the language its schema, procedures and pages are written in |
| **Zeytun** | the web front end, on port 2190 |

A Parsi source is compiled to C++, then to a shared object, and loaded into the
running server. A page is a compiled object that writes a response; a procedure
is one a client can call. There is one database underneath both ports.

The only third-party code is the vendored `zlib`. Everything else — the storage
engine, the compiler, the HTTP layer, the RSA and AES and SHA — is in the tree.

## 2. Building

You need a C++11 compiler and `make`. The compiler is needed at **run time** as
well, because the server compiles Parsi sources while it is running.

```bash
git clone https://github.com/saman-pasha/ZiguratIP.git && cd ZiguratIP
```

```bash
make
```

That publishes every public header into `home/include`, builds fourteen shared
libraries into `home/lib`, and four programs into `home/bin`:

| | |
|---|---|
| `ziguratip` | the server: Zigurat and Zeytun together |
| `parsi` | the compiler, for building sources ahead of time |
| `ca` | the certificate authority tool |
| `Test` | the test suite |

A release build turns the optimiser on for everything:

```bash
make MODE=Release
```

`Core` and `Cryptography` are optimised in either mode. The big-integer
arithmetic is the hot code of the whole system — one RSA private-key operation
is seconds unoptimised and a fraction of one at `-O3` — and nobody debugging the
storage engine should pay that.

### Checking the build

```bash
make -C Test && Test/run-e2e.sh
```

That starts a server, runs every suite against it including the live Connector
round trip and a keep-alive check, and stops it again. Expect
`257 cases, 1111 checks, result: PASS`.

Once the demo is compiled — step 5 below — two more scripts run.
`Test/run-permissions-e2e.sh` adds the certificate side: it issues several
certificates granting different things and checks what each one can reach, over
both protocols. `Test/run-reload-e2e.sh` replaces a library under a running
server and checks that it survives it.

That last one is worth knowing about before you meet it by accident. Do not
rebuild `home/lib` while a server is running. A compiled object names the core
libraries as dependencies, and the loader resolves those names again every time
one is opened — so a library replaced mid-run is a different file from the one
the server mapped at startup, and gets loaded a second time. The object then
runs against a storage engine that was never opened. The server notices and
refuses, and the only cure is the restart it asks for.

## 3. Deploying

`home` is both the install prefix and the runtime directory. Copy it wherever
the server should live — nothing outside it is needed at run time.

```
home/bin      the four programs
home/lib      the shared libraries
home/include  headers, for compiling Parsi objects and your own C++ clients
home/etc      configuration, and etc/cert for certificates
home/http     what Zeytun serves
home/data     the page store: hexmap and data
home/ld       compiled Parsi objects
home/catalog  what the store knows about them
home/tmp      generated C++ on its way to being an object
home/log      where you may want the server's output
```

Three variables point at it:

```bash
export ZIGURATIP_HOME=/path/to/ZiguratIP/home
export PATH=$ZIGURATIP_HOME/bin:$PATH
export LD_LIBRARY_PATH=$ZIGURATIP_HOME/lib:$ZIGURATIP_HOME/ld:$LD_LIBRARY_PATH
```

On macOS that last one is `DYLD_LIBRARY_PATH`. `home/ld` belongs on it because
compiled Parsi objects link against each other.

## 4. First run

```bash
ziguratip
```

The store is created on first run. The output names every setting it read, which
is the quickest way to see whether it found your configuration:

```
Memory page size: '8192'
Server type: 'TCP'
Server service: '2160'
Server TLS mode: 'FALSE'
Zigurat is ready ...
Zeytun TLS mode: 'FALSE'
Zeytun is ready ...
```

Open <http://localhost:2190/>. That page is `home/http/index.html`, served as a
static file, and it links to everything below.

Stop it with Ctrl-C. A commit that has happened is on disk; `home/data` survives
restarts unless `RESET_MODE` says otherwise.

## 5. Running the samples

The demo builds a small book catalogue — tables, indexes, a sequence, procedures
and five pages — and explains each piece as it goes. Compile it:

```bash
demo/build.sh
```

Fifteen objects are compiled into `home/ld`. Then, with the server running:

| | |
|---|---|
| <http://localhost:2190/setup.zt> | seeds the data through a procedure |
| <http://localhost:2190/catalog.zt> | reads it back |
| <http://localhost:2190/lookup.zt> | queries served from the B-trees |
| <http://localhost:2190/bulk.zt> | loads 500 rows |
| <http://localhost:2190/report.zt> | queries them through a two-column index |

Visit them in that order the first time — `catalog.zt` has nothing to show until
`setup.zt` has run, and `report.zt` nothing until `bulk.zt` has.

[demo/README.md](../demo/README.md) is the walk-through: what each source does,
why the indexes are shaped the way they are, and what the generated C++ looks
like. Start there before writing your own.

To compile a source of your own:

```bash
parsi my/source.parsi
```

The server has to be restarted to pick up a recompiled object — nothing unloads
one that is already in memory.

## 6. Configuring

`home/etc/ziguratip.conf` documents every setting the server reads, inline, with
its default. The ones worth knowing first:

```
RESET_MODE: FALSE       # TRUE erases every table at startup
TRACE_MODE: TRUE        # log each request and transaction; turn off when busy

MEMORY:
	PAGE_SIZE: 8192

TRANSACTION:
	MODE: NON-AUTOCOMMIT
	ISOLATION_LEVEL: READ-COMMITTED

LIBRARY:
	CACHE_MODE: NONE    # reload objects on use; GLOBAL or LOCAL cache them

COMPILER:
	CPP: c++            # must be on PATH at run time

SERVER:                 # Zigurat
	PORT: 2160
	POOL_SIZE: 5

HTTP:                   # Zeytun
	PORT: 2190
	POOL_SIZE: 5
	SESSION_TIMEOUT: 1800
```

`POOL_SIZE` is worker threads, and a thread is held for as long as a connection
lives — Zeytun keeps connections alive for `TIMEOUT` seconds. Raise it if you
expect more concurrent clients than five.

Full reference: [configuration.md](configuration.md).

## 7. Securing it with the CA

ZiguratIP can require every client to hold a certificate you issued. There is no
password anywhere in it: membership is the price of admission, and the
certificate can carry what its holder may reach as well.

[security.md](security.md) is the reference and covers this in more detail. In
short, you issue an authority, then a certificate for each end.

### The authority

```bash
cp $ZIGURATIP_HOME/etc/cert/issuer.conf authority.conf
```

Edit it so it describes you, then:

```bash
ca keygen --signature=RSA-2048 --encoding=DER --private=authority.key --public=authority.pub
```

```bash
ca csr --subject=authority.conf --subject-pik=authority.key --hash=SHA-256 --encoding=DER --csr=authority.csr
```

```bash
ca issue --serial=1 --issuer=authority.conf --issuer-pik=authority.key --csr=authority.csr --hash=SHA-256 --encoding=DER --certificate=authority.crt
```

Guard `authority.key`. Whoever holds it can issue a certificate your servers
will accept.

### One for the server, one for each client

The same three steps with the authority as issuer. Set a `COMMON_NAME` that says
which end it is — that is what the other end will see.

```bash
cp authority.conf server.conf   # then set COMMON_NAME: zigurat-server
```

```bash
ca keygen --signature=RSA-2048 --encoding=DER --private=server.key --public=server.pub && ca csr --subject=server.conf --subject-pik=server.key --hash=SHA-256 --encoding=DER --csr=server.csr && ca issue --serial=2 --issuer=authority.conf --issuer-pik=authority.key --csr=server.csr --hash=SHA-256 --encoding=DER --certificate=server.crt
```

Repeat with `client.conf` and a different `--serial`.

OpenSSL will confirm the chain, and has no stake in the answer:

```bash
openssl x509 -in authority.crt -inform DER -out authority.pem && openssl x509 -in server.crt -inform DER -out server.pem && openssl verify -CAfile authority.pem server.pem
```

### Turning it on

Put the files somewhere the server can read and nothing else can, then in
`ziguratip.conf`:

```
SECURITY:
	CERTIFICATE_PATH: /etc/ziguratip/cert
	CERTIFICATE:  server.crt
	PRIVATE_KEY:  server.key
	AUTHORITY:    authority.crt

SERVER:
	TLS_MODE: TRUE
```

A server told to be secure that cannot read its certificate refuses to start,
rather than quietly listening in the clear.

And for a client, in `home/etc/connector.conf`:

```
HOST: localhost
PORT: 2160
TLS_MODE: TRUE
CERTIFICATE:  client.crt
PRIVATE_KEY:  client.key
AUTHORITY:    authority.crt
```

A client with no certificate, or one some other authority issued, is refused
during the handshake with `UNKNOWN_CA` — before it can send a request.

### Deciding what each client may reach

Membership lets a client in. Permissions say what it may then touch, and they
are written into the certificate rather than kept on the server — there is no
account and no grant table anywhere in ZiguratIP.

Issue with `--permission`, as many as you need. A schema covers everything in
it; a qualified name covers one table or procedure; `*` covers the lot:

```bash
ca issue --serial=3 --issuer=authority.conf --issuer-pik=authority.key --csr=client.csr --hash=SHA-256 --encoding=DER --permission=DEMO --certificate=client.crt
```

Then say the subject may connect at all. That is one file in a directory, and
it is the only thing the server keeps:

```bash
ca put --certificate=client.crt
```

```bash
ca users
```

Turn it on once at least one subject is registered — with nobody registered,
every connection is refused:

```
SECURITY:
	PERMISSIONS_MODE: TRUE
```

To withdraw access, take the subject off the register. Every certificate it
holds stops working on the next connection:

```bash
ca off --certificate=client.crt
```

[security.md](security.md#permissions) has the whole of it — what a permission
matches, why pages are not in the model, and why declaring an object needs
permission for everything it requires.

## 8. Putting Zeytun behind nginx or Apache

Zeytun's own TLS is TLS 1.2 with **static RSA key exchange only**. Browsers
dropped that years ago, so `HTTP/TLS_MODE: TRUE` is reachable from
OpenSSL-based clients and **not from a browser**.

For a browser, leave `HTTP/TLS_MODE: FALSE`, bind Zeytun to the loopback, and
let a proxy in front of it terminate real HTTPS. The proxy speaks modern TLS to
the browser and plain HTTP to Zeytun over localhost.

```
browser ──HTTPS(TLS 1.3)──▶ nginx / Apache ──HTTP──▶ Zeytun 127.0.0.1:2190
```

The certificate the proxy presents is a normal web certificate. For a public
site use one from a public CA; the `ca` tool can issue one for a private network,
and the examples below do exactly that with `COMMON_NAME: localhost`. Both want
PEM, which OpenSSL converts the DER output into:

```bash
openssl x509 -in proxy.crt -inform DER -out proxy.pem && openssl pkey -in proxy.key -inform DER -out proxy-key.pem
```

### nginx

```nginx
server {
    listen 443 ssl;
    server_name example.com;

    ssl_certificate     /etc/ziguratip/cert/proxy.pem;
    ssl_certificate_key /etc/ziguratip/cert/proxy-key.pem;
    ssl_protocols       TLSv1.2 TLSv1.3;

    location / {
        proxy_pass http://127.0.0.1:2190;

        proxy_set_header Host              $host;
        proxy_set_header X-Real-IP         $remote_addr;
        proxy_set_header X-Forwarded-For   $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

```bash
nginx -t && nginx -s reload
```

### Apache

Needs `mod_ssl`, `mod_proxy`, `mod_proxy_http` and `mod_headers`.

```apache
<VirtualHost *:443>
    ServerName example.com

    SSLEngine on
    SSLCertificateFile    /etc/ziguratip/cert/proxy.pem
    SSLCertificateKeyFile /etc/ziguratip/cert/proxy-key.pem
    SSLProtocol -all +TLSv1.2

    ProxyPreserveHost On
    ProxyPass        / http://127.0.0.1:2190/
    ProxyPassReverse / http://127.0.0.1:2190/
    RequestHeader set X-Forwarded-Proto "https"
</VirtualHost>
```

```bash
apachectl configtest && apachectl graceful
```

### Two things to get right

**Bind Zeytun to the loopback.** A proxy in front is no protection if the world
can still reach port 2190 directly. Use a firewall rule, or run the server on a
host where only the proxy can see that port.

**Raise `HTTP/POOL_SIZE`.** A proxy keeps its connections to the backend open
and reuses them, and Zeytun holds a worker thread for each live connection. With
the default of five, a busy proxy will exhaust the pool. Set it comfortably
above the proxy's own backend connection limit.

Both examples above were run against a live Zeytun serving the demo, and both
returned the demo pages over HTTPS.

## 9. Where to go next

- [demo/README.md](../demo/README.md) — the sample application, explained line
  by line. The best next step.
- [parsi.md](parsi.md) — the language, and from there the clause pages:
  [TABLE](table.md), [PROCEDURE](procedure.md), [SELECT](select.md),
  [PAGE](page.md).
- [security.md](security.md) — the certificate arrangement in full, and what it
  is not.
- [connector.md](connector.md) and [cpp.md](cpp.md) — reaching the database from
  your own C++.
- [configuration.md](configuration.md) — every setting.

The project [README](../README.md) has a Status section listing what is known to
be incomplete. Read it before relying on any of this in earnest.
