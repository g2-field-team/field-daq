cd "/Alarms/classes"

cp "Alarm" "AlarmMsg"
cd "AlarmMsg"

set "System message interval" 0
set "Stop run" n
set "Display BGColor" red
set "Execute Command" "/home/newg2/Applications/field-daq/online/alarms/send_Msg.sh '%s'"
set "Execute Interval" 36000

cd "/Alarms/Alarms"
cp "Demo ODB" "Compressor Status"
cd "Compressor Status"
set "Active" n
set "Check interval" 10
set "Condition" "/Equipment/CompressorChiller/Variables/Sta1 = 0"
set "Alarm Class" "AlarmMsg"
set "Alarm Message" "Compressor is off or not read properly."

cd "/Alarms/Alarms"
cp "Demo ODB" "Chiller Status"
cd "Chiller Status"
set "Active" n
set "Check interval" 10
set "Condition" "/Equipment/CompressorChiller/Variables/Sta3 = 0"
set "Alarm Class" "AlarmMsg"
set "Alarm Message" "Chiller is off or not read properly."

cd "/Alarms/Alarms"
cp "Demo ODB" "Chiller Temperature"
cd "Chiller Temperature"
set "Active" n
set "Check interval" 10
set "Condition" "/Equipment/CompressorChiller/Variables/Temp > 80"
set "Alarm Class" "AlarmMsg"
set "Alarm Message" "Chiller temperature is too high."

cd "/Alarms/Alarms"
cp "Demo ODB" "Helium Level"
cd "Helium Level"
set "Active" n
set "Check interval" 10
set "Condition" "/Equipment/HeLevel/Variables/HeLe < 74"
set "Alarm Class" "AlarmMsg"
set "Alarm Message" "Helium level is too low."

cd "/Alarms/Alarms"
cp "Demo ODB" "Helium Status"
cd "Helium Status"
set "Active" n
set "Check interval" 10
set "Condition" "/Equipment/HeLevel/Settings/Alarm > 50"
set "Alarm Class" "AlarmMsg"
set "Alarm Message" "Helium level is off or not read properly."

cd "/Alarms/classes"
cp "Alarm" "FE Monitor"
cd "FE Monitor"
set "Write system message" n
set "Write Elog message" n
set "System message interval" 0
set "Stop run" n
set "Display BGColor" ""
set "Display FGColor" ""
set "Execute Command" ""
set "Execute Interval" 300

cd "/Alarms/Alarms"
cp "Demo periodic" "FE Monitor"
cd "FE Monitor"
set "Active" n
set "Check interval" 300
set "Alarm Class" "FE Monitor"
set "Alarm Message" "For FE Monitoring Purposes"

cd "/Alarms/Alarms"
cp "Demo ODB" "FE Status"
cd "FE Status"
set "Active" n
set "Check interval" 10
set "Condition" "/Alarms/Alarms/FE Monitor/Triggered > 1"
set "Alarm Class" "AlarmMsg"
set "Alarm Message" "Frontend is not running correctly. Likely due to failure to read port."

cd "/Alarms/Alarms"
cp "Demo ODB" "Helium Warning"
cd "FE Status"
set "Active" n
set "Check interval" 10
set "Condition" "/Equipment/HeLevel/Settings/Alarm = 100"
set "Alarm Class" "AlarmMsg"
set "Alarm Message" "Helium Monitor is off or not read correctly."
