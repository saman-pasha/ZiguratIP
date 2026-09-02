# install/ — from a clone to a built `home/`, on one machine

Two scripts, one per operating system, and the part they share:

| file | what it is |
|---|---|
| `install-linux.sh` | Debian and Ubuntu through `apt-get`, Fedora and the Red Hat family through `dnf`: the packages, a clang 16+ (Fedora's own; on Ubuntu clang 18 from apt.llvm.org, because apt's is 14), then the common part |
| `install-macos.sh` | macOS: the Xcode command line tools for clang, Homebrew for `sbcl`, `libtool` and `openssl@3`, then the common part |
| `common.sh` | sourced by both: the Cicili checkout (found or cloned beside this one), the four Lisp systems Cicili is built from, `make MODE=Release`, and a check of the **artifacts** — fourteen libraries and three programs — because the top-level make steps over a failed project and its exit code proves nothing |
| `lisp/` | `sha1` and `base64`, the two Lisp systems Cicili depends on that are published nowhere; mirrors of cocolog's copies, see the README there |

```sh
sh install/install-linux.sh        # or install-macos.sh
```

Then put the three lines it prints — `CICILI`, `ZIGURATIP_HOME` and the
library path — in your shell profile, and `sh Test/run-e2e.sh` proves the
build against a live server.

## Knobs

* `NO_PACKAGES=1` skips the package step: no root, or already done.
* `CICILI=/path` names a Cicili checkout elsewhere; the default is
  `../cicili`, and it is cloned there when absent.
* `CICILI_CC=gcc CICILI_CXX=g++` builds with gcc and needs no clang at all
  (Linux). On Red Hat Enterprise Linux and its rebuilds, `sbcl` is in EPEL. The wrappers in `tools/cc` read exactly those two.
* `LOG=/path` moves the make log from `/tmp/ziguratip-install.log`.

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
clang 18 from apt.llvm.org, or you build with g++.

## What they do not do

They do not start the server, run the suite, or touch `home/data`.

`install-macos.sh` was run end to end on a fresh clone on a macOS 26 Intel
machine, under a HOME that had no Quicklisp: 14 libraries, 5 executables,
4 min 1 s. `install-linux.sh` was run end to end on an Ubuntu 22.04 Colab
VM through its `apt-get` branch, 2 min 16 s -- a VM whose packages the
same commands had installed earlier that day, so the apt step was
exercised but not from empty. The `dnf` branch has not been run. Say so in
an issue if either fails you, with the log it names.
