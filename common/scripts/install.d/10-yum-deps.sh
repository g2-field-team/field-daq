#!/bin/bash

echo -e "\e[31mInstalling requisite yum packages\e[0m"

yum -y install yum-plugin-priorities epel-release osg-oasis \
    openssh-clients openssh-server libusb1-devel mysql-devel \
    python-devel libpng-devel qt-devel PyQt4 tkinter cmake lapack blas \
    libsodium-devel libXpm-devel devtoolset-2-gcc devtoolset-2-gcc-c++ \
    devtoolset-2-binutils-devel fftw-devel unixODBC sqlite-devel \
    armadillo-devel screen emacs devtoolset-2-git
