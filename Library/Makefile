
PROJECT := Library
TYPE := Library

HEADERS := libraryloader.hpp \
	librarypool.hpp

SOURCES := libraryloader.cpp \
	librarypool.cpp

ifeq ($(shell uname),Linux)
	LIBS := -ldl
endif 


include ../Makefile.global
