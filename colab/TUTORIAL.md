# Running ZiguratIP on Colab, through a Cloudflare tunnel

Everything here is in [`ZiguratIP_Colab.ipynb`](ZiguratIP_Colab.ipynb) beside this file.
This explains what each step does and why, so you can change it rather than only run it.

**Open it in Colab:**

<https://colab.research.google.com/github/saman-pasha/ZiguratIP/blob/colab/colab/ZiguratIP_Colab.ipynb>

**Or download it:** the notebook is a single file — `colab/ZiguratIP_Colab.ipynb` — and
Colab's *File → Upload notebook* takes it directly. Nothing else in this directory is
needed at runtime; the notebook clones the repository itself.

---

## What you end up with

A Colab VM running one process — ZiguratIP is a database, a language and a web server in
the same binary — and a public HTTPS URL that reaches its web front end.

| part | what it is | port |
|---|---|---|
| **Zigurat** | MVCC storage engine | 2160, binary protocol |
| **Parsi** | SQL-like language, compiled to a `.so` the server `dlopen`s | — |
| **Zeytun** | HTTP server for static files and `.zt` pages | 2190 |

---

## Why a tunnel and not Colab's port proxy

Colab ships a proxy: `google.colab.kernel.proxyPort(2190)` hands back an HTTPS URL that
is supposed to front a port in the VM. **It does not deliver the request.**

That is not a guess. The notebook used to carry a diagnostic for it: record the size of
`server.log`, open the proxy URL in a browser, wait for it to fail, then look at the size
again. Nothing was ever appended. The request never reached Zeytun, so nothing about
ZiguratIP could have fixed it — four rounds of "blank page" were spent before that
measurement settled which hop was at fault. Those cells are gone; this is what replaced
them.

A tunnel inverts the direction. `cloudflared` opens an **outbound** connection to
Cloudflare, and Cloudflare hands back a hostname that forwards down it. Nothing has to
reach *in*, so Colab's routing stops mattering.

There is a second reason, and it matters as soon as you look at a page rather than a
status code. A proxy that serves you at `.../proxy/2190/` makes the page's relative links
resolve against Colab's host: `src="zeytun.png"` 404s and `href="setup.zt"` goes nowhere.
A tunnel maps a **whole hostname** to Zeytun, so those resolve the way the page's author
meant.

---

## 🔓 Read this before you set `TUNNEL = True`

A Cloudflare *quick tunnel* — the kind that needs no account — has **no authentication
and no access control of any kind**. Anyone with the URL reaches the server. The URL is
unguessable, but it is not a secret: it is issued by Cloudflare and travels through their
infrastructure.

What is behind it is a sandbox:

- **Cross-site scripting by construction.** Parsi's `ECHO` does not escape; a demo page
  echoes what it is given.
- **No write-ahead log.** A hard kill mid-commit can corrupt the store.
- **No authentication in front of any page.**

So: use it to look at the demo, keep it short, and stop it when you are done. Do not point
one at data you care about.

### The two things the notebook refuses to expose

ZiguratIP compiles Parsi to C++ and **links it at request time**. A compiler reachable
from the network is therefore arbitrary code execution as the server's user, and the
notebook checks for both ways that can happen before it starts a tunnel:

**1. `COMPILER/REMOTE_MODE`** in `home/etc/ziguratip.conf`. When `TRUE`, any client that
can open the binary port may send source to be compiled. It is `FALSE` by default and the
server refuses the request before it reads the code:

```
compiling over the network is disabled; compile with the parsi program and let the
server load the object, or set COMPILER/REMOTE_MODE to TRUE if this instance is not
exposed
```

Leave it off. It exists for a server on a private network.

**2. A compiled `Compiler` page.** `System/compiler.parsi` is a *web page* whose POST
handler is:

```
DECLARE con AS Connector;
CALL con.open();
CALL con.compile(request.post('code'));
```

That is a compiler behind an HTTP form. `demo/build.sh` does not build it, and `home/ld`
is gitignored, so a fresh clone has none — but if you have compiled `System/` yourself it
is sitting there, and the tunnel would publish it.

**This is not hypothetical.** Run against this repository's own working checkout, the
check found two objects — the compiler page and the drawer that renders it — both left
over from ordinary development. That is exactly the situation it exists to catch.

The check names what it found and stops. `I_UNDERSTAND = True` overrides it; the only
good reason to set that is a tunnel nobody else can actually reach.

---

## The steps

### 1 · Build

Clones the `colab` branch and runs `make`. **Use that branch, not `master`** — `master`
does not build on Linux, and worse, every recipe in the top-level `Makefile` is prefixed
with `@-`, so failures are stepped over and the run still prints `******* all done *******`
while having produced 2 of 14 libraries and no executables.

The cell checks the branch exists on the remote before cloning, because a clone of a
missing branch fails quietly enough that every later cell blames something else.

### 2 · Runtime environment

`ZIGURATIP_HOME` is both the install prefix and the runtime home. On Linux the shared
libraries are found through `LD_LIBRARY_PATH` — not macOS's `DYLD_LIBRARY_PATH`.

A C++ compiler must stay on `PATH`, because Parsi pages are compiled to a `.so` *at
request time*, not only at build time.

### 3 · Persistence (optional)

Colab VMs are ephemeral. `PERSIST = True` copies a snapshot from Google Drive onto local
disk **before** the server starts. The engine never runs against the Drive mount.

### 4 · Build the demo objects

`demo/build.sh` runs the offline `parsi` compiler over `demo/0*.parsi` in order — each
step `REQUIRES` objects the one before it created. It starts nothing.

This is the supported way to compile, and the reason the network compiler stays off.

### 5 · Start the server, and check it answers

The server blocks forever, so it runs as a background process with its output in
`server.log`.

Then a loopback request, before any URL is handed out. This matters more than it looks: a
tunnel forwards to 2190 whether or not anything is listening, so **a dead server and a
healthy one look identical from a browser** — both render a blank page. Asking over
loopback first is what tells the two apart.

### 6 · The tunnel

Downloads `cloudflared`, runs it against `http://localhost:2190`, and waits for the
hostname to appear in its log. Re-running the cell stops the previous tunnel first, so a
second one does not quietly take a different hostname while the first still holds the port.

If no URL appears it prints what `cloudflared` said, rather than leaving a dead cell. The
usual causes are an egress policy blocking `api.trycloudflare.com`, or nothing listening
on 2190.

### 7 · Or no tunnel at all

`visit()` fetches a page **and every asset it references** over loopback, embeds them as
`data:` URIs, and renders the result inline. Nothing is exposed, and what you see is what
Zeytun actually served, images included.

```python
visit('/setup.zt')     # seed the catalogue -- run once
visit('/catalog.zt')   # the books and authors it created
visit('/lookup.zt')    # queries served from single-column indexes
visit('/report.zt')    # a two-column index
```

Running `setup.zt` twice reports `unique key 'IDX_DEMO_AUTHORS_NAME'` — that is the index
refusing a duplicate author, which is the point of it.

### 8 · Snapshot to Drive (optional)

Only meaningful with `PERSIST = True`, and only **after** a clean stop — see below.

### 9 and 10 · Stop the server, then the tunnel

In that order, and stop both. The tunnel outlives the server: stopping ZiguratIP alone
leaves `cloudflared` up and the URL answering 502, which reads as a broken server rather
than a stopped one.

Snapshot to Drive only **after** a clean stop — there is no write-ahead log, so a snapshot
taken mid-write can be inconsistent.

---

## Editing Parsi with an editor

`emacs/parsi-mode.el` in this repository gives syntax highlighting, indentation, and two
compile commands. The remote one (`C-c C-r`) runs `parsic`, which reads
`home/etc/connector.conf` and asks a running server to compile — which needs
`COMPILER/REMOTE_MODE` on, so it is **not** something to combine with an open tunnel.

Inside Colab, the local command's equivalent is what you want: run `parsi` over the file
and let the server load the object.

```
!ZIGURATIP_HOME=/content/ZiguratIP/home \
 LD_LIBRARY_PATH=/content/ZiguratIP/home/lib \
 /content/ZiguratIP/home/bin/parsi mypage.parsi
```

It exits 0 on success and 1 with `file:line:column: message` on a syntax error.

---

## Limits worth knowing

- **Durability** — no WAL, no fsync on the data path. A hard runtime kill mid-commit can
  corrupt the store.
- **Concurrency** — thread per connection, pool of 64, backlog 64. A single browser opens
  about six connections for one page.
- **TLS** — the server speaks it, but a quick tunnel terminates TLS at Cloudflare and
  reaches your VM over plain HTTP on loopback. The hop that matters is inside the VM, so
  that is fine here; it is not a model for a real deployment.
- **A quick tunnel has no uptime guarantee.** The hostname changes every time and
  Cloudflare may drop it whenever they like.
