#!/bin/bash
# Start collecting information from ODB first
Run_number=`odbedit -c 'ls "/Runinfo/Run number"'`
Data_dir=`odbedit -c 'ls "/Logger/Data dir"'`
dir_name=`echo $Data_dir | awk '{print $3}'`
number=`echo $Run_number | awk '{print $3}'`
filename=`echo "$dir_name/run$(printf "%05d" $number).json"`

python /home/newg2/Applications/field-daq/online/Database/store_json.py /home/newg2/gm2Data/run00482.json gm2_writer gm2_online_prod g2db-priv 5433

