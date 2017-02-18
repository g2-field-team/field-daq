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
create FLOAT "Power Factor"
create FLOAT "Temperature 1"
create FLOAT "Pressure Temperature"
create FLOAT "Pressure"
create FLOAT "Vmin 1"
create FLOAT "Vmax 1"
create FLOAT "Vmin 2"
create FLOAT "Vmax 2"
create INT "Data Frame Index"
create INT "Buffer Load"

cd "/Equipment/TrolleyInterface/Settings"
create BOOL "Root Output"
set "Root Output" false
create STRING "Root Dir[1][256]"
set "Root Dir" "/home/newg2/Applications/field-daq/resources/Root/"
create INT "Cmd" 
set "Cmd" 0

cd "/Equipment/TrolleyInterface/Settings"
mkdir "Sg382"
cd "/Equipment/TrolleyInterface/Settings/Sg382"
create FLOAT "RF Frequency"
create FLOAT "RF Amplitude"
set "RF Frequency" 61.766
set "RF Amplitude" 6.1

cd "/Equipment/TrolleyInterface/Settings"
mkdir "Trolley Power Registry"
cd "/Equipment/TrolleyInterface/Settings/Trolley Power Registry"
create INT "Voltage"
set "Voltage" 156

cd "/Equipment/TrolleyInterface/Settings"
mkdir "Communication Registry"
cd "/Equipment/TrolleyInterface/Settings/Communication Registry"
create INT "Interface Comm Start"
create INT "Interface Comm Data Start"
create INT "Interface Comm Stop"
create INT "Trolley Comm Start"
create INT "Trolley Comm Data Start"
create INT "Trolley Comm Stop"
create INT "Switch To RF"
create INT "Power ON"
create INT "RF Enable"
create INT "RF Disable"
create INT "Power OFF"
create INT "Switch To Comm"
create INT "Cycle Length"
set "Interface Comm Start" 0
set "Interface Comm Data Start" 100
set "Interface Comm Stop" 400 
set "Trolley Comm Start" 500 
set "Trolley Comm Data Start" 600
set "Trolley Comm Stop" 9700 
set "Switch To RF" 12991
set "Power ON" 9700
set "RF Enable" 13091
set "RF Disable" 29091
set "Power OFF" 29091
set "Switch To Comm" 29191
set "Cycle Length" 29191

cd "/Equipment/TrolleyInterface/Settings"
mkdir "NMR Registry"
cd "/Equipment/TrolleyInterface/Settings/NMR Registry"
create INT "RF Prescale"
create INT "Probe Select"
create INT "Probe Delay" 
create INT "Probe Period"
create INT "Preamp Delay" 
create INT "Preamp Period"
create INT "Gate Delay"
create INT "Gate Period"
create INT "Transmit Delay"
create INT "Transmit Period"
set "RF Prescale" 62
set "Probe Select" 0
set "Probe Delay" 12700
set "Probe Period" 20000
set "Preamp Delay" 373 
set "Preamp Period" 12327
set "Gate Delay" 0
set "Gate Period" 15000
set "Transmit Delay" 300
set "Transmit Period" 310

cd "/Equipment/TrolleyInterface/Settings"
mkdir "Barcode Registry"
cd "/Equipment/TrolleyInterface/Settings/Barcode Registry"
create DOUBLE "LED Voltage"
create INT "Sample Period"
create INT "Acq Delay"
set "LED Voltage" 1.0
set "Sample Period" 100000
set "Acq Delay" 800

cd /
cp "/Equipment/TrolleyInterface" "/Equipment/SimTrolleyInterface"
cd "/Equipment/SimTrolleyInterface/Common"
set "Frontend name" "Sim Trolley Interface"
set "Frontend file name" "src/sim_trolley.cxx"
set "Status" "Sim Trolley Interface@localhost"

create STRING "/Equipment/SimTrolleyInterface/Settings/Data Source[1][256]"
set "/Equipment/SimTrolleyInterface/Settings/Data Source" "/home/newg2/Applications/field-daq/resources/NMRDataTemp/data_NMR_61682000Hz_11.70dbm-2016-10-27_19-36-42.dat"
