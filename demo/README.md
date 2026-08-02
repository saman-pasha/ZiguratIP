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

Then open <http://127.0.0.1:2190/setup.zt> to load the data, and
<http://127.0.0.1:2190/catalog.zt> to browse it.

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

## 4. What happens when you request the page

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

**`RESET_MODE: TRUE`** in `home/etc/ziguratip.conf` erases `home/data` on every
start, which is convenient for a demo and wrong for anything else. Set it to
`FALSE` to keep your rows, and re-run `/setup.zt` only when you want more.

**Recompiling** an object while the server is running has no effect until it is
restarted — shared objects are loaded once and cached.

**Reading the generated code** is the fastest way to understand a compile
error. The `.out` file next to it holds the exact compiler invocation and its
diagnostics.
