
print-%  : ; @echo $* = $($*)

BIN_DIR := ../home/bin
OUT_DIR := ../home/ld

ifeq ($(OS),Windows_NT)
	PRE_LIB := _
	POST_LIB := _.dll
else
	PRE_LIB := lib_
	POST_LIB := _.so
endif

lc = $(subst A,a,$(subst B,b,$(subst C,c,$(subst D,d,$(subst E,e,$(subst F,f,$(subst G,g,$(subst H,h,$(subst I,i,$(subst J,j,$(subst K,k,$(subst L,l,$(subst M,m,$(subst N,n,$(subst O,o,$(subst P,p,$(subst Q,q,$(subst R,r,$(subst S,s,$(subst T,t,$(subst U,u,$(subst V,v,$(subst W,w,$(subst X,x,$(subst Y,y,$(subst Z,z,$1))))))))))))))))))))))))))

# session.parsi is gone: the session store is HTTP/session.cpp, and keeping a
# second class of the same name only gave pages a wrong one to pick.
PARSI_FILES := isolationlevel.parsi \
	resulttype.parsi \
	connector.parsi \
	htmldrawer.parsi \
	compilerdrawer.parsi \
	compiler.parsi \
	serializer.parsi \
	catalog.parsi \
	catalog_pages.parsi \
	rpc.parsi \
	mem.parsi

LIB_FILES := $(patsubst %.parsi,$(OUT_DIR)/$(PRE_LIB)$(call lc,%)$(POST_LIB),$(PARSI_FILES))

# parsi reads its configuration from here, and these pages are compiled by
# running it. A plain make from a fresh shell has no ZIGURATIP_HOME set, so the
# build supplies the one it is building into rather than failing on a path it
# could have worked out.
export ZIGURATIP_HOME ?= $(abspath $(CURDIR)/../home)
export LD_LIBRARY_PATH := $(ZIGURATIP_HOME)/lib:$(LD_LIBRARY_PATH)

.PHONY: all headers clean

all: $(LIB_FILES)
	@echo [System] "******* all done *******"

# Nothing here publishes a C++ header; the target exists so the workspace can
# call it uniformly.
headers:

clean:
	@- rm -f $(LIB_FILES)
	@- rm -f $(patsubst %.parsi,$(OUT_DIR)/_$(call lc,%)_.hpp,$(PARSI_FILES))

$(OUT_DIR)/$(PRE_LIB)$(call lc,%)$(POST_LIB): %.parsi
	$(BIN_DIR)/parsi $<
