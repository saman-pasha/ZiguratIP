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

`TRANSACTION/MODE` in `ziguratip.conf` is `NON-AUTOCOMMIT` by default, and a
procedure reached through the connector is not committed for you: closing the
connection discards the work, and `Connector::commit()` does not pick it up. A
procedure that writes and is meant to be called this way commits itself:

```parsi
PROCEDURE demo::add_visitor(name AS String)
RETURNS Long
REQUIRES demo::visitors, demo::visitors_id_sequence
BEGIN
    DECLARE id AS Long = demo::visitors_id_sequence::NEXT();
    INSERT INTO demo::visitors VALUES (id, name);
    TRANSACTION COMMIT;
    RETURN id;
END
```

`Connector::auto_commit(true)` is the alternative: the server then commits after
every `call`, and the procedure needs no clause of its own.

Zeytun is different — an HTTP request is already one transaction, opened before
the page runs and committed when it returns cleanly — so a page needs no
`TRANSACTION COMMIT;`.

See also: [Connector](connector.md), [Configuration](configuration.md)
