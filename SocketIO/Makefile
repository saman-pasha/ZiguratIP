
PROJECT := SocketIO
TYPE := Library

HEADERS := socketioexception.hpp \
	socket.hpp \
	socketbuf.hpp \
	tcpbuf.hpp \
	tcpstream.hpp \
	tcpserver.hpp \
	ipcbuf.hpp \
	ipcstream.hpp \
	ipcserver.hpp \
	tlsexception.hpp \
	tls.hpp \
	tlsbuf.hpp \
	tlsstream.hpp \
	tlsserver.hpp

SOURCES := socket.cpp \
	socketbuf.cpp \
	tcpbuf.cpp \
	tcpstream.cpp \
	tcpserver.cpp \
	ipcbuf.cpp \
	ipcstream.cpp \
	ipcserver.cpp \
	tls.cpp \
	tlsbuf.cpp \
	tlsstream.cpp \
	tlsserver.cpp

# tlsbuf.cpp reaches for AES, SHA and X509 out of Cryptography, DER out of
# Encoding and ZLib out of Compression. None of them were named here: Mach-O
# resolves them dynamically, so the gap only surfaces as an undefined symbol in
# whichever ELF executable links this library.
LIBS := -lCore -lStreamIO -lThreading -lCryptography -lEncoding -lCompression -lssl -lcrypto

ifeq ($(OS),Windows_NT)
	LIBS := $(LIBS) -lws2_32
else ifeq ($(shell uname),SunOS)
	LIBS := $(LIBS) -lsocket -lnsl
endif

include ../Makefile.global

