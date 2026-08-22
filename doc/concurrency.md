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

`Memory::Streams` (`MVCCS/memory.hpp`) is what stops that. It is a guard over
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
| `READ COMMITTED` | waits for a row another transaction has locked, then takes what it committed |
| `REPEATABLE READ` | marks the rows it reads, and restarts the query if one changed underneath |
| `SNAPSHOT` | follows each row back to the version that was current when the transaction began |
| `SERIALIZABLE` | one transaction at a time, server wide |

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

## What is still true and worth knowing

* **A row being rewritten is briefly invisible to a concurrent reader.** An
  update is a delete and an insert, and the replacement can land at an address a
  scan already went past — so a reader at `READ COMMITTED` can see neither
  version for the length of the writer's transaction. It is consistent, in that
  no reader sees a half-written row; it is not the last committed version, which
  is what `READ COMMITTED` normally promises. A client polling for the presence
  of a row that is being rewritten has to ask more than once.
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
