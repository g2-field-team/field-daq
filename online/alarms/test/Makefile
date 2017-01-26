#####################################################################
#
#  Name:         Makefile
#  Created by:   Matthias Smith
#
#  Contents:     Makefile for MIDAS NMR frontends, based on
#		 a sample Makefile written by Stefan Ritt.
#
#####################################################################

OBJECTS = $(patsubst obj/%.cxx, build/%.o, $(wildcard obj/*.cxx))
TARGETS = $(patsubst src/%.cxx,bin/%,$(wildcard src/*.cxx))

#-------------------------------------------------------------------
# This is for Linux
CPPFLAGS = -fpermissive -O2
CXXFLAGS = -std=c++0x

#----------------------------------------n---------------------------
# The following lines define direcories. Adjust if necessary
MID_INC = $(MIDASSYS)/include
MID_LIB = $(MIDASSYS)/linux/lib

# Libraries && include flags
LIBS += -L$(MID_LIB) -Lcore/lib -L/usr/lib64/mysql -L/usr/local/lib 
LIBS += -L/usr/lib -L$(BOOST_LIB)
LIBS += -lmidas-shared  -lmysqlclient -lsqlite3
LIBS += -lm -lz -lpthread -lodbc -lrt
CPPFLAGS += -I$(MID_INC) -Icore/include -Iobj -Iinclude -I/usr/include

# Set compilers
CC = gcc
CXX = g++

#-------------------------------------------------------------------
# Finally we have the actual make directives.

# Make commands

all: $(TARGETS)

bin/%: src/%.cxx
	$(CXX) -o $@ $+ $(CPPFLAGS) $(CXXFLAGS) $(LIBS)
	ln -sf $(shell pwd)/$@ ../../bin/

clean:
	rm -f *~ $(TARGETS)
