# PAGE Clause

Page object is like class object but all pages should override PAGE_LOAD method.
[Request](request.md) and [Response](response.md) objects are only available in Page object scope.

## Naming and the URL

A page is an object like any other and may sit inside a schema. Zeytun reads the
directories of the request path as the schema levels, so

```parsi
PAGE demo::scoped
```

is reached at `/demo/scoped.zt`, and `PAGE catalog` at `/catalog.zt`. Nesting
goes as deep as you like: `/a/b/c.zt` asks for `A::B::C`.

Nothing is served from the filesystem for a `.zt` request -- the path names an
object in `home/ld`, not a file -- so a page that has not been compiled is a
404 however the directories look.

## Example

```parsi
PAGE web_service
BEGIN
PUBLIC:
    OVERRIDE FUNCTION page_load() RETURNS Void
    BEGIN
        response.set_header('content-type', 'text/html');
        ECHO '
        <table style="width: 100%">
            <thead>
                <tr>
                    <th>ID</th>
                    <th>NAME</th>
                </tr>
            </thead>
            <tbody>';
        SELECT '<tr>',
            '<td>', id, '</td>',
            '<td>', name, '</td>',
            '</tr>'
        FROM human_resources::employees;
        ECHO '</tbody>';
        ECHO '</table>';
    END
END
```
