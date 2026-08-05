
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

PARSI_FILES := session.parsi \
	isolationlevel.parsi \
	resulttype.parsi \
	connector.parsi \
	htmldrawer.parsi \
	compilerdrawer.parsi \
	compiler.parsi \
	serializer.parsi \
	rpc.parsi \
	mem.parsi

LIB_FILES := $(patsubst %.parsi,$(OUT_DIR)/$(PRE_LIB)$(call lc,%)$(POST_LIB),$(PARSI_FILES))

.PHONY: all

all: $(LIB_FILES)
	@echo [System] "******* all done *******"

$(OUT_DIR)/$(PRE_LIB)$(call lc,%)$(POST_LIB): %.parsi
	$(BIN_DIR)/parsi $<
