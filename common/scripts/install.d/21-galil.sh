mkdir -p ~/Packages/galil && cd ~/Packages/galil
wget http://www.galil.com/sw/pub/rhel/6/galilrpm-2-1.noarch.rpm 
sudo rpm -ivh galilrpm-2-1.noarch.rpm

sudo yum install gclib

wget http://www.galilmc.com/download/software/galilsuite/linux/galil_public_key.asc
sudo rpm --import galil_public_key.asc

wget http://www.galilmc.com/download/software/galilsuite/linux/galilsuite.x86_64.rpm
sudo rpm -i galilsuite.x86_64.rpm
