#!/bin/bash

echo -e "\e[31mAdding devtoolset-2 to PATH in bashrc.\e[0m"

if [ "$(grep '/opt/rh/devtoolset-2/root/usr/bin' ~/.bashrc)" ]; then
    echo -e "\e[31mdevtoolset already in PATH\e[0m"
else
    echo -e "\e[31mAdding devtoolset to PATH\e[0m"
    echo -e "\nexport PATH=/opt/rh/devtoolset-2/root/usr/bin:\$PATH\n" >> ~/.bashrc
fi

echo -e "\e[31mAdding UPS init file to bashrc.\e[0m"

if [ "$(grep 'upsrc' ~/.bashrc)" ]; then
    echo -e "\e[31mUPS init script already being sourced\e[0m"
else
    echo -e "\e[31mCopying UPS init script and adding source call\e[0m"
    cp ../src/upsrc ~/.upsrc
    echo -e "\n# Setup UPS packages\n. ~/.upsrc" >> ~/.bashrc
fi

echo -e "\e[31mAdding daq environment setup file to bashrc.\e[0m"

if [ "$(grep 'daq-env' ~/.bashrc)" ]; then
    echo -e "\e[31mdaq environment script already being sourced\e[0m"
else
    echo -e "\e[31mCopying daq environment script and adding source call\e[0m"
    cp ../src/daq-env ~/.daq-env
    echo -e "\n# Setup daq environment\n. ~/.daq-env" >> ~/.bashrc
fi

. ~/.bashrc
