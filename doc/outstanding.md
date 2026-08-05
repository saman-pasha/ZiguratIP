# Outstanding

What is known to be wrong and has not been fixed. Everything here was found
while doing the security work on the `colab` branch and left deliberately --
because it was outside what was being done, or because doing it properly needed
more room than was available. None of it is speculative: each item names where
it lives.

Read `security.md` first for what the security model is meant to be. This file
is what it is not, yet.

---

## Reachable from the network

### The Zigurat tools go around the permission system

`mem.zt`, `rpc.zt` and `compiler.zt` reach the storage engine by opening their
own `Connector`, and `con.open()` with no arguments reads `home/etc/connector.conf`
-- `localhost:2160`, `TLS_MODE: FALSE`. That connection presents no certificate,
so it carries no subject and no permissions.

The permission system does not need anything added to it. These pages simply do
not go through it: `PERMISSIONS_MODE` governs what a visitor's certificate lets
them reach, and then a page opens a second, anonymous connection and reaches
whatever it likes on the other side of it. A visitor who can load `/rpc.zt` can
call any procedure, whatever their own certificate grants -- and `/mem.zt` hands
out the page store's layout, and `/compiler.zt` is the compiler.

`require_objects` cannot see any of it, because what these pages touch is chosen
at request time rather than declared. What is needed is for them to check
`Globals::permits()` themselves against the object being asked for, before
passing it on -- the same call `require_objects` makes, from the page instead of
the loader. Until then they belong behind `PERMISSIONS_MODE: FALSE` or off a
public port entirely.

### The compiler is gated, not fixed

`COMPILER/REMOTE_MODE` defaults to `FALSE`, so the network cannot reach the
compiler, and that is the whole of the mitigation. The code behind the gate is
unchanged: `Compiler/compiler.cpp:229-262` still builds four `system()` command
lines by string concatenation, and Parsi's `INCLUDE` and `LINK` clauses are
spliced onto them.

Turning `REMOTE_MODE` on, on anything exposed, is still arbitrary code
execution as the server's user.

The fix is `posix_spawn` with an argv array -- never a shell -- and an allowlist
on the `INCLUDE`/`LINK` tokens: `[A-Za-z0-9_./-]+`, no `..`, and everything
resolving under a configured root. The check at `compiler.cpp:132-135` validates
object *names* and says nothing about the generated code.

---

## Storage

### Every indexed row gets its own page

`MVCCS/btreeindex.hpp:180` keys an index bucket by the row it points at:

```cpp
std::string composite_key = this->_name + std::to_string(key.pointer.address);
```

The address is per row, so every indexed row hashes to its own bucket, and every
bucket is its own page file. A bucket that can only ever hold one entry is not a
bucket, and a B-tree node holding one key is not a node.

Measured on a fresh store after the demo and a bulk load: **569 page files, 557
of them holding exactly one record**, each an 8 KB page carrying a single index
entry. Table pages pack properly by comparison -- the busiest one holds 74 rows
with no free space -- so this is the index and not the pager.

It shows up in the Memory Viewer as an unusable list of page files and, on any
of them, one row marked INSERTED. That is the symptom; the cost is roughly 8 KB
of store per indexed row, and a B-tree that never branches.

**There is a test suite for this, and it is red on purpose.** `Test Memory` --
three cases in `Test/test_btree.cpp` -- says what the layout should be and
measures what it is. Four hundred rows with four indexes over them:

```
[400 rows] objects=1209 pages=1226 single-page objects=1205
```

Twelve hundred pages for four hundred small rows, twelve hundred of the objects
holding exactly one page. At 8 KB a page that is about ten megabytes of store
for a few kilobytes of data.

Left failing rather than weakened, the same way `run-reload-e2e` is: a test
changed to pass is worth less than one that says something true. It goes green
when an index keeps its entries together the way a table keeps its rows.

Fixing it means deciding what a bucket should be keyed by -- the indexed value,
presumably, so rows sharing a key share a node -- and letting nodes hold many
entries. That is a change to the index rather than a repair to it.

### The Memory Viewer lists every object's pages

`do=pagefiles` returns the whole page list, so the tool's dropdown holds every
object in the store rather than the one whose name was typed into it. With 569
of them it cannot be used. The checksum box already works out which hash key is
yours; the listing should take it and filter.

---

## Durability

### No write-ahead log, and no fsync

A `kill -9` in the wrong millisecond corrupts the store, and there is no
recovery path: no log to replay, no checkpoint to fall back to, no fsck.

This is arguably the most serious defect in the project. It is not a security
problem, which is the only reason it is not at the top of this file.

---

## Certificates

### `ca issue` emits v1 certificates with no subjectAltName

The shipped PKI issues X.509 v1 with no extensions at all. Chrome has ignored
`CN` for host matching since v58, and every other browser followed, so a
ZiguratIP-issued certificate cannot satisfy a browser however it is installed.

Needs `X509::issue` migrated to OpenSSL -- the parsing side already is, the
issuing side is not -- emitting v3 with `subjectAltName`, `basicConstraints`,
`keyUsage`, `extendedKeyUsage` and SKI/AKI, plus `--dns=` and `--ip=` on
`ca issue`.

For a real host the cheaper answer is Let's Encrypt for Zeytun, whose chain
OpenSSL reads natively through `SSL_CTX_use_certificate_chain_file`, and the
private CA kept for Zigurat's mutual authentication.

### Encrypted private keys cannot be read by anything else

`Cryptography/x509.cpp` encrypts private keys with **AES-ECB**, pads the
passphrase with zeroes, uses **no key derivation function at all**, and then
writes the **CBC** algorithm OIDs with `NULL` parameters where the IV belongs.

The result is not PKCS#8, is not loadable by OpenSSL, and is much weaker than
its own file format claims.

It should emit PKCS#8 PBES2. It should **not** grow a reader for the old format:
every key written by it predates the RNG fix below and is already compromised,
so code to read them is code to keep something that has to be destroyed.

### All key material issued before the RNG fix is compromised

`Core/bigint.cpp` seeded from `time(0)` and drew from `rand()`, so two `ca keygen`
runs in the same second produced byte-identical private keys. Fixed on `colab`,
but nothing regenerates what was already issued.

Anything generated before that commit -- including the shipped `dont-use-*`
material, which is 2047-bit for the same reason -- must be reissued and never
trusted.

---

## TLS

### Capped at TLS 1.2 wherever the register is used

Under TLS 1.3 a client's certificate is not examined until after it has sent
its Finished and considers the handshake done. An unregistered subject
therefore gets success out of `SSL_connect` and hears the refusal afterwards, as
an alert on a connection it believes it already has.

Since the users register is the only revocation this design has, contexts that
refuse peers by subject stay at 1.2, where the answer arrives inside the
handshake. See `SocketIO/tlsbuf.cpp`, `context_for`.

Lifting it means the client confirming the server accepted it before reporting
a connection -- a change on the client side, not the server's.

Ports that ask for no certificate are unaffected and negotiate 1.3 today.

### ANONYMOUS_PERMISSIONS was designed and not built

With `TLS_CLIENT_AUTH: OPTIONAL` or `NONE` and `PERMISSIONS_MODE: TRUE`, a peer
that presents no certificate has no subject, and a peer with no subject holds no
permissions. It therefore reaches nothing.

That is fail-closed, which is the right default, but it means a browser gets a
refusal on every page rather than a useful one. The design was a
`SECURITY/ANONYMOUS_PERMISSIONS` list, defaulting to empty, with `*` rejected at
startup: granting everything to unauthenticated peers should require deleting a
check, not typing a character.

---

## Dependencies and process

- **zlib 1.2.11 is vendored** -- CVE-2018-25032 and CVE-2022-37434 both apply.
- **No continuous integration.** Nothing builds or tests this except by hand.
- **No fuzzing corpus.** The HTTP parser was fuzzed once, by hand, during the
  memory-safety work; nothing keeps doing it.
- **No dependency monitoring**, and no way to be told when something vendored
  here gets a CVE.
- **`Test/run-reload-e2e.sh` fails**, and failed identically before any of this
  work. Its expectation is Mach-O-specific -- it wants a library name that ELF
  records differently. Left red rather than weakened, because a test changed to
  pass is worth less than one that says something true.

---

## Standing recommendation

Do not put this on a public port on its own.

Run it behind a reverse proxy -- nginx or Caddy -- terminating TLS from the
internet, with request-size and rate limits, forwarding to ZiguratIP on
localhost. That puts a widely-audited component between the internet and
47,000 lines of single-author C++, and it costs an afternoon.

The binary protocol cannot go behind an HTTP proxy, which is why its TLS had to
be made real rather than merely fronted. Zeytun can, and should.
