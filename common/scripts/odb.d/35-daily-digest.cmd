cd "/Alarms/classes"

cp "Alarm" "DigestMsg"
cd "DigestMsg"

set "Write system message" y
set "Write Elog message" n
set "System message interval" 0
set "Stop run" n
set "Display BGColor" green
set "Display FGColor" yellow
set "Execute Command" "/home/newg2/Applications/field-daq/online/alarms/daily_digest.sh"
set "Execute Interval" 3600

cd "/Alarms/Alarms"
cp "Demo periodic" "Daily Digest"
cd "Daily Digest"
set "Active" n
set "Check interval" 86400
set "Alarm Class" "DigestMsg"
set "Alarm Message" "Sending Daily Digest."
