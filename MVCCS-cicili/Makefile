# The Cicili MVCCS engine's place in the workspace build. The real work
# is build.sh -- the transpiler needs sbcl and a cicili checkout (set
# CICILI to point at one) -- so `make` here hands off to it, and
# `make headers` publishes the two headers every consumer compiles
# against, the same contract every other project's headers target keeps.

PROJECT := MVCCS-cicili

.PHONY: all headers clean

all:
	@sh build.sh
	@echo [Project: MVCCS-cicili] "******* all done *******"

headers:
	@mkdir -p ../home/include
	@cp engine.hpp engine-compat.hpp ../home/include/

clean:
	@rm -f consumer_test contention_test carryover_new mvccs_test schema_test
	@rm -f *.o *.lo
	@rm -rf .libs
