
PROJECT := Cryptography
TYPE := Library

# AES rounds, the SHA cores and RSA's modular arithmetic run in the innermost loops of
# everything, so this library is optimised even in a debug build. Unrolling
# is what the word-at-a-time loops want; there is deliberately no
# -ffast-math, which only relaxes floating point and this code is integer
# throughout.
OPTIMIZE := -O3 -funroll-loops

HEADERS := sha-private.hpp \
	sha.hpp \
	shahelper.hpp \
	aes.hpp \
	rsa.hpp \
	certificateexception.hpp \
	x509.hpp

SOURCES := sha1.cpp \
	sha224-256.cpp \
	sha384-512.cpp \
	usha.cpp \
	hmac.cpp \
	hkdf.cpp \
	shahelper.cpp \
	aes.cpp \
	rsa.cpp \
	x509.cpp

include ../Makefile.global

