
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

#export COMPILER := clang++
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
