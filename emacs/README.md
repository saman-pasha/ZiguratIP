# parsi-mode

Emacs support for Parsi: syntax highlighting, indentation, and three commands.

```elisp
(add-to-list 'load-path "/path/to/ZiguratIP/emacs")
(require 'parsi-mode)
```

`.parsi` then opens in `parsi-mode`.

| key | command | what it does |
|---|---|---|
| `C-c C-c` | `parsi-compile` | compiles this buffer **here**, with the local `parsi` |
| `C-c C-r` | `parsi-compile-remote` | compiles it **on a running Zigurat**, through the connector |
| `C-c C-f` | `parsi-open-config` | opens the `connector.conf` the remote one uses |

---

## Compiling

Both commands save the buffer first — not as a convenience, but because the
compiler reads the *file*: an unsaved buffer compiles the previous version and
reports success about code that is not on screen.

**Two compilers, and they are not interchangeable.**

* **`parsi`** (`C-c C-c`) reads `ziguratip.conf` and compiles *here*, writing
  the `.so`, the generated header and the catalogue entry into this machine's
  home tree. Nothing is sent anywhere.
* **`parsic`** (`C-c C-r`) reads `connector.conf` and asks a server, so the
  object lands where that server will load it — and it works when the server is
  not on this machine at all.

Which you want: the local one while finding out whether the code is right at
all, and when the server reading that home tree is this machine. The remote one
when the server is elsewhere, or when you want the object where a running server
will pick it up — an object compiled into a home directory nobody is reading
does not answer a request.

Output goes to a compilation buffer. Both print a positioned failure the same
way, as `file:line:column: message`, which `compilation-mode` already
understands — so `C-x \`` and clicking both land on the column:

```
demo/01-schema.parsi:4:3: syntax error at line 4 column 3 near 'RETRN'
```

They agree because they share `Utility::diagnostic`: the parser says "at line 4
column 3" and has no idea what the caller called the file, so the two halves
only meet there, and one editor command parses both.

**The server has to allow the remote one.** `COMPILER/REMOTE_MODE` in its
`ziguratip.conf` is `FALSE` by default, and a compile arriving over the network
is refused before the code is read. Turned off, the answer says so and it
appears in the compilation buffer. Turning it on means anyone who can open a
connection can run the compiler — see `doc/security.md`.

### Both have to be findable

The top-level `Makefile` builds them into `home/bin`, which is not usually on
`PATH`, and they link against the shared libraries in `home/lib`, which are not
usually on the loader's path either. An absolute path alone may not be enough:

```elisp
(setq parsi-local-executable  "/opt/ZiguratIP/home/bin/parsi")
(setq parsi-remote-executable "/opt/ZiguratIP/home/bin/parsic")
```

with `LD_LIBRARY_PATH` (`DYLD_LIBRARY_PATH` on macOS) carrying `home/lib`, or a
wrapper script that sets it. Both commands say which setting to fix and what it
should point at, rather than reporting the loader's complaint as a compile
failure.

`parsic --config` prints the `connector.conf` it would use and nothing else,
which is what `parsi-open-config` asks — so the file you edit is the one the
next remote compile really connects with, even if the search order changes.

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
