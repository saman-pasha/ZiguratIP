# Concurrency

What the server does when more than one client is talking to it at once: what
holds, what it costs, and where the limits are.

## The shape of it

A connection to the binary port gets **one worker thread and one transaction**,
and keeps both for its whole life. So:

* `SERVER/POOL_SIZE` is the most clients that can be connected at once, not a
  throughput knob. See [configuration.md](configuration.md).
* Every call down one connection is part of the same transaction until the
  client commits or rolls back. See [transaction.md](transaction.md).

Zeytun is the other way round: an HTTP request is one transaction, opened before
the page runs and committed when it returns.

## What is shared, and what protects it

Each thread has its own transaction — `Memory::transaction` is
`static thread_local` — but **not its own page store**. `_hexmap_io` and
`_data_io` are one pair of streams shared by every thread in the process.

A seek and the read that follows it are therefore one operation. Let another
thread in between them and the read comes back from that thread's file position:
the bytes are taken for a chunk header, a size, an address, and what surfaces is

```
hexmap ends inside the chunk at 29987569464559153
```

with an address out of nowhere — or a `Pointer` assembled from somebody else's
offset, a walk into it, and the server gone.

`Streams` (the engine, `MVCCS-cicili/mvccs-lib.cicili`) is what stops that. It is a guard over
both mutexes with two properties that a plain `lock_guard` pair cannot have:

* **It nests.** `online_insert` holds the streams and then calls `object.map()`,
  which is the row's indexes — and an index has to take them itself, because
  procedure code reaches `cursor_equal` directly with nothing held. `std::mutex`
  is not recursive, so an index that locked unconditionally would deadlock the
  write path. A guard on a thread that already holds them owns nothing and does
  nothing.
* **It can be handed back.** A scan cannot hold the streams across a caller's
  callback: the callback is procedure code and the ordinary thing for it to do
  is update the row it was just handed. `Memory::_cursor` and
  `BTreeIndex::_cursor_output` release them around it; `_check_lock` releases
  them while it waits for somebody else's row lock. `Memory::Streams::held()` is
  null exactly while this thread holds nothing, so anything reached inside one
  of those windows takes them properly.

Everything that touches the store goes through it: the table API, the B-tree
indexes, sequences, commit, rollback and truncate.

**The indexes did not, until recently.** `BTreeIndex` reached into `Memory`'s
streams directly and took no lock at all, so any `WHERE indexed_column == value`
from two connections at once was the race above. It never showed in the test
suite because nothing in the tree used an index from two threads; it showed the
moment a real client did. `Test/test_contention.cpp` is that case, and several
more like it.

## What the isolation levels mean here

| level | what a scan does |
|---|---|
| `READ UNCOMMITTED` | takes rows as it finds them, staged or not |
| `READ COMMITTED` | takes the version of every row that was current when the statement began, and waits for nobody |
| `REPEATABLE READ` | marks the rows it reads, and restarts the query if one changed underneath |
| `SNAPSHOT` | follows each row back to the version that was current when the transaction began |
| `SERIALIZABLE` | one transaction at a time, server wide |

### READ COMMITTED does not wait, and that matters

A reader at this level never queues behind a writer. It does not need to: an
update writes the new version to a *new* address and leaves the old one where it
is until it commits, so the version the reader is entitled to see is already
sitting at the address it is looking at.

It used to wait, because reads went through `_check_lock` — which is a *writer's*
question, "may I have this row?", and the only answer to that is to wait. The
cost was not just latency. The wait was long enough for the writer to commit
while the reader stood there, so the version being waited on was retired and its
replacement was at an address the scan had already gone past: **a transaction
that had done nothing but read saw neither version of a row that existed
throughout**. A client polling for the presence of a row that was being rewritten
was told, for the whole length of the rewrite, that it did not exist.

`Memory::_read_committed` is the rule now, and it is short: your own staged work
counts, anybody else's does not, and what is left is whatever was committed at
this address. A writer still waits for a writer — that is `_check_lock`, and it
still times out rather than wedging.

### A statement sees one version of the store

Not waiting is not enough on its own, because **a scan spans time**. It visits
addresses in order and takes a while about it, while a writer puts a replacement
row wherever the allocator has room. If the two interleave freely, a scan can
pass a replacement before it is committed and then find the original retired —
the row missing — or see the original and then reach the committed replacement,
and count the same row twice. Neither is a torn read; both are wrong. Measured
before the fix: 865 double counts in a single run of three scanners against one
writer.

So a reader decides every row against a single point in time, taken once when the
statement begins (`Memory::Statement`, held by table scans and index walks
alike). Each version of a row carries when it was committed and when it was
superseded, and `Memory::_alive_at` asks the obvious question of the two. An
update stamps the old version's death and the new one's birth with the **same**
commit time, so the two conditions change over at the same instant: exactly one
version of a row satisfies them for any given time, and never both.

**That needed a better clock.** The stamps were `std::time(0)` — one second — and
two versions of a row born and retired inside the same second are
indistinguishable, which under any load at all is all of them.
`Memory::version_time` is microseconds and strictly increasing within the
process, so no two commits ever share a stamp. It goes in the same 8-byte
`time_t` fields, so the on-disk control block is the shape it always was; a store
written by an older build has second-scale stamps in it, which read as "committed
long ago" and "retired long ago" — both the right answer.

One trap worth knowing about, because it cost an afternoon: `modify_time` only
means *died* when `offline_state` says so. `_offline_update` rewrites a row in
place and stamps `modify_time` on it while it is still perfectly current — that
is how the index catalogue records a root address and how a B-tree key records
its value chain. Reading that stamp as a death makes every index invisible, so a
reopened store looks empty and quietly builds itself a fresh, blank one.

`SERIALIZABLE` is a semaphore of one and only against other `SERIALIZABLE`
transactions — `READ COMMITTED` never waits for it. That makes it affordable
over a short critical section and ruinous over a long one. The intended use is
the read-then-write that a queue claim is:

```parsi
PROCEDURE work::claim(p_name AS String, p_worker AS String)
RETURNS String
REQUIRES work::jobs
BEGIN
    TRANSACTION ISOLATION LEVEL SERIALIZABLE;

    DECLARE found AS String = '';
    SELECT found = name FROM work::jobs WHERE name == p_name AND status == 'idle';
    IF found <> '' BEGIN
        UPDATE work::jobs SET status = p_worker WHERE name == found;
    END
    RETURN found;
END
```

At `READ COMMITTED` two clients arriving together both see `idle`, both update,
and both believe they own it. At `SERIALIZABLE` one of them waits. The rest of
their work — reading the job, doing it, writing the result — stays at
`READ COMMITTED` and runs at the same time as everyone else's.

The level lasts for one transaction. Commit, and the connection is back at the
configured default.

## Waits are bounded, all of them

| wait | bound | on timeout |
|---|---|---|
| a row lock held by another transaction | `Memory::lock_wait_timeout_ms` (10s) | raises `lock wait timeout` |
| a turn at `SERIALIZABLE` | the same | raises `serializable wait timeout` |

Neither waits for ever, because both can be held by a client that has simply
stopped talking. A transaction that gives up rolls back and the caller may
retry.

## Latency: Nagle is off on every accepted connection

Both protocols this server speaks are conversations of small messages — read a
request, write a reply, wait for the next. That is precisely the pattern
Nagle's algorithm coalesces and the peer's delayed ACK then stalls, and neither
side is at fault alone:

* Nagle holds a small write until the previous one is acknowledged.
* Delayed ACK holds the acknowledgement for up to 40ms, hoping to piggyback it
  on a reply.
* The reply is the write being held.

Nothing is wrong, nothing errors, nothing times out — and every exchange costs
40ms. `TCPServer::run` therefore calls `Socket::set_nodelay` on each connection
before handing it to the handler, best effort: a socket that will not take the
option still works, it is merely slow.

**What it cost before that line existed.** One turn of a cocolog worker is a few
dozen exchanges. Twelve of them took a minute to do a second's work, and the
test that runs twelve at once failed on its own timeout with no error anywhere
to say why. A client that speaks this protocol should set `TCP_NODELAY` at its
end too; the server setting it fixes only the half the server sends.

## Deleted rows are kept, and `TRUNCATE` is what reclaims them

Under MVCC a deleted row is not gone: it is kept so that a transaction entitled
to an earlier view of the store can still read it. **Nothing removes it
afterwards.** There is no background vacuum — `TRUNCATE` is the vacuum, and
something has to call it.

This is easy to miss because it does not look like deleting. A workload that
rewrites a row — delete the old version, insert a replacement — leaves a dead
row behind every time it does so, and a workload that rewrites the same row
thirty times leaves twenty-nine. The table's live contents never grow; what
grows is everything an index entry has to walk past to reach them.

**Measured, in cocolog:** twelve interpreters over four machine states took
**14 seconds on one run and 32 on the fifth**, identical work each time, while
the store file grew 72KB. Not more data to find — more dead data to walk past.

**And it could not be reclaimed**, because `TRUNCATE` refuses any table holding
a NULL column: see `doc/truncate.md`. A store that has been written to for long
enough therefore has no way back, which makes reading a NULL column the more
urgent of the two bugs.

`TRUNCATE` frees only rows that are committed as deleted and that no running
transaction can still be entitled to; live rows are untouched. It is therefore
safe against a table in use, and is a vacuum rather than an emptying — see
`doc/truncate.md`. **An application that rewrites rows needs to run it on a
schedule.** The freed space returns to the allocator's free list for reuse; the
file does not shrink.

## A scan copies the page list before it walks it

`_page_list` is a `std::multimap` that `_allocate_page` and `_free_key` modify
under `_page_list_access`. `Memory::_cursor` and `Memory::_dead_pointers` walked
it holding **no lock at all**.

Inserting into a multimap does not invalidate an iterator, which is what makes
this easy to get wrong. But it does rotate the red-black tree, and an iterator
being incremented follows the very parent and child pointers a rotation is
rewriting — so a walk running beside an insert can land in a subtree belonging to
a different hash key. A scan then reads rows that are not its table's, and a
`TRUNCATE` frees rows belonging to a table nobody named.

Both now copy the matching entries under the lock and walk the copy. The lock is
held across the **copy and not the walk**: the callback runs with the streams
handed back and is free to insert, which takes this same lock, so holding it for
the length of a scan would deadlock against the first callback that allocates a
page. A page allocated after the copy is not scanned, which is what a statement's
fixed view means anyway.

## Open: a reader can see another session's staged row

`Test/test_concurrency.cpp`'s `readers_do_not_queue_behind_staged_writes` fails
about **one run in three**, and has since well before the page-list fix above —
measured at `416b86f` as 8 failures in 24 runs.

Four sessions each stage an insert at `READ COMMITTED` and then scan; each must
see only its own row. Intermittently one sees another's. What is established:

* **It is not a transaction-id collision.** All four ids print distinct on a
  failing run.
* **It is not the page-list race.** Fixing that left the rate unchanged.
* **`_read_committed` approves it legitimately.** Logged at the moment of the
  dirty read, the control block reads `offline_state = INSERTED` with a
  `create_time` earlier than the reader's snapshot — so `_alive_at` is right to
  call it alive. A row that is only staged is presenting a control block that
  says committed.

So the fault is upstream of the visibility rules, in what a staged insert leaves
in its control block or in when that becomes durable — not in the rules
themselves.

**This matters beyond the bug.** At a one-in-three false-failure rate this suite
cannot validate a concurrency change: it reports a failure often enough that any
new work looks broken, and a genuine regression looks like the usual flake. It
should be fixed before anything else in this file is touched.

## What is still true and worth knowing

* **A statement is the unit of consistency at `READ COMMITTED`, not a
  transaction.** Two scans in one transaction can see different states of the
  store, which is what the level means; a transaction that needs one view
  throughout is asking for `SNAPSHOT` or `REPEATABLE READ`.
* **`_offline_update` is outside MVCC.** It rewrites a row in place rather than
  writing a new version, so a reader holding an older statement sees the new
  bytes. It is for metadata that is true the moment it is written — an index's
  root address, a B-tree key's value chain — and not for rows.
* **An exception ends the connection.** The request loop writes
  `EXCEPTION_THROWN` and breaks, so any refusal — including a lock timeout — is
  fatal to that connection rather than to the call. A client that means to carry
  on has to redial.
* **A stale compiled object is fatal to the server.** Parsi objects in
  `home/ld` are compiled against the engine's headers, and a change to those
  headers changes the symbols they import. Loading one that no longer matches
  ends the process with a `symbol lookup error` from the dynamic linker, not a
  catchable exception. Recompile the objects after building the engine.
* **Abandoned transactions hold their locks** until the thread that owned them
  ends.

## Testing it

`Test/test_concurrency.cpp` drives Memory's own API from several threads.
`Test/test_contention.cpp` drives the indexes, which is where the sharp edges
were, and the transaction's own state. `Test/test_isolation.cpp` covers what
each level promises a single reader.

```sh
home/bin/Test Contention
home/bin/Test Isolation
home/bin/Test Concurrency
```
