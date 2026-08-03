
PROJECT := Core
TYPE := Library

# BigInt's schoolbook multiply and modular exponentiation run in the innermost loops of
# everything, so this library is optimised even in a debug build. Unrolling
# is what the word-at-a-time loops want; there is deliberately no
# -ffast-math, which only relaxes floating point and this code is integer
# throughout.
OPTIMIZE := -O3 -funroll-loops

HEADERS := zexception.hpp \
	utility.hpp \
	array.hpp \
	polynomial.hpp \
	bigint.hpp

SOURCES := zexception.cpp \
	utility.cpp \
	array.cpp \
	polynomial.cpp \
	bigint.cpp

LIBS :=

include ../Makefile.global
