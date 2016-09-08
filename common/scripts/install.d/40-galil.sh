sudo mkdir -p /usr/local/opt/galil
cd /usr/local/opt/galil
sudo wget http://www.galil.com/sw/pub/rhel/6/galilrpm-2-1.noarch.rpm | sudo rpm -ivh

sudo yum install gclib

sudo wget http://www.galilmc.com/download/software/galilsuite/linux/galil_public_key.asc
su -c 'rpm --import galil_public_key.asc'

sudo wget http://www.galilmc.com/download/software/galilsuite/linux/galilsuite.x86_64.rpm
su -c 'rpm -i galilsuite.x86_64.rpm'
