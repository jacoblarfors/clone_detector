# Makefile for clone detector

# Program name
PROG_NAME=main

# the C++ compiler
CXX     = g++

# options to pass to compiler
CXX_FLAGS=-g -rtti
CXX_INCL=
CXX_LINK=


all:
	$(CXX) -c $(CXX_FLAGS) $(CXX_INCL) $(CXX_LINK) tinyxml.cpp
	$(CXX) -c $(CXX_FLAGS) $(CXX_INCL) $(CXX_LINK) tinystr.cpp
	$(CXX) -c $(CXX_FLAGS) $(CXX_INCL) $(CXX_LINK) tinyxmlparser.cpp
	$(CXX) -c $(CXX_FLAGS) $(CXX_INCL) $(CXX_LINK) tinyxmlerror.cpp

clean:
	rm -f *.o $(PROG_NAME)
