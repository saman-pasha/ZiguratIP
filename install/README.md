# install/ — from a clone to a built `home/`, on one machine

Two scripts, one per operating system, and the part they share:

| file | what it is |
|---|---|
| `install-linux.sh` | Debian and Ubuntu through `apt-get`, Fedora and the Red Hat family through `dnf`: the packages, a clang 16+ (Fedora's own; on Ubuntu clang 18 from apt.llvm.org, because apt's is 14), then the common part |
| `install-macos.sh` | macOS: the Xcode command line tools for clang, Homebrew for `sbcl`, `libtool` and `openssl@3`, then the common part |
| `common.sh` | sourced by both: the Cicili checkout (found or cloned beside this one), the four Lisp systems Cicili is built from, `make MODE=Release`, and a check of the **artifacts** — fourteen libraries and three programs — because the top-level make steps over a failed project and its exit code proves nothing |
| `lisp/` | `sha1` and `base64`, the two Lisp systems Cicili depends on that are published nowhere; see the README there |

```sh
sh install/install-linux.sh        # or install-macos.sh
```

Then put the three lines it prints — `CICILI`, `ZIGURATIP_HOME` and the
library path — in your shell profile, and `sh Test/run-e2e.sh` proves the
build against a live server.

## Knobs

* `NO_PACKAGES=1` skips the package step: no root, or already done.
* Run as yourself, the Linux script asks `sudo` only for the packages.
  `sudo sh install/install-linux.sh` works too: the packages go in as root,
  and everything after them runs again as the user who called `sudo`, in
  that user's own home. Quicklisp and `~/common-lisp` are found through
  `$HOME`, which `sudo` sets to `/root`, so done as root they landed where
  the user's own `sbcl` never looks. A root login with no `sudo` (a
  container) uses root's home, which is its own. The macOS script refuses
  root outright, as Homebrew does.
* `CICILI=/path` names a Cicili checkout elsewhere; the default is
  `../cicili`, and it is cloned there when absent.
* `CICILI_CC=...`, `CICILI_CXX=...` name a particular clang (`clang-18`, a
  path); the wrappers in `tools/cc` read exactly those two. Every build here
  is clang — the libraries, the server and the Parsi objects it loads —
  and the scripts refuse a compiler that is not. gcc's packages are still
  installed, for the libstdc++ headers clang compiles against
  (`--gcc-install-dir`); nothing is compiled by gcc.
* On Red Hat Enterprise Linux and its rebuilds, `sbcl` is in EPEL.
* `LOG=/path` moves the make log from `/tmp/ziguratip-install.log`. A log
  there that is not yours to write (an earlier run as root left it) is
  refused by name before anything is built, rather than read back as this
  run's failure.

## Quicklisp

When `$QUICKLISP_HOME` (`~/quicklisp`) has no Quicklisp, it is installed as
[its own page](https://www.quicklisp.org/beta/) says: `quicklisp.lisp` and
its signature are fetched, the signature is checked against Quicklisp's
release key with `gpgv` — the key is held to the fingerprint that page
publishes, `D7A3489DDEFE32B7D0E7CC61307965AB028B5FF7`, and nothing is
installed when it does not verify — and then `(quicklisp-quickstart:install)`.
Either way `(ql:add-to-init-file)` follows, unless `~/.sbclrc` loads a
Quicklisp already, so your own `sbcl` has it too. `gnupg` is among the
packages for this; with `NO_PACKAGES=1` and no `gpg`, the script stops and
says so.

`QUICKLISP_HOME` elsewhere than `~/quicklisp` wants Cicili 1.0.1 or later.
The build runs `sbcl --script cicili.lisp` (`MVCCS-cicili/build.sh`), which
reads no `~/.sbclrc`, so Cicili finds Quicklisp itself: from
`$QUICKLISP_HOME`, or `~/quicklisp` when that is unset. An older Cicili loads
`(user-homedir-pathname)/quicklisp/setup.lisp` and nothing else, and a
Quicklisp anywhere else would install, load cicili, and then fail the build
with `Component "str" not found` — so against such a checkout the scripts
refuse it by name before Quicklisp is installed (update the checkout, or
make `~/quicklisp` a symlink to it). It must be an absolute path, and it
must stay exported for every later build too; the exports printed at the
end include it.

## Two things the scripts know that cost real time

**The compiler must exist before the first `make`.** Every project writes
its `<Project>-<OS>-<CXX>.depend` by running the compiler with `-MM` under
`@-`, so a make without a compiler still writes the file — with none of
the object rules — and, being newer than every source, it is never
regenerated: every later make says `No rule to make target home/obj/x.o`.
The scripts install the compiler first and drop any rule-less `.depend`
before building; `make clean` is the cure by hand.

**On Ubuntu 22.04 the apt clang is 14**, and `tools/cc/cxx` passes
`--gcc-install-dir`, which exists from clang 16 — so the Linux script takes
clang 18 from apt.llvm.org.

## What they do not do

They do not start the server, run the suite, or touch `home/data`.

`install-macos.sh` was run end to end on a fresh clone on a macOS 26 Intel
machine, under a HOME that had no Quicklisp: 14 libraries, 5 executables,
4 min 1 s. `install-linux.sh` was run end to end on an Ubuntu 22.04 Colab
VM through its `apt-get` branch, 2 min 16 s -- a VM whose packages the
same commands had installed earlier that day, so the apt step was
exercised but not from empty. The `dnf` branch has not been run. Say so in
an issue if either fails you, with the log it names.
