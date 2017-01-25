#!/bin/bash

echo -e "\e[31mInstalling requisite yum repos\e[0m"

if [ ! -e "/etc/yum.repos.d/epel.repo" ]; then
    echo -e "\e[31mAdding EPEL repo\e[0m"
    rpm -UvH https://dl.fedoraproject.org/pub/epel/epel-release-latest-6.noarch.rpm
    yum-config-manager --enable epel 
fi

if [ ! -e "/etc/yum.repos.d/slc6-devtoolset.repo" ]; then
    echo -e "\e[31mAdding devtoolset-2 repo\e[0m"
    wget -O /etc/yum.repos.d/slc6-devtoolset.repo http://linuxsoft.cern.ch/cern/devtoolset/slc6-devtoolset.repo
fi

if [ ! -e "/etc/yum.repos.d/osg-el6.repo" ]; then
    echo -e "\e[31mAdding OSG repos\e[0m"
    rpm -UvH https://repo.grid.iu.edu/osg/3.3/osg-3.3-el6-release-latest.rpm
fi

yum clean expire-cache
