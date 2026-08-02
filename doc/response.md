# Response Object

Response object is only available in Page object scope and users can send information by HTTP response.

## FLUSH() RETURNS Void

Flushes the buffer to the client stream.

## PROTOCOL() RETURNS String

Returns the HTTP response protocol part.

## SET_PROTOCOL(String) RETURNS Void

Sets the HTTP response protocol part.

## STATUS_CODE() RETURNS String

Returns the HTTP response status code.

## SET_STATUS_CODE(String) RETURNS Void

Sets the HTTP response status code.

## REASON_PHRASE() RETURNS String

Returns the HTTP response reason phrase.

## SET_REASON_PHRASE(String) RETURNS Void

Stes the HTTP response reason phrase.

## HEADER(String) RETURNS String

Returns the value of the specific HTTP header.

## HAS_HEADER(String) RETURNS Bool

Returns TRUE whether the specific HTTP header is available.

## SET_HEADER(String, String) RETURNS Void

Sets thhe value of the specific HTTP header.

## REMOVE_HEADER(String) RETURNS Void

Removes the specific HTTP header.

## COOKIE(String) RETURNS String

Returns the value of the specific HTTP cookie.

## HAS_COOKIE(String) RETURNS Bool

Returns TRUE whether the specific HTTP cookie is available.

## SET_COOKIE(String, String) RETURNS Void

Sets the value of the specific HTTP cookie.

## REMOVE_COOKIE(String) RETURNS Void

Removes the specific HTTP cookie.

## COOKIE_ATTRIBUTE(String, String) RETURNS String

Returns the value of the specific attribute of the specific HTTP cookie.

## HAS_COOKIE_ATTRIBUTE(String, String) RETURNS Bool

Returns TRUE whether the specific HTTP cookie has the specific attribute.

## SET_COOKIE_ATTRIBUTE(String, String, String = '') RETURNS Void

Sets the value of the specific attribute of the specific HTTP cookie.

## REMOVE_COOKIE_ATTRIBUTE(String, String) RETURNS Void

Removes the specific attribute of the specific HTTP cookie.

## Example

```parsi
PAGE hello_echo
BEGIN
PUBLIC:
    OVERRIDE FUNCTION PAGE_LOAD() RETURNS Void
    BEGIN
        IF request.method() == 'GET' BEGIN
            response.set_header('content-type', 'text/plain');
            ECHO 'Hello World!';
        END ELSE BEGIN
            IF request.has_cookie('counter') BEGIN
                response.set_cookie('counter', (request.cookie('counter').to_int() + 1).to_string());
            END ELSE BEGIN
                response.set_cookie('counter', Int(1).to_string());
            END
        END
    END
END
```
