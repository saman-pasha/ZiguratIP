
PROJECT := Library
TYPE := Library

HEADERS := libraryloader.hpp \
	librarypool.hpp \
	modelpool.hpp

SOURCES := libraryloader.cpp \
	librarypool.cpp \
	modelpool.cpp

ifeq ($(shell uname),Linux)
	LIBS := -ldl
endif 


include ../Makefile.global
