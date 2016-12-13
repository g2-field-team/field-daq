mkdir "/Equipment/TrolleyInterface/Settings"
mkdir "/Equipment/TrolleyInterface/Monitor"
mkdir "/Equipment/TrolleyInterface/Hardware"

create SHORT "/Equipment/TrolleyInterface/Hardware/Pressure Sensor Calibration[7]"

cd "/Equipment/TrolleyInterface/Monitor"

create BOOL "Barcode Error"
create BOOL "Temperature Interrupt"
create BOOL "Power Supply Status[3]"
create BOOL "Read Thread Active"
create BOOL "NMR Check Sum"
create BOOL "Frame Check Sum"
create INT "Data Frame Index"
create INT "Buffer Load"

cd "/Equipment/TrolleyInterface/Settings"
create DOUBLE "LED Voltage"
set "LED Voltage" 1.0

cd /
cp "/Equipment/TrolleyInterface" "/Equipment/SimTrolleyInterface"

create STRING "/Equipment/SimTrolleyInterface/Settings/Data Source"[1][256]
set "/Equipment/SimTrolleyInterface/Settings/Data Source" "/home/newg2/Applications/field-daq/resources/NMRDataTemp/data_NMR_61682000Hz_11.70dbm-2016-10-27_19-36-42.dat"
