cd "/Alarms/Classes/"

cp Alarm Failure
cd Failure


set "Execute command" "/home/newg2/Applications/field-daq/common/scripts/alarms/alarm_script.sh failure \"%s\""
set "Stop run" y
set "Display BGColor" "red"
set "Display FGColor" "black"

