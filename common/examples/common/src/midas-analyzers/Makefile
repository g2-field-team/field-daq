#####################################################################
#
#  Name:         Makefile
#  Created by:   Matthias Smith
#
#  Contents:     Makefile for MIDAS NMR frontends, based on
#		 a sample Makefile written by Stefan Ritt.
#
#####################################################################

# The following lines define direcories. Adjust if necessary
INC_DIR = $(MIDASSYS)/include
LIB_DIR = $(MIDASSYS)/linux/lib
BIN_DIR = bin
LINK_DIR = ~/Applications/field-daq/common/examples/common

# Grab our compilation objects.
SOURCES = $(wildcard modules/*.cxx)
OBJECTS := $(patsubst modules%.cxx,build%.o,$(SOURCES))
SOURCES = $(wildcard modules/*.c)
OBJECTS += $(patsubst modules%.c,build%.o,$(SOURCES))
TARGETS = $(patsubst src/%.c,bin/%,$(wildcard src/*.c))
TARGETS += $(patsubst src/%.cxx,bin/%,$(wildcard src/*.cxx))

#-----------------------------------------
# This is for Linux
OSFLAGS = -DOS_LINUX -DHAVE_USB -DHAVE_LIBUSB10 -Dextname
CFLAGS = -fpermissive -O2
CXXFLAGS = -std=c++0x
LIBS = -lm -lz -lpthread -lfftw3 -lcurl -lodbc

ROOTFLAGS = $(shell root-config --cflags)
ROOTLIBS = $(shell root-config --libs)

#-------------------------------------------------------------------

# Libraries && include flags
LIB = -L$(LIB_DIR) -Lpkg/lib -L/usr/lib64/mysql
LIB += -lmidas-shared -lmysqlclient -lsqlite3 -lboost_system
CFLAGS += -I$(INC_DIR) -Icore/include -Iinclude -Wl,-rpath,/usr/local/lib

# Set compilers
CC = gcc
CXX = g++

#-------------------------------------------------------------------
# Finally we have the actual make directives.

.SECONDARY: $(OBJECTS)

.PHONY: print_vars

# Make commands

all: $(TARGETS)

print_vars:
	echo $(TARGETS)
	echo $(OBJECTS)

bin/%: src/%.cxx $(LIB_DIR)/rmana.o $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(CFLAGS) $(OSFLAGS) $(ROOTFLAGS) -o $@ $+ \
	$(LIB) $(LIBS) $(ROOTLIBS)
	ln -fs $(shell pwd)/$@ $(LINK_DIR)/$@

bin/%: src/%.c $(LIB_DIR)/mana.o $(OBJECTS)
	$(CC) $(CFLAGS) $(OSFLAGS) -o $@ $+ \
	$(LIB) $(LIBS) $(ROOTLIBS)
	ln -fs $(shell pwd)/$@ $(LINK_DIR)/$@

build/%.o: modules/%.cxx
	$(CXX) $(CXXFLAGS) $(CFLAGS) $(OSFLAGS) $(ROOTFLAGS) -c $< -o $@

build/%.o: modules/%.c
	$(CC) $(CFLAGS) $(OSFLAGS) -c $< -o $@

clean:
	rm -f *~ $(OBJECTS) $(TARGETS)
