
PROJECT := Library
TYPE := Library

HEADERS := libraryloader.h \
	librarypool.h

SOURCES := libraryloader.cpp \
	librarypool.cpp

ifeq ($(shell uname),Linux)
	LIBS := -ldl
endif 


include ../Makefile.global
