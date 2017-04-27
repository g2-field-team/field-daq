mkdir /Script
cd /Script

create STRING "Restart DAQ"
set "Restart DAQ" "/home/newg2/Applications/field-daq/online/bin/restart_daq.sh
"
create STRING "Restart Front-Ends"
set "Restart Front-Ends" "/home/newg2/Applications/field-daq/online/bin/restart_frontends.sh"