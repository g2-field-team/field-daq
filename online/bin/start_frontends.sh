#!/bin/bash

# Load the experiment variables.
source $(dirname $(readlink -f $0))/../../common/.expt-env

for fe in "${EXPT_FE[@]}"; do
    fename="${EXPT_DIR}/common/bin/${fe}"
    scname="${EXPT}.${fe//_/-}"
    screen -dmS $scname
    sleep 0.05
    screen -S $scname -p 0 -rX stuff "${fename} -e $EXPT$(printf \\r)"
done

for fe in "${EXT_FE[@]}"; do
    # Split into an array.
    fe_arr=(${fe//\:/ })

    # Redefine the fe variable.
    fe=${fe_arr[1]}
    fename="${EXPT_DIR}/common/bin/${fe}"
    scname="${EXPT}.${fe//_/-}"
    cmd="screen -dmS $scname"
    ssh "${fe_arr[0]}" "$cmd"
    sleep 0.05
    cmd1="screen -S $scname -p 0 -rX stuff"
    cmd2="${fename} -e $EXPT -h $EXPT_IP_L$(printf \\r)"
    cmd="$cmd1 \"$cmd2\""
    ssh "${fe_arr[0]}" "$cmd"

done

# end script
