
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

LIBS := -lCore -lStreamIO -lThreading

ifeq ($(OS),Windows_NT)
	LIBS := $(LIBS) -lws2_32
else ifeq ($(shell uname),SunOS)
	LIBS := $(LIBS) -lsocket -lnsl
endif

include ../Makefile.global

