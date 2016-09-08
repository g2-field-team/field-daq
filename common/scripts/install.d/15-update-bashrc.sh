#!/bin/bash

echo -e "\e[31mAdding devtoolset-2 to PATH in bashrc.\e[0m"

if [ "$(grep '/opt/rh/devtoolset-2/root/usr/bin' ~/.bashrc)" ]; then
    echo -e "\e[31mdevtoolset already in PATH\e[0m"
else
    echo -e "\e[31mAdding devtoolset to PATH\e[0m"
    echo -e "\nexport PATH=/opt/rh/devtoolset-2/root/usr/bin:\$PATH\n" >> ~/.bashrc
fi