# parsi-mode

Emacs support for Parsi: syntax highlighting, indentation, and two commands.

```elisp
(add-to-list 'load-path "/path/to/ZiguratIP/emacs")
(require 'parsi-mode)
```

`.parsi` then opens in `parsi-mode`.

| key | command | what it does |
|---|---|---|
| `C-c C-f` | `parsi-open-config` | opens the `connector.conf` a compile would use |
| `C-c C-c` | `parsi-compile` | compiles this buffer on a running Zigurat |

---

## Compiling

`parsi-compile` saves the buffer and runs **`parsic`**, which connects the way
`connector.conf` says to and asks the server to compile.

**It compiles on the server, not here, and that is the point.** There are two
compilers and they are not interchangeable:

* **`parsi`** reads `ziguratip.conf` and compiles locally, writing the `.so`,
  the header and the catalogue entry into *this* machine's home tree.
* **`parsic`** reads `connector.conf` and asks a server, so the object lands
  where that server will load it.

An object compiled into a home directory no server is reading does not answer a
request. `parsic` is also the only one of the two that works when the server is
somewhere else.

Output goes to a compilation buffer. A failure that names a position is printed
as `file:line:column: message`, which `compilation-mode` already understands, so
`C-x \`` and clicking both land on the column:

```
demo/01-schema.parsi:4:3: syntax error at line 4 column 3 near 'RETRN'
```

**The server has to allow it.** `COMPILER/REMOTE_MODE` in its `ziguratip.conf`
is `FALSE` by default, and a compile arriving over the network is refused before
the code is read. Turned off, the answer says so and `parsi-compile` shows it.
Turning it on means anyone who can open a connection can run the compiler — see
`doc/security.md`.

### `parsic` has to be findable

It is built by the top-level `Makefile` into `home/bin`, which is not usually on
`PATH`, and it links against the shared libraries in `home/lib`, which are not
usually on the loader's path either. An absolute path alone may not be enough:

```elisp
(setq parsi-executable "/opt/ZiguratIP/home/bin/parsic")
```

with `LD_LIBRARY_PATH` (`DYLD_LIBRARY_PATH` on macOS) carrying `home/lib`, or a
wrapper script that sets it. `parsi-compile` says which of the two is missing
rather than reporting the loader's complaint as a compile failure.

`parsic --config` prints the `connector.conf` it would use and nothing else,
which is what `parsi-open-config` asks — so the file you edit is the one the
next compile really connects with, even if the search order changes.

---

## Indentation

`parsi-indent-offset` (default 4) is a level; tabs or spaces follow
`indent-tabs-mode`. The tree is not consistent — `demo/` is four spaces,
`Test/ai` and `System/` are one tab — so set both to match the file you are in.

`BEGIN` opens and `END` closes, and **both can share a line with other words**:
`IF cond BEGIN` and `END ELSE BEGIN` are ordinary Parsi, the second closing and
opening at once. A line's effect is counted rather than assumed.

Four things are deliberately left alone:

* **verbatim blocks** — the C++ between `BEGIN HPP`/`BEGIN CPP` and its `END` is
  copied through by the tokenizer byte for byte, and it is usually pasted in
  from elsewhere.
* **the inside of a string** — Parsi strings run across lines, and
  `System/catalog_pages.parsi` keeps a whole HTML table in one. Every byte is
  data the server will send.
* **alignment inside an open bracket** — one level in is a convention, not the
  only one. `System/catalog.parsi` lines its arguments up under the opening
  paren with tabs and spaces mixed to hit the column; a continuation that
  already has an indentation keeps it, and only one with none gets placed.
* **comma-continued lists** — `REQUIRES a,` / `SELECT x,` run on with no bracket
  to detect them by, and are treated the same way.

### What it was tested against

Every `.parsi` file in the repository — all 20 — re-indents **byte-identical**,
tabs and spaces alike. That is the whole test, and it is a real one: it caught
four separate mistakes, each of which had looked right.

That a file is unchanged is not by itself proof the indenter does anything, so
`demo/04-bulk.parsi` is also stripped of all indentation and rebuilt from
nothing; it comes back byte-identical too.

---

## Highlighting

The keyword list is not written from memory. It is every literal word the
grammar matches, taken from `home/etc/patterns.conf` — the file the parser
itself reads — so a keyword added to the grammar shows up as a word this mode
does not colour, rather than as a silent disagreement.

Backticked names (`` `std::`shared_ptr ``) are raw C++ reached from Parsi and
are coloured differently, because they are not checked the same way.

**Verbatim blocks are quote-neutralised.** A single apostrophe in a C++ comment
— "the pool's lock", which `Test/ai/classifier.parsi` really contains — would
otherwise open a Parsi string. Measured on that file: 152 characters of ordinary
code coloured as a string literal, with nothing about it looking like a quoting
mistake. This does not attempt to highlight the C++ itself; that would mean
running a second major mode over the region.
