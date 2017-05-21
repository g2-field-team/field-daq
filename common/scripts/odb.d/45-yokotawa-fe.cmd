mkdir "/Equipment/Yokogawa/Settings"
mkdir "/Equipment/Yokogawa/Monitors"

cd "/Equipment/Yokogawa/Settings"

create STRING "IP address[1][256]"
set "IP address" "192.168.5.160"
create BOOL "Root Output"
set "Root Output" false
create STRING "Root Dir[1][256]"
set "Root Dir" "/home/newg2/Applications/field-daq/resources/Root/"
create BOOL "Simulation Mode"
set "Simulation Mode" false
create BOOL "Feedback Active"
set "Feedback Active" false
create DOUBLE "Current Setpoint"
set "Current Setpoint" 0.000
create DOUBLE "Field Setpoint"
set "Field Setpoint" 0.000
create DOUBLE "Field Readout Value"
set "Field Readout Value" 0.000

cd "/Equipment/Yokogawa/Monitors"

create INT "Buffer Load"
set "Buffer Load" 0
create BOOL "Read Thread Active"
set "Read Thread Active" false
create DOUBLE "Average Field"
set "Average Field" 0.00
create DOUBLE "Current Value"
set "Current Value" 0.00
