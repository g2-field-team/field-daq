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

cd /
cp "/Equipment/TrolleyInterface" "/Equipment/SimTrolleyInterface"

