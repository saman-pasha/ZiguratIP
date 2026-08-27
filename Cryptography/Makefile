
PROJECT := Cryptography
TYPE := Library

# AES rounds, the SHA cores and RSA's modular arithmetic run in the innermost loops of
# everything, so this library is optimised even in a debug build. Unrolling
# is what the word-at-a-time loops want; there is deliberately no
# -ffast-math, which only relaxes floating point and this code is integer
# throughout.
OPTIMIZE := -O3 -funroll-loops

HEADERS := shahelper.hpp \
	aes.hpp \
	rsa.hpp \
	certificateexception.hpp \
	x509.hpp

SOURCES := shahelper.cpp \
	aes.cpp \
	rsa.cpp \
	x509.cpp

# x509.cpp stands on DER and CTE from Encoding, BigInt from Core,
# binarystream from StreamIO and Configuration for the CA settings it
# reads. Naming them here rather than leaving them to whoever links this
# library last: Mach-O was told to look symbols up dynamically, so the
# omission never showed on the machine this was written on, while an ELF
# executable refuses to link at all unless -lEncoding happens to follow
# -lCryptography on its command line.
#
# -lConfiguration WAS THE ONE STILL MISSING, and it took a change of
# compiler to find it. g++ let every consumer through -- ld resolves a
# shared library's leftovers from whatever else is on the command line,
# and something always was -- while clang++ reported the truth:
#
#   libCryptography.so: undefined reference to `Zigurat::Option::~Option()'
#
# from MVCCS-cicili's contention test, which links no Configuration and
# has no reason to. The library that calls it is the library that names
# it; the fix is one word here rather than one word in every consumer.
LIBS := -lCore -lStreamIO -lEncoding -lConfiguration -lcrypto

include ../Makefile.global

