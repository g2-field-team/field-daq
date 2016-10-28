#!/bin/bash

# The script starts general midas utilites for the experiment.
source $(dirname $(readlink -f $0))/../../common/.expt-env

source $EXPT_DIR/online/bin/start_midas.sh
source $EXPT_DIR/online/bin/start_frontends.sh
source $EXPT_DIR/online/bin/start_analyzers.sh

# end script