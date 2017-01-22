#!/bin/bash
. ~/.bashrc

echo -e "\e[31m\nInstalling libfid.\e[0m"
mkdir -p ~/Packages && cd ~/Packages

if [ ! -d libfid ]; then
    git clone https://github.com/mwsmith2/libfid.git
fi

cd libfid
make clean
make
sudo make install

# End script
