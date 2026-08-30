# MVCCS, written again in Cicili

An experiment, asked for in one sentence: *take a list of everything
MVCCS is responsible for, then try to write MVCCS again in Cicili with
`:cpp #t` — the scoped syntax may be useful.* The list is
[RESPONSIBILITIES.md](RESPONSIBILITIES.md). This directory is the try.

## What is here

* **`mvccs-lib.cicili`** — the engine and the three `def…` macros as an
  **importable library**: one `mvccs-engine` macro a target invokes after
  its includes, which is how a Cicili library ships (an imported file is
  evaluated Lisp). `mvccs.cicili` imports it and carries the suite;
  `schema-test.cicili` imports it beside **the `.cicili` files the Parsi
  compiler now generates** — see below.
* **`mvccs.cicili`** — the storage engine core **and the
  B-tree index tier**, written in Cicili's C++ layer, compiled against
  the **real** `StreamIO` and `Core` libraries (`Zigurat::binarystream`,
  `Zigurat::filestream`, `ZiguratException`, `Utility`). One source
  target emits `mvccs.cpp`, compiles it with g++ and links `mvccs_test`.
* **`build.sh`** — `CICILI=$HOME/cicili sh build.sh`; the binary prints
  a check line per behaviour and exits with the failure count.

Ninety-three checks, all green, five runs in a row: insert/commit/read-back,
update versioning (the old version retired and the new adopted at one
commit instant), rollback undoing a staged delete, TRUNCATE reclaiming
exactly the settled dead row and sparing the superseded one, allocation
overflow refused with a real thrown-and-caught `MemoryException`, two
pthread sessions racing 20 inserts through the shared streams,
REPEATABLE READ taking and releasing shared row locks, the SERIALIZABLE
gate admitting and releasing, **SNAPSHOT holding its point in time while
another session commits an update under it**, and a fresh `Memory` over
the same two files recovering the store through the startup walk — and
the index tier: lookups tracking updates, deletes and rollbacks (a
rolled back delete **reappears in the index on its own**, by the value
chain's visibility), thirty shuffled keys split their way through a
branching-3 tree and walk back out in order, a unique index refuses a
duplicate with a real `IndexException`, ranges over the split tree
answer exactly, and every tree comes back through the catalogue after a
restart — including a **two-column composite index** over a five-by-six
grid: tuple lookups, one cell holding a bucketed pair, deletion of one
of the pair, whole-tuple uniqueness (a shared first column is no
duplicate), and the grid intact after reopening, which is the proof of
the dependent-root fix below. The fourth pass adds **sequences** (NEXT
answers FROM first and advances; refusals for out-of-range, below-FROM
and exhaustion are loud `SequenceException`s; a drawn value survives
rollback because a sequence is outside transactions by design; the
counter survives a restart) and **true key deletion** (`unmap_key`:
the ten lowest keys leave a thirty-key tree through leftmost-leaf
unlinks, underflow merges and root shrinks; twenty remain in order;
the deletions are durable across another reopen, where two mid-tree
separators then go through successor replacement against reloaded
pages). The fifth pass closes the inventory: **Globals** (the mode
switches; the connection defaults flowing into the next transaction —
isolation level and autocommit both; a bound client stream coming
back and an unbound one refusing loudly; the whole permission-path
model — a schema grant covers its tables, case never decides, an
object grant covers itself and what is under it but never its schema,
`*` covers everything, a cleared peer is a plain connection again, and
a denial names its subject; the runtime-instance canary) and the
**DBA plumbing** (`dba_pagefiles` and `dba_pointers` describing the
raw store, and the watcher hearing one line per engine operation
through an attached stream).

## What was rewritten, and what was not

Rewritten, behaviour for behaviour (the file cites the original section
beside each piece): the layout arithmetic and hexmap encoding; the
strictly-increasing version clock; `Pointer`/`Control`; the allocator
(first fit, split, coalescing free, whole pages handed back); the
`Streams` pair with per-thread re-entrancy and hand-back windows;
`Statement`; the transaction (context list, SERIALIZABLE semaphore,
bounded waits); commit's three-sync intention ordering; rollback,
partial rollback, and the recovery walk; `_check_lock`; visibility at
all five isolation levels including SNAPSHOT's version-chain walk; the
full page-scan cursor with the repeatable-read retry; ISUD online and
offline; `_dead_pointers` and TRUNCATE.

Also rewritten, second pass: **the B-tree index tier** — nodes, keys
and values as rows under the index's hash key, the value chains with
per-link visibility (newest first, the next address read before the
callback), `map` with node splits up the ancestor path and root
growth, `unmap` marking one row's value and never the key's chain,
dead-value unlinking ahead of TRUNCATE, the catalogue records under
the original's `__INDICES__` hash key, and all seven cursors (`full`,
`equal`, `not_equal`, the two `less`, the two `greater`) with the
original's descent rules and the streams handed back around every
callback.

Third pass: **the multi-level composite index** — the original's
variadic `BTreeIndex<Table, First, Rest...>` type recursion becomes
runtime *levels*: every key here is an int64, so a dependent level is
a stack-built `BTreeIndex` value (`bt_dependent`) sharing one interned
dependent hash key, exactly the role of the original's stack
`dependent_index`. Outer levels hold no value chains; each key's
`dependents_address` roots the next level's tree; `bt_map_multi` and
`bt_unmap_multi` walk the levels; uniqueness is judged where the value
chains live, so it is the whole tuple's; `bt_truncate` sweeps both
hash keys; and the cursors are generalised through one emit point
(`BTEmit`): a qualifying key yields rows at the innermost level and
the **dependent index** everywhere else — `bt_cursor_dep`,
`bt_cursor_equal_dep` for composition, `bt_cursor_rows_deep` and
`bt_cursor_equal_multi` for the common whole-tree and whole-tuple
questions.

Fourth pass: **sequences and true key deletion**. `defsequence`
replaces `BaseSequence`'s CRTP statics with expansion-time arguments —

    (defsequence SEQ_ORDER "smoke::OrderSeq" 100 999 1)

expands the instance, its hash key, an attach, and argument-free
`_current` / `_next` / `_back` / `_set_current` / `_reset` wrappers.
Every write is offline (a sequence never rolls back), each operation
runs under a per-sequence RAII `MutexGuard`, and every refusal names
its cause, as the upstream header insists. `bt_unmap_key` is the
operation upstream ships but never calls: a key leaves the tree
wholesale and its records — the key, its value chain, an emptied
node — go back to the allocator, the second reclaimer beside
TRUNCATE. Leaf unlink, in-order successor replacement for internal
keys, underflow merging through the parent's separator
(`bt_combine_nodes`), re-split of an over-full merge, root shrink.
The composite form is refused loudly — upstream's own is unfinished
(the outer key goes wholesale, taking other tuples with it).

Fifth pass: **Globals and the DBA plumbing** — the last units.
Globals' class of statics becomes file-scope state behind `globals_`
accessors: the process switches, the connection defaults (read by
`transaction_reset`, so a new default reaches the next transaction),
the stream and store singletons, and the thread-local peer identity
with the original's permission-path matching ported word for word —
upper-cased trimmed levels split on `::`, empties dropped so a stray
separator cannot widen a grant, a grant covering what it names and
everything under it, `*` covering all, and `require_permission`
throwing 7800 with the subject named. `globals_client_stream` refuses
with 7802 rather than letting a null vtable call surface three frames
later, and the `extern "C"` runtime-instance canary is emitted through
Cicili's own `extern-c` clause. The DBA tier adds the watcher to
`Memory` — attach owns the stream, the first failed write detaches it,
and one guarded line per engine operation is sprinkled at the
original's sites — plus `dba_pagefiles` (page list under its lock) and
`dba_pointers` (the chunk-by-chunk page dump, run under the Streams
pair — the original reads the shared streams there with nothing held,
one more seek-and-read race).

**Not rewritten:** composite `unmap_key` semantics (upstream would
need to define them first) and Globals' `Parser`/`Compiler` slots —
there are no such components beside this engine.

## The experiment's answer

**The scoped syntax is useful exactly where it was hoped.** `Streams`
and `Statement` — the two RAII guards the whole engine's correctness
hangs on — are `struct` + `ctor`/`dtor` in Cicili and read better than
the originals, because the guard discipline (publish on lock, withdraw
before release, idempotent both ways) sits in one place. `letin*` scopes
them per operation; real `try`/`throw*`/`catch` against the real
`ZiguratException` subclass replaces error codes.

**Cicili macros replace C++ templates outright for the table tier**
(the second half of the ask). The original stamps
`Memory::online_insert<T>` per generated table with per-class statics.
Here `deftable`:

    (deftable Book "smoke::Book" id value)
    (deftable COCOLOG::MACHINES ID NAME KB STATUS CHUNKS NOTE)

The second spelling is the one the generated files use: Cicili's `::`
is a name, so a schema-qualified object needs no string beside it —
the SQL name is the spelling itself, and the identifiers fold the
`::` to `_` (`COCOLOG_MACHINES_insert`, and so on). A string after
the name still overrides, which is what the unqualified test tables
use.

expands at read time into the `BaseTable` subclass with `pack`/`unpack`/
`pack_size` written out, the 20-byte hash key computed **in Lisp at
expansion time**, typed `Book_insert` / `Book_update` / `Book_delete` /
`Book_cursor` / `Book_truncate` wrappers, and a hooks record — the seam
an index attaches through. `defindex` is its counterpart:

    (defindex IDX_BOOK_VALUE Book value 0 3)
    (defindex IDX_LOAN_MB Loan (member_id book_id) 0 3)

expands the index instance, its expansion-time hash key and catalogue
id (a composite also gets the shared dependent-level key), an `_attach`
function the program calls once after `memory_open` (it finds or
creates the catalogue record and hooks the table's
`map`/`unmap`/`truncate`), and the typed cursor wrappers riding the
table's own row shim — all seven for a single column; for a composite,
`_equal` takes one key per column and `_cursor` descends every level,
with the engine's `*_dep` cursors there for hand-rolled composition.
Several indexes share one table by **chaining**: each attach keeps the
hooks it found and calls them ahead of its own work, registered once
per process so a re-attach cannot loop the chain — cocolog's
`machines`, with three indexes, is the test. What a template instantiates invisibly, the
macro emits greppably. The
branching factor is a parameter (the original derives it from the key
type's size), which is what lets a test force splits with a tree of
branching 3.

**The plumbing went C, deliberately.** Cicili lambdas are lifted and
cannot capture, so `std::function` callbacks became context structs +
function pointers; `std::multimap` became intrusive lists (every use was
equal-range walks and insert/erase); `std::condition_variable` +
predicate lambdas became pthread + bounded polling; `std::atomic` CAS
became a mutex-guarded clock. One `(code …)` escape survives in the
whole engine: `clock_gettime` on a `struct timespec`.

## The last inserted version, from the first of history

An update writes its new version at a new address and points it *back* at
the version it supersedes, so a row's version chain runs newest to
oldest — and a reader holding an old address had no road forward. The
page scan meets a row's **first** version first; to find the one that is
current it chased the whole growing history to its end, and that chase
is what "a slow suite is the store ageing" was made of.

The road forward is paid for with a field that was already spent.
`commit_pointer` zeroes `query_id` when a version settles, so on every
settled superseded version the field was dead weight. Now commit runs a
second pass (`stamp_successor`): each new version of an update writes its
own address into its predecessor's `query_id` — a forward link, written
only after both sides are committed. `row_latest` is the reader's half:
from any version — in practice the first — it follows the stamps forward
and lands on the last inserted version in one control-read per hop,
never touching the rows between.

**The stamp is a hint and never an answer.** Every hop is verified by
the successor's own `reference_address` pointing back at the version
being left, so a stamp that is missing (an old store, a crash between
flip and stamp, startup recovery), erased (a staged write over a settled
version rolls back and zeroes the field), or torn can only end the walk
early — it cannot land on the wrong row. Where the walk ends still goes
through `visible` under the caller's own isolation level, and because
stamps are written only at commit, the walk cannot overshoot onto
another transaction's staged version: the newest stamped version is the
newest committed one. Under SNAPSHOT the two directions compose — the
stamps carry a reader forward to the newest, `reference_address` carries
it back to the version alive at its snapshot.

Both engines carry the change — this one and the C++ twin the server
links — with the same guards, and the smoke tests walk eight stamps from
a row's first version to its ninth, through a rollback that stamps
nothing, a deletion that ends the road without erasing it, and a restart
the stamps survive.

## One engine instance, behind a header

The engine was born a macro: every target expanded its own copy, which
is the right shape for an embedded store and the wrong one for the
server, whose Parsi-compiled procedure objects are separate `.so` files
that must all speak to one engine in one process. `engine.cicili`
expands the engine exactly once and builds **`libMVCCS.so`**;
`engine.hpp` is the consumer's view of it — the enums, `Pointer` and
`BaseTable` copied verbatim from the emitted C++ (build.sh diffs the
copies on every build, so a drift is a build failure rather than a
vtable crash), an *opaque* `Memory`, and free-function declarations.
Nothing RAII crosses the boundary: the guarded cursor and the
isolation setter live inside the library as `engine_*` wrappers.

`consumer-test.cpp` is the keystone proof, and the first consumer: a
table subclass compiled by plain g++ against the header only — exactly
the shape the Parsi compiler's emission will take — driving insert,
commit, the guarded cursor, updates, `row_latest` across the version
chain, rollback, and a restart, all green against the shared library.
The road from here to retiring the C++ twin runs through the compiler's
emission and the server's `load*` bindings, with a both-engines-one-
store parallel run as the acceptance gate.

## Deliberate divergences

* Hash keys are interned once per process and every `Pointer` aims at
  the canonical copy — the multimaps' `new hashkey_t`/`delete[]`
  ownership soup is gone. `deftable` derives its 20 bytes with a Lisp
  FNV spread, not SHA-1 (equality and stability are all the engine asks
  of a key; a store written by this engine is therefore not key-compatible
  with one written by the C++ engine).
* Columns are plain `int64_t`, `(TEXT c)` — a `std::string` packed as a
  16-bit length and the bytes — or `(VECTOR c)`: the real engine's
  `Vector<Double>`, packed as an int64 count and the doubles, eight
  bytes each, exactly as they are. The nullable `Type` layer is out of
  scope (and `doc/truncate.md` records why a store is better off
  without NULLs anyway).
* Transactions are explicit (`begin_transaction` per session thread)
  rather than riding a `thread_local` constructor — `__thread` cannot
  run one.
* The catalogue is found by scanning the `__INDICES__` rows — the
  original's self-hosting catalogue index (`BTreeRecord` +
  `IDX_ZIGURAT_BTREERECORD_HASH_NAME`, with its careful bootstrap and
  offline-written index entries) is not reproduced; its record shape
  replaces the `String hash_name` with the int64 catalogue id.
* An index attaches explicitly (`IDX_..._attach` after `memory_open`)
  instead of in a static initialiser — the original's own comments
  record what static-initialiser lookups cost it under `dlopen`.
* The cursor snapshots the page list into a bounded array (512 pages
  per key per scan) instead of a heap copy.
* No overloading in Cicili, so `_pointer`'s four overloads are
  `pointer_at` / `pointer_at_state`, and streams flow through
  `pack`/`unpack` virtuals instead of `operator<</>>`.

## The Parsi compiler generates these objects now

`Compiler/compilerddl.cpp` emits, beside every generated `.hpp`/`.cpp`
pair, **one `.cicili` per TABLE and SEQUENCE** — a macro file
(`define-<NAME>`) whose expansion is the `deftable`/`defindex`/
`defsequence` forms for the same object, columns and index shapes and
bounds carried over (a `LONG::MAX` bound becomes the literal). The
files under `generated/` are byte-for-byte what it wrote for cocolog's
whole schema — `clauses`, `props`, `machines`, `machine_state` and
their four sequences; `schema-test.cicili` imports the machines pair
untouched and runs twelve checks green — three machines through the
generated table, ids drawn from the generated sequence, the PRIMARY
ID index refusing a duplicate, and rows, index and sequence coming
back through the catalogue after a restart. The generated forms carry
the object's schema-qualified name bare —
`(deftable COCOLOG::MACHINES ID …)` — because Cicili's `::` is a name;
no string rides beside it.

Two mappings make a real schema fit. A Parsi STRING/TEXT column emits
as `(TEXT col)`: a `std::string` member, packed as a 2-byte length and
the bytes, the row's pack size folded from what each string actually
holds. And an index over such a column emits **commented out** — the
Cicili B-tree keys int64 and nothing else — so a consumer scans for
by-name lookups, which is what cocolog's embedded backend
(`cocolog/embed/embed.cicili`) does: it imports these very files, runs
the eighteen `cocolog::*` procedures over them in-process, and passes
the same twelve-worker group test the server passes. A Parsi schema
now compiles to either engine from one source, and cocolog runs on
both.

## The server runs on this engine now

The replacement landed in three passes. **Pass 1** made the engine ONE
shared library, `libMVCCS.so` — the macro expanded once, consumers
plain g++ against `engine.hpp`, nothing RAII crossing the boundary.
**Pass 2** retargeted the Parsi compiler's generated C++ onto it
through `engine-compat.hpp`, which keeps every spelling the DML
emitters use — `Globals::memory()->cursor<T>`, `T::IDX.cursor_equal`,
the ALL-CAPS type family, the keyword macros — and changes the engine
under them; the whole cocolog application (schema, procedures, pages)
compiles and dlopens against it. String index keys ride as a 64-bit
FNV-1a fold; every indexed lookup re-applies its full WHERE predicate,
so a collision costs a row visit and never a wrong answer, and the
WHERE compiler routes any non-equality over a hashed level to a scan.
**Pass 3** rewired `ziguratip` itself: `load_memory` opens the store
with `engine_memory_new`/`memory_open` and `globals_set_memory`, the
binary protocol's transaction verbs ride `engine_transaction_id`,
`engine_isolate`, `engine_set_autocommit`, `commit_transaction` and
`rollback_transaction`, Zeytun's per-request transaction does the
same, and the runtime canary became `mvccs_runtime_instance`. The old
`libMVCCS` stays linked only for the server's own `class Globals`
(parser, compiler, peers) until it is retired.

ONE LESSON COST A CRASH: while both engines live in one process, the
old `class Globals` statics and the compat header's
`namespace Globals` inlines carry IDENTICAL mangled names, and a
dlopen'd object bound to the old one through the global scope — a null
`Zigurat::Memory*` taken for an engine handle, dead on the first
insert. The compat `Globals` functions are
`visibility("hidden")` now, so every object resolves them inside
itself; the comment beside them says why they must stay so.

**Pass 5 retired the C++ engine.** `MVCCS/` is gone; this directory
holds the one MVCCS and `build.sh` links it as **`libMVCCS.so`** -- the
old name, because there is only one engine to name. What outlived the
old tree moved out first: the server's `class Globals` (parser,
compiler, peers -- the bookkeeping the engine never owned) lives in the
Compiler library now, `isolationlevel.hpp` beside the Connector's other
wire types. The old engine's own test files retired with it --
`test_contention.cpp` lives on here as `contention-test.cpp`, and the
carry-over acceptance keeps running against `golden/`: the last store
the old engine ever wrote, checked in, opened by the new engine on
every build.

The engine also carries a DML tier for Cicili consumers — `defquery`,
`defcount`, `deffind`, `defupdate`, `defdelete` in `mvccs-lib.cicili` —
each folding the context struct, lifted callback and runner that
hand-written queries spell out; `mvccs.cicili` proves them against the
hand-rolled twins they replace.

## Found upstream while porting — and now fixed in the C++ too

Every cluster below is **fixed in the original sources** in the same
commit that added the compiler emission: `_free` rekeys the returned
page in the list; a dependent's root split no longer writes the
catalogue through a default Pointer, and `_map_callback` writes the
root back whenever it changed; `_unmap_key` deletes internal keys by
leftmost-leaf successor replacement, combines on the live separator
with the boundary children adopted, and shrinks only the actual root;
`dba_pointers` runs under the Streams pair. Verified: ZiguratIP's own
suite 303 cases / 0 failed, and cocolog's ten suites `red: 0` against
the rebuilt server with every Parsi object recompiled.

The findings, as the port originally recorded them:

`Memory::dba_pointers` (memory.cpp): walks a page through the shared
`_hexmap_io`/`_data_io` holding neither stream mutex — the same
seek-and-read race the `Streams` pair exists to prevent, reachable
from any admin connection while sessions write. The rewrite runs the
walk under the pair.

`BTreeIndex::_unmap_key` / `_combine_nodes` (btreeindex.hpp): shipped
but called by nothing, and unfinished in ways a port would inherit:
the internal-key path recurses into the right child looking for a key
that is not there (a no-op), then lifts that child's *first* key as
the successor — correct only when the child is a leaf; the replaced
key's right neighbour is relinked to the old key's left neighbour
instead of the replacement; `second_child_key.left_address = nullptr`
assigns a NULL `Long` into a store that cannot read NULLs back; the
underflow combine is invoked on the just-freed key, resurrecting a
record the allocator now owns into the tree; the demoted separator's
child links are zeroed, which loses the two boundary subtrees on an
internal merge; and the root is reassigned on *any* node reaching
degree zero, root or not. The rewrite keeps the structure and corrects
each: successor from the leftmost leaf, payload moved before the leaf
copy is deleted, combine on the live separator, boundary children
adopted, root shrink only for the root.

`BTreeIndex` dependent root splits (btreeindex.hpp): when a *dependent*
level's root splits, `_split_node` calls `_update_btreeindex()`
unconditionally — but a dependent index has no catalogue record, and
its `_pointer` is a default `Pointer`, so the update writes a control
block and record at **address 0**, over the first page's header area.
And the split's new root is never stored into the parent key:
`_map_callback` writes `dependents_address` back only when it was `-1`
(creation), so after a dependent root split the parent still points at
the old root — the lower half — and every key promoted above it leaves
the index. Both trigger as soon as one first-column key holds more
than `2d` second-column keys. The rewrite guards the catalogue write on
`is_dependent` and writes the root back **whenever it changed**; the
restart-survives-the-grid check is the regression test.

`Memory::_free` (memory.cpp): when a freed run covers a whole page, the
page is rewritten under `__FREE__` on disk, but the in-memory page list
gets a **second entry under the table's key** and keeps the old one —
`_page_list.insert({pointer.hash_key, …})` after
`_allocate_page(FREE_HASHKEY, …)`. `_allocate_new_page` looks for
`FREE_HASHKEY` entries, so a page handed back this way is invisible to
the allocator until restart, and the duplicate entry makes the same
page scanned twice under its old key. The rewrite rekeys the entry in
place; the original is unfixed as of `eed884f`.

## What the transpiler could not say — and what got fixed

Hit while writing this, worked around in place. Three turned out to be
genuine transpiler bugs and are **fixed in cicili** (commit `803766b`,
each with a regression test in cicili's own suite):

* Top-level `var` (and `typedef`) inside a `module` under `:cpp` was
  name-mangled while references were not — every other construct asked
  `module-mangles<`; these asked `*module-path*`.
* A dotted `var` init inside a macro expansion (`(var int ,x . 0)`
  through `$$$`) died in the body dispatcher's `CL:LENGTH` before the
  var specifier — which handles the dotted tail — ever saw it.
* A `(code "…")` escape could not carry a double quote: the reader
  keeps backslashes (right for string literals, emitted back between
  quotes) and the escape's bare emission never took them off.

Still the dialect, worked with rather than around:

1. A qualified base class cannot appear in a ctor's `init`;
   `using namespace` + the bare name works.
2. `letin*` compiles to a GNU statement expression, so a block whose
   last statement returns a non-copyable (`stream->flush()` returning
   `basic_ostream&`) fails to compile; end such blocks with
   `(cast void 0)`.
3. Member access needs bound storage: `(-> (txn) id)` and member access
   through a cast expression are refused; bind first.
4. `(out …)` types are flat (`(out const uint8_t *)`), parameter
   descriptors are flat, but `cast` wants parenthesized pointer types
   without `const`.
5. `**` is one token in a type descriptor: `(BTNode ** ancestors)`,
   never `* *`.

None of these blocked the port; every one had an in-language answer.

## Insertion time, measured -- and where it actually went

The question was "7000 inserts take ~70 seconds; why?" -- asked of this
engine, and answered by measuring it from three heights: the engine
alone (`bench/`), the server over the wire (a cocolog `consult`), and a
`sample` of the live server mid-statement. Every number below is from
one Mac (APFS, Apple clang), and the benchmark that produced it ships
in `bench/` so the next machine can disagree with a number rather than
a sentence.

**The engine inserts fast.** 7000 rows in one transaction:

| scenario | time | per row |
|---|---|---|
| no index | 0.081 s | 0.012 ms |
| one index, branching 3 | 1.6 s | 0.23 ms |
| one index, branching 65, sequential keys -- the server's `PRIMARY KEY` from a sequence | 5.9 s | 0.84 ms, and growing: first quarter 1.09 s, third 2.97 s |
| no index, one transaction per row | 2.5 s | 0.35 ms -- the commit's six fsyncs, 0.043 ms each here |

**Over the wire, a fresh knowledge base loads at 1.8 ms a clause**,
linearly (1000 / 3500 / 7000 clauses: 1.8 / 6.5 / 12.7 s), in ONE
transaction, the client at 3% CPU. So 70 seconds is not a fresh load.
It is a REWRITE: cocolog writes a predicate back as `forget_clauses`
plus every clause again, and the second consult of the same 7000 into
the same base took 51.6 s, the third 94.3 s; a `retractall` of 7000
clauses whose chain carried 14000 dead links took 116.6 s -- 16.7 ms a
row, for a delete.

**The delete was the finding.** A `sample` of the server during
`forget_clauses` put 82% of its time in `bt_unmap_rec` ->
`bt_walk_values_from`: the value-chain walk the unmap resume mark
exists to prevent. In the engine alone, through `cursor_equal` -- the
statement's own shape -- the mark works: 0.087 ms a row, and 0.112
with 14000 dead links behind the live ones. Add a second index keyed by
the row id, as the server's table has, and it is 1.37 ms a row; make
those id keys avoid the chain's slot and it is 0.58. The mark table was
filed by `key & 63`, and a sequence's keys sweep all 64 slots: every
~64 rows the primary key's unmap evicted the chain's mark, and the next
unmap on that chain walked from the head -- past every newer row of the
base and every dead link, which is why history made it worse. The fix
files the slot by the INDEX, four sub-slots by key (the `UnmapMark`
comment in `mvccs-lib.cicili` has the whole story), and the numbers
after it are below.

**What is left in the 1.8 ms**, in order: ~0.85 ms the branching-65
descent -- a node's keys are one row each, read through a `filebuf`
that drops its buffer on every seek, so a descent is ~200 records and
several syscalls apiece; ~0.3-0.5 ms the round trip and the
statement's own framing (a `dlopen` per call under `CACHE_MODE: NONE`
was suspected and measured out -- see the table below; the default is
GLOBAL now anyway, and the close that would have unloaded a cached
object is fixed with it, `ziguratip/loadzigurat.cpp`); the rest the
row, two chain links, the sequence, and two flushes per record. The fixes that remain are in
`doc/outstanding.md` under "The engine's insert path", ranked by what
they were measured to be worth.

**After the fix** (same machine, same benchmarks):

| measurement | before | after |
|---|---|---|
| engine, delete via `cursor_equal`, chain + unique id index (the server's shape) | 1.37 ms/row | **0.56 ms/row** -- and 0.58 with 14000 dead links, where the same run was 1.41 |
| `retractall` of 21000 clauses with 21000 dead links behind them | 116.6 s | **21.6 s** |
| `forget` of the whole base | 59.5 s | **19.5 s** |
| second consult of the same 7000 (a rewrite: forget 7000, write 14000) | 51.6 s | **32.0 s** |
| third | 94.3 s | **50.8 s** |
| consult after vacuum (forget 7000, write 14000) | 52.3 s | **32.6 s** |
| consult 7000 into a fresh base | 12.7 s | 13.9 s -- unchanged: the insert path was never the fault |
| `vacuum` | 31-33 s | 31-48 s -- unchanged; it is the chain walk, "Dead links leave a chain only at vacuum" in `doc/outstanding.md` |

The delete now costs what the control experiment said it should, and
history no longer changes it. What the rewrite still pays is the
inserts -- 1.8-2 ms a clause, the descent above -- which is the next
item on the outstanding list, not this one. `CACHE_MODE: GLOBAL` made no
measurable difference to the fresh load (13.9 s against 12.7), so the
per-call floor is the round trip and the statement, not the `dlopen`;
the default stays GLOBAL because the `dlclose` it exposed was a bug
either way.

## Build and run

    sh MVCCS-cicili/build.sh     # needs sbcl + the cicili checkout
    ./mvccs_test                 # from the cicili directory, or note the
                                 # binary lands beside the transpiler CWD

The store files are `/tmp/mvccs-cicili-{hexmap,data}.bin`, recreated
fresh each run and reopened once mid-run to prove recovery.
