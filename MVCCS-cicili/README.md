# MVCCS, written again in Cicili

An experiment, asked for in one sentence: *take a list of everything
MVCCS is responsible for, then try to write MVCCS again in Cicili with
`:cpp #t` — the scoped syntax may be useful.* The list is
[RESPONSIBILITIES.md](RESPONSIBILITIES.md). This directory is the try.

## What is here

* **`mvccs.cicili`** (~1,950 lines) — the storage engine core, written in
  Cicili's C++ layer, compiled against the **real** `StreamIO` and `Core`
  libraries (`Zigurat::binarystream`, `Zigurat::filestream`,
  `ZiguratException`, `Utility`). One source target emits `mvccs.cpp`
  (~2,100 lines of C++), compiles it with g++ and links `mvccs_test`.
* **`build.sh`** — `CICILI=$HOME/cicili sh build.sh`; the binary prints
  a check line per behaviour and exits with the failure count.

Eighteen checks, all green, five runs in a row: insert/commit/read-back,
update versioning (the old version retired and the new adopted at one
commit instant), rollback undoing a staged delete, TRUNCATE reclaiming
exactly the settled dead row and sparing the superseded one, allocation
overflow refused with a real thrown-and-caught `MemoryException`, two
pthread sessions racing 20 inserts through the shared streams,
REPEATABLE READ taking and releasing shared row locks, the SERIALIZABLE
gate admitting and releasing, **SNAPSHOT holding its point in time while
another session commits an update under it**, and a fresh `Memory` over
the same two files recovering the store through the startup walk.

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

**Not rewritten:** the B-tree index family (`btreeindex.hpp`, 1,888
lines — the `map`/`unmap`/split/combine machinery and the six
comparison cursors), `BaseSequence`, `Globals`, and the DBA plumbing
(watcher, `dba_pagefiles`, `dba_pointers`). The index tier is the
natural next step and `deftable` is where it would grow a `defindex`.

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

expands at read time into the `BaseTable` subclass with `pack`/`unpack`/
`pack_size` written out, the 20-byte hash key computed **in Lisp at
expansion time**, and typed `Book_insert` / `Book_update` /
`Book_delete` / `Book_cursor` / `Book_truncate` wrappers. What a
template instantiates invisibly, the macro emits greppably.

**The plumbing went C, deliberately.** Cicili lambdas are lifted and
cannot capture, so `std::function` callbacks became context structs +
function pointers; `std::multimap` became intrusive lists (every use was
equal-range walks and insert/erase); `std::condition_variable` +
predicate lambdas became pthread + bounded polling; `std::atomic` CAS
became a mutex-guarded clock. One `(code …)` escape survives in the
whole engine: `clock_gettime` on a `struct timespec`.

## Deliberate divergences

* Hash keys are interned once per process and every `Pointer` aims at
  the canonical copy — the multimaps' `new hashkey_t`/`delete[]`
  ownership soup is gone. `deftable` derives its 20 bytes with a Lisp
  FNV spread, not SHA-1 (equality and stability are all the engine asks
  of a key; a store written by this engine is therefore not key-compatible
  with one written by the C++ engine).
* Columns are plain `int64_t` — the nullable `Type` layer is out of
  scope (and `doc/truncate.md` records why a store is better off
  without NULLs anyway).
* Transactions are explicit (`begin_transaction` per session thread)
  rather than riding a `thread_local` constructor — `__thread` cannot
  run one.
* The cursor snapshots the page list into a bounded array (512 pages
  per key per scan) instead of a heap copy.
* No overloading in Cicili, so `_pointer`'s four overloads are
  `pointer_at` / `pointer_at_state`, and streams flow through
  `pack`/`unpack` virtuals instead of `operator<</>>`.

## Found upstream while porting

`Memory::_free` (memory.cpp): when a freed run covers a whole page, the
page is rewritten under `__FREE__` on disk, but the in-memory page list
gets a **second entry under the table's key** and keeps the old one —
`_page_list.insert({pointer.hash_key, …})` after
`_allocate_page(FREE_HASHKEY, …)`. `_allocate_new_page` looks for
`FREE_HASHKEY` entries, so a page handed back this way is invisible to
the allocator until restart, and the duplicate entry makes the same
page scanned twice under its old key. The rewrite rekeys the entry in
place; the original is unfixed as of `eed884f`.

## What the transpiler could not say (findings for cicili)

Hit while writing this, worked around in place, listed for the record:

1. A qualified base class cannot appear in a ctor's `init`
   (`(init (Zigurat::ZiguratException 9390 …))` → "wrong init entry");
   `using namespace` + the bare name works.
2. Top-level `var` inside a `module` under `:cpp` is name-mangled while
   references to it are not — so the engine lives at file scope.
3. A dotted `var` init inside a macro expansion (`(var int ,x . 0)`
   through `$$$`) breaks the reader path that handles the same form in
   plain source. Zero-initialized namespace-scope vars sidestep it.
4. `letin*` compiles to a GNU statement expression, so a block whose
   last statement returns a non-copyable (`stream->flush()` returning
   `basic_ostream&`) fails to compile; end such blocks with
   `(cast void 0)`.
5. Member access needs bound storage: `(-> (txn) id)` and member access
   through a cast expression are refused; bind first.
6. `(out …)` types are flat (`(out const uint8_t *)`), parameter
   descriptors are flat, but `cast` wants parenthesized pointer types
   without `const`.
7. A `(code "…")` escape re-escapes embedded quotes on emission, so a
   string literal cannot ride through one.

None of these blocked the port; every one had an in-language answer.

## Build and run

    sh MVCCS-cicili/build.sh     # needs sbcl + the cicili checkout
    ./mvccs_test                 # from the cicili directory, or note the
                                 # binary lands beside the transpiler CWD

The store files are `/tmp/mvccs-cicili-{hexmap,data}.bin`, recreated
fresh each run and reopened once mid-run to prove recovery.
