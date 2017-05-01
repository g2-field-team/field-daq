cd "/Alarms/Alarms"
cp "Demo ODB" "Surface Coil Currents"
cd "Surface Coil Currents"
set "Active" y
set "Type" 3
set "Check interval" 5
set "Condition" "/Equipment/Surface Coils/Settings/Monitoring/Current Health = n"
set "Alarm Class" "Alarm"
set "Alarm Message" "A surface coil current is out of spec"

cd "/Alarms/Alarms"
cp "Demo ODB" "Surface Coil Temperatures"
cd "Surface Coil Temperatures"
set "Active" y
set "Type" 3
set "Check interval" 5
set "Condition" "/Equipment/Surface Coils/Settings/Monitoring/Temp Health = n"
set "Alarm Class" "Alarm"
set "Alarm Message" "A surface coil channel is too hot"
