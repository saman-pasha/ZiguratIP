# Connector Object

Connector object is a tool to create connections to ZiguratIP DBMS ports, compile code, Remote Procedure Call ,send and retrive data.
Because of ZiguratIP supports two protocol IPC and TCP, Connector should config to use one of them.

## CONSTRUCTOR()

Default constructor tells the connector to load the connection configuration from ZIGURATIP_HOME/etc/connector.conf.

## CONSTRUCTOR(String)

This constructor gets the IPC path of the DBMS server.

## CONSTRUCTOR(String, Int, Int)

This constructor gets the host name or address and a port number of DBMS server, third parameter is connection timeout in second.

## OPEN() RETURNS Void

Opens a connection to the server by settings from constructor and begins a transaction.

## IS_OPEN() RETURNS Bool

Checks whether the connection is open or closed.

## COMPILE(Text) RETURNS Void

Gets a Text, send it to the server for compile and gets the result of compilation back.

The source is a suite: `TABLE`, `SEQUENCE`, `PROCEDURE`, `CLASS`, `PAGE`, `TYPE`
and `ENUM` declarations. A bare statement is not one, and neither is an empty
string; both come back as an exception rather than a silent success.

## AUTO_COMMIT(Bool) RETURNS Void

Sets the connection auto commit option.
If this option be true, the server calls a commit after all Remote Procedure Call.

With the shipped `TRANSACTION/MODE: NON-AUTOCOMMIT` this is off, and the
transaction is instead the connection's: it lives as long as the connection
does, so every call made down one is part of it. `COMMIT()` makes all of that
work stand and `ROLLBACK()` discards all of it; closing without either discards
it. See [TRANSACTION](transaction.md).

## ISOLATE(IsolationLevel) RETURNS Void

Sets the Transaction Isolation Level of the current transaction.
ENUM IsolationLevel
BEGIN
    READ_UNCOMMITTED,
    READ_COMMITTED,
    REPEATABLE_READ,
    SNAPSHOT,
    SERIALIZABLE
END

## CALL(String) RETURNS Void

Calls a remote procedure.
Procedure parameters should be sent by WRITE function.

Over a secure connection with the server's `SECURITY/PERMISSIONS_MODE` on, the
procedure named here has to be covered by a permission in the certificate this
connection presented, and so does anything `COMPILE` declares. Nothing is
configured client side for that: the permissions travel in the certificate. See
[security.md](security.md#permissions).
All IN or INOUT parameters should be sent and OUT parameters must not be sent.

## WRITE<T>(T) RETURNS Void

Writes the parameters to the client stream for Remote Procedure Call.

## RESULT() RETURNS ResultType

If the procedure has any result, this method would returns the type of result.
ENUM ResultType
BEGIN
    SUCCESSFUL_DONE,
    CURSOR_OPEN,
    CURSOR_FETCH,
    CURSOR_CLOSE,
    RETURN_VALUE,
    EXCEPTION_THROWN
END

## COLUMNS() RETURNS Vector<String>

If the result type is CURSOR_OPEN, it would returns the column's names of the result set.

## FETCH(T...) RETURNS Void

If the result type is CURSOR_FECTH, it would fetche the row.
All parameters types of this method should be the types which has fetched.

## READ<T>() RETURNS T

If the result type is CURSOR_FECTH, it would fetche the field.

## READ<T>(T) RETURNS Void

If the result type is CURSOR_FECTH, it would fetche the field.

## COMMIT() RETURNS Void

Commits the current transation.

## ROLLBACK() RETURNS Void

Rollbacks the current transaction.

## CLOSE() RETURNS Void

Closes the connection if it is open.

## Reading what a call sends back

`CALL` returns as soon as the server has accepted the name. Everything the
procedure produces — its cursors and its return value — arrives afterwards as a
sequence of results, and the caller reads them until `SUCCESSFUL_DONE`:

| `RESULT()` | What to do |
|---|---|
| `CURSOR_OPEN` | `COLUMNS()` — the column names of the cursor about to be read |
| `CURSOR_FETCH` | `FETCH(...)` — one row, in the column order just given |
| `CURSOR_CLOSE` | nothing; that cursor is finished |
| `RETURN_VALUE` | `FETCH(value)` — the procedure's `RETURNS` value, once |
| `SUCCESSFUL_DONE` | the call is over; stop reading |
| `EXCEPTION_THROWN` | `RESULT()` throws it as a `ConnectorException` |

A procedure returning `Void` sends no `RETURN_VALUE`, and one that runs no
`SELECT` sends no cursor, so a loop that handles whatever turns up is the only
form that works for every procedure:

```cpp
db.call("demo::add_visitor");
db.write_string(String("pitarugi"));    // arguments go out after the call

Long id(0);
for (ResultType r = db.result(); r != ResultType::SUCCESSFUL_DONE; r = db.result()) {
    if (r == ResultType::CURSOR_OPEN)       db.columns();
    else if (r == ResultType::RETURN_VALUE) db.fetch(id);
}
std::cout << "inserted visitor " << id.value() << std::endl;
```

The value has to be read into the type the procedure declares — a `RETURNS Long`
into a `Long` — because the fetch unpacks by type. Reading the results is not
optional: what is left unread stays in the stream and the next call reads it
instead of its own reply.

## Example

```parsi
PAGE all_employees
BEGIN
PUBLIC:
    OVERRIDE FUNCTION PAGE_LOAD() RETURNS Void
    BEGIN
        DECLARE con AS Connector;
        CALL con.open();
        CALL con.call('human_resources::read_employees');
        ECHO '<table>';
        ECHO '<thead><tr>';
        ECHO '<th>ID</th><th>Name</th>';
        ECHO '</tr></thead><tbody>';
        DECLARE id AS Long, name AS String;
        WHILE con.result() == ResultType::CURSOR_FETCH BEGIN
            ECHO '<tr>';
            con.fetch(id, name);
            ECHO '<td>', id, '</td>', '<td>', name, '</td>';
            ECHO '</tr>';
        END
        ECHO '</tbody>';
        ECHO '</table>';
    END
END
```
