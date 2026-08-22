# What MVCCS is responsible for, unit by unit

MVCCS is ZiguratIP's storage engine: a multi-version concurrency-control
store over two shared binary streams (a *hexmap* of chunk states and a
*data* file of pages), with transactions at five isolation levels, crash
recovery by shadow paging, and on-disk B-tree indexes. This document lists
every unit, what it is responsible for, and what it *tries* to do — the
guarantees it works to uphold — as the ground truth for the Cicili rewrite
beside it.

Everything here was read out of the sources at `MVCCS/` as of commit
`eed884f`; the line references are to those files.

## The vocabulary (`memorybase.hpp`, `rowstate.hpp`, `rowlock.hpp`, `isolationlevel.hpp`, `control.hpp`, `pointer.hpp`)

* **`hashkey_t` / `hashkey_ptr`** — a 20-byte SHA-1 that names a table (or
  an index, or an engine-internal family like `__FREE__`). Everything in
  the store is filed under one.
* **`RowState`** — `NONE / INSERTED / UPDATED / DELETED`, encoded in bits
  2–3 of a hexmap control byte (values 0/4/8/12 so they OR cleanly with a
  lock).
* **`RowLock`** — `NONE / SHARED / EXCLUSIVE` in bits 0–1. Has `|` and `&`
  operators because `_check_lock` asks about either lock at once.
* **`IsolationLevel`** — the five levels. SERIALIZABLE is *not* decided
  here; it is READ COMMITTED-style row visibility plus a semaphore of one
  in `Transaction`.
* **`Control`** — one row version's whole story, twice over: an *online*
  (staged, uncommitted) state+lock and an *offline* (committed) state+lock
  in the hexmap; and in the data file the staging time, owning
  transaction id, owning query id, a `reference_address` linking an
  update's new version back to the version it supersedes (and a SNAPSHOT
  reader forward through the chain), and the two commit stamps
  `create_time` / `modify_time`.
* **`Pointer`** — (hash key, byte address, *data* size). The one subtlety
  it carries: a Pointer's `size` measures the record's data, while a free
  list entry's `size` measures the whole span including the control
  block; `_allocate` documents the corruption confusing the two caused.

## The exceptions (`memoryexception.hpp`, `btreeexception.hpp`, `indexexception.hpp`, `sequenceexception.hpp`)

Four one-liners over `ZiguratException`, each pinning a stable error code
(9390, 8710, 1063, 5330) so a client can tell an engine refusal from a
parser one.

## `globals.hpp/.cpp` — the process and the connection

Not MVCCS proper, but its front door. Static process-wide switches (reset,
trace, permissions, default autocommit + isolation level), the two shared
store streams, the singletons (`Memory`, `Parser`, `Compiler`), and
`thread_local` per-connection state: the client stream, echo stream, and
the peer identity + permission paths from the TLS handshake (`permits` /
`require_permission` do prefix matching over `SCHEMA::OBJECT` paths).
Also `zigurat_runtime_instance()` — the canary that catches a second copy
of the library mapped into one process.

## `transaction.hpp/.cpp` — one connection's transaction identity

* One `Transaction` per connection thread (`Memory::transaction` is
  `static thread_local`), living as long as the thread.
* Owns: the id (from `Utility::generate_id`), `init_time` (the SNAPSHOT
  anchor, taken from the *version clock*, not the wall clock),
  `query_id`/`query_time` (the running statement), the autocommit flag,
  the isolation level, the on-disk transaction record's `Pointer`, and
  the `context` — the ordered list of (touch time, row pointer) this
  transaction staged, which commit walks forward and rollback walks
  backward.
* **`reset()`** — returns everything transaction-scoped to the
  *connection's* defaults at commit and rollback. Exists because a
  connection outlives its transactions: without it a SERIALIZABLE level
  (and its semaphore slot) and a stale `init_time` leaked into the next
  transaction.
* **`set_isolation_level`** — the SERIALIZABLE semaphore of one:
  counter + condition variable under one static mutex, a *bounded* wait
  (`Memory::lock_wait_timeout_ms`), giving back a held slot before
  waiting, and notifying when a slot is released. The destructor
  releases the slot and best-effort rolls back, swallowing everything
  because it can run during process teardown.

## `memory.hpp/.cpp` — the store itself

The 2,400-line heart. Its responsibilities, in the order the file states
them:

### Layout arithmetic
One chunk is 16 bytes; one hexmap byte describes one chunk. A page starts
with 3 page-control chunks (48 bytes: the 20-byte hash key, padded); every
record is 3 control chunks (48 bytes: the six control fields) followed by
its data chunks. The `_pointer_*` helpers are this arithmetic and nothing
else — hexmap address = data address / 16, data begins `CONTROL_SIZE`
past the record start, sizes round up to whole chunks, the final chunk's
low 5 bits carry `size % 16`.

### The hexmap encoding
One byte per chunk: bit 7 = allocated, bit 6 = standalone (last chunk of
its run), bit 5 = control, bit 4 = has-tail-size, bits 2–3 = RowState,
bits 0–1 = RowLock. `_full_hexmap` / `_free_hexmap` write runs;
`_free_run_count` measures a free run *from the hexmap only*, because a
hole has no control block to measure from (measuring it as a record once
merged a hole with the live record behind it).

### The version clock — `version_time()`
Microseconds, strictly increasing across the process via an atomic CAS
loop, stored in the same 8-byte `time_t` fields the old second-scale clock
used. Every commit stamp, snapshot anchor and statement time comes from
here; two versions of a row can never share a stamp.

### `Statement` — a reader's fixed point in time
RAII. The outermost cursor of a statement takes `query_time` (the
snapshot the whole statement reads against) and a random positive
`query_id` (so an UPDATE's cursor can recognise, and skip, rows its own
query inserted). Inner cursors nest silently; the destructor of the owner
clears both.

### `Streams` — the two shared file streams
The store has ONE `_hexmap_io` and ONE `_data_io` for every connection;
a seek and the read after it are one operation, so both mutexes guard
them. `Streams` is a re-entrant-by-thread guard: `thread_local _held`
publishes the real holder; a nested guard is a no-op; `lock()`/`unlock()`
are idempotent because scans *hand the streams back* around procedure
callbacks and `_check_lock` hands them back while it waits. Lock order is
hexmap then data, everywhere. **This pair is the measured one-core
ceiling of the whole server** (see `doc/concurrency.md`).

### Allocation
`_allocate` serves from the per-hash-key free list (exact fit, or split),
else `_allocate_new_page` reuses a `__FREE__` page or appends a fresh one.
`_free` returns a run, coalescing with the free run before and after it,
and hands a fully-free page back to `__FREE__`. The free list and page
list (both `std::multimap` keyed by hash key) have their own mutexes; the
page list is *copied under its lock before any walk* because tree
rotations during insert break a concurrent iterator (`_cursor`,
`_dead_pointers`).

### The control block I/O
`_dump_control` / `_load_control` write and read the two hexmap bytes and
the six data-file fields as one unit.

### ISUD — what a write stages
`online_insert/update/delete<T>`: allocate (update: allocate the *new*
version elsewhere and link it by `reference_address`), push touched
pointers onto the transaction context, stage the online state
(INSERTED/UPDATED/DELETED + EXCLUSIVE + owner ids) via `_control_*`,
write the row bytes, and `map()`/`unmap()` the row's indexes — all under
one `Streams` guard so the index's own guard nests. `_check_lock` is the
writer's wait: bounded spin (10 ms steps up to `lock_wait_timeout_ms`) on
someone else's lock, streams handed back while sleeping.
`_offline_*` are the same operations *without* transaction machinery —
raw engine writes used by the indexes and sequences for their own nodes,
invisible to rollback by design.

### Visibility — what a read may see
Five levels, decided per row against hexmap state then control block:
* READ UNCOMMITTED: hexmap alone (staged-INSERTED or committed-INSERTED).
* READ COMMITTED — `_read_committed` + `_alive_at`: never waits. Own
  staged work is seen by its own rules (own insert visible, but not to
  the query that inserted it; own update/delete hides the old version);
  anybody else's staged work is invisible; what is left is "was the
  committed version alive at the statement's snapshot time" —
  `create_time <= at < modify_time`, with the crucial exception that
  `modify_time` only means death when the offline state says
  UPDATED/DELETED (an in-place `_offline_update` stamps it on perfectly
  current index roots).
* REPEATABLE READ / SERIALIZABLE: a read *writes* — it takes a SHARED
  row lock (staging it in the control block and pushing the row onto the
  transaction context so commit/rollback release it), or waits in
  `_check_lock` for whoever holds the row. A scan that finds a row
  changed under it rolls the transaction back to the statement's start
  and rewalks the whole hash key.
* SNAPSHOT: follows the `reference_address` chain backwards until it
  finds the version alive at the *transaction's* `init_time` — reads
  never block and never lock; deleted versions are still shown when the
  delete postdates the snapshot.

`_visible` answers for one record (an index asking); `_cursor` applies
the same rules while walking every record of a hash key, page by page,
chunk by chunk, handing the streams back around each callback.
`MAX_CURSOR_COUNT` (16) bounds cursor nesting per thread.

### Transactions — A, C and D
* `begin_transaction`: allocates the transaction's own on-disk record
  under `__TRANSACTIONS__` (id + commit time), with a thread-local
  re-entry latch because constructing the `thread_local Transaction`
  calls back into it.
* `commit_transaction`: ONE stamp for the whole transaction; then the
  write-ahead intention (`_write_transaction(id, commit_time)` +
  `_sync`), then every staged control block flipped to committed
  (`_commit_pointer`) + `_sync`, then the intention retired + `_sync`.
  Data is synced before hexmap every time — the hexmap is the claim, the
  data the substance.
* `rollback_transaction`: walk the context *backwards* un-staging
  (`_rollback_pointer` — a never-committed row is flagged
  offline-DELETED, never freed: chunks are only reclaimed by TRUNCATE),
  free the transaction record exactly once, `_sync`.
* `rollback_transaction_to`: the partial rollback the repeatable-read
  retry uses — undo context entries at or after the statement's time.
* Recovery (`_initialize`): read every page's hash key into the page
  list; read `__TRANSACTIONS__`; walk every page — every locked record
  found is finished the way its transaction's intention says (commit
  time recorded → `_commit_pointer`, else `_rollback_pointer`); free
  runs are measured from the hexmap and registered; then every spent
  transaction record is freed. The walk continues *past* holes
  (rollback leaves free space mid-page).

### TRUNCATE — the one reclaimer
`truncate(hash_key)`: collect every *settled* dead row (offline DELETED,
online NONE/no lock) via `_dead_pointers` — collected in full before any
`_free`, because `_free` mutates what the walk stands on — then free
them all. `truncate<T>()` also runs `T::truncate_indexes()`. This is the
operation that spends point-in-time readability for space.

### DBA plumbing
`dba_pagefiles` / `dba_pointers` (raw store dumps for the Memory Viewer),
and the watcher — a `binarystream*` that gets one line per engine
operation, detached on the first write failure.

## `basetable.hpp/.cpp` — what a stored thing must be

The abstract row: a `Pointer`, virtual `prepare()` (compute derived
columns), `map()`/`unmap()` (this row's index entries), `pack_size()`,
and per-class statics every generated table shadows: `name`, `path` (the
permission path), `hash_key`, `truncate_indexes()`. Streaming a row is
`operator<</>>` on `binarystream` in each concrete class.

## `basesequence.hpp` — sequences

CRTP template over the generated sequence class. One row per sequence
under the sequence's own hash key, guarded by a per-class static mutex.
`_initialize` (create the row if missing, and *say so* if FROM/TO/STEP
are null or there is no store), `_with_current` (fetch the row or refuse
loudly — a silent default-constructed Long became "not null column"
crashes far away), `CURRENT/SET_CURRENT/NEXT/BACK/RESET` with range
checks. All row I/O is `_offline_*` — sequences are outside transactions
by design (NEXT is never rolled back).

## The B-tree (`btreenode`, `btreekey`, `btreevalue`, `btreerecord`, `btreeindex.hpp`)

* **`BTreeNode`** — degree, is_internal, address of the first key. Nodes
  and keys are rows under the *index's* hash key, written with
  `_offline_*`.
* **`BTreeKey<K>`** — a node's keys are a doubly-linked list
  (left/right_address); children hang off keys (`left_node_address` /
  `right_node_address`, consecutive keys sharing a child); plus
  `dependents_address` (composite indexes) and `values_address` — the
  head of this key's value chain.
* **`BTreeValue`** — one row address + `next_address`: values are a
  chain under the index's own hash key (they used to be a
  page-file-per-key hash bucket; the header records why that died).
  Nothing is ever unlinked on delete — the control block marks it dead
  and visibility hides it, which is what makes a rolled-back delete
  reappear for free.
* **`BTreeRecord`** — the index catalogue: one row per index
  (hash_name, is_unique, branching factor, root address), itself
  indexed by the one self-hosting index
  (`IDX_ZIGURAT_BTREERECORD_HASH_NAME`), which cannot look *itself* up
  and is flagged `_is_catalogue`.
* **`BTreeIndex<Table, First>`** — the index proper. Responsibilities:
  register/find/update itself in the catalogue; `map` (descend to the
  leaf, insert the key into the key list or add a value to an existing
  key, enforce uniqueness, split full nodes up the ancestor path —
  median key promoted, keys relinked in place); `unmap` (mark one row's
  value dead — never the key's whole chain); `unmap_key` (true key
  deletion with node combining — present, documented as not yet called);
  `truncate` (unlink settled-dead values from every chain, then
  `Memory::truncate` the index's hash key); and the six comparison
  cursors (`equal`, `not_equal`, `less_than`, `less_than_equal`,
  `greater_than`, `greater_than_equal`), each a proper ordered walk that
  descends only the children that can contain qualifying keys. Every
  walk is under a `Streams` guard and a `Statement`, and hands the
  streams back around the caller's callback (`_cursor_output`), which
  reads each value's row through `_visible` — the index never shows a
  row the isolation level would hide.
* **`BTreeIndex<Table, First, Rest...>`** — composite indexes: each
  level is a tree whose keys hold no values but point at a *dependent*
  index (next column) through `dependents_address`; all levels share
  one hash key; only the innermost level holds value chains. The
  variadic specialisation forwards each cursor into the dependent level.

## Cross-cutting guarantees the whole library tries to uphold

1. **A seek and its read are one operation** — everything on the shared
   streams happens under the `Streams` pair, in hexmap→data order.
2. **Reads never tear and never miss**: every statement reads against
   one instant of the version clock.
3. **A reader at READ COMMITTED/SNAPSHOT never waits**; a reader at
   REPEATABLE READ+ takes real shared locks; writers wait bounded, not
   forever.
4. **Crash safety by ordering, not logging**: new version elsewhere →
   intention synced → control blocks flipped → intention retired; data
   synced before hexmap.
5. **Nothing is reclaimed implicitly.** Delete marks; only TRUNCATE
   frees, and only settled rows.
6. **Callbacks run unguarded.** Procedure code inside a cursor may
   write; every guard is handed back around it and retaken after.
7. **Refusals are loud and near the cause** — a sequence with no row, a
   hexmap that ends mid-chunk, an allocation overflow all throw with
   the name of the thing at fault.
