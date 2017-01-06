mkdir "/Equipment/GalilPlatform/Settings"
mkdir "/Equipment/GalilPlatform/Monitors"
mkdir "/Equipment/GalilPlatform/AutoControl"
mkdir "/Equipment/GalilPlatform/ManualControl"

cd "/Equipment/GalilPlatform/Settings"
create STRING "CmdScript[1][256]"
set "CmdScript" "Platform"
create STRING "Script Directory[1][256]"
set "Script Directory" "/home/newg2/Applications/field-daq/online/GalilMotionScripts/"
create BOOL "Root Output"
set "Root Output" false
create STRING "Root Dir[1][256]"
set "Root Dir" "/home/newg2/Applications/field-daq/resources/Root/"

cd "/Equipment/GalilPlatform/Monitors"

create BOOL "Active"
create INT "Positions[4]"
create INT "Velocities[4]"
create INT "ControlVoltages[4]"
create BOOL "ReadyToMove"

set "ReadyToMove" FALSE

cd "/Equipment/GalilPlatform/ManualControl"
create INT "Cmd"
create INT "AbsPos[4]"
create INT "RelPos[4]"

cd "/Equipment/GalilPlatform/AutoControl"
create INT "RelPos[4]"
create INT "StepNumber[4]"

