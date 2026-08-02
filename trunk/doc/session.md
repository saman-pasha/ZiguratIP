# SESSION Object

Session object implements virtual session by using HTTP cookie.

## GLOBAL ID() RETURNS String

Returns the session id.

## GLOBAL INITIALIZE(Request, Response) RETURNS Void

Enables session on this page.

## GLOBAL SET<T>(String, T) RETURNS Void

Keeps a value of type T by given key.

## GLOBAL GET<T>(String) RETURNS T

Retrives a value of type T which holds by given key.

## Example

```parsi
PAGE hello_echo
REQUIRES Session
BEGIN
PUBLIC:
    OVERRIDE FUNCTION PAGE_LOAD() RETURNS Void
    BEGIN
        session::initialize(request, response);
        IF request.method() == 'GET' BEGIN
            session::set<String>('message', 'Hello World!');
        END ELSE BEGIN
            ECHO session::get<String>('message');
        END
    END
END
```
