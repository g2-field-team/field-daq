cd "/Alarms/classes"

cp "Alarm" "AutoRunStop"
cd "AutoRunStop"

set "System message interval" 0
set "Stop run" y
set "Display BGColor" yellow

cd "/Alarms/Alarms"
cp "Demo ODB" "Absolute Probe Stop"
cd "Absolute Probe Stop"
set "Active" n
set "Check interval" 1
set "Condition" "/Equipment/AbsoluteProbe/Monitor/Finished = 2"
set "Alarm Class" "AutoRunStop"
set "Alarm Message" "Absolute probe job finished."

cd "/Alarms/Alarms"
cp "Demo ODB" "Galil Platform Stop"
cd "Galil Platform Stop"
set "Active" n
set "Check interval" 1
set "Condition" "/Equipment/GalilPlatform/Monitors/Finished = 2"
set "Alarm Class" "AutoRunStop"
set "Alarm Message" "Galil Platform scanning finished."
