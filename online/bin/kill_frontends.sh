#!/bin/bash

# The script kills active MIDAS frontends and their container screens.
source $(dirname $(readlink -f $0))/../../common/.expt-env

for fe in "${EXPT_FE[@]}"; do
    for session in $(screen -ls | grep -o "[0-9]*\.${EXPT}.${fe//_/-}"); do
        screen -S "${session}" -p 0 -rX stuff '!'
	sleep 1
        screen -S "${session}" -p 0 -X quit
    done
done

for fe in "${EXT_FE[@]}"; do
    fe_arr=(${fe//\:/ })
    fe=${fe_arr[1]}
    cmd="for session in \$(screen -ls | grep -o \"[0-9]*\.${EXPT}.${fe//_/-}\"); do screen -S \"\${session}\" -p 0 -rX stuff \'!\'; done;"
    ssh "${fe_arr[0]}" "$cmd"
    sleep 1
    cmd="for session in \$(screen -ls | grep -o \"[0-9]*\.${EXPT}.${fe//_/-}\"); do screen -S \"\${session}\" -p 0 -X quit; done;"
    ssh "${fe_arr[0]}" "$cmd"

done

# end script
