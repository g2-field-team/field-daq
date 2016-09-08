#!/bin/bash

echo -e "\e[31mInstalling requisite yum repos\e[0m"

if [ ! -e "/etc/yum.repos.d/slc6-devtoolset.repo" ]; then
    echo -e "\e[31mAdding devtoolset-2 repo\e[0m"
    wget -O /etc/yum.repos.d/slc6-devtoolset.repo http://linuxsoft.cern.ch/cern/devtoolset/slc6-devtoolset.repo
fi

echo -e "\e[31mAttempting to add epel-release repo\e[0m"
yum -y install epel-release

