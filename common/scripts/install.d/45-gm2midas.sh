#!/bin/bash

# Note that you need an active kerberos ticket to run this.

echo -e "\n\e[31mInstalling MIDAS/ROME\e[0m"

if [ ! -e "~/.midasrc" ]; then
    echo -e "\e[31mCopying MIDAS environment variables file.\e[0m"
    cp ../rc.d/midasrc ~/.midasrc
fi

if [ "$(grep midasrc ~/.bashrc)" ]; then
    echo -e "\e[31mMIDAS environment vars already set in .bashrc.\e[0m"
else
    echo -e "\e[31mAdding line to .bashrc for MIDAS environment vars.\e[0m"
    echo -e "# Source file with MIDAS environment variables." >> ~/.bashrc
    echo -e ". ~/.midasrc" >> ~/.bashrc
fi

cd ~
. .midasrc

mkdir -p ~/Packages && cd ~/Packages

if [ ! -d "gm2midas" ]; then
    git clone ssh://p-gm2midas@cdcvs.fnal.gov/cvs/projects/gm2midas
fi

cd gm2midas
make -C midas
make -C mscb
make -C rome

# if [ ! -d "g2-midas" ]; then
#    echo -e "\e[31mUpdating MIDAS to a newer version.\e[0m"
#    mv midas g2-midas
#
#    git clone https://bitbucket.org/tmidas/midas.git
#    cd midas
#    git checkout e885e8f
#    make
# fi

source ~/.bashrc