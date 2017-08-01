#!/bin/bash

# The script starts general midas utilites for the experiment.
source $(dirname $(readlink -f $0))/../../common/.expt-env

host=$1
fe=$2

scname="${EXPT}.${fe//_/-}"
fename="${EXPT_DIR}/common/bin/${fe}"
echo Starting $fe on $host
cmd="screen -S $scname -p 0 -rX stuff \'!\';"
ssh $host "$cmd"
sleep 1
cmd="screen -S $scname -p 0 -X quit;"
ssh $host "$cmd"
sleep 0.5
cmd="screen -dmS $scname"
ssh $host "$cmd"
cmd1="screen -S $scname -p 0 -rX stuff"
cmd2="${fename} -e $EXPT -h $EXPT_IP_L$(printf \\r)"
cmd3="cd /home/newg2/Applications/field-daq/common/src/frontends/bin $(printf \\r)"
cmd="$cmd1 \"$cmd3\""
ssh $host "$cmd"
cmd="$cmd1 \"$cmd2\""
ssh $host "$cmd"

# end script
