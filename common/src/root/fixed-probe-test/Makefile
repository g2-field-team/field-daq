#####################################################################
#
#  Name:         Makefile
#  Created by:   Matthias Smith
#
#  Contents:     Makefile for fixed probe test program.
#		
#####################################################################

# Gather source files.
OBJECTS = $(patsubst src/%.cxx, build/%.o, $(wildcard src/*.cxx))
TARGETS = $(patsubst main/%.cxx,bin/%,$(wildcard main/*.cxx))

# Set complier + flags
CXX = g++ -O2 -fpermissive
CC = gcc -O2

ifdef DEBUG
	CXX += -g
	CC += -g
endif

# Add standard paths
LIBS += -L/usr/lib -L/usr/local/lib 
CPPFLAGS += -I/usr/include -I/usr/local/include -Isrc
CPPFLAGS += -Wl,-rpath,/usr/local/lib

# ROOT paths and flags
ROOTFLAGS = $(shell root-config --cflags)
ROOTLIBS = $(shell root-config --libs)

# Add boost (checking for UPS boost)
ifdef BOOST_LIB
	LIBS += -L$(BOOST_LIB)
	CPPFLAGS += -I$(BOOST_INC)
endif

# Add system linker flags
LIBS += -lm -lz -lpthread -lodbc -lrt

# And boost linker flags
LIBS += -lboost_system -lboost_filesystem

# Projects linker flags
LIBS += -lg2fieldvme -lfid


# Make directives
all: $(OBJECTS) $(TARGETS)

build/%.o: src/%.cxx src/%.hh
	$(CXX) $(CPPFLAGS) $(ROOTFLAGS) -c $< -o $@

bin/%: main/%.cxx $(OBJECTS)
	$(CXX) -o $@ $+ $(CPPFLAGS) $(ROOTFLAGS) $(LIBS) $(ROOTLIBS)

clean:
	rm -f *~ $(TARGETS) $(OBJECTS)
