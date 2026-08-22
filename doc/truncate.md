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

## What it will not touch

- **Live rows.** A row that has not been deleted is never reclaimed. Running
  `TRUNCATE` on a table with no deleted rows does nothing at all.
- **Deletes that are still open.** Only a committed delete is reclaimable, since
  an open one may still roll back and the rollback would have nothing to restore.
  A `DELETE` and a `TRUNCATE` in the same transaction therefore reclaim nothing —
  the truncate has to come after the delete commits, in a later transaction.
- **Rows a running transaction holds.** Anything under a lock is left alone.
- **Index storage.** A table's indexes keep their own pages under their own
  keys, and truncating the table does not compact them.

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
