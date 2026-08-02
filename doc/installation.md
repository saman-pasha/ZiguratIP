# Installation

Build from source with `make` at the repository root — see the
[project README](../README.md). Everything installs into `home`, which is both
the install prefix and the runtime home directory, holding the data, headers,
configuration, temporaries, binaries and libraries.

The `ziguratip` process has to be told where that directory is, so set
`ZIGURATIP_HOME` to it:

```bash
export ZIGURATIP_HOME=/path/to/ZiguratIP/home
```

Put the binaries and libraries on the search paths as well:

```bash
export PATH=$ZIGURATIP_HOME/bin:$PATH
export LD_LIBRARY_PATH=$ZIGURATIP_HOME/lib:$ZIGURATIP_HOME/ld:$LD_LIBRARY_PATH
# macOS uses DYLD_LIBRARY_PATH in place of LD_LIBRARY_PATH
```

Configure the platform before running it. In particular the server invokes a
C++ compiler at run time to build Parsi objects, so if one is not on `PATH`
then `COMPILER/CPP` has to name it. See the
[configuration reference](configuration.md); `home/etc/ziguratip.conf`
documents every setting inline.
