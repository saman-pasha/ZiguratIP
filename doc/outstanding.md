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

### Cross-site scripting, by construction

There is no escaping helper anywhere in the tree, and nothing in the `ECHO`
path escapes what it is given. `demo/03-pages.parsi:42-44` writes database
columns straight into HTML, which is the shipped example of how to write a
page, so it is the pattern anybody following the tutorial will copy.

Anything a visitor can get into a table comes back out as markup.

The fix is escaping by default in `ECHO`, with a deliberate way to opt out for
the cases that really do emit markup -- not a helper that authors have to
remember, because escaping-by-omission has never worked for anyone.

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

### The RPC console's transaction belongs to a thread, not a visitor

`System/rpc.parsi` holds its `Connector` in a `THREAD LOCAL` and never closes
it, and `do=commit` and `do=rollback` arrive as separate requests carrying no
identifier, so the server commits whatever connection the thread it landed on
happens to hold. Two visitors sharing a worker share a transaction.

`Connector/rpcpool.{hpp,cpp}` is the half of the fix that exists: connections
addressed by the transaction they carry, owned by whoever opened them, with
`RPCPool::owner` deciding what "whoever" means -- a certificate subject where
there is one, the session cookie otherwise. It is built, and its ownership
policy is tested.

What is missing is the page using it. `RPCPool::held` returns a
`Zigurat::Connector&`, and the Parsi `Connector` is a *subclass* of that
(`System/connector.parsi:5`), so a base reference cannot be handed to a Parsi
function whose parameter is the subclass. Converting the page to the base type
means rewriting `_write_parameter`'s fifty-odd `con.write(Bool(...))` calls,
which depend on the subclass's per-type overloads.

The way through is to stop making the Parsi `Connector` a subclass and have it
*hold* a `Zigurat::Connector&` instead. Then the pool hands out the reference,
the wrapper wraps it, and the page keeps its overloads. That is a change to
`connector.parsi` rather than to the page, and it is the last piece.

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
