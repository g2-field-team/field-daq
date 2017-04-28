#!/bin/bash
MIDAS_EXPT_NAME='CR'
# Start collecting information from ODB first
Run_number=`odbedit -e $MIDAS_EXPT_NAME -c 'ls "/Runinfo/Run number"'`
Data_dir=`odbedit -e $MIDAS_EXPT_NAME -c 'ls "/Logger/Data dir"'`
dir_name=`echo $Data_dir | awk '{print $3}'`
number=`echo $Run_number | awk '{print $3}'`
filename=`echo "$dir_name/run$(printf "%05d" $number).json"` 

python /home/newg2/Applications/field-daq/online/DataBase/store_json.py $filename test


# Now create the temporary file to be sent to the elog
