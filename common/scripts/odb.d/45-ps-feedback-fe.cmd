mkdir "/Equipment/PS Feedback/Settings"
mkdir "/Equipment/PS Feedback/Monitors"

cd "/Equipment/PS Feedback/Settings"

create STRING "IP address[1][256]"
set "IP address" "192.168.5.160"
create BOOL "Root Output"
set "Root Output" true 
create STRING "Root Dir[1][256]"
set "Root Dir" "/home/newg2/Applications/field-daq/resources/root/"
create BOOL "Simulation Mode"
set "Simulation Mode" false
create BOOL "Feedback Active"
set "Feedback Active" false
create BOOL "Write Test File"
set "Write Test File" false
create DOUBLE "Current Setpoint (mA)"
set "Current Setpoint (mA)" 0.000
create DOUBLE "Field Setpoint (kHz)"
set "Field Setpoint (kHz)" 0.000
create DOUBLE "P Coefficient"
set "P Coefficient" 0.000
create DOUBLE "I Coefficient"
set "I Coefficient" 0.000
create DOUBLE "D Coefficient"
set "D Coefficient" 0.000
create DOUBLE "Scale Factor (kHz per mA)"
set "Scale Factor (kHz per mA)" 1.000
create BOOL "Use Single Probe for Field Avg"
set "Use Single Probe for Field Avg" false
create INT "Probe Number for Field Avg"
set "Probe Number for Field Avg" -1 
create DOUBLE "Feedback Threshold (Hz)"
set "Feedback Threshold (Hz)" 100.00 

cd "/Equipment/PS Feedback/Monitors"

create INT "Buffer Load"
set "Buffer Load" 0
create BOOL "Read Thread Active"
set "Read Thread Active" false
create DOUBLE "Current Value (mA)"
set "Current Value (mA)" 0.00
