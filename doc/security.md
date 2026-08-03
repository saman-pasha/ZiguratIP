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
`UNKNOWN_CA`, before it can send a request. Membership is the whole access
control — there is no password anywhere in this.

The handshake carries the peer's distinguished name through to the server, where
`tlsbuf::peer_subject()` returns it. Nothing is keyed on it yet. That is the
natural place for per-subject permissions to attach later.

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

## Keeping it in order

- **The authority's private key is the whole system.** Anyone holding it can
  issue a certificate that your servers will accept. Keep it off the servers.
- **Certificates expire.** `ca issue` takes `--from` and `--to`; the default is
  one year. An expired certificate stops connections dead, so track the dates.
- **There is no revocation.** Nothing here reads a CRL or speaks OCSP. To
  withdraw access you re-issue the authority and everything under it. Keep the
  number of clients small enough that this is bearable.
- **Never commit a private key.** `.gitignore` refuses `*.key` and `*.pem` for
  that reason. The one exception is the deliberately useless `dont-use-*`
  sample.

## What this is not

**This is ZiguratIP's own secure channel, not TLS.** It is shaped like TLS 1.2
and uses the same record layer, handshake sequence and key derivation, but it is
not interoperable: `openssl s_client` cannot speak to it, and neither can a
browser. Turning on `HTTP/TLS_MODE` secures Zeytun for ZiguratIP clients and
makes it unreachable from an ordinary browser. If you need a browser to reach
Zeytun over HTTPS, put a reverse proxy in front of it and leave `TLS_MODE` off.

**It has had no adversarial review.** The cryptography underneath is ZiguratIP's
own — its RSA, AES, SHA and PRF, not a vetted library. Timing behaviour has not
been analysed, and the MAC comparison is not constant time. Treat it as a
measure for a closed network among machines you control, not as transport
security against a capable attacker on the path.

**Private keys are stored under AES-ECB.** `ca` encrypts a pass-phrased private
key with the cipher in electronic codebook mode, which reveals repeated blocks.
The pass phrase protects the file at rest only weakly; file permissions matter
more.
