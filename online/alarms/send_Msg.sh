#!/bin/bash

input="/home/newg2/Applications/field-daq/online/alarms/emaillist.txt"
while IFS= read -r var
do
  echo $1 | mail -s "Alarm" "$var" 
done <"$input"
