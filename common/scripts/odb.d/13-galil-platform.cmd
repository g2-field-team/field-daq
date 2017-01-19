mkdir "/Equipment/GalilPlatform/Settings"
mkdir "/Equipment/GalilPlatform/Monitors"
mkdir "/Equipment/GalilPlatform/AutoControl"
mkdir "/Equipment/GalilPlatform/ManualControl"
mkdir "/Equipment/GalilPlatform/Emergency"

cd "/Equipment/GalilPlatform/Emergency"
create INT Abort
create INT Reset
set Abort 0
set Reset 0

cd "/Equipment/GalilPlatform/Settings"
create STRING "CmdScript[1][256]"
set "CmdScript" "Platform"
create STRING "Script Directory[1][256]"
set "Script Directory" "/home/newg2/Applications/field-daq/online/GalilMotionScripts/"
create BOOL "Root Output"
set "Root Output" false
create STRING "Root Dir[1][256]"
set "Root Dir" "/home/newg2/Applications/field-daq/resources/Root/"
create BOOL "TestMode"
set "TestMode" false

cd "/Equipment/GalilPlatform/Monitors"

create BOOL "Active"
create INT "Positions[4]"
create INT "Velocities[4]"
create INT "ControlVoltages[4]"
create INT "GoalDist[4]"
create INT "LimF[4]"
create INT "LimR[4]"
create BOOL "ReadyToMove"
create INT "Finished"

set "ReadyToMove" FALSE
set "Finished" 1

cd "/Equipment/GalilPlatform/ManualControl"
create INT "Cmd"
create INT "AbsPos[4]"
create INT "RelPos[4]"

cd "/Equipment/GalilPlatform/AutoControl"
create INT "RelPos[4]"
create INT "StepNumber[4]"

