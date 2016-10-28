#!/bin/bash
# The script starts general midas utilites for gm2-nmr experiment.

source $(dirname $(readlink -f $0))/../../common/.expt-env

# Restart the daq components for the basic_vme setup.
odbedit -e $EXPT -c clean 
mserver -a $EXPT_IP -a localhost -p $MSERVER_PORT -D
mhttpd  -e $EXPT -a $EXPT_IP -a localhost -p $MHTTPD_PORT -D
mlogger -e $EXPT -D


# end script