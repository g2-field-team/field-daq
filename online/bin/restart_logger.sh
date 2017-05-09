#!/bin/bash

# The script restarts general the mlogger for the experiment.
source $(dirname $(readlink -f $0))/../../common/.expt-env

# First kill the mlogger if it is running.
for session in $(screen -ls | grep -o "[0-9]*\.${EXPT}.mlogger")
do 
  screen -S "${session}" -p 0 -rX stuff '!'
  sleep 2
  screen -S "${session}" -p 0 -X quit
done

# Now start the mloggger in a screen.
cmd="mlogger -e $EXPT$(printf \\r)"
screen -dmS "${EXPT}.mlogger"
screen -S "${EXPT}.mlogger" -p 0 -rX stuff "$cmd"

unset cmd

# end script

