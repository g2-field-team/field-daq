mkdir -p /usr/local/opt/galil
cd /usr/local/opt/galil
wget http://www.galil.com/sw/pub/rhel/6/galilrpm-2-1.noarch.rpm 
rpm -ivh galilrpm-2-1.noarch.rpm

yum install gclib

wget http://www.galilmc.com/download/software/galilsuite/linux/galil_public_key.asc
rpm --import galil_public_key.asc

sudo wget http://www.galilmc.com/download/software/galilsuite/linux/galilsuite.x86_64.rpm
rpm -i galilsuite.x86_64.rpm
