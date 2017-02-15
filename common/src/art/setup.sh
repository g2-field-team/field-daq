#!/bin/bash

# A setup script for field art modules.

# Make sure CervVM-FS is setup already.
if [ ! -d "/cvmfs/gm2.opensciencegrid.org" ]; then
    exit 1
fi;

# Switch to the scripts directory.
cd $(dirname $(readlink -f $0))

# Now proceed with setting up gm2 packages via UPS.
source /cvmfs/gm2.opensciencegrid.org/prod7/g-2/setup
setup gm2 v7_02_00 -q prof

# And initialize a new development space.
mrb newDev -f
source localProducts*/setup
cd $MRB_SOURCE

mrb g -b feature/tier0v7-field gm2dataproducts
mrb g -b feature/tier0v7-field gm2midastoart
mrb g -b feature/tier0v7-field gm2unpackers
mrb g -b feature/tier0v7-field gm2dqm
mrb g -b develop gm2field
mrb g -b develop gm2util
mrb g -b develop gm2geom

# MIDAS (create UPS product stub)
GM2MIDAS_DIR=/home/newg2/Packages/gm2midas
product-stub gm2midas v0_00_01 $GM2MIDAS_DIR $MRB_INSTALL

# End script