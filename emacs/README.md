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
| `C-c C-f` | `parsi-open-config` | opens `connector.conf` — `C-u C-c C-f` opens `ziguratip.conf` |

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

### The two configurations

`C-c C-f` opens the one belonging to whichever compile you are about to run:

| | file | read by | holds |
|---|---|---|---|
| `C-c C-f` | `connector.conf` | `parsic` | HOST, PORT, TIMEOUT, TLS_MODE and the client certificate |
| `C-u C-c C-f` | `ziguratip.conf` | `parsi` **and the server** | COMPILER/CPP and its flags, the catalogue and library paths, COMPILER/REMOTE_MODE |

Both are looked for in the three places `Utility::config_path` walks, in the same
order: `$ZIGURATIP_HOME/etc/`, then `~/ZiguratIP/etc/`, then `/etc/ZiguratIP/`.
Not found, the error names all three rather than leaving you to guess which to
create.

`connector.conf` is asked for rather than searched when `parsic` is available:
`parsic --config` prints exactly the file it will connect with, so what you edit
is what the next remote compile really uses even if the search order changes.
There is no equivalent question for `ziguratip.conf` — `parsi` prints its
configuration path only while compiling, and running a compile to find out where
the settings live is not a trade worth making — so that one is searched here.

Note that turning on `COMPILER/REMOTE_MODE` in `ziguratip.conf` is what gives
`C-c C-r` a server to talk to at all, and it means anyone who can open the binary
port can run the compiler. See `doc/security.md`.

### `C-c C-r` over HTTP, for a server you cannot open a socket to

Some servers you can reach, you cannot speak the binary protocol to. A Colab VM
has no inbound port, and the tunnel that gives it one is an **HTTP** reverse
proxy — so `HOST`/`PORT` have nothing to connect to and no port number fixes it.

Set `URL` in `connector.conf` instead:

```
URL: https://wide-brave-yellow-cat.trycloudflare.com
```

`C-c C-r` then POSTs the file to `compiler.zt` there rather than running
`parsic`. No port belongs in it — `https` is 443 and the hostname is the whole
address.

That endpoint is `System/compiler.parsi`, a page that hands what it is given to
the compiler, so a server set up this way **will compile and run whatever anyone
with the URL sends it**. It needs `COMPILER/REMOTE_MODE` on as well. Throwaway
sandboxes only; [`colab/TUTORIAL.md`](../colab/TUTORIAL.md) walks the whole
scenario through.

The answer comes back as a rendered page rather than a status, because
`compiler.zt` is a form a human fills in — the outcome is read out of the markup,
which is as fragile as it sounds. Anything unrecognised is shown whole rather
than guessed at, and any failure opens a `*parsi compile*` buffer with the
server's answer in full.

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

Every `.parsi` file in the repository — all 22 — re-indents **byte-identical**,
tabs and spaces alike. That is the whole test, and it is a real one: it caught
four separate mistakes, each of which had looked right, and one file the tree
had been carrying with mixed indentation.

`tab-width` has to equal `parsi-indent-offset` for a tab-indented file, which is
what "set both to match the file" above means in practice: with the stock
`tab-width` of 8 and an offset of 4, a level is four columns and cannot be a
tab, and every tab-indented file comes back different — which reads as a broken
indenter and is not one.

The file it caught was `System/compiler.parsi`, whose two `TAB SPC SPC TAB`
lines are now tabs throughout. One of them was not merely untidy: at
`tab-width 4` it sat at column 20 among neighbours at 16, a level deeper than
the block it belonged to.

That a file is unchanged is not by itself proof the indenter does anything, so
`demo/04-bulk.parsi` is also stripped of all indentation and rebuilt from
nothing; it comes back byte-identical too.

---

## Highlighting

The keyword list is not written from memory. It is every literal word the
grammar matches, taken from `home/etc/patterns.conf` — the file the parser
itself reads — so a keyword added to the grammar shows up as a word this mode
does not colour, rather than as a silent disagreement. Both lists carry the
command that re-derives them, so the claim can be re-checked rather than taken
on trust.

**Both lists**, because the grammar keeps two kinds of word apart and a list
built from one kind alone misses the other. Keywords are `NAME` directives;
`AND`, `OR`, `IS`, `LIKE`, `BETWEEN` and `NOT` are `OP` directives, most of them
inside an alternation:

```
{O}OP: [=<>]|==|<=|<>|>=|IS|LIKE
{O}OP: AND|OR
{E}OP: NOT
```

They went uncoloured until this was re-derived from the grammar rather than
maintained by hand — and they are not rare: `NOT NULL` is on most columns in
`System/`, `AND` joins nearly every `WHERE`, and `demo/03-pages.parsi` turns on
`LIKE`. They take `font-lock-builtin-face`, so `NOT NULL` reads as an operator
beside a constant, which is what it is.

Backticked names (`` `std::`shared_ptr ``) are raw C++ reached from Parsi and
are coloured differently, because they are not checked the same way.

**Verbatim blocks are quote-neutralised.** A single apostrophe in a C++ comment
— "the pool's lock", which `Test/ai/classifier.parsi` really contains — would
otherwise open a Parsi string. Measured on that file: 152 characters of ordinary
code coloured as a string literal, with nothing about it looking like a quoting
mistake. This does not attempt to highlight the C++ itself; that would mean
running a second major mode over the region.
