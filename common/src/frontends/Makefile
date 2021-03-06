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
LIBS += -L$(ZMQ_LIB_DIR)
LIBS += -lmidas-shared  -lmysqlclient -lsqlite3
LIBS += -lboost_system -lboost_filesystem
LIBS += -lm -lz -lpthread -lodbc -lrt
LIBS += -lgclib -lgclibo
LIBS += -lzmq
LIBS += -ltrolleyinterface -lsg382interface -lg2fieldvme -lfid
CPPFLAGS += -I$(MID_INC) -Icore/include -Iobj -Iinclude -I/usr/include
CPPFLAGS += -I$(BOOST_INC) -I$(shell echo $(ZMQ_INCLUDE_DIR))
CPPFLAGS += -I/usr/local/include/ -I/usr/local/include/g2field
CPPFLAGS += -Wl,-rpath,/usr/local/lib
CPPFLAGS += -I$(ZMQ_INC)

BUILD_DIR = ./build

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
	ln -sf $(shell pwd)/$@ ../../bin/

#-------------------------------------------------------------------
#Hear belongs to Ran Hong
RHLIBS += -L$(MID_LIB) -Lcore/lib -L/usr/lib64/mysql -L/usr/local/lib 
RHLIBS += -L/usr/lib -L$(BOOST_LIB) -L$(PQXX_LIB)
RHLIBS += -lmidas-shared  -lmysqlclient -lsqlite3
RHLIBS += -lboost_system -lboost_filesystem -lpqxx
RHLIBS += -lm -lz -lpthread -lodbc -lrt
RHLIBS += -lgclib -lgclibo -ltrolleyinterface -lsg382interface
RHCPPFLAGS = -fpermissive -O2
RHCPPFLAGS += -I$(MID_INC) -Icore/include -Iinclude -I/usr/include
RHCPPFLAGS += -I$(BOOST_INC) -I$(shell echo $(ZMQ_INCLUDE_DIR)) -I$(PQXX_INC)
RHCPPFLAGS += -I/usr/local/include/ -I/usr/local/include/g2field
RHCPPFLAGS += -Wl,-rpath,/usr/local/lib

# these look the same as Ran's, but let's copy them over here just in case I want to change it later. 
DFLIBS += -L$(MID_LIB) -Lcore/lib -L/usr/lib64/mysql -L/usr/local/lib
DFLIBS += -L/usr/lib -L$(BOOST_LIB)
DFLIBS += -lmidas-shared  -lmysqlclient -lsqlite3
DFLIBS += -lboost_system -lboost_filesystem
DFLIBS += -lm -lz -lpthread -lodbc -lrt
DFLIBS += -lyokogawainterface
DFCPPFLAGS = -fpermissive -O2
DFCPPFLAGS += -I$(MID_INC) -Icore/include -Iinclude -Iobj -I/usr/include
DFCPPFLAGS += -I$(BOOST_INC) -I$(shell echo $(ZMQ_INCLUDE_DIR))
DFCPPFLAGS += -I/usr/local/include/ -I/usr/local/include/g2field
DFCPPFLAGS += -Wl,-rpath,/usr/local/lib

# Alec's version, copied and altered from Ran's. Added Nation Instruments driver.

ATBLIBS += -L$(MID_LIB) -Lcore/lib -L/usr/lib64/mysql -L/usr/local/lib 
ATBLIBS += -L/usr/lib -L$(BOOST_LIB)
ATBLIBS += -lmidas-shared  -lmysqlclient -lsqlite3
ATBLIBS += -lboost_system -lboost_filesystem
ATBLIBS += -lm -lz -lpthread -lodbc -lrt
ATBLIBS += -lgclib -lgclibo -ltrolleyinterface -lsg382interface
#ATBLIBS += -L/usr/local/natinst/nidaqmxbase/include -L/usr/local/natinst/nidaqmxbase/lib
ATBLIBS += -lnidaqmxbase
ATBCPPFLAGS = -fpermissive -O2
ATBCPPFLAGS = -m64 #this is for DAQmxbase
ATBCPPFLAGS += -I$(MID_INC) -Icore/include -Iinclude -I/usr/include
ATBCPPFLAGS += -I$(BOOST_INC) -I$(shell echo $(ZMQ_INCLUDE_DIR))
ATBCPPFLAGS += -I/usr/local/include/ -I/usr/local/include/g2field
ATBCPPFLAGS += -Wl,-rpath,/usr/local/lib

allRH: bin/trolley bin/sim_trolley bin/galil-fermi bin/abs-probe bin/galil-platform
allDF: bin/yokogawa
allATB: bin/fluxgate

bin/trolley: src/trolley.cxx $(MID_LIB)/mfe.o
	$(CXX) -o $@ $+ $(RHCPPFLAGS) $(CXXFLAGS) $(ROOTFLAGS) \
	        $(RHLIBS) $(ROOTLIBS)
	ln -sf $(shell pwd)/$@ ../../bin/

bin/sim_trolley: src/sim_trolley.cxx $(MID_LIB)/mfe.o
	$(CXX) -o $@ $+ $(RHCPPFLAGS) $(CXXFLAGS) $(ROOTFLAGS) \
	$(RHLIBS) $(ROOTLIBS)
	ln -sf $(shell pwd)/$@ ../../bin/

bin/galil-fermi: src/galil-fermi.cxx $(MID_LIB)/mfe.o
	$(CXX) -o $@ $+ $(RHCPPFLAGS) $(CXXFLAGS) $(ROOTFLAGS) \
	        $(RHLIBS) $(ROOTLIBS)
	ln -sf $(shell pwd)/$@ ../../bin/

bin/sim_galil: src/sim_galil.cxx $(MID_LIB)/mfe.o
	$(CXX) -o $@ $+ $(RHCPPFLAGS) $(CXXFLAGS) $(ROOTFLAGS) \
	        $(RHLIBS) $(ROOTLIBS)
	ln -sf $(shell pwd)/$@ ../../bin/

bin/abs-probe: src/abs-probe.cxx $(MID_LIB)/mfe.o
	$(CXX) -o $@ $+ $(RHCPPFLAGS) $(CXXFLAGS) $(ROOTFLAGS) \
		$(RHLIBS) $(ROOTLIBS)
	ln -sf $(shell pwd)/$@ ../../bin/

bin/galil-platform: src/galil-platform.cxx $(MID_LIB)/mfe.o
	$(CXX) -o $@ $+ $(RHCPPFLAGS) $(CXXFLAGS) $(ROOTFLAGS) \
	        $(RHLIBS) $(ROOTLIBS)
	ln -sf $(shell pwd)/$@ ../../bin/

bin/yokogawa: src/yokogawa.cxx $(MID_LIB)/mfe.o $(BUILD_DIR)/PID.o
	$(CXX) -o $@ $+ $(DFCPPFLAGS) $(CXXFLAGS) $(ROOTFLAGS) \
		$(DFLIBS) $(ROOTLIBS)

bin/fluxgate: src/fluxgate.cxx $(MID_LIB)/mfe.o
	$(CXX) -o $@ $+ $(ATBCPPFLAGS) $(CXXFLAGS) $(ROOTFLAGS) \
		$(ATBLIBS) $(ROOTLIBS)


#-------------------------------------------------------------------
clean:
	rm -f *~ $(TARGETS) $(OBJECTS)
