#!/bin/bash

# Grab a handle for the install directory.
INSTALL_DIR="$(dirname $(readlink -f $0))/install.d"
cd $INSTALL_DIR

for script in `ls [0-9]?-*.sh`
do
    if [ ${script:1:1} == 0 ]; then
	sudo ./$script
    else
	. $script
    fi
    
    . ~/.bashrc
    cd $INSTALL_DIR
done

# Move back into the script directory.
cd ..

unset INSTALL_DIR