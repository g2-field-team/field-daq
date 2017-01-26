#!/bin/bash

# Download pip and install it for the system python.
wget https://bootstrap.pypa.io/get-pip.py -O /tmp/get-pip.py
/usr/bin/python /tmp/get-pip.py

# Install python virtual environments and create one.
pip install virtualenv

virtualenv /usr/local/opt/pyenv
chown -R newg2:newg2 /usr/local/opt/pyenv

if [ ! "$(grep 'alias pyenv' ~/.bashrc)" ]; then
    echo '' >> /home/newg2/.bashrc
    echo '# Set alias for activating python virualenv.' >> /home/newg2/.bashrc
    echo 'alias pyenv=". /usr/local/opt/pyenv/bin/activate"' >> /home/newg2/.bashrc
fi
