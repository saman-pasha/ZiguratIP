# A ZiguratIP demo, end to end

A small book catalogue: two tables with indexes, sequences, procedures that
write, and web pages that read — all in Parsi, all compiled to native code and
loaded into the database process.

Every listing here compiles and runs as shown.

```bash
make                 # from the repository root, if you have not already
demo/build.sh        # compile the demo objects
home/bin/ziguratip   # start the server
```

Then, in order:

| | |
|---|---|
| <http://127.0.0.1:2190/setup.zt> | seed the catalogue |
| <http://127.0.0.1:2190/catalog.zt> | browse it |
| <http://127.0.0.1:2190/lookup.zt> | queries served from single-column indexes |
| <http://127.0.0.1:2190/bulk.zt> | load 500 rows into a second table |
| <http://127.0.0.1:2190/report.zt> | query those through a two-column index |

---

## 1. Tables, indexes and sequences

[`01-schema.parsi`](01-schema.parsi)

```parsi
TABLE demo::authors
BEGIN
    COLUMN id      AS Long   PRIMARY KEY;
    COLUMN name    AS String UNIQUE KEY NOT NULL;
    COLUMN country AS String;
END
```

Three kinds of key, and each one builds a B-tree index in the storage engine:

| | |
|---|---|
| `PRIMARY KEY` | unique, and the row's identity |
| `UNIQUE KEY` | unique, but not the identity — two authors cannot share a name |
| `INDEX KEY` | not unique; lookups by this column use the index instead of scanning |

`demo::books` uses all three ideas at once:

```parsi
TABLE demo::books
BEGIN
    COLUMN id        AS Long   PRIMARY KEY;
    COLUMN title     AS String NOT NULL;
    COLUMN author_id AS Long   INDEX KEY;
    COLUMN year      AS Int;
END
```

**Sequences are declared, not implied.** A `PRIMARY KEY` does not generate
numbers for you, so each table that wants them gets its own sequence object:

```parsi
SEQUENCE demo::books_seq
BEGIN
    FROM 1;
    TO Long::MAX;
    STEP 1;
END
```

`demo::` is a domain. It keeps names apart in the catalogue, and it matters
later: a page may not share a name with a table.

---

## 2. Procedures

[`02-procedures.parsi`](02-procedures.parsi)

```parsi
PROCEDURE demo::add_book(title AS String, author_id AS Long, year AS Int)
RETURNS Long
REQUIRES demo::books, demo::books_seq
BEGIN
    DECLARE id AS Long = demo::books_seq::NEXT();
    INSERT INTO demo::books VALUES (id, title, author_id, year);
    RETURN id;
END
```

`REQUIRES` names every object this one links against. It is not documentation —
the compiler links the listed shared objects, so **anything you name must
already be compiled**. That is why the demo builds in numbered order.

A procedure may call another, so long as it requires it:

```parsi
PROCEDURE demo::seed()
RETURNS Void
REQUIRES demo::add_author, demo::add_book
BEGIN
    DECLARE le_guin AS Long = demo::add_author('Ursula K. Le Guin', 'US');
    CALL demo::add_book('A Wizard of Earthsea', le_guin, 1968);
END
```

---

## 3. Web pages

[`03-pages.parsi`](03-pages.parsi)

A `PAGE` is a class that overrides `PAGE_LOAD`. Zeytun turns the URL into an
object name, so `/catalog.zt` loads the page named `catalog`, and
`/shop/cart.zt` would load `shop::cart`.

```parsi
PAGE catalog
REQUIRES demo::books, demo::authors
BEGIN
PUBLIC:
    OVERRIDE FUNCTION PAGE_LOAD() RETURNS Void
    BEGIN
        ECHO '<h1>Catalog</h1><table>';
        SELECT '<tr><td>', id, '</td><td>', title, '</td></tr>'
        FROM demo::books;
        ECHO '</table>';
    END
END
```

**`SELECT` is a cursor, not a result set.** Everything listed between `SELECT`
and `FROM` is emitted once per row, so the row markup goes inside the statement
and the surrounding markup goes in `ECHO` either side of it. There is no result
object to iterate.

Two names to keep apart: the page is `catalog`, not `demo::books`. A page
sharing a table's name would collide in the catalogue.

---

## 4. Lookups that use the indexes

The catalog reads every row, because it wants every row. A `WHERE` on an
indexed column is different: the compiler resolves it against the B-tree
instead of scanning.

[`03-pages.parsi`](03-pages.parsi) ends with a `lookup` page doing exactly
that:

```parsi
SELECT '<li>', title, ' (', year, ')</li>'
FROM demo::books WHERE author_id == 1;
```

You can see which path was taken, because the generated C++ names it. After
`demo/build.sh`, look at `home/tmp/_LOOKUP_.cpp`:

| The query | What it compiles to |
|---|---|
| `WHERE author_id == 1` | `IDX_DEMO_BOOKS_AUTHOR_ID.cursor_equal` |
| `WHERE id == 3` | `IDX_DEMO_BOOKS_ID.cursor_equal` |
| `WHERE id > 2` | `IDX_DEMO_BOOKS_ID.cursor_greater_than` |
| `WHERE name == '...'` | `IDX_DEMO_AUTHORS_NAME.cursor_equal` |
| `WHERE year > 1950` | `Globals::memory()->cursor<...>` — a full scan |

So all three key kinds are used, and not only for equality: `>` becomes
`cursor_greater_than`, `<>` becomes `cursor_not_equal`. `year` is the control
case — it carries no key, so that query reads every row and filters.

In a compound condition the compiler indexes what it can and filters the rest,
so `WHERE author_id == 1 AND year > 1960` still enters through
`IDX_DEMO_BOOKS_AUTHOR_ID`.

Adding an index is therefore just a column attribute — `INDEX KEY` on
`author_id` is what turns that first query from a scan into a lookup.

---

## 5. Indexes: creating them and searching them

The catalogue has seven rows, too few to tell a lookup from a scan.
[`04-bulk.parsi`](04-bulk.parsi) adds a table that carries as many as you like,
and this section covers every way to index it and every way to search it.

### Creating an index

An index is a column attribute or a table-level declaration. There is no
separate `CREATE INDEX`; the table definition is the whole story.

```parsi
TABLE demo::sales
BEGIN
    COLUMN id     AS Long PRIMARY KEY;   -- unique, and the row's identity
    COLUMN region AS String;
    COLUMN year   AS Long;
    COLUMN amount AS Long;

    INDEX KEY (region, year);            -- one index over two columns
END
```

| Written as | Kind | Index built |
|---|---|---|
| `COLUMN a AS Long PRIMARY KEY` | unique, the identity | `IDX_DEMO_SALES_A` |
| `COLUMN b AS String UNIQUE KEY` | unique, not the identity | `IDX_DEMO_SALES_B` |
| `COLUMN c AS Long INDEX KEY` | not unique | `IDX_DEMO_SALES_C` |
| `PRIMARY KEY (a, b);` | composite identity | `IDX_DEMO_SALES_A_B` |
| `UNIQUE KEY (a, b);` | composite unique | `IDX_DEMO_SALES_A_B` |
| `INDEX KEY (a, b);` | composite, not unique | `IDX_DEMO_SALES_A_B` |

Every one becomes a B-tree in the storage engine, named
`IDX_<DOMAIN>_<TABLE>_<COLUMNS>`. That name is what you look for in the
generated C++ to confirm which index a query used.

**Two columns is the limit.** `INDEX KEY (b, c, d)` fails to compile with
`call to non-static member function without an object argument`. The engine's
`BTreeIndex` template is variadic, so this is a code generation gap rather than
a storage limit — but for now, two.

### Searching through an index

Every comparison operator has an index cursor behind it. Compile the demo and
read `home/tmp/_REPORT_.cpp` to see which one a query became:

| `WHERE` | Compiles to |
|---|---|
| `id == 1` | `IDX_DEMO_SALES_ID.cursor_equal` |
| `id <> 1` | `IDX_DEMO_SALES_ID.cursor_not_equal` |
| `id < 5` | `IDX_DEMO_SALES_ID.cursor_less_than` |
| `id <= 5` | `IDX_DEMO_SALES_ID.cursor_less_than_equal` |
| `id > 5` | `IDX_DEMO_SALES_ID.cursor_greater_than` |
| `id >= 5` | `IDX_DEMO_SALES_ID.cursor_greater_than_equal` |
| `amount > 100` | `Globals::memory()->cursor<...>` — `amount` has no index, so every row is read |
| `name LIKE 'The %'` | a scan — a pattern cannot be looked up in a B-tree |
| `id BETWEEN 1 AND 5` | a scan — see below |
| `id == 1 OR id == 2` | a scan — see below |

So indexes are not only for equality: ranges compile to range cursors too, and
they stay correct as the table grows. `Test/test_btree.cpp` is the suite that
holds them to that; it loads thousands of rows and checks every cursor against
the answer worked out in plain C++ over the same data.

`OR` reads every row and filters. One cursor per operand would be faster, but a
row satisfying both sides would come back twice and there is nowhere to
deduplicate it, so the whole condition becomes a filter over a scan. `AND` is
the one that reaches an index.

### BETWEEN and LIKE

```parsi
SELECT title FROM demo::books WHERE title LIKE 'The %';
SELECT id, region FROM demo::sales WHERE id BETWEEN 1 AND 12;
```

`BETWEEN` is inclusive at both ends and expands to `(x >= low) && (x <= high)`.
It always reads every row: writing the two comparisons out by hand instead lets
the leading one reach an index. Its subject is evaluated twice, so keep side
effects out of it.

`LIKE` matches a pattern, with `%` for any run of characters and `_` for
exactly one. `NULL` on either side gives `NULL`. It is always a scan.

Both bind tighter than `AND`, so
`WHERE id BETWEEN 1 AND 12 AND region LIKE '%U%'` reads as two conditions, not
one. The `report` page runs exactly that.

### Three rules worth knowing

**A composite index is used left to right.** `INDEX KEY (region, year)`
answers a question about `region`, or about `region` and `year` together, but
not about `year` alone:

| `WHERE` | Result |
|---|---|
| `region == 'EU' AND year == 2022` | `IDX_DEMO_SALES_REGION_YEAR` |
| `region == 'EU'` | `IDX_DEMO_SALES_REGION_YEAR` — a leading prefix still uses it |
| `year == 2022` | full scan — `year` is not the leading column |

If you need to search on `year` by itself, declare a second index for it.

**Inside `AND`, only the first condition chooses the index.** These return
identical rows, and only the first is a lookup:

```parsi
SELECT amount FROM demo::sales WHERE region == 'EU' AND amount > 4000;  -- index
SELECT amount FROM demo::sales WHERE amount > 4000 AND region == 'EU';  -- scan
```

The same applies to a composite: `year == 2022 AND region == 'EU'` scans,
because the leading column of the index is not the leading condition. **Put the
indexed column first.** The `report` page shows both orders side by side.

**`OR` reads every row.** `WHERE id == 1 OR id == 2` is correct but is a scan,
for the reason given above. Put the condition you want indexed on its own, or
join it with `AND`.

### Counting and totalling

`SELECT` is a cursor, so there is no result set to ask how many rows came back
or what they add up to. An item written `name = expression` assigns to that
variable once per row instead of being emitted — the `SET` clause, in the one
statement that has no `BEGIN`/`END` to put one in:

```parsi
DECLARE rows AS Long = 0;
DECLARE total AS Long = 0;

SELECT rows = rows + 1, total = total + amount
FROM demo::sales WHERE region == 'EU';

ECHO 'EU rows: ', rows, ' &mdash; amount total: ', total;
```

`=` assigns and `==` compares, which is the same split `SET` and `DECLARE`
already use. An assignment emits nothing and produces no column, so the `SELECT`
above prints nothing by itself; the `ECHO` after it does the printing. Inside a
`WHERE` clause both spellings still mean comparison.

The `report` page ends with exactly this.

### Loading enough rows to matter

```parsi
PROCEDURE demo::bulk_load(rows AS Long)
RETURNS Void
REQUIRES demo::sales, demo::sales_seq
BEGIN
    DECLARE i AS Long = 0;
    WHILE i < rows
    BEGIN
        DECLARE id AS Long = demo::sales_seq::NEXT();
        INSERT INTO demo::sales VALUES (id, region, i % 5 + 2020, i * 10);
        SET i = i + 1;
    END
END
```

<http://127.0.0.1:2190/bulk.zt> loads 500 rows and can be reloaded for more;
<http://127.0.0.1:2190/report.zt> queries them. The whole load is one
transaction, so 500 rows either all arrive or none do.

Note `i % 5 + 2020` rather than `2020 + i % 5`: the typed operators are members
of their left operand, so a bare literal cannot start the expression. For the
same reason, mixing `Int` and `Long` in one expression will not compile — keep
a column and the variables feeding it the same width. And Parsi has no escape
for a quote inside a string, so a literal `'` in output is written `&#39;`.

---

## 6. What happens when you request the page

1. Zeytun maps `/catalog.zt` to the object `catalog` and loads
   `home/ld/lib_CATALOG_.so`, compiling nothing at request time.
2. It **opens a transaction** before `PAGE_LOAD` runs.
3. The page runs. `SELECT` walks the store through the engine's cursor.
4. On a clean return Zeytun **commits**; on an exception it **rolls back**.
5. The worker thread is released back to the pool, and its transaction and
   session bindings are cleared so the next visitor inherits nothing.

One HTTP request is one transaction. If `/setup.zt` throws halfway through
seeding, nothing it inserted is left behind.

---

## Where things end up

| Path | Contents |
|---|---|
| `home/ld/lib_*.so` | the compiled objects, loaded on demand |
| `home/catalog/*.conf` | catalogue entries describing each object |
| `home/data/{hexmap,data}` | the page store itself |
| `home/tmp/*.cpp`, `*.out` | generated C++ and the compiler's output — useful when something will not build |

To see what Parsi actually generates, read `home/tmp/_DEMO::BOOKS_.cpp` after a
build.

---

## Notes

**Your rows survive a restart.** `RESET_MODE` is `FALSE` in the shipped
configuration, so the store in `home/data` is created on first use and kept
afterwards — run `/setup.zt` once, not on every start. Set `RESET_MODE: TRUE`
if you would rather start from an empty store each time.

Requesting `/setup.zt` a second time is refused, and refused cleanly:

```
Zeytun Catch: unique key 'IDX_DEMO_AUTHORS_NAME'
```

`demo::authors.name` is a `UNIQUE KEY`, so the duplicate author is rejected —
and because the request is one transaction, the books it had already inserted
before reaching that point go back too. The catalogue still holds exactly seven
rows, not eleven. That is the constraint and the rollback both doing their job,
in one line of log.

**Recompiling an object needs a server restart**, whatever `CACHE_MODE` says.
Nothing calls `dlclose`, so the dynamic loader keeps handing back the copy it
first loaded. Rebuild, restart, then reload the page.

**Reading the generated code** is the fastest way to understand a compile
error. The `.out` file next to it holds the exact compiler invocation and its
diagnostics.
