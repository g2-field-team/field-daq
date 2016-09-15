#!/bin/bash

# The script starts general midas utilites for the experiment.
source $(dirname $(readlink -f $0))/../../common/.expt-env

source $EXPT_DIR/online/bin/kill_frontends.sh
sleep 2
source $EXPT_DIR/online/bin/start_frontends.sh

# end script