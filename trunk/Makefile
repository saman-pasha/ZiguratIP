
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
	MVCCS \
	Compiler \
	ca \
	parsi \
	ziguratip \
	Test

#export COMPILER := clang++
export MODE := Debug

ifeq ($(OS),Windows_NT)
	SEP := &
else
	SEP := ;
endif

.PHONY: all clean

all:
	@- $(foreach project,$(PROJECTS), \
		$(MAKE) -C $(CURDIR)/$(project)/ $(SEP)\
	)
	@echo [Workspace] "******* all done *******"

clean:
	@- $(foreach project,$(PROJECTS), \
		$(MAKE) -C $(CURDIR)/$(project)/ clean $(SEP)\
	)
	@echo [Workspace] "******* clean done *******"
