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

## AUTO_COMMIT(Bool) RETURNS Void

Sets the connection auto commit option.
If this option be true, the server calls a commit after all Remote Procedure Call.

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
