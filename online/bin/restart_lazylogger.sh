#!/bin/bash

# The script starts general midas utilites for the experiment.
source $(dirname $(readlink -f $0))/../../common/.expt-env

LoggerName=$1

scname="${EXPT}.LazyLogger-${LoggerName}"

screen -S "$scname" -p 0 -rX stuff \'!\'
sleep 0.05
screen -S "$scname" -p 0 -X quit

screen -dmS $scname
sleep 0.05
screen -S $scname -p 0 -rX stuff "lazylogger -c $LoggerName $(printf \\r)"

# end script
