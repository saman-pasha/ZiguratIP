
PROJECTS := Core \
	StreamIO \
	Type \
	Library \
	Encoding \
	Compression \
	Cryptography \
	Configuration \
	Threading \
	SocketIO \
	Connector \
	HTTP \
	MVCCS-cicili \
	Compiler \
	ca \
	parsi \
	parsic \
	ziguratip \
	Test \
	System

# CLANG, AND SAID OUT LOUD. This was a commented-out line for as long as
# the file existed, which meant the workspace built with whatever `g++'
# happened to be -- and cocolog, which links libCore into its own binary,
# built with something else again. One toolchain across the three
# repositories is the point; `make COMPILER=g++' still works.
#
# IT NAMES A WRAPPER, not clang++ itself, because clang borrows libstdc++
# from the newest gcc it can find and on Ubuntu 24.04 that is gcc-14's
# runtime directory -- which ships no C++ headers, so a bare clang++ dies
# on `#include <string>' naming a header that is plainly installed.
# tools/cc/cxx works out which gcc install dir actually has a header set
# and passes --gcc-install-dir; tools/cc/README has the rest.
#
# A BARE NAME ON PATH, not a path. Makefile.global names its dependency
# file `$(PROJECT)-$(OS)-$(CXX).depend', so a COMPILER with slashes in it
# asks make to write `Core-Linux-/home/.../cxx.depend' -- which silently
# produces no rules, and every project then dies at
#
#     No rule to make target '../home/obj/zexception.o'
#
# with nothing whatever to say about compilers. PATH carries the location;
# COMPILER carries the name.
export PATH := $(CURDIR)/tools/cc:$(PATH)
export COMPILER := cxx
export MODE := Release

ifeq ($(OS),Windows_NT)
	SEP := &
else
	SEP := ;
endif

.PHONY: all clean headers

all: headers
	@- $(foreach project,$(PROJECTS), \
		$(MAKE) -C $(CURDIR)/$(project)/ $(SEP)\
	)
	@echo [Workspace] "******* all done *******"

# Every public header goes to home/include before anything is compiled, so a
# clean checkout builds without needing a previously populated include tree.
headers:
	@- $(foreach project,$(PROJECTS), \
		$(MAKE) -C $(CURDIR)/$(project)/ headers $(SEP)\
	)
	@echo [Workspace] "******* headers done *******"

clean:
	@- $(foreach project,$(PROJECTS), \
		$(MAKE) -C $(CURDIR)/$(project)/ clean $(SEP)\
	)
	@echo [Workspace] "******* clean done *******"
