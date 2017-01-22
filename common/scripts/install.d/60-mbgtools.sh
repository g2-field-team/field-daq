#!/bin/bash
. ~/.bashrc

# Download and build the driver.
mkdir -p ~/Packages && cd ~/Packages

wget https://www.meinbergglobal.com/download/drivers/mbgtools-lx-3.4.0.tar.gz

tar xf mbgtools-lx-3.4.0.tar.gz
rm mbgtools-lx-3.4.0.tar.gz
cd mbgtools-lx-3.4.0

make clean
make
make install
