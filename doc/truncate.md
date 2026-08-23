# TRUNCATE Clause

## Syntax

```ebnf
TRUNCATE [domain::]* name ;

domain ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
name ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
```

See also: [DELETE](delete.md), [TRANSACTION](transaction.md)

## What it does

`DELETE` does not reclaim anything. It flags the row deleted and leaves its
chunks where they are, which is what lets the store be read back at an earlier
point in time and what stops a rollback handing space to the allocator while
another session is working beside it. Those rows are invisible to every query
and still occupy the page files.

`TRUNCATE` is the operation that gives that history up in exchange for the
space. It reclaims every settled deleted row of one table, and each reclaimed
row is coalesced with the free space next to it, so a page that ends up empty
goes back to the allocator for any table to use.

```parsi
TRUNCATE demo::sales;
```

It names a table and nothing else. There is no `WHERE`: it does not choose
between rows, it removes what `DELETE` already removed.

## It cannot truncate a table that holds a NULL

`TRUNCATE` reads whole rows to unlink their index entries, and **a NULL column
cannot be read back at all** — the engine refuses the row with `NULL value`. So a
table with a nullable column that anything ever left NULL can never be reclaimed,
and the call fails outright rather than skipping the row.

Two things make this easy to walk into. An **empty String is stored as NULL**, so
writing `""` into a nullable column is enough. And nothing else notices: an
ordinary `SELECT` naming other columns reads fine, so a table can look healthy
for as long as nobody truncates it.

Until reading a NULL column works, a table meant to be reclaimable needs every
column `NOT NULL`, and its writers need to send something rather than nothing.

## What it reclaims

Both kinds of dead version, not just the obvious one:

- **Settled deletes** — rows `DELETE` removed, once the delete has committed.
- **Superseded versions** — the old version an `UPDATE` leaves behind. Only a
  superseded version ever carries the `UPDATED` state (a commit stamps the new
  version `INSERTED`), so the state alone marks it dead. Skipping these was
  measured before it was fixed: a row claimed and released a thousand times
  left a thousand old versions no truncate would touch — 22 pages carrying
  4 live rows — and a polling workload paid for all of them on every read.

## What it will not touch

- **Live rows.** A row's current version is never reclaimed. Running
  `TRUNCATE` on a table with no dead versions does nothing at all.
- **Deaths that are still open.** Only a committed delete or update is
  reclaimable, since an open one may still roll back and the rollback would
  have nothing to restore. A `DELETE` and a `TRUNCATE` in the same
  transaction therefore reclaim nothing — the truncate has to come after the
  commit, in a later transaction.
- **Rows a running transaction holds.** Anything under a lock is left alone.
- **Index storage — in the C++ engine.** Its indexes keep their own pages
  under their own keys, and truncating the table only unlinks the dead value
  entries; the keys and nodes of every id the table ever held stay. The
  Cicili engine (`MVCCS-cicili/`) goes further: a truncate is the one moment
  the table is at its smallest, so each of its indexes is REBUILT there —
  storage dropped wholesale, pages handed back to the allocator, and the
  surviving rows mapped into a fresh tree.

## When to run it

**Whenever the application rewrites rows, on a schedule.** Nothing calls
`TRUNCATE` for you: there is no background vacuum, and a store that is never
truncated grows for as long as it is used.

It is easy to miss because the workload does not look like deleting. A
procedure that updates a row by removing the old version and inserting a
replacement leaves a dead row behind every time it runs. The table's live
contents stay the same size; what grows is the number of dead versions every
index entry has to walk past to reach them, and so every read gets slower.

**Measured.** cocolog keeps a suspended interpreter's state in a table and
rewrites it once per turn. Twelve interpreters over four such states took
**12 seconds against an empty store and 60 against one a few hundred test runs
had been through** — identical work, identical live data, five times the wall
clock. One `TRUNCATE` over its four tables brought it back to 16 seconds.

If the answer to "when was this store last truncated" is "never", that is
almost certainly where an unexplained slowdown is coming from.

## What it costs

The store cannot be rolled back to a time before the truncate for the rows it
reclaimed — that is the whole trade. `rollback_transaction_to` and the snapshot
isolation level both read old row versions, and the reclaimed ones are gone.

The file does not shrink. Pages are returned to the allocator rather than to the
filesystem, so the space is reused by the next insert instead of being handed
back to the operating system.

## Example

Reclaiming after a batch delete, in two transactions because the delete has to
settle first:

```parsi
PROCEDURE demo::purge_old_sales(before AS Long)
RETURNS Void
REQUIRES demo::sales
BEGIN
    DELETE FROM demo::sales WHERE year < before;
    TRANSACTION COMMIT;

    TRUNCATE demo::sales;
END
```
