#!/bin/bash

# The script restarts general the mserver for the experiment.
source $(dirname $(readlink -f $0))/../../common/.expt-env

# First kill the mserver if it is running.
for session in $(screen -ls | grep -o "[0-9]*\.${EXPT}.mserver")
do 
  screen -S "${session}" -p 0 -rX stuff $'\003'
  sleep 2
  screen -S "${session}" -p 0 -X quit
done

# Now start the mloggger in a screen.
cmd="mserver -p $MSERVER_PORT -e $EXPT $(printf \\r)"
screen -dmS "${EXPT}.mserver"
screen -S "${EXPT}.mserver" -p 0 -rX stuff "$cmd"

unset cmd

# end script

