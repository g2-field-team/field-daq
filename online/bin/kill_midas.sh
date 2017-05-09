#!/bin/bash

# The script starts general midas utilites for the experiment.
source $(dirname $(readlink -f $0))/../../common/.expt-env

# First kill the mlogger if it is running.
for session in $(screen -ls | grep -o "[0-9]*\.${EXPT}.mlogger")
do
  screen -S "${session}" -p 0 -rX stuff '!'
  sleep 2
  screen -S "${session}" -p 0 -X quit
done

# Second kill the mserver if it is running.
for session in $(screen -ls | grep -o "[0-9]*\.${EXPT}.mserver")
do
  screen -S "${session}" -p 0 -rX stuff $'\003'
  sleep 2
  screen -S "${session}" -p 0 -X quit
done

# Third kill the mhttpd if it is running.
for session in $(screen -ls | grep -o "[0-9]*\.${EXPT}.mhttpd")
do
  screen -S "${session}" -p 0 -rX stuff $'\003'
  sleep 2
  screen -S "${session}" -p 0 -X quit
done


# end script

