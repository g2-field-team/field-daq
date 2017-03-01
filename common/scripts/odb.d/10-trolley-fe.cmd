mkdir "/Equipment/TrolleyInterface/Settings"
mkdir "/Equipment/TrolleyInterface/Monitor"
mkdir "/Equipment/TrolleyInterface/Hardware"

create SHORT "/Equipment/TrolleyInterface/Hardware/Pressure Sensor Calibration[7]"

cd "/Equipment/TrolleyInterface/Monitor"

create BOOL "Barcode Error"
create BOOL "Temperature Interrupt"
create BOOL "Power Supply Status[3]"
create BOOL "Read Thread Active"
create BOOL "Control Thread Active"
create BOOL "NMR Check Sum"
create BOOL "Config Check Sum"
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
create INT "Interface Buffer Load"
create INT "FIFO Status"
create STRING "Current Mode"

cd "/Equipment/TrolleyInterface/Settings"
create BOOL "Root Output"
set "Root Output" false
create STRING "Root Dir[1][256]"
set "Root Dir" "/home/newg2/Applications/field-daq/resources/Root/"
create BOOL "Simulation Switch"
set "Simulation Switch" false
create INT "Cmd" 
set "Cmd" 0

cd "/Equipment/TrolleyInterface/Settings"
mkdir "Simulation"
cd "/Equipment/TrolleyInterface/Settings/Simulation"
create STRING "Data Source[1][256]"
set "Data Source" "/home/newg2/Applications/field-daq/resources/NMRDataTemp/data-2017-02-28_20-08-11.dat"

cd "/Equipment/TrolleyInterface/Settings"
mkdir "Sg382"
cd "/Equipment/TrolleyInterface/Settings/Sg382"
create DOUBLE "RF Frequency"
create DOUBLE "RF Amplitude"
set "RF Frequency" 61.766
set "RF Amplitude" 6.1

cd "/Equipment/TrolleyInterface/Settings"
mkdir "Trolley Power"
cd "/Equipment/TrolleyInterface/Settings/Trolley Power"
create INT "Voltage"
set "Voltage" 156

cd "/Equipment/TrolleyInterface/Settings"
mkdir "Run Config"
cd "/Equipment/TrolleyInterface/Settings/Run Config"
create STRING "Mode"
create STRING "Idle Mode"
create INT "Debug Level"
set "Mode" "Continuous"
set "Idle Mode" "Idle"
set "Debug Level" 0
mkdir "Continuous"
cd "/Equipment/TrolleyInterface/Settings/Run Config/Continuous"
create INT "Baseline Cycles"
set "Baseline Cycles" 1
cd "/Equipment/TrolleyInterface/Settings/Run Config"
mkdir "Idle"
cd "/Equipment/TrolleyInterface/Settings/Run Config/Idle"
create BOOL "Read Barcode"
set "Read Barcode" false
cd "/Equipment/TrolleyInterface/Settings/Run Config"
mkdir "Sleep"
cd "/Equipment/TrolleyInterface/Settings/Run Config/Sleep"
create BOOL "Power Down"
set "Power Down" false
cd "/Equipment/TrolleyInterface/Settings/Run Config"
mkdir "Interactive"
cd "/Equipment/TrolleyInterface/Settings/Run Config/Interactive"
create INT "Trigger"
create INT "Repeat"
set "Trigger" 0
set "Repeat" 1

cd "/Equipment/TrolleyInterface/Settings"
mkdir "Cycle"
cd "/Equipment/TrolleyInterface/Settings/Cycle"

create INT "Interface Comm Stop"
create INT "Trolley Comm Start"
create INT "Trolley Comm Data Start"
create INT "Trolley Comm Stop"
create INT "Switch To RF"
create INT "Power ON"
create INT "RF Enable"
create INT "Switch To Comm"
create INT "Interface Comm Start"
create INT "Cycle Length"
create INT "RF Prescale"
create INT "Switch RF Offset"
create INT "Switch Comm Offset"

set "Interface Comm Stop" 22 
set "Trolley Comm Start" 5250
set "Trolley Comm Data Start" 5350
set "Trolley Comm Stop" 14150 
set "Switch To RF" 14250
set "Power ON" 13250
set "RF Enable" 13750
set "Switch To Comm" 29350
set "Interface Comm Start" 29390
set "Cycle Length" 29410
set "RF Prescale" 62
set "Switch RF Offset" 100
set "Switch Comm Offset" 100

cd "/Equipment/TrolleyInterface/Settings"
mkdir "Probe"
cd "/Equipment/TrolleyInterface/Settings/Probe"
create STRING "Source[1][256]"
set Source "Odb"
create STRING "Script Dir[1][256]"
set "Script Dir" "/home/newg2/Applications/field-daq/online/TrolleyProbeScripts/"
create STRING "Script[1][256]"
set "Script" "Test"

mkdir "Probe0"
cd "Probe0"
create INT "Probe ID"
create INT "Probe Enable"
create INT "Preamp Delay" 
create INT "Preamp Period"
create INT "ADC Gate Delay"
create INT "ADC Gate Offset"
create INT "ADC Gate Period"
create INT "TX Delay"
create INT "TX Period"
create INT "User Data"
set "Probe ID" 0
set "Probe Enable" 1
set "Preamp Delay" 373 
set "Preamp Period" 12327
set "ADC Gate Delay" 0
set "ADC Gate Offset" 0
set "ADC Gate Period" 15000
set "TX Delay" 300
set "TX Period" 310
set "User Data" 8606
cd "/Equipment/TrolleyInterface/Settings/Probe"
cp "Probe0" "Probe1"
cp "Probe0" "Probe2"
cp "Probe0" "Probe3"
cp "Probe0" "Probe4"
cp "Probe0" "Probe5"
cp "Probe0" "Probe6"
cp "Probe0" "Probe7"
cp "Probe0" "Probe8"
cp "Probe0" "Probe9"
cp "Probe0" "Probe10"
cp "Probe0" "Probe11"
cp "Probe0" "Probe12"
cp "Probe0" "Probe13"
cp "Probe0" "Probe14"
cp "Probe0" "Probe15"
cp "Probe0" "Probe16"
set "Probe1/Probe ID" 1
set "Probe2/Probe ID" 2
set "Probe3/Probe ID" 3
set "Probe4/Probe ID" 4
set "Probe5/Probe ID" 5
set "Probe6/Probe ID" 6
set "Probe7/Probe ID" 7
set "Probe8/Probe ID" 8
set "Probe9/Probe ID" 9
set "Probe10/Probe ID" 10
set "Probe11/Probe ID" 11
set "Probe12/Probe ID" 12
set "Probe13/Probe ID" 13
set "Probe14/Probe ID" 14
set "Probe15/Probe ID" 15
set "Probe16/Probe ID" 16

cd "/Equipment/TrolleyInterface/Settings"
mkdir "Barcode"
cd "/Equipment/TrolleyInterface/Settings/Barcode"
create INT "LED Voltage"
create INT "LED Status"
create INT "Sample Period"
create INT "Acq Delay"
set "LED Voltage" 0
set "LED Status" 1
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
