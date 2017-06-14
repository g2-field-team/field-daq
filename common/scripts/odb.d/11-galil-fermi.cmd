mkdir "/Equipment/GalilFermi/Settings"
mkdir "/Equipment/GalilFermi/Monitors"
mkdir "/Equipment/GalilFermi/Alarms"
mkdir "/Equipment/GalilFermi/Settings/Auto Control"
mkdir "/Equipment/GalilFermi/Settings/Manual Control"
mkdir "/Equipment/GalilFermi/Settings/Emergency"
mkdir "/Equipment/GalilFermi/Settings/Interlock"

cd "/Equipment/GalilFermi/Settings/Emergency"
create INT Abort
set Abort 0

cd "/Equipment/GalilFermi/Settings"
create STRING "Cmd Script[1][256]"
set "Cmd Script" "FermiScript"
create STRING "Script Directory[1][256]"
set "Script Directory" "/home/newg2/Applications/field-daq/online/GalilMotionScripts/"
create BOOL "Root Output"
set "Root Output" false
create STRING "Root Dir[1][256]"
set "Root Dir" "/home/newg2/Applications/field-daq/resources/Root/"
create BOOL "Simulation Mode"
set "Simulation Mode" false

cd "/Equipment/GalilFermi/Monitors"
create BOOL "Monitor Thread Active"
create BOOL "Control Thread Active"
create INT "Time Stamp"
create INT "Positions[6]"
create INT "Velocities[6]"
create INT "Control Voltages[6]"
create INT "Analogs[6]"
create INT "Limit Switches Forward[6]"
create INT "Limit Switches Reverse[6]"
create BOOL "Motor Status[6]"
create INT "Auto Motion Finished"
create INT "Buffer Load"
create FLOAT "Motor Temperature Fish"
create FLOAT "Motor Temperature Sig"
create FLOAT "Motor Tension Fish"
create FLOAT "Motor Tension Sig"
create BOOL "Trolley Motion Allowed"
create BOOL "Garage Motion Allowed"
create BOOL "Trolley Parked"
set "Auto Motion Finished" 1

cd "/Equipment/GalilFermi/Settings/Manual Control"
create INT "Cmd"
set "Cmd" 0
mkdir "Trolley"
mkdir "Plunging Probe"
cd "/Equipment/GalilFermi/Settings/Manual Control/Trolley"
create INT "Garage Rel Pos"
create INT "Garage Velocity"
create BOOL "Garage Switch"
create INT "Trolley Rel Pos"
create INT "Trolley1 Rel Pos"
create INT "Trolley2 Rel Pos"
create INT "Trolley Velocity"
create FLOAT "Tension Range Low"
create FLOAT "Tension Range High"
create FLOAT "Tension Offset 1"
create FLOAT "Tension Offset 2"
create BOOL "Trolley Switch"
create INT "Trolley Def Pos1"
create INT "Trolley Def Pos2"
create INT "Garage Def Pos"
set "Garage Switch" false
set "Trolley Switch" false
set "Trolley Velocity" 100
set "Tension Range Low" 0.8
set "Tension Range High" 2.0
set "Tension Offset 1" 0.0
set "Tension Offset 2" 0.18
set "Trolley Def Pos1" 0
set "Trolley Def Pos2" 0
set "Garage Def Pos" 0
set "Garage Velocity" 1000
cd "/Equipment/GalilFermi/Settings/Manual Control/Plunging Probe"
create INT "Plunging Probe Abs Pos[3]"
create INT "Plunging Probe Rel Pos[3]"
create BOOL "Plunging Probe Switch"
set "Plunging Probe Switch" false


cd "/Equipment/GalilFermi/Settings/Auto Control"
create INT "Trigger Trolley"
create INT "Trigger Plunging Probe"
create STRING "Mode[1][256]"
create STRING "Continuous Motion[1][256]"
set "Trigger Trolley" 0
set "Trigger Plunging Probe" 0
set "Mode" "Continuous Motion"
set "Continuous Motion" "Full Run with Garage"

mkdir "Trolley"
mkdir "Plunging Probe"
cd "/Equipment/GalilFermi/Settings/Auto Control/Trolley"
create INT "Trolley Rel Pos"
create INT "Trolley Step Number"
cd "/Equipment/GalilFermi/Settings/Auto Control/Plunging Probe"
create INT "Plunging Probe Rel Pos[3]"
create INT "Plunging Probe Step Number[3]"

cd "/Equipment/GalilFermi/Settings/Interlock"
create BOOL "Expert Approve"
create BOOL "Garage In"
create BOOL "Garage Out"
create BOOL "Trolley and Garage Aligned"
create BOOL "Collimator and Fiber Harps Out"

cd "/Equipment/GalilFermi/Alarms"
create BOOL "Motion Trip"
create BOOL "Bad Motion Attempt"
