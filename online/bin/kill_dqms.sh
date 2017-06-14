#!/bin/bash

# The script kills active MIDAS frontends and their container screens.
source $(dirname $(readlink -f $0))/../../common/.expt-env

for dqm in "${EXPT_DQM[@]}"; do
    # Split into an array.
    dqm_arr=(${dqm//\:/ })

    # Redefine the fe variable.
    dqmname=${dqm_arr[0]}
    dqmpage=${dqm_arr[1]}

    for session in $(screen -ls | grep -o "[0-9]*\.${EXPT}.${dqmname//_/-}.node"); do
        screen -S "${session}" -p 0 -rX stuff $'\003'
	sleep 1
        screen -S "${session}" -p 0 -X quit
    done
    for session in $(screen -ls | grep -o "[0-9]*\.${EXPT}.${dqmname//_/-}.art"); do
        screen -S "${session}" -p 0 -rX stuff $'\003'
	sleep 1
        screen -S "${session}" -p 0 -X quit
    done
done

# end script
