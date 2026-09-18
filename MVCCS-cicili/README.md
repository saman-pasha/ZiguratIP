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

**Every column kind is an index key.** The tree's key is one int64 —
`BTKey` packs it in a 56-byte record and every `bt_*` takes it as such —
so each kind folds to one, by a rule the engine states beside
`text_key64` and exports through `engine_text_key` / `engine_real_key`
/ `engine_words_key`, which is how the Parsi compiler's generated C++
derives the same key from the same value that a `defindex` expansion
does. An **INT** key is the value. A **REAL** column (`(REAL c)`, a
double, eight bytes as they are) folds order-preserving — the bit
pattern as a signed int64 already orders the positives, the negatives
get their low 63 bits flipped, and `-0.0` is `+0.0` first — so a range
over a REAL index is a real range. A **TEXT** key is FNV-1a 64 of the
bytes, masked positive, and a **VECTOR** key is FNV-1a over the REAL
fold of each element: hashes, in hash order, so those two kinds get
`_equal` and nothing else, and two values can collide — every consumer
of a hashed index re-checks the column on each row it is handed, which
makes a collision a wasted visit and never a wrong answer. A UNIQUE
hashed key can falsely refuse a distinct value at 2^63 odds, loudly.
`deftable` leaves the column kinds on the table symbol's plist and
`defindex` reads them back, which is the one place two macro expansions
in one image can meet; an index over a column the table does not have,
or over a table not yet declared, is an error at expansion time. The
wrappers are typed by kind: `_equal` over a TEXT column takes a
`const std::string &`, over a REAL a `double`, over a VECTOR a
`const dvec_t &`, and a composite takes one such parameter per level —
and a composite also answers its leading column alone through
`_equal_first` (equality on the first level, every dependent level
walked whole, which is what the server's WHERE compiler emits for a
predicate on the first column only; cocolog's embedded store walks a
`(kb name)` index for one kb that way).

**A composite may be any depth, and the WHERE compiler descends it.** A
predicate that binds only the leading column of a `(kb, name, arity)` key
reaches the index through `cursor_equal` on that level and must then walk
every dependent level whole down to the rows; the compiler wrote those
lower walks as SIBLINGS — each middle level's lambda returning at once,
the row walk then called on the OUTER handle — which compiled for two
levels, where there is no middle, and failed for three with `no matching
function for call to object of type lambda`. cocolog's `predicates_of`
over its `(kb, name, arity)` index was the first to meet it. Each middle
level now opens a lambda that stays open until the innermost row walk is
written, and they close in reverse; `Test/run-keys-e2e.sh` proves it on
`demo::triple` — four rows under `a == 'x'`, two under `(a, b)`, one under
all three, and the generated C++ grepped for both dependent handles.

`_attach` answers **1 the first time a store meets the index** — its
catalogue row was just created, and whatever rows the table already
holds are not in the tree — and `_rebuild` then fills the tree from
them (storage dropped, one exclusive table walk, this index alone),
answering how many rows the tree refused: 0 is a complete index, and
anything else is a UNIQUE key the table already held twice, counted
rather than thrown because the walk runs inside a cursor's callback
window. `schema_test` proves the whole sequence on cocolog's generated
`machines`: the TEXT name index finds and misses, its UNIQUE refuses a
twin, the KB index is attached only at the reopen, answers 1, rebuilds
to three rows, and is known and full on a third open.

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

**After the fixes** -- same machine, same benchmarks, timed by the
shell's own clock. (A first draft of this table was timed by a helper
that called `python3` through a pyenv shim costing 1.8-3.7 s a call, and
every "after" number in it was inflated by that; the engine-level rows,
timed inside the benchmark, were never affected. These are the
re-measured numbers, three configurations run back to back on one
server: the mark fix alone; the mark fix and the record cache; both plus
cocolog's pipelined client.)

| measurement | before | mark fix | + record cache | + pipelined client |
|---|---|---|---|---|
| engine, insert, branching 65, sequential keys (the server's primary key) | 0.85 ms/row, growing | 0.85 | **0.11 ms/row**, flat | -- |
| engine, insert, branching 65, shuffled keys | 0.44 | 0.44 | **0.16** | -- |
| engine, delete via `cursor_equal`, chain + unique id (the server's shape) | 1.37 | 0.56 | **0.12** | -- |
| consult 7000 into a fresh base | 12.7 s | 11.4 | 3.1 | **2.5 s** |
| second / third consult of the same 7000 (rewrites) | 51.6 / 94.3 s | 28.5 / 51.6 | 8.2 / 13.3 | **8.0 / 11.9 s** |
| `retractall` of 21000 clauses, 21000 dead links behind | 116.6 s | 22.4 | 7.2 | **7.5 s** |
| `forget` of the whole base | 59.5 s | 12.9 | 5.1 | **5.2 s** |
| consult after vacuum (a rewrite) | 52.3 s | 24.6 | 8.7 | **7.5 s** |
| `vacuum` | 31-33 s | 19-21 | 20-24 | 20-24 s |

Three things the columns say. The mark fix is the delete side only, as
it should be: the fresh load barely moves, the rewrites and the
retractall fall by two to five times. The record cache is everything
that descends a tree, so it takes the fresh load from 11.4 s to 3.1 and
the rewrites down again. The pipelined client (cocolog's, not this
repository's: `zg_call_send` / `zg_call_wait` in `client/zigurat.c`,
up to 128 calls in flight) is worth 0.6 s on a fresh load and nothing
on a rewrite, because what a rewrite pays now is the statements and the
commit, not the waits: a `sample` of the server during a consult puts
the remaining time in `commit_transaction` -- 28000 control blocks
flipped through the same buffer-dropping seeks the tree used to pay --
and in the statement's own work. Those are the next items; the wire is
no longer one. `CACHE_MODE: GLOBAL` made no measurable difference to
any row, so the per-call floor was never the `dlopen`; the default
stays GLOBAL because the `dlclose` it exposed was a bug either way. So
the question's own number -- a rewrite of 7000 clauses over history --
went from the 50-120 s range to 8-12 s, and a fresh load from 1.8 ms a
clause to 0.36.

## The mapped store -- and the seeks are gone

The next item after the record cache was the I/O itself: the two store
streams were `std::filebuf`s, and a filebuf drops its buffer at every
seek, so an engine that seeks before every read and every write paid a
syscall or two per access -- a commit of 7000 rows flipped 28000
control blocks that way, and every control read, hexmap byte and chain
link did the same. `StreamIO/mapbuf` is a `std::streambuf` over `mmap`
and `mapstream` is `filestream`'s shape over it: a seek is an integer,
a read a memcpy, the kernel's page cache the buffer, every mapping of
the file sharing it. The engine is untouched -- it holds a
`binarystream*` and never knew what was behind it -- and the server
chooses with `MEMORY/STORE_IO: MAP` (the default now) or `FILE`. Three
rules the buffer keeps, because the engine depends on them: the file is
EXACTLY as long as what was written (address space is reserved ahead,
the file grows by the bytes of each write, `page_count = length /
page_size` stays true); ONE position shared by reads and writes, as a
filebuf has; and a read-only mapping learns a writer's growth by itself
(fstat when a read reaches past the length it knew), so per-thread
readers over a growing store keep answering. Durability is `msync` of
the written range and then `fsync`, in the same data-before-hexmap
order as before. `Test/test_streamio.cpp` pins the three rules.

Same machine, same benchmarks, timed by the shell; the last column is
the store mapped, everything before it in place:

| measurement | before all | + mark, cache, pipelined client | **+ mapped store** |
|---|---|---|---|
| engine, insert, branching 65, sequential keys | 0.85 ms/row | 0.096 | **0.031 ms/row** |
| engine, commit of those 7000 rows | 0.25 s | 0.25 | **0.008 s** |
| engine, delete via `cursor_equal`, chain + unique id | 1.37 ms/row | 0.108 | **0.004 ms/row** |
| engine, one transaction per row (six syncs each) | 0.35 ms/row | 0.35 | 0.38 -- the syncs, unchanged |
| consult 7000 into a fresh base | 12.7 s | 2.5 | **0.7 s** |
| second / third consult of the same 7000 (rewrites) | 51.6 / 94.3 s | 8.0 / 11.9 | **1.4 / 2.1 s** |
| `retractall` of 21000 clauses, 21000 dead links behind | 116.6 s | 7.5 | **0.8 s** |
| `forget` of the whole base | 59.5 s | 5.2 | **0.5 s** |
| consult after vacuum (a rewrite) | 52.3 s | 7.5 | **1.5 s** |
| `vacuum` | 31-33 s | 20-24 | **1.0-1.7 s** |

So the question's own number -- 7000 clauses rewritten over history --
is 1.4-2.1 s where it was 50-120, and a fresh load is 0.1 ms a clause
where it was 1.8. What the engine's suites say: consumer is green over
the mapped store (`STORE_MAP=1 ./consumer_test`), contention over it
fails at the same macOS `rewrite vs index` line it fails at over a
filebuf and nowhere before it, and the two Cicili suites, which open
filestreams, pass as before -- which is the point of the engine not
knowing. cocolog's store cases and its 7000-clause sequence above ran
against the mapped server. The record cache still stands in front of
the mapping, and
still earns its place: a cached descent is lookups, a mapped one is
memcpys with a `pointer_at` walk of the hexmap in front of each.

## The page walk that stopped at 1024 pages

Found by the composite index, and older than everything above. The
page-scan cursor -- `cursor_walk`, what a SELECT without an index, the
vacuum's counts and the index REBUILD at TRUNCATE all walk -- snapshotted
the table's page list into fixed arrays: 512 entries a round, 1024
walked in all, and `walk_done` when the 1024 were spent. A table past
1024 pages (8 MB at the default page) was therefore walked only that
far, silently: every full scan answered a prefix of the table, and the
rebuild mapped a prefix into the fresh trees and dropped the rest. A
knowledge base of 377 predicates lost `hex_direction/2`'s first clause
at one vacuum and the whole predicate at the next; the vacuum's own
live count drifted by thousands between passes; and `dead_pointers`,
the walk TRUNCATE reclaims by, had the same 512, so a rewrite past 512
pages left its dead rows where the vacuum walked no further and the
base grew on every pass. `bench/composite-check.cpp` at `3 4000` --
144 000 rows, past the cap -- reclaimed 0 dead rows on the old walk
and 72 000 on the new. Both walks size their snapshot to the list now,
and the walked set is a byte per page number, grown as pages appear.
Nothing in the format changed.

## The hole in the page, and the walk that went a phase out of step

The oldest bug of the hunt, found last, on the live store, byte by
byte. The in-page walk that `cursor_walk` and `dead_pointers` share
assumed a page is a dense run of records: three hexmap bytes of
control, then data chunks, then the next record. But a page also holds
HOLES -- the free tail a first-fit split leaves behind, the span a
reclaim freed -- and the walk consumed a hole's bytes as a control
block and then read every record after it one phase out of step: live
rows that no cursor, no index rebuild and no reclaim could see. A
knowledge base then lost single clauses at every vacuum -- the rebuild
dropped what the walk could not show it -- and the loss looked random
because it depended on where the allocator had split. `sides/2` gone
from eleven bases, one `hex_direction/2` row at a time, four
`map_tile/3` rows of 960: the family's year of "store transients" has
this shape.

The proof was forensic: a raw scan of a snapshot found 14 live
`unit_hurry/2` records where the index answered 12, and the hexmap
before the first missed record read `[144 144 198 0 0 64]` -- a record
ending, then a three-chunk hole, exactly a control block wide. The
startup walk had learned this lesson long ago and wrote it down ("a
read straight through a SHORT free run into the record behind it...
one byte decides"); the fix gives the other two walks the same one
byte: a record's first chunk always carries the high bit, a free chunk
never does, so a low byte advances the walk by one and nothing else.
`bench/composite-check.cpp` pins the worst case deliberately -- a
7-chunk span reclaimed, a 4-chunk row allocated into it, the 3-chunk
tail as the hole, and the row behind it must still answer.

A vacuum on the fixed engine HEALS a store the bug damaged: the
rebuild finally sees the hidden rows and maps them back -- one pass
brought a live store from 170 000 visible rows to 263 000, and every
base that had "lost" clauses answered whole again. Rows hidden by the
bug were never gone; they were unlit.

## The page list was one chain, and a write was quadratic in its own rows

Reported from downstream, diagnosed here. cocolog 1.2.16 wrote rows into a
fresh `--embed` store and the time went **16 000 rows 0.53 s, 32 000 1.42,
64 000 5.77, 128 000 26.9** on Linux, while the assert loop that fed them
stayed flat at 3.8 µs a clause -- so it was the store, not the interpreter.
Two facts came with it, and both were load-bearing: **splitting the rows
over 128 predicates did not help** (29.0 s against 24.9 s for one predicate
of 128 000), which ruled out the obvious guess of one index key's value
chain; and **no vacuum moved it**, which ruled out dead rows. It was
recorded as undiagnosed rather than guessed at.

Reproduced on macOS at 1.13 / 2.15 / 5.14 / 15.0 s for the same four
sizes, and a sampling profile named it in one line: **57 % of the process
inside `cursor_walk`, called from `seq_next` through `seq_with_current`.**

Every row a consumer inserts draws a sequence value, and a draw is a
CURSOR -- over the sequence's own key, which owns one page holding one
row. But `cursor_walk` snapshots the page list under the lock, and the
list was ONE chain for the whole store: it walked every entry to count
them, allocated two arrays of that size, walked every entry again to
filter, and did all of it twice, because a second round is what proves no
page appeared during the first. So a draw cost O(pages in the store) --
and the store's pages grow with the rows -- which is a write quadratic in
its own rows, with the two arrays' allocation and zeroing as the constant.
That also explains both of the facts above: pages are one store's however
the rows are named, and the pages a walk steps over are LIVE, so a vacuum
has nothing to take.

The fix is a second chain. Every `PageEntry` now also sits in the chain of
the pages under ITS KEY (`key_next`, bucketed by an FNV fold of the key's
twenty bytes into 251 heads), and `cursor_walk`, `dead_pointers` and
`drop_key_pages` walk that one; `allocate_new_page` takes the `__FREE__`
chain instead of scanning everything for a free page, and `_free` finds
the page it is emptying in its own key's chain. The whole-store list stays
exactly as it was, because startup walks every page whatever its key, and
so does the DBA's page dump. A bucket may hold two keys, so every walk
still checks the key it reads. Nothing in the format changed.

| rows one process writes | before | after |
|---|---|---|
| 16 000 | 1.13 s | 0.97 s |
| 32 000 | 2.15 s | 1.82 s |
| 64 000 | 5.14 s | **3.69 s** |
| 128 000 | 15.0 s | **7.45 s** |

-- each doubling now costs twice where it cost nearly three times before
(macOS, the same machine and the same binary but for this change).
`mvccs_test` guards it with a counter rather than a stopwatch:
`mvccs_cursor_steps` is a thread-local tally of the page entries a cursor
has stepped over, and the case takes a difference across one draw and
refuses a walk longer than the store has pages. The single chain stepped
over four times that.

What was left was linear, and it was `ftruncate`: two thirds of the
remaining time, because the mapped store grows the file by exactly the
bytes written and a fresh page is six extending writes. That is the next
section.

## And then the growing itself, which was six ftruncates a page

A mapped page may not be touched past the file's end, so `mapbuf`
ftruncated to the new length before every extending write and then
memcpy'd into the mapping. On APFS moving a file's end is a metadata
transaction -- **115 µs a call, measured** -- and a fresh store page is six
of them: four for its hexmap slice, two for the page itself. So the store
grew at 688 µs a page, and after the page-list fix above that was two
thirds of what a writing process spent.

`StreamIO/mapbuf.cpp` grows the file **a megabyte at a time** and cuts it
back to what was written at every `sync_to_disk` and at `close`. Every byte
of content still goes through the mapping, exactly as before; the kernel is
asked only to move the end, and asked 1/128th as often. Three ways were
measured on the store's own write pattern -- `bench/grow-bench.cpp`, 2 000
pages of six extending writes each plus the in-place hexmap rewrites a row
allocation makes, which is the mix a bench that only appends would miss:

| | a page, APFS | a page, ext4 |
|---|---|---|
| ftruncate per extending write | ~700 µs | 23 µs |
| pwrite the extending write | ~45 µs | 4.9 µs |
| **chunked grow, cut back at sync** | **~6 µs** | **5.2 µs** |

End to end, 128 000 rows into a fresh `--embed` store, three runs each on
one machine: **9.22 s → 3.6 s** here, and 26.9 s → 2.2 s on the Linux box
with the page-list fix above.

**THE MIDDLE ROW WAS TRIED AND WITHDRAWN, AND THE REASON GIVEN FOR
WITHDRAWING IT WAS WRONG.** `pwrite` appends and extends in one call and
keeps the length exact at every instant; it went out because a store
written that way came out intermittently unreadable to the next process on
Linux/ext4 -- about 30 % of first reads -- and because it was the one thing
that had changed (saman-pasha/ZiguratIP#32, found by an interleaved 20-run
A/B swapping only `libStreamIO.so`). This file said the mixing of write
paths was the variable. **It was not**, and "The stamp in the future" below
is what it actually was: the fault survived the withdrawal at the same
rate, and what the three growth strategies did was move a clock's lead
across zero. The chunked grow stays because it is the fastest of the three
and because one path writing the bytes is a good rule -- not because it
fixed anything.

What the chunked grow costs is a window: between one sync and the next the
file is up to a megabyte longer than its writes, so a KILL can leave whole
pages of zeros past the last real one. A chunk is a whole number of store
pages, and `memory_initialize` now refrees a page whose twenty-byte key is
all zeros -- growth nobody wrote, or a page header torn mid-write, which
shadow paging says never happened either way. Measured: a writer killed
mid-write leaves a 20 MiB store, the next open refrees the zeros, and 500
rows written after it do not grow the file by a byte.

## The stamp in the future, which the faster writes only exposed

Diagnosed downstream, on the store the two sections above made faster, and
it is the better finding of the three because it explains why they looked
guilty. `version_time` answers the wall clock in microseconds -- or
`clock_last + 1` when two calls land inside one, and `clock_last` is PER
PROCESS and only ratchets. So a flush that makes more calls than it has
microseconds pushes the clock past real time, where nothing outside that
process can see where it got to. `commit_transaction` takes ONE such value
for the whole transaction and `commit_pointer` writes it into every
committed row's `create_time` — which are the only reader-visible stamps a
commit sets.

**So the writer exits before the clock it stamped its own rows with.** A
reader is a new process: its `clock_last` is 0, its snapshot is the true
wall clock, and `alive_at`'s "born after this read began" then hides every
row of that commit — the catalogue row included, which is why the symptom
is a missing predicate rather than an empty answer, why a second read
milliseconds later is perfect, and why nothing is ever lost.

Measured on Linux/ext4 over fresh `--embed` stores, the commit stamp against
the wall clock at the writer's exit, with the first read at three delays:

| rows | commit stamp leads by | d=0 | 5 ms | 20 ms |
|---|---|---|---|---|
| 500 | -3.0 ms | 8/8 | 8/8 | 8/8 |
| 2 000 | +1.9 ms | 8/8 | 8/8 | 8/8 |
| 8 000 | +10.4 ms | 7/8 | 8/8 | 8/8 |
| 32 000 | **+25.6 ms** | **0/8** | 3/8 | 8/8 |

-- the delay a store needs is the lead its own stamp carries. And with only
`libStreamIO.so` swapped, at 32 000 rows: **-16.5 ms** on one ftruncate a
write, **+9.6** on the pwrite, **+26.3** on the chunked grow. The lead
crosses zero exactly at the two commits that were blamed, in the order their
failure rates ran. Neither put a defect in; both made the flush fast enough
for the same number of clock calls to outrun the microseconds available.

`clock_settle` is the answer: **a commit does not return until real time has
reached the stamp it wrote**, waited out after the rows are durable and the
streams guard is back, so a committer waiting holds nothing. An ordinary
commit leads by microseconds and spins them out; a flush that outran the
clock sleeps the difference in one call. `mvccs_test` pins the invariant
without needing two processes or a fast machine: it burns the clock
milliseconds ahead on purpose, commits, and requires `clock_ahead()` to be
back at or below zero with the row it stamped readable behind it.

## The store is in the writing machine's byte order

Asked as a question and worth an answer in the file, because the answer is
asymmetric and was written down nowhere: `grep -ri endian` over this
repository hit no document, no engine source and no header — only the
anonymous namespace inside `StreamIO/nbostream.cpp`.

| | base | an `int64_t` |
|---|---|---|
| `networkstream` | `nbostream` | `reorder()` — octets reversed on a little-endian host, so **the protocol is big-endian on the wire** |
| `filestream`, `mapstream`, `bufferstream` | **`hbostream`** | a raw eight-byte read, **no swap** |

That asymmetry is the right one: a connection crosses machines and a store
does not. But nothing said so, and a store carried across the boundary
anyway would **open** — the page list is built from twenty-byte hash keys,
which are byte arrays and read the same everywhere — and only then start
reading control blocks whose every int64 is reversed: transaction ids,
stamps, reference addresses, B-tree keys. What comes out of that is not an
error, it is nonsense, and the first thing it does is write more of it.

So a store now keeps a mark beside it, `byteorder.bin`: a magic written in
host order at the store's first open and read back at every later one.
Equal is the same order; **reversed is refused by name**, saying what
happened and why a store does not travel; anything else means the mark
itself is damaged. `store_order_check` is called by the two consumers that
know a store's directory — `ziguratip/loadmemory.cpp` and cocolog's
embedded open — because the engine is handed two streams and never learns a
path. The test binaries make their own stores in `/tmp` with no directory
to mark, and are unaffected.

**What it cannot know** is where bytes written before the mark existed came
from: a store that predates it gets a mark stamped with the opening host's
order, which is a guess. It is honest about everything after that. The same
limit applies to `golden/` — those are little-endian bytes, so the
carry-over acceptance would be wrong on a big-endian box and would now
refuse rather than mislead.

## A lookup is a read, and the guard now prefers writers itself

This one took four measured rounds, a livelock, a withdrawal and a machine
this repository is not developed on. It is written out at length because the
shape of the mistake is more useful than the fix.

**What started it.** The tenth trace point was added so ZiguratIP#37 could be
answered without patching a working copy. Bracketing a reading thread's
acquisitions showed **98 % of its exclusive guard time was the index lookup**
-- two exclusive acquisitions per `cursor_equal` at 223 µs each, against
3.2 µs for its `begin` and 5.7 µs for its `commit`. Neither acquisition is a
write. `bt_cursor_equal` took the exclusive side because it never asked for
anything else, exactly as `read_row` did before `e8ada3f`.

**The window was the blocker.** A cursor releases the guard around its
callback, and found what to hand back through `tl_streams_held` -- a name
only an *exclusive* hold ever set. A lookup holding the shared side would
release nothing, and a callback that wrote would construct an exclusive guard
under a held shared one, which `Streams::lock` refuses by design.
`tl_streams_shared` names a shared hold the same way, and the window hands
back whichever side the thread has.

**Then it livelocked a four-core box, deterministically.** Shipped as
`0dd8b2a` on the strength of a sixteen-thread measurement -- exclusive guard
time 10 248 ms to 7 624 ms, the suite 10.61 s to 8.04 s -- and on four cores
`lookups_survive_a_writer` never finished, four runs of four. Six readers in
a tight `do { cursor_equal } while (writing)` loop overlapped continuously
and the writer they were waiting on was granted the guard **zero times in
120 rounds**. Not a red case: a **hang inside `build.sh`**, so the box could
not build the release at all. Withdrawn in `3e2ce4e` the same evening.

**The cause was not the ratio of readers to cores.** 32 readers on 16 cores
is 2x oversubscribed and finishes in 0.21 s; 6 on 4 is 1.5x and never
finishes. Instrumented on the box that had it: **36 729 941 of 36 731 243
shared grants were made while a writer was already queued.** `streams_rw` is
created `PREFER_WRITER_NONRECURSIVE_NP` and on glibc that defers nothing.
The same barging is in the grants `read_row` has taken since `e8ada3f` (98 of
99, 163 of 163, 7 108 of 7 109): it was always there, and only the volume of
a shared cursor made it load-bearing.

### So the guard prefers writers itself

`Memory` carries `writers_waiting` under a mutex and a condition variable.
The exclusive path announces itself before `pthread_rwlock_wrlock` and
broadcasts once granted; a shared acquirer **sleeps** while the count is
above zero. Three decisions in that sentence were each bought with a
measurement:

* **It sleeps rather than polls.** The first build stood a reader down with
  `usleep 20` in a loop. The writer passed 120/120 rounds at every reader
  count -- and the **lookups collapsed 14x to 57x**, ranges not overlapping
  at any count. 15 688 sleeps to serve 142 lookups, 110 sleeps each, because
  a nominal 20 µs sleep among thirteen runnable threads on four cores
  measures ~137 µs. A reader was standing down 137 µs to let through a
  `begin` that holds the guard for 3.2 µs. The gate's *decision* was never
  the cost; its *granularity* was.
* **The preference is bounded** -- `GATE_STANDDOWNS = 2`, then the reader
  takes the shared side regardless. Strict preference is unbounded by
  construction: a reader defers while *any* writer is queued, so a workload
  whose writers never stop arriving is one where readers never run. That is
  this fault mirrored, and cocolog's `library(httpd)` pool -- every request a
  short exclusive acquisition through `run_isolated/2` -- is exactly that
  shape. The bound removed **98 %** of stand-down time and is the change that
  mattered most; a `pthread_cond_wait` is only 56-88 µs against the poll's
  86-99 µs, so the win was the bound and not the sleep.
* **The broadcast fires on every grant**, not when the queue empties. A
  reader waiting for zero would sleep through a continuous stream of writers,
  and never reaching zero is what a busy store looks like.

The mutex stays because a condition variable needs one: it is what makes the
wait and the wake race-free, not what protects an integer.

### And why the two halves landed as one commit

**Neither is an improvement alone.** The gate by itself is a *regression* on
a tree whose lookups are exclusive -- 0.02 to 0.18x of ungated lookups at
every reader count -- because `writers_enqueue` cannot know its caller, so
**every reader's own lookup enqueues as a writer**. At twelve readers about
90 % of the queued "writers" are readers, all deferring to each other. The
shared lookup by itself livelocks. Only the pair is better than master, so
they are one commit and there is nothing to bisect into.

**Measured on the four-core box, five repeats an arm at every reader count,
100 runs with no `STARVED` anywhere:**

| readers | ungated lookups | gate alone | **both** |
|---|---|---|---|
| 2 | 149 | 4 | **279** |
| 4 | 352 | 62 | **1 727** |
| 6 | 1 449 | 119 | **2 192** |
| 8 | 931 | 63 | **2 414** |
| 12 | 4 167 | 79 | **3 638** |

Five range separations out of five against the gate alone; against ungated
the ranges overlap everywhere except N = 4, so the honest claim is *at least
as good as ungated at every count and clearly better at four readers*. The
writer pays **1.13-1.37x** against a ceiling of 3x -- the gate alone makes it
*faster* than ungated and the shared lookup gives that back, which is the
same trade seen from both ends.

**The number worth keeping** is the exclusive acquisition count, which is
flat in N: **364 at two readers, 384 at twelve**, against ungated's 662 and
8 718. The difference of 20 is exactly the 20 extra `begin`/`commit` calls
ten more readers make. The lookups have left the exclusive side entirely.
Shared grants made while a writer was queued fall to **17-26 %**, from the
99.996 % that opened the issue, and the stand-down fires 0.8-1.9 times a
lookup -- under the bound, and rarely reaching it.

### What is still exclusive, and deliberately

`bt_cursor_dep` and `bt_cursor_equal_dep` invoke their callback with the
guard **held** -- `bt_emit_key` calls `dcb` directly, there is no window
there -- so a write inside a dependent callback rides an exclusive hold as a
nested no-op and would meet the refusal on a shared one. The first attempt
gave them the ask too and `composite under load` threw
`an exclusive streams guard under a shared one` on the first run. A window
for the dependent callback would let the pair go shared as well, and would
also be a new unlocked window in the machinery ZiguratIP#33 was fixed in.

The case `update in a lookup` is what holds all of this honest: a write from
**inside** a lookup's callback at READ COMMITTED, eight threads on their own
rows. Nothing in the suite did that before -- `find_then_update` looks like
it and is not, capturing the row in the callback and writing after the cursor
returns.

### The lesson that is not about locks

A sixteen-thread box cannot find this and a four-core box cannot miss it.
The suite ran green here twenty times over while the machine that mattered
could not complete a single run. **Anything that changes the guard's mode
goes to four cores before it gets a version number**, and the measurements in
this section are not ours -- they are ZiguratIP#37's, on the only box that
has ever seen either fault.

## Hard debugging, without changing a line

Cicili ships four logging macros — `info!`, `warn!`, `debug!`, `syslog!` —
that a **transpiler flag** decides the fate of: at the default level each one
expands to nothing at all, so the code below costs a shipped build exactly
zero and a `grep` of `engine.cpp` finds none of it. Turn them on when a
question needs them:

```bash
sh MVCCS-cicili/build.sh                       # nothing; 0 trace sites emitted
MVCCS_DEBUG=info  sh MVCCS-cicili/build.sh     # 6 lines over one schema_test run
MVCCS_DEBUG=warn  sh MVCCS-cicili/build.sh     # 11 over the same run
MVCCS_DEBUG=debug sh MVCCS-cicili/build.sh     # 505, of which 369 are the guard
```

| level | what it answers | what it prints |
|---|---|---|
| `info` | *what did this store do?* | every open with its page count and hexmap coverage, every commit with its transaction and stamp |
| `warn` | *what was unusual?* | a torn record salvaged, a keyless page refreed, a commit that had to WAIT for the wall clock, the pages a short hexmap cost, and any streams guard **waited for or held over a millisecond** |
| `debug` | *which thread, in what order?* | every read with the stream and the guard it used, every cursor callback window and its eligibility decision, every synthesised clock value, and **every guard acquisition with its mode, its wait and its hold** |

Everything goes to **stderr**, never stdout — a consumer's answers live there
(cocolog prints its own on stdout and is parsed by scripts) — and every line
carries the low sixteen bits of its thread, which is what makes eight threads
tellable apart in a file of ten thousand lines.

**The points were chosen by two bugs that took days without them.** #32 was a
commit stamp in the future; at `debug` it is one line per call:

```
mvccs[t5c40] clock synth 1789680767950128, ahead by 1 us
```

and #33 was a read holding nothing inside a cursor's callback window, which
is the pair of lines that sit next to each other:

```
mvccs[t0a10] cursor window: release=1 flip=0
mvccs[t0a10] read 8240  held=0 shared=0 window=0 eligible=0
```

— the guard released, no redirect to a private stream, and then a read on the
stream every other thread is seeking. Eleven comments and four days of
patched builds the first time; two lines the next.

**And the tenth point was asked for by a third** — ZiguratIP#37, which
measured that 99.9 % of a `begin_transaction` is spent *acquiring* the one
streams guard and 0.1 % doing the writes under it, and had to patch a working
copy to learn it. The guard now times itself:

```
mvccs[t1a04] guard taken exclusive waited 84 us
mvccs[t1a04] guard held 14 us
mvccs[t5c40] guard exclusive WAITED 13549 us
mvccs[t5c40] guard HELD 259262 us
```

The lower-case pair is `debug`, one per acquisition, and the SHOUTED pair is
`warn`, which carries only what crossed a millisecond. From one
`contention_test` run at `debug` — 36 444 exclusive acquisitions, 82 952
shared, 20 222 nested no-ops:

| | median | p90 | p99 | max |
|---|---|---|---|---|
| wait, exclusive | 84 µs | 1 438 µs | 4 866 µs | 13 549 µs |
| wait, shared | 0 µs | 170 µs | 1 576 µs | 8 821 µs |
| held | 14 µs | 124 µs | 1 698 µs | 259 262 µs |

**Read that table knowing what produced it.** At `debug` the point writes two
lines per acquisition — 119 396 of them in that run — so those figures belong
to a build busy writing to stderr, not to the build that ships. The shape is
the finding; the absolute numbers are inflated. `warn` showed 7 386 waits and
2 893 holds over a millisecond in the same suite, at two orders of magnitude
fewer lines, and is what to ask when the numbers themselves matter.

## Build and run

    sh MVCCS-cicili/build.sh     # needs sbcl + the cicili checkout
    ./mvccs_test                 # from the cicili directory, or note the
                                 # binary lands beside the transpiler CWD

The store files are `/tmp/mvccs-cicili-{hexmap,data}.bin`, recreated
fresh each run and reopened once mid-run to prove recovery.
