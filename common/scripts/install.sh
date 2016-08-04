#!/bin/bash

# Grab a handle for the install directory.
INSTALL_DIR="$(dirname $(readlink -f $0))/install.d"
cd $INSTALL_DIR

for script in `ls .`
do
    . $script
    cd $INSTALL_DIR
done

# Move back into the script directory.
cd ..

unset INSTALL_DIR