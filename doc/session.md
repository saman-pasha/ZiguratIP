# SESSION Object

Session object implements virtual session by using HTTP cookie.

**A page must call `INITIALIZE` before it uses a session, on every request.**
Nothing does it for you. Zeytun sweeps idle sessions before serving and releases
the binding afterwards, and neither of those binds one.

Without it, `SET`, `GET`, `HAS`, `REMOVE`, `CLEAR` and `DESTROY` throw. They used
to return quietly, which was worse: a page that forgot set values that vanished,
read back nothing, and reported success, so the mistake surfaced later as data
that was never written.

`ID()` and `IS_INITIALIZED()` never throw -- those are the questions a page asks
*before* deciding, and they answer plainly when there is no session.

The binding is per request, not per page load and not per thread: it is released
when the request ends, so a page cannot initialize once and rely on it later.

## GLOBAL ID() RETURNS String

Returns the session id.

## GLOBAL INITIALIZE(Request, Response) RETURNS Void

Binds this request to the session named by the visitor's cookie, minting a new
one and setting the cookie when they have none. Required before any other call
below.

## GLOBAL IS_INITIALIZED() RETURNS Bool

Whether this request has a session bound. Never throws.

## GLOBAL SET<T>(String, T) RETURNS Void

Keeps a value of type T by given key.

## GLOBAL GET<T>(String) RETURNS T

Retrives a value of type T which holds by given key.

## Example

```parsi
PAGE hello_echo
BEGIN
PUBLIC:
    OVERRIDE FUNCTION PAGE_LOAD() RETURNS Void
    BEGIN
        -- Required. Without it every session call below throws.
        -- No REQUIRES clause: Session is not a database object, and REQUIRES is
        -- for the tables and procedures a page reaches.
        session::initialize(request, response);
        IF request.method() == 'GET' BEGIN
            session::set<String>('message', 'Hello World!');
        END ELSE BEGIN
            ECHO session::get<String>('message');
        END
    END
END
```
