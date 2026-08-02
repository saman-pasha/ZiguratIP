# PAGE Clause

Page object is like class object but all pages should override PAGE_LOAD method.
[Request](request.md) and [Response](response.md) objects are only available in Page object scope.

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
