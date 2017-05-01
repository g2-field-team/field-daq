#!/bin/bash

# The script restarts general the lazylogger for the experiment.
source $(dirname $(readlink -f $0))/../../common/.expt-env

# First kill the lazylogger if it is running.
for session in $(screen -ls | grep -o "[0-9]*\.${EXPT}.lazylogger")
do 
  screen -S "${session}" -p 0 -rX stuff '!'
  sleep 2
  screen -S "${session}" -p 0 -X quit
done

# Now start the mloggger in a screen.
cmd="lazylogger -e $EXPT$(printf -c ToRaid \\r)"
screen -dmS "${EXPT}.lazylogger"
screen -S "${EXPT}.lazylogger" -p 0 -rX stuff "$cmd"

unset cmd

# end script

