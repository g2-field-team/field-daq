#!/bin/bash

# Make sure the ODB is initialized.
odbedit -e g2-field -s 20000000 -c 'clean'

# Run all ODB subtree init scripts.
cd $(dirname $(readlink -f $0))/odb.d

for script in `ls [0-9]?-*.cmd`
do
    odbedit -e g2-field -c @${script}
done
