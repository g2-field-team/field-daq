cd "/Alarms/Alarms"
cp "Demo ODB" "Trolley Motion Trip"
cd "Trolley Motion Trip"
set "Active" y
set "Type" 3
set "Check interval" 1
set "Condition" "/Equipment/GalilFermi/Alarms/Motion Trip = y"
set "Alarm Class" "Alarm"
set "Alarm Message" "Trolley or Garage Motion Tripped!"

cd "/Alarms/Alarms"
cp "Demo ODB" "Bad Motion Attempt"
cd "Bad Motion Attempt"
set "Active" y
set "Type" 3
set "Check interval" 1
set "Condition" "/Equipment/GalilFermi/Alarms/Bad Motion Attempt = y"
set "Alarm Class" "Warning"
set "Alarm Message" "Invalid Motion Attempt!"

