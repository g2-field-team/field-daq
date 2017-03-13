mkdir "/Equipment/Yokogawa/Settings"
mkdir "/Equipment/Yokogawa/Monitor"

cd "/Equipment/Yokogawa/Settings"

create STRING "IP address[1][256]"
set "IP address" "000.000.000.000"
create BOOL "Root Output"
set "Root Output" false
create STRING "Root Dir[1][256]"
set "Root Dir" "/home/newg2/Applications/field-daq/resources/Root/"
create BOOL "Simulation Mode"
set "Simulation Mode" false

cd "/Equipment/Yokogawa/Monitor"

create INT "Buffer Load"
set "Buffer Load" "0"
create INT "Read Thread Active"
set "Read Thread Active" "0"
create DOUBLE "Average Field"
set "Average Field" "0"
