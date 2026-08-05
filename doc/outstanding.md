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

### What an index costs now, and what it used to

**Fixed.** Kept here because the numbers are the guard: `Test Memory` in
`Test/test_btree.cpp` measures the layout, and if it moves back, this is what it
moved back from.

An index bucket used to be keyed by the row it pointed at:

```cpp
std::string composite_key = this->_name + std::to_string(key.pointer.address);
```

The address is per row, so every indexed row hashed to a bucket of its own and
every bucket became a page file -- roughly 8 KB of store for eight bytes of row
address. Nodes and keys were never the problem: they already allocated under the
index's own hash key. It was only the values.

A key's values are a chain now -- `BTreeValue::next_address`, headed by
`BTreeKey::values_address` -- allocated under the index's hash key beside the
nodes and keys. Four hundred rows with four indexes over them:

```
before   [400 rows] objects=1209 pages=1226 single-page objects=1205
after    [400 rows] objects=9    pages=43   single-page objects=4
```

The one thing the chain does not get for free is isolation. A bucket was walked
by `Memory::_cursor`, which decided what this transaction was allowed to see;
a chain has no scan behind it, so each link asks `Memory::_visible` the same
question directly. That is what keeps a rolled back insert out of the index.

### Two things the bucket was hiding

Both were found by making the values findable, and both are fixed:

- **`_unmap` deleted a key's whole chain**, not the entry for the row being
  unmapped. On any non-unique index that took every row sharing the key out of
  the index along with the one that was asked for.
- **A composite index never unmapped its dependent level.** Its values hang off
  the innermost level, so the outer level had none, so the walk that was meant
  to reach `_unmap_callback` never ran once.

### The catalogue's index entry was written inside a transaction

Also fixed, and worth writing down because of what it cost. `_insert_btreeindex`
writes the `BTreeRecord` row with `_dump_control` and an offline `INSERTED`
state: metadata, true the moment it is written, holding no lock. Its *index
entry* went in through `_control_insert`, which takes an `EXCLUSIVE` lock and
waits for a commit -- so a request that registered an index and never committed,
or a server killed before it did, left that lock in the store permanently.

Every index looks itself up in the catalogue from a static initialiser, so the
next server to load that object waited out the lock timeout and threw from
inside `__static_initialization_and_destruction_0` under `dlopen`, where no
catch can reach it. The process died on a metadata read. Both halves of one
metadata write now follow the same rule.

### One HTTP request can leave a store that will not survive a restart

An insert then fails with `Server side error code: 1000, message: NULL value`,
and under gdb it is one frame:

```
#1 Zigurat::Long::operator<(Zigurat::Long const&) const          libType.so
#2 Zigurat::BTreeIndex<DEMO::BOOKS, Zigurat::Long>::_map(...)::{lambda}
#7 Zigurat::BTreeIndex<DEMO::BOOKS, Zigurat::Long>::_cursor_keys(...)
#9 ...::map(DEMO::BOOKS const&)  ->  DEMO::ADD_BOOK  ->  DEMO::SEED
```

`Long::operator<` throws `Object::NULL_EXCEPTION` when either side is null, so
**`IDX_DEMO_BOOKS_ID` holds a `BTreeKey` whose key is null**. Every insert into
that table then dies walking into it, and because each index looks itself up
from a static initialiser, the next server to load the object aborts under
`dlopen` where no catch reaches.

**Bisected.** It was first seen after `run-e2e.sh` then
`run-permissions-e2e.sh`, but the permissions suite has nothing to do with it:

| step | store afterwards |
|---|---|
| connect probes | fine |
| `call demo::count_books` x4 | fine |
| **fetch one page** | **broken** |

Narrowed from there. The certificate that is turned away at the TLS handshake --
never reaching the page -- leaves the store fine; an ordinary `curl` of
`/catalog.zt` on the plain server breaks it. Permissions, the compiler probes
and the second `open.conf` server are all innocent.

Narrowed again, on *when*:

```
insert                              ok
fetch /catalog.zt                   HTTP 200
insert, same server                 ok        <- store is fine in memory
stop, restart, insert               FAILED    <- and broken on disk
```

So the page does not damage anything a running server can see. It leaves the
*on-disk* hexmap and free list in a state `Memory::_initialize` reconstructs
differently, and the rebuilt free list then hands out space that is already
occupied.

**It needs a store with some history.** The same sequence on a store created
fresh -- twenty books loaded, one page fetched, restart -- passes. The store
that fails is one that has been through the e2e suite's churn of rolled back
inserts, deletes and truncates. That is the next thing to bisect, and it is the
harder half.

Two leads worth reading before starting:

- `Memory::_initialize` frees every transaction record while iterating
  `_page_list` (`memory.cpp:158-164`), and `_free` moves entries between the
  page and free lists as it goes. `truncate` has a comment explaining why it
  collects the dead set *before* freeing any of it, for exactly this reason.
  `_initialize` does not.
- `_initialize`'s page walk stops at the first non-data chunk (`memory.cpp:144`)
  and records it as the page's free tail. A page with a freed hole in the middle
  would lose everything after the hole.

What a null key most likely *is*: a `BTreeValue` read as a `BTreeKey`. A key
unpacks seven fields where a value wrote two, so the key field lands past the
record and a zero byte is a null descriptor. That means two allocations were
handed the same address. Keys and values share pages now, so a duplicated
address can land on one where it could not before -- which makes this easier to
hit, without being a defect the change introduced.

**Not attributed to a commit.** Checking `0e8d0bd` needs a clean rebuild -- an
incremental one leaves mixed libraries and fails to link -- and that was not
done.

### The Memory Viewer lists every object's pages

`do=pagefiles` returns the whole page list, so the tool's dropdown holds every
object in the store rather than the one whose name was typed into it. Far less
pressing now that a demo store is tens of pages rather than hundreds, but the
checksum box already works out which hash key is yours and the listing should
take it and filter.

---

## Durability

### No write-ahead log

**The fsync half of this is done.** `binarystream::sync_to_disk` is a no-op for
every stream that is not a file; `filestream` keeps a second descriptor on the
same path and calls `fsync` on it. `Memory::_sync` flushes both files and pushes
them to the disk **data before hexmap** -- the hexmap is what says a record is
there and in what state, the data is the record, and a hexmap that lands first
describes a record that may not have.

`commit_transaction` now syncs three times, in the order that makes the sequence
recoverable: the intention (`_write_transaction` at `commit_time`), then the
control blocks it licenses, then the retirement of the intention. A crash
anywhere in the middle leaves `_initialize` reading the same transaction record
it would have read anyway, and deciding the same way.

That is what makes copy-on-write mean something: a row is updated by writing a
whole new version elsewhere and flipping the control block that adopts it, which
is shadow paging and needs no log to recover -- but only if the writes land in
the order they were made, and `flush()` alone gives no such promise.

**What is still missing** is a log, and with it recovery from a torn write
rather than from a reordered one: no replay, no checkpoint, no fsck. A `kill -9`
between the two `fsync` calls of one commit is survivable now; a half-written
*page* is still not.

Also missing: `_offline_update` writes in place with no size check, which is
safe only because everything that uses it writes a record of unchanged size.
That is an invariant nothing enforces.

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
