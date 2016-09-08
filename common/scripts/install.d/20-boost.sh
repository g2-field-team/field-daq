#!/bin/bash

echo -e "\e[31mInstalling requisite external packages\e[0m"

# Make sure packages exists.
mkdir -p /usr/local/opt

# Download boost.
if [ -d /usr/local/opt/boost ]; then
    echo -e "\e[31mBoost is already downloaded.\e[0m"
else
    echo -e "\e[31mCloning boost.\e[0m"
    cd /usr/local/opt
    git clone --recursive https://github.com/boostorg/boost
fi

# Build and install boost.
if [ -d /usr/local/lib/libboost_system.so ]; then
    echo -e "\e[31mBoost is already installed.\e[0m"
else
    echo -e "\e[31mBuilding and installing boost.\e[0m"
    cd /usr/local/opt/boost
    ./bootstrap.sh
    ./b2 install
fi