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

ROOTFLAGS = $(shell root-config --cflags)
ROOTLIBS = $(shell root-config --libs)

#----------------------------------------n---------------------------
# The following lines define direcories. Adjust if necessary
MID_INC = $(MIDASSYS)/include
MID_LIB = $(MIDASSYS)/linux/lib

# Libraries && include flags
LIBS += -L$(MID_LIB) -Lcore/lib -L/usr/lib64/mysql -L/usr/local/lib 
LIBS += -L/usr/lib -L$(BOOST_LIB)
LIBS += -lmidas-shared  -lmysqlclient -lsqlite3
LIBS += -lboost_system -lboost_filesystem
LIBS += -lm -lz -lpthread -lodbc -lrt
LIBS += -lgclib -lgclibo
LIBS += -ltrolleyinterface -lg2fieldvme -lfid
CPPFLAGS += -I$(MID_INC) -Icore/include -Iobj -Iinclude -I/usr/include
CPPFLAGS += -I$(BOOST_INC) -I$(shell echo $(ZMQ_INCLUDE_DIR))
CPPFLAGS += -I/usr/local/include/ -I/usr/local/include/g2field
CPPFLAGS += -Wl,-rpath,/usr/local/lib

# Set compilers
CC = gcc
CXX = g++

#-------------------------------------------------------------------
# Finally we have the actual make directives.

# Make commands

all: $(OBJECTS) $(TARGETS)

build/%.o: obj/%.cxx obj/%.hh
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ROOTFLAGS) -c $< -o $@

bin/%: src/%.cxx $(MID_LIB)/mfe.o $(OBJECTS)
	$(CXX) -o $@ $+ $(CPPFLAGS) $(CXXFLAGS) $(ROOTFLAGS) \
	$(LIBS) $(ROOTLIBS)
	ln -s $(shell pwd)/$@ ../../bin/

#-------------------------------------------------------------------
#Hear belongs to Ran Hong
RHLIBS += -L$(MID_LIB) -Lcore/lib -L/usr/lib64/mysql -L/usr/local/lib 
RHLIBS += -L/usr/lib -L$(BOOST_LIB)
RHLIBS += -lmidas-shared  -lmysqlclient -lsqlite3
RHLIBS += -lboost_system -lboost_filesystem
RHLIBS += -lm -lz -lpthread -lodbc -lrt
RHLIBS += -lgclib -lgclibo -ltrolleyinterface
RHCPPFLAGS = -fpermissive -O2
RHCPPFLAGS += -I$(MID_INC) -Icore/include -Iinclude -I/usr/include
RHCPPFLAGS += -I$(BOOST_INC) -I$(shell echo $(ZMQ_INCLUDE_DIR))
RHCPPFLAGS += -I/usr/local/include/ -I/usr/local/include/g2field
RHCPPFLAGS += -Wl,-rpath,/usr/local/lib

allRH: bin/gm2TrolleyFe bin/gm2TrolleyFeSim bin/gm2GalilFe
bin/gm2TrolleyFe: src/gm2TrolleyFe.cxx $(MID_LIB)/mfe.o
	$(CXX) -o $@ $+ $(RHCPPFLAGS) $(CXXFLAGS) $(ROOTFLAGS) \
	        $(RHLIBS) $(ROOTLIBS)

bin/gm2TrolleyFeSim: src/gm2TrolleyFeSim.cxx $(MID_LIB)/mfe.o
	$(CXX) -o $@ $+ $(RHCPPFLAGS) $(CXXFLAGS) $(ROOTFLAGS) \
	$(RHLIBS) $(ROOTLIBS)

bin/gm2GalilFe: src/gm2GalilFe.cxx $(MID_LIB)/mfe.o
	$(CXX) -o $@ $+ $(RHCPPFLAGS) $(CXXFLAGS) $(ROOTFLAGS) \
	        $(RHLIBS) $(ROOTLIBS)

#-------------------------------------------------------------------
clean:
	rm -f *~ $(TARGETS) $(OBJECTS)
