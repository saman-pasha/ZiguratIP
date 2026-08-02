# Request Object

Request object is only available in Page object scope and users can extract information from HTTP request.

## METHOD() RETURNS String

Returns the HTTP request method. GET or POST.

## URI() RETURNS String

Returns the HTTP request full path URI.

## PROTOCOL() RETURNS String

Returns the HTTP request protocol part.

## HOST() RETURNS String

Returns the HTTP request host section.

## PORT() RETURNS String

Returns the HTTP request port number.

## PATH() RETURNS String

Returns the HTTP request page path.

## FRAGMENT() RETURNS String

Returns the HTTP request fragment.

## CONTENT_TYPE() RETURNS String

Returns the HTTP request content type.

## CONTENT() RETURNS Vector<Char>

Returns the HTTP request data recieved.

## CONTENT_LENGTH() RETURNS ULong

Returns the length of HTTP request data.

## HEADER(String) RETURNS String

Returns the value of the specific HTTP header.

## HAS_HEADER(String) RETURNS Bool

Returns TRUE whether the specific HTTP header is available.

## COOKIE(String) RETURNS String

Returns the value of the specific HTTP cookie.

## HAS_COOKIE(String) RETURNS Bool

Returns TRUE whether the specific HTTP cookie is available.

## QUERY(String) RETURNS String

Returns the value of the specific URL query string.

## HAS_QUERY(String) RETURNS Bool

Returns TRUE whether the specific URL query string is available.

## ARRAY_QUERY(String) RETURNS Vector<String>

Returns the array values of the specific URL query string.

## HAS_ARRAY_QUERY(String) RETURNS Bool

Returns TRUE whether the array of specific URL query string is available.

## POST(String) RETURNS String

Returns the value of the specific posted data.

## HAS_POST(String) RETURNS Bool

Returns TRUE whether the specific posted data is available.

## ARRAY_POST(String) RETURNS String

Returns the array values of the specific posted data.

## HAS_ARRAY_POST(String) RETURNS Bool

Returns TRUE whether the array of specific posted data is available.

## Example

```parsi
PAGE hello_echo
BEGIN
PUBLIC:
    OVERRIDE FUNCTION PAGE_LOAD() RETURNS Void
    BEGIN
        IF request.method() == 'GET' BEGIN
            ECHO 'Hello World!';
        END ELSE BEGIN
            ECHO request.query('message');
        END
    END
END
```
