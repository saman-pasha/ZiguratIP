
PROJECT := SocketIO
TYPE := Library

HEADERS := socketioexception.h \
	socket.h \
	socketbuf.h \
	tcpbuf.h \
	tcpstream.h \
	tcpserver.h \
	ipcbuf.h \
	ipcstream.h \
	ipcserver.h \
	tls.h \
	tlsbuf.h \
	tlsstream.h

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
	tlsstream.cpp

LIBS := -lCore -lStreamIO -lThreading

ifeq ($(OS),Windows_NT)
	LIBS := $(LIBS) -lws2_32
else ifeq ($(shell uname),SunOS)
	LIBS := $(LIBS) -lsocket -lnsl
endif

include ../Makefile.global

