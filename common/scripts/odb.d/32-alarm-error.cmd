cd "/Alarms/Classes/"

cp Alarm Error
cd Error

set "Execute command" "/home/newg2/Applications/field-daq/common/scripts/alarms/alarm_script.sh error \"%s\""
set "Stop run" y
set "Display BGColor" "yellow"
set "Display FGColor" "black"

