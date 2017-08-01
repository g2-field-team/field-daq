#!/bin/bash

# Load the experiment variables.
source $(dirname $(readlink -f $0))/../../common/.expt-env

for dqm in "${EXPT_DQM[@]}"; do
    # Split into an array.
    dqm_arr=(${dqm//\:/ })

    # Redefine the fe variable.
    dqmname=${dqm_arr[0]}
    dqmpage=${dqm_arr[1]}
    artscname="${EXPT}.${dqmname//_/-}-art"
    nodescname="${EXPT}.${dqmname//_/-}-node"
    screen -dmS $nodescname
    sleep 0.05
    screen -dmS $artscname
    sleep 0.05
    screen -S $nodescname -p 0 -rX stuff ". ~/online_env.sh $(printf \\r)"
    screen -S $artscname -p 0 -rX stuff ". ~/online_env.sh $(printf \\r)"
    sleep 5
    screen -S $nodescname -p 0 -rX stuff "cd /home/newg2/Workplace/Online/art/OnlineDev_v7_06_00/srcs/gm2dqm/node $(printf \\r)"
    screen -S $artscname -p 0 -rX stuff "cd /home/newg2/Workplace/Online $(printf \\r)"
    sleep 0.05
#    screen -S $nodescname -p 0 -rX stuff "export NODE_ENV=production"
    screen -S $nodescname -p 0 -rX stuff "node ${dqmpage}.js $(printf \\r)"
    screen -S $artscname -p 0 -rX stuff "gm2 -c fcl/${dqmname}-dqm-online.fcl $(printf \\r)"

done
# end script
