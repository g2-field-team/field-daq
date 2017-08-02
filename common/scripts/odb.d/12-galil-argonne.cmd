mkdir "/Equipment/Galil-Argonne/Settings"
mkdir "/Equipment/Galil-Argonne/Monitors"
mkdir "/Equipment/Galil-Argonne/Alarms"
mkdir "/Equipment/Galil-Argonne/Settings/Auto Control"
mkdir "/Equipment/Galil-Argonne/Settings/Manual Control"
mkdir "/Equipment/Galil-Argonne/Settings/Emergency"
mkdir "/Equipment/Galil-Argonne/Settings/Interlock"

cd "/Equipment/Galil-Argonne/Settings/Emergency"
create INT Abort
set Abort 0

cd "/Equipment/Galil-Argonne/Settings"
create STRING "Cmd Script[1][256]"
set "Cmd Script" "ArgonneScript"
create STRING "Script Directory[1][256]"
set "Script Directory" "/home/newg2/Applications/field-daq/online/GalilMotionScripts/"
create BOOL "Root Output"
set "Root Output" false
create STRING "Root Dir[1][256]"
set "Root Dir" "/home/newg2/Applications/field-daq/resources/Root/"
create BOOL "Simulation Mode"
set "Simulation Mode" false

cd "/Equipment/Galil-Argonne/Monitors"
create BOOL "Monitor Thread Active"
create BOOL "Control Thread Active"
create DOUBLE "Time Stamp"
create INT "Positions[6]"
create INT "Velocities[6]"
create INT "Control Voltages[6]"
create INT "Analogs[6]"
create INT "Limit Switches Forward[6]"
create INT "Limit Switches Reverse[6]"
create BOOL "Motor Status[6]"
create INT "Auto Motion Finished"
create INT "Buffer Load"
create BOOL "Motion Allowed"
set "Auto Motion Finished" 1

cd "/Equipment/Galil-Argonne/Settings/Manual Control"
create INT "Cmd"
set "Cmd" 0
mkdir "Platform1"
mkdir "Platform2"
cd "/Equipment/Galil-Argonne/Settings/Manual Control/Platform2"
create INT "Platform2 Rel Pos"
create INT "Platform2 Abs Pos"
create INT "Platform2 Def Pos"
create BOOL "Platform2 Switch"
cd "/Equipment/Galil-Argonne/Settings/Manual Control/Platform1"
create INT "Platform1 Rel Pos[4]"
create INT "Platform1 Abs Pos[4]"
create INT "Platform1 Def Pos[4]"
create BOOL "Platform1 Switch"

cd "/Equipment/Galil-Argonne/Settings/Auto Control"
create INT "Trigger Platform1"
create INT "Trigger Platform2"
create STRING "Mode[1][256]"
set "Trigger Platform2" 0
set "Trigger Platform1" 0
set "Mode" "Self Scan"

mkdir "Platform1"
mkdir "Platform2"
cd "/Equipment/Galil-Argonne/Settings/Auto Control/Platform2"
create INT "Platform2 Rel Pos"
create INT "Platform2 Step Number"
cd "/Equipment/Galil-Argonne/Settings/Auto Control/Platform1"
create INT "Platform1 Rel Pos[4]"
create INT "Platform1 Step Number[4]"

cd "/Equipment/Galil-Argonne/Settings/Interlock"
create BOOL "Expert Approve"

cd "/Equipment/Galil-Argonne/Alarms"
create BOOL "Bad Motion Attempt"
create BOOL "Motion Trip"
