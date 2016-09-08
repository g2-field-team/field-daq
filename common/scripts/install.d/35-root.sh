#!/bin/bash

echo -e "\e[31m\nInstalling ROOT.\e[0m"

mkdir -p ~/Packages && cd ~/Packages/

if [ ! -d root ]; then
    git clone http://root.cern.ch/git/root.git root
fi

cd root
git checkout -b v5-34-36 v5-34-36
./configure --all
make

if [ "$(grep 'thisroot.sh' ~/.bashrc)" ]; then
    echo -e "\e[31mSource root line already in .bashrc\e[0m"
else
    echo -e "\e[31mAdding a line to source root to the .bashrc\e[0m"
    echo -e "# Source ROOT setup script." >> ~/.bashrc
    echo -e "cd ~/Packages/root && . bin/thisroot.sh && cd -\n" >> ~/.bashrc
fi

source ~/.bashrc