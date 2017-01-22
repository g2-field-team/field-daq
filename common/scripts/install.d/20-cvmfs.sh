#!/bin/bash

echo -e "\e[31mInstalling requisite external packages\e[0m"

# Configure fuse.
if [ ! -e /etc/fuse.conf ]; then
    echo -e "\e[31mCreating fuse.conf file.\e[0m"
    echo -e "user_allow_other\n" > /etc/fuse.conf
else
    if [ "$(grep '/opt/rh/devtoolset-2/root/usr/bin' ~/.bashrc)" ]; then
	echo -e "\e[31mAdding config line to fuse.conf\e[0m"
	echo -e "\nuser_allow_other\n" >> /etc/fuse.conf
    fi
fi

# Configure cvmfs.
echo -e "\e[31mConfiguring cvmfs defaults.local\e[0m"

conf=/etc/cvmfs/default.local
echo -e "# Pull repositories that are in /cvmfs/*.*\n" > $conf
echo -e "CVMFS_REPOSITORIES=\"gm2.opensciencegrid.org\"\n" >> $conf
echo -e "# Talk directly to the stratum 1 server unless overridden.\n" >> $conf
echo -e "CVMFS_HTTP_PROXY=DIRECT\n" >> $conf
echo -e "# Expand quota (units in MB)\n" >> $conf
echo -e "CVMFS_QUOTA_LIMIT=2000\n" >> $conf
unset conf

echo -e "\e[31mAdding gm2.opensciencegrid to cvmfs mounts\e[0m"
cp -r /etc/cvmfs/config.d /etc/cvmfs/backup-config.d
rm -f /etc/cvmfs/config.d/*
echo -e 'CVMFS_SERVER_URL="http://cvmfs-s1bnl.opensciencegrid.org:8000/cvmfs/@fqrn@;http://cvmfs-s1fnal.opensciencegrid.org:8000/cvmfs/@fqrn@;http://cvmfs-s1goc.opensciencegrid.org:8000/cvmfs/@fqrn@"' > /etc/cvmfs/config.d/gm2.opensciencegrid.org.conf

sudo mkdir -p /cvmfs/gm2.opensciencegrid.org

echo -e "\e[31mMount cvmfs with \n\nsudo mount -t cvmfs gm2.opensciencegrid.org /cvmfs/gm2.opensciencegrid.org\n\n\e[0m"
echo -e "\e[31mTesting contents of mount\e[0m"

sudo mount -t cvmfs gm2.opensciencegrid.org /cvmfs/gm2.opensciencegrid.org
ls -l /cvmfs/gm2.opensciencegrid.org/

echo -e "\e[31mAdd cvmfs to automount by adding \n/cvmfs /etc/auto.cvmfs\n\n to /etc/auto.master before the line containing +auto.master.\e[0m"
echo -e "\e[31mThen restart autofs with 'sudo service autofs restart'\e[0m"
