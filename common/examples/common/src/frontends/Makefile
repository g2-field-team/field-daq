#####################################################################
#
#  Name:         Makefile
#  Created by:   Matthias Smith
#
#  Contents:     Makefile for MIDAS NMR frontends, based on
#		 a sample Makefile written by Stefan Ritt.
#
#####################################################################

# The MIDASSYS should be defined prior the use of this Makefile
ifndef MIDASSYS
missmidas::
	@echo "...";
	@echo "Missing definition of environment variable 'MIDASSYS' !";
	@echo "...";
endif

# get OS type from shell
OSTYPE = $(shell uname)

#--------------------------------------------------------------------
# The following lines contain specific switches for different UNIX
# systems. Find the one which matches your OS and outcomment the 
# lines below.

#-----------------------------------------
# This is for Linux
ifeq ($(OSTYPE),Linux)
  OSTYPE = linux
endif

ifeq ($(OSTYPE),linux)
OS_DIR = linux
OSFLAGS = -DOS_LINUX -Dextname
CFLAGS = -g -O2 -Wall
# add to compile in 32-bit mode
# OSFLAGS += -m32
LIBS = -lm -lz -lutil -lnsl -lpthread -lrt
endif

#-----------------------
# MacOSX/Darwin is just a funny Linux
#
ifeq ($(OSTYPE),Darwin)
OSTYPE = darwin
endif

ifeq ($(OSTYPE),darwin)
OS_DIR = darwin
FF = cc
OSFLAGS = -DOS_LINUX -DOS_DARWIN -DHAVE_STRLCPY -DAbsoftUNIXFortran -fPIC -Wno-unused-function
LIBS = -lpthread -lz
SPECIFIC_OS_PRG = $(BIN_DIR)/mlxspeaker
NEED_STRLCPY=
NEED_RANLIB=1
NEED_SHLIB=
NEED_RPATH=

endif
#-----------------------------------------

OBJECTS = $(patsubst obj/%.cxx, build/%.o, $(wildcard obj/*.cxx))
TARGETS = $(patsubst src/%.cxx,bin/%,$(wildcard src/*.cxx))

#-------------------------------------------------------------------
CPPFLAGS = -fpermissive -O2
#CXXFLAGS = -std=c++0x
CXXFLAGS = -std=c++11

#-----------------------------------------
# ROOT flags and libs
#
ROOTFLAGS = $(shell root-config --cflags)
ROOTLIBS = $(shell root-config --libs)

#-------------------------------------------------------------------
# The following lines define direcories. Adjust if necessary
MID_INC = $(MIDASSYS)/include
MID_LIB = $(MIDASSYS)/$(OS_DIR)/lib

# Libraries && include flags
#LIBS += -L$(MID_LIB) -Lcore/lib `mysql_config --libs`
#LIBS += -lmidas-shared  -lmysqlclient -lsqlite3
#LIBS += -lboost_system -lboost_filesystem
#LIBS += -lm -lz -lpthread -lodbc
#CPPFLAGS += -I$(MID_INC) -Icore/include -Iobj -Iinclude `mysql_config --include`
#CPPFLAGS += -I/usr/local/include/labdaq 
LIBS += -L$(MID_LIB) -Lcore/lib -L/usr/lib64/mysql -L/usr/local/lib -L$(BOOST_LIB)
LIBS += -lmidas-shared  -lmysqlclient -lsqlite3
LIBS += -lboost_system -lboost_filesystem
LIBS += -lm -lz -lpthread -lodbc -lrt
CPPFLAGS += -I$(MID_INC) -Icore/include -Iobj -Iinclude
<<<<<<< HEAD
CPPFLAGS += -I/usr/local/opt/boost/
=======
#CPPFLAGS += -I/usr/local/opt/boost/
CPPFLAGS += -I$(BOOST_INC)
>>>>>>> 91009e6affa7cbf7298c46627f1845747a7f5616
CPPFLAGS += -I/usr/local/include/
CPPFLAGS += -Wl,-rpath,/usr/local/lib

# Set compilers
CC = gcc
CXX = g++
LINK_DIR = ../../../common

#-------------------------------------------------------------------
# Finally we have the actual make directives.

# Make commands

all: $(OBJECTS) $(TARGETS)

build/%.o: obj/%.cxx obj/%.hh
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ROOTFLAGS) -c $< -o $@

bin/%: src/%.cxx $(MID_LIB)/mfe.o $(OBJECTS)
	$(CXX) -o $@ $+ $(CPPFLAGS) $(CXXFLAGS) $(OSFLAGS) $(ROOTFLAGS) \
	$(LIBS) $(ROOTLIBS)
	ln -fs $(shell pwd)/$@ $(LINK_DIR)/$@

clean:
	rm -f *~ $(TARGETS) $(OBJECTS)
