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
    fe_arr=(${fe//\:/ })
    ssh "${fe_arr[0]}" "source start_${fe_arr[1]}.sh"

done

# end script
