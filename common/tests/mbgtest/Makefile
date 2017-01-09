# The following lines define direcories. Adjust if necessary
MID_INC = $(MIDASSYS)/include
MID_LIB = $(MIDASSYS)/linux/lib

# Libraries && include flags
FLAGS += -std=c++11 -I$(MID_INC) -I$(BOOST_INC)
FLAGS += -L$(MID_LIB) -L$(BOOST_LIB)
FLAGS += -lmidas-shared -lboost_system -lboost_filesystem

# Set compilers
CC = gcc
CXX = g++

all:
	$(CXX) -o mbg-timestamp-test mbg-timestamp-test.cxx $(FLAGS)