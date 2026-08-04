# Server Security

ZiguratIP can require every client to hold a certificate you issued. A
connection is then encrypted, and both ends know who the other is before a
single byte of protocol is exchanged.

Read [what this is not](#what-this-is-not) before deciding to rely on it.

See also: [Configuration](configuration.md), [Connector](connector.md)

## How it works

There is one certificate authority, and it is yours. You issue a certificate to
each server and to each client that may connect. Both ends of every connection
present theirs, and both check the other against the authority's certificate.

```
                    your authority
                   /              \
        server certificate      client certificate
                   |                    |
              ziguratip  <---------->  connector
                        both present,
                        both check
```

A client with no certificate cannot connect. Nor can one holding a certificate
some other authority issued: it is refused during the handshake, with
`UNKNOWN_CA`, before it can send a request. Membership is the price of
admission — there is no password anywhere in this.

Beyond membership, ZiguratIP can key what a connection may reach on the
certificate it presented. That is [Permissions](#permissions), and it is off
until you turn it on.

## Issuing the certificates

Everything below uses the `ca` tool. `ca` with no arguments prints its options.

Work in a directory outside the repository. Private keys do not belong in
version control, and the shipped `home/etc/cert/dont-use-*` files are a sample
that everybody who has ever cloned ZiguratIP also has.

### 1. The authority

The authority is a self-signed certificate and the key that goes with it. Guard
this key: whoever holds it can mint clients.

```bash
cp $ZIGURATIP_HOME/etc/cert/issuer.conf authority.conf
```

Edit `authority.conf` so it describes you — `COMMON_NAME`, `ORGANIZATION`,
`EMAIL_ADDRESS`. Then:

```bash
ca keygen --signature=RSA-2048 --encoding=DER --private=authority.key --public=authority.pub
```

```bash
ca csr --subject=authority.conf --subject-pik=authority.key --hash=SHA-256 --encoding=DER --csr=authority.csr
```

```bash
ca issue --serial=1 --issuer=authority.conf --issuer-pik=authority.key --csr=authority.csr --hash=SHA-256 --encoding=DER --certificate=authority.crt
```

The issuer is itself, which is what makes it self-signed and the root of your
small world.

### 2. A certificate for each end

The same three steps, except that the issuer is now the authority. Do this once
for the server and once for every client. Give each a `COMMON_NAME` that says
which it is — that name is what `peer_subject()` will report.

```bash
cp authority.conf server.conf
```

Set `COMMON_NAME: zigurat-server` in `server.conf`, then:

```bash
ca keygen --signature=RSA-2048 --encoding=DER --private=server.key --public=server.pub
```

```bash
ca csr --subject=server.conf --subject-pik=server.key --hash=SHA-256 --encoding=DER --csr=server.csr
```

```bash
ca issue --serial=2 --issuer=authority.conf --issuer-pik=authority.key --csr=server.csr --hash=SHA-256 --encoding=DER --certificate=server.crt
```

Repeat with `client.conf`, `COMMON_NAME: some-client`, and a different
`--serial`. Every certificate the authority issues needs its own serial number.

### 3. Check what you made

`ca` verifies its own work:

```bash
ca pukval --issuer-puk=authority.pub --certificate=server.crt
```

OpenSSL is the more convincing witness, since it has no stake in the answer:

```bash
openssl x509 -in authority.crt -inform DER -out authority.pem && openssl x509 -in server.crt -inform DER -out server.pem && openssl verify -CAfile authority.pem server.pem
```

`server.pem: OK` means the chain is sound.

## Configuring the server

Put the files where the server will look for them. `SECURITY/CERTIFICATE_PATH`
sets that directory; it defaults to `$ZIGURATIP_HOME/etc/cert`. Pointing it
somewhere outside the install prefix is the better habit.

In `ziguratip.conf`:

```
SECURITY:
	CERTIFICATE_PATH: /etc/ziguratip/cert

	CERTIFICATE:  server.crt
	PRIVATE_KEY:  server.key
	AUTHORITY:    authority.crt
```

A bare name is resolved under `CERTIFICATE_PATH`; an absolute path is taken as
given. `PRIVATE_KEY_CIPHER` carries the pass phrase if the key has one.

Then turn it on for whichever server should require it:

```
SERVER:
	TLS_MODE: TRUE      # Zigurat, the binary protocol on 2160

HTTP:
	TLS_MODE: TRUE      # Zeytun, the web server on 2190
```

Both may be on, and they share the one identity: they are two doors into one
database, not two systems.

A server told to be secure that cannot read its certificate **refuses to
start**. That is deliberate. Falling back to plaintext would be worse than never
offering security at all, because nothing would look wrong.

## Configuring a client

`home/etc/connector.conf` is the connector's own file, used when
`Connector::open()` is called with no arguments:

```
HOST: localhost
PORT: 2160

TLS_MODE: TRUE

CERTIFICATE:  client.crt
PRIVATE_KEY:  client.key
AUTHORITY:    authority.crt
```

`AUTHORITY` is the same certificate the server names — both ends check against
the same authority. `TLS_MODE` has to agree with the server's: a plain client
and a secure server do not negotiate, they fail.

A program that passes a host and a service to `open()` ignores this file, and
can pass `TLS::HandshakeParameters` to the secure overload instead.

## Permissions

Membership says a client may connect. Permissions say what it may then reach.

The rule that shapes everything below: **the server stores nothing about its
users.** There is no account, no grant table, no row anywhere naming a person.
What a connection may do is written into the certificate that opened it, by
you, at the moment you issued it. The server reads it off the handshake and
keeps none of it. Nothing to migrate, nothing to back up, and nothing a restore
can quietly bring back to life.

Two things are kept on the server, and only two:

- `SECURITY/PERMISSIONS_MODE`, one switch, off by default.
- `SECURITY/USERS_PATH`, a directory holding one file per subject that may
  connect. Its name is the subject, which is the part that matters; its content
  is whichever certificate that subject was registered with, kept so the
  directory can be read by a person rather than just tested by the server.

```
PERMISSIONS_MODE: TRUE
```

With it off, everything in this section is inert: a connection is still
encrypted and both ends still check the authority, but which certificate the
client presented decides nothing and the users directory is not read. With it
on, a client must pass both tests — registered, and permitted.

It needs TLS to mean anything. A plain connection presents no certificate, so
there is nothing to judge it on and it reaches everything. Turning permissions
on without turning `SERVER/TLS_MODE` or `HTTP/TLS_MODE` on enforces nothing at
all on that port, and the server says so at startup. That matters most for
Zeytun behind a reverse proxy, which is the ordinary way to run it: the proxy
terminates TLS, Zeytun sees plain HTTP, and no page is permission-checked.

### What a permission is

A path. The schema levels, then the object name, written the way Parsi writes
it:

| Permission | Reaches |
|---|---|
| `DEMO` | every table and procedure in the `DEMO` schema |
| `DEMO::AUTHORS` | that one object |
| `*` | everything |
| *(none)* | nothing |

A permission covers what it names and everything under it, so a schema is one
entry rather than a list. Matching ignores case, because Parsi does.

**A certificate naming no permissions reaches nothing.** An issuer grants by
naming; leaving the extension out cannot mean "everything", or every
certificate issued before this existed would be a master key.

Only **tables, sequences and procedures** can appear in a permission. Pages
cannot: a page is a URL that Zeytun decides to serve, not a name you can grant.
What is checked for a page is what the page *requires* — so a visitor loading
`/catalog.zt`, which requires `demo::books` and `demo::authors`, needs those
two. Requiring a procedure instead stops there: the procedure answers for what
it touches, which is what makes granting a procedure worth doing.

### Issuing a certificate that grants something

`--permission` on `ca issue`, as many times as you need. It is what makes the
certificate v3.

```bash
ca issue --serial=3 --issuer=authority.conf --issuer-pik=authority.key --csr=alice.csr --hash=SHA-256 --encoding=DER --permission=DEMO --certificate=alice-demo.crt
```

One subject may hold several, each granting something different — a full-schema
certificate for the nightly job, a single-table one for the reporting box. They
are the same subject and only one of them has to be registered. Which
certificate opens a connection is what decides that connection.

### The users directory

A file here is a subject's right to connect. `ca put` writes one, `ca off`
removes it, `ca users` lists them.

```bash
ca put --certificate=alice-demo.crt
```

```bash
ca users
```

```bash
ca off --subject-name="C=US, O=Acme, CN=alice"
```

Removing the file refuses **every** certificate that subject holds, at the
handshake, with `ACCESS_DENIED` — before a request can be sent. That is the
revocation this design has: there is no CRL and no OCSP, but there is one file
to delete, and it takes effect on the next connection.

It is also the *only* revocation. There is no way to withdraw one of a
subject's certificates and leave the others working: the register is keyed on
the subject, not on the certificate. If one leaks, un-register the subject,
issue it a fresh set, and register it again. That is a reason to keep the
number of certificates per subject small.

`ca off` also accepts `--certificate=` and reads the subject out of it, which is
easier than typing a distinguished name.

The directory defaults to `$ZIGURATIP_HOME/etc/users`; `SECURITY/USERS_PATH`
moves it. Subjects are filed under their full distinguished name, percent
encoded so that a name carrying a slash or a pair of dots is still one file in
one directory:

```
C=US%2C%20O=Acme%2C%20CN=alice
```

### Where it is enforced

At three points, all of them before anything runs:

1. **The handshake.** An unregistered subject is refused with `ACCESS_DENIED`.
2. **Calling.** `Connector::call("demo::add_author")` needs permission for
   `DEMO::ADD_AUTHOR`. Loading `/catalog.zt` needs permission for what that
   page requires.
3. **Compiling.** Declaring an object needs permission for the object being
   declared *and* for everything it requires. Without that, a client allowed
   one schema could compile a procedure that reads another and then call it
   entirely in order.

Each compiled object carries its own answer: the library exports
`objects()`, listing what running it lets a caller reach, and the server reads
that off the loaded code rather than working it out from a catalogue. The
answer cannot drift from the code it describes. A library compiled before this
existed has no such symbol and is refused with *compiled before permissions
existed; recompile it*.

The same object's qualified name is available as a path — `DEMO::AUTHORS::path`
is `{"DEMO", "AUTHORS"}` — and as `PATH:` in its catalogue entry.

Who the caller is reaches generated code as `Globals::peer_subject()` and
`Globals::peer_permissions()`, on both protocols, whether or not
`PERMISSIONS_MODE` is on. Parsi has no keyword for it yet: that would come
through a `System` object, and those do not currently build.

### Turning it on

Order matters, because an empty users directory refuses everybody:

1. Issue certificates with the permissions you mean, as above.
2. `ca put` each one. `ca users` to check.
3. Set `PERMISSIONS_MODE: TRUE` and restart.

The server says what it found on startup — how many subjects are registered,
and loudly if that is none.

## Keeping it in order

- **The authority's private key is the whole system.** Anyone holding it can
  issue a certificate that your servers will accept. Keep it off the servers.
- **Certificates expire.** `ca issue` takes `--from` and `--to`; the default is
  one year. An expired certificate stops connections dead, so track the dates.
- **There is no revocation list.** Nothing here reads a CRL or speaks OCSP.
  With permissions on, `ca off` is the withdrawal: the subject stops being able
  to connect on its next attempt. With permissions off there is nothing keyed
  on the subject, so withdrawing access means re-issuing the authority and
  everything under it — keep the number of clients small enough that this is
  bearable, or turn permissions on.
- **Never commit a private key.** `.gitignore` refuses `*.key` and `*.pem` for
  that reason. The one exception is the deliberately useless `dont-use-*`
  sample.

## What this is not

**It is TLS 1.2, but only with RSA key exchange.** The handshake is verified
against OpenSSL: `openssl s_client -tls1_2 -cipher AES256-SHA256` completes a
mutually authenticated connection and reports `Verify return code: 0 (ok)`. What
is missing is everything modern: no elliptic curve key exchange, no AEAD suites,
no session resumption, no renegotiation. Browsers dropped static RSA key
exchange years ago, so turning on `HTTP/TLS_MODE` secures Zeytun for
OpenSSL-based clients and makes it unreachable from an ordinary browser. If you
need a browser to reach Zeytun over HTTPS, put a reverse proxy in front of it
and leave `TLS_MODE` off.

To connect with OpenSSL, note that its default security level rejects these
suites; `@SECLEVEL=0` is needed to try them:

```bash
openssl s_client -connect localhost:2160 -tls1_2 -cipher 'AES256-SHA256@SECLEVEL=0' -cert client.pem -key client-key.pem -CAfile authority.pem
```

Its `-cert` and `-key` want PEM, which `openssl x509` and `openssl pkey` convert
the `ca` tool's DER output into.

**It has had no adversarial review.** The cryptography underneath is ZiguratIP's
own — its RSA, AES, SHA and PRF, not a vetted library. Timing behaviour has not
been analysed, and the MAC comparison is not constant time. Treat it as a
measure for a closed network among machines you control, not as transport
security against a capable attacker on the path.

**Private keys are stored under AES-ECB.** `ca` encrypts a pass-phrased private
key with the cipher in electronic codebook mode, which reveals repeated blocks.
The pass phrase protects the file at rest only weakly; file permissions matter
more.
