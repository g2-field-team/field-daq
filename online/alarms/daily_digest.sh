#!/bin/bash

# Change to the directory of the script.
# cd $(dirname $(readlink -f $0))

# Get ODB values for all important values 
#Compressor
comp_sta1=`odbedit -c 'ls "/Equipment/CompressorChiller/Variables/Sta1"'`
comp_sta1v=`echo $comp_sta1 | awk '{print $2}'`

comp_hour=`odbedit -c 'ls "/Equipment/CompressorChiller/Variables/Hour"'`
comp_hourv=`echo $comp_hour | awk '{print $2}'`

#Chiller
chil_sta3=`odbedit -c 'ls "/Equipment/CompressorChiller/Variables/Sta3"'`
chil_sta3v=`echo $chil_sta3 | awk '{print $2}'`

chil_temp=`odbedit -c 'ls "/Equipment/CompressorChiller/Variables/Temp"'`
chil_tempv=`echo $chil_temp | awk '{print $2}'`

chil_pres=`odbedit -c 'ls "/Equipment/CompressorChiller/Variables/Pres"'`
chil_presv=`echo $chil_pres | awk '{print $2}'`

chil_flow=`odbedit -c 'ls "/Equipment/CompressorChiller/Variables/Flow"'`
chil_flowv=`echo $chil_flow | awk '{print $2}'`

#Helium Monitor
HeLevel=`odbedit -c 'ls "/Equipment/HeLevel/Variables/HeLe"'`
HeLevelv=`echo $HeLevel | awk '{print $2}'`

./send_Msg.sh "Compressor Data\n====================================\n$comp_sta1\n$comp_hour\n\nChiller Data\n====================================\n$chil_sta3\n$chil_temp\n$chil_pres\n$chil_flow\n\nPort 2 Data\n====================================\n$HeLevel"

#Reset Alarm
odbedit -c 'set "/Alarms/Alarms/Daily Digest/Triggered" 0'
