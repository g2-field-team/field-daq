#!/bin/bash

cd $(dirname $(readlink -f $0))/odb.d

for script in `ls [0-9]?-*.cmd`
do
    odbedit -e example-expt -c @${script}
done
