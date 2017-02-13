#!/bin/bash

input="emaillist.txt"
while IFS= read -r var
do
  echo $1 | mail -s "Alarm" "$var" 
done <"$input"
