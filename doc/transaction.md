# TRANSACTION Clause

## Syntax

```ebnf
TRANSACTION [ COMMIT | ROLLBACK ];

TRANSACTION BEGIN
    [parsi_clause]*
END

TRANSACTION ISOLATION LEVEL [ READ UNCOMMITTED | READ COMMITTED | REPEATABLE READ | SNAPSHOT | SERIALIZABLE ] ;
```

## Example

```parsi
TRANSACTION ISOLATION LEVEL SNAPSHOT;
```

## Procedures called over the binary protocol

`TRANSACTION/MODE` in `ziguratip.conf` is `NON-AUTOCOMMIT` by default, and
nothing commits for you. The transaction belongs to the connection: a connection
holds one worker thread for its whole life and the transaction is that thread's,
so every call made down one connection is part of the same transaction.

The client ends it. `Connector::commit()` makes the work of every call so far
stand; `Connector::rollback()` discards all of it; closing without either
discards it too. That is what lets a client treat several calls as one unit:

```cpp
db.call("demo::add_visitor");   // ... and its arguments and results
db.call("demo::add_visitor");
db.commit();                    // both, or with rollback() neither
```

`Connector::auto_commit(true)` is the alternative: the server commits after
every `call`, so each one stands on its own and none can be rolled back.

A procedure may also commit its own work with `TRANSACTION COMMIT;`, which is
what you want when the procedure is a unit in itself — the TRUNCATE example in
[TRUNCATE](truncate.md) needs it, because a truncate only reclaims deletes that
have already settled. Be aware that it takes the decision away from the client:
what a procedure has committed, the client can no longer roll back.

Zeytun is different — an HTTP request is already one transaction, opened before
the page runs and committed when it returns cleanly — so a page needs no
`TRANSACTION COMMIT;`.

## The isolation level belongs to the transaction, not the connection

`TRANSACTION ISOLATION LEVEL ...;` applies to the transaction that runs it and
no further. Commit or roll back, and the next transaction on that connection is
back at `TRANSACTION/ISOLATION_LEVEL` from the configuration. A procedure that
needs a stronger level therefore says so every time it runs, and cannot leave a
connection stronger than the client asked for.

That is worth stating because it was not true until recently: a connection kept
whatever level a procedure had set until it hung up. With `SERIALIZABLE` the
consequence was not subtle — see below.

## What SERIALIZABLE costs

`SERIALIZABLE` admits **one transaction at a time across the whole server**, and
only against other `SERIALIZABLE` transactions: a `READ COMMITTED` transaction
never waits for one. So it is affordable over a short critical section — the
read-then-write of a queue claim, say — and ruinous over a long one, because
everything else at that level queues behind it.

A transaction waiting for its turn gives up after `lock_wait_timeout_ms` and
raises `serializable wait timeout`, the same way a row lock does. A client that
takes the level and then goes quiet delays others for that long and no longer.

See also: [Connector](connector.md), [Configuration](configuration.md),
[Concurrency](concurrency.md)
