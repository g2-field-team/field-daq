mkdir "/Equipment/Yokogawa/Settings"
mkdir "/Equipment/Yokogawa/Monitor"

cd "Equipment/Yokogawa/Settings"

create STRING "IP address[1][256]"
set "IP address" "000.000.000.000"
create BOOL "Root Output"
set "Root Output" false
create STRING "Root Dir[1][256]"
set "Root Dir" "/home/newg2/Applications/field-daq/resources/Root/"

cd "Equipment/Yokogawa/Monitor"

create INT "Buffer Load"
