#!/bin/bash

echo -e "\e[31m\nInstalling libfid.\e[0m"

mkdir -p /usr/local/opt && cd /usr/local/opt

if [ ! -d libfid ]; then
    git clone http://github.com/mwsmith2/libfid.git
fi

cd libfid
make clean
make
make install

# End script