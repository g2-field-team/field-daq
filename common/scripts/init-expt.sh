#!/bin/bash

EXPT_DIR=$(dirname $(readlink -f "$0"))/../..
EXPT_NAME='g2-field'

# Make sure the exptab exists.
sudo touch /etc/exptab

# Check if the example was already initialized.
if [ "$(grep g2-field /etc/exptab)" ]; then
    echo -e "\e[31mExperiment already in exptab\e[0m"
else
    echo -e "\e[31mAdding experiment to exptab\e[0m"
 echo "${EXPT_NAME} ${EXPT_DIR}/resources ${USER}" | sudo tee --append /etc/exptab
fi

# Make sure the ODB is initialized.
odbedit -e g2-field -s 20000000 -c 'clean'

./init-odb.sh

# Copy the environment variables for expt (may need adjusting).
cp ../templates/expt-env ../.expt-env
