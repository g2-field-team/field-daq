#!/bin/bash

EXPT_DIR=$(dirname $(readlink -f "$0"))/..
EXPT_NAME='example-expt'

# Make sure the exptab exists.
sudo touch /etc/exptab

# Check if the example was already initialized.
if [ "$(grep example-expt /etc/exptab)" ]; then
    echo -e "\e[31mExperiment already in exptab\e[0m"
    exit
fi

# Add the sample experiment
echo -e "\e[31mAdding experiment to exptab\e[0m"
echo "${EXPT_NAME} ${EXPT_DIR}/resources ${USER}" | sudo tee --append /etc/exptab

# Initialize the ODB to reasonable size.
odbedit -e ${EXPT_NAME} -s 100000 -c 'clean'

cd ${EXPT_DIR}/common
./init-odb.sh