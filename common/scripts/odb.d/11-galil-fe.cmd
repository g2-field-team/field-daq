mkdir "/Equipment/Galil/Settings"
mkdir "/Equipment/Galil/Monitor"

cd "/Equipment/Galil/Settings"
create STRING "CmdScript[1][256]"
set "CmdScript" "GalilRunScript"
create STRING "Script Directory[1][256]"
set "Script Directory" "/home/newg2/Applications/field-daq/online/GalilMotionScripts/"
create BOOL "Root Output"
set "Root Output" false
create STRING "Root Dir[1][256]"
set "Root Dir" "/home/newg2/Applications/field-daq/resources/Root/"

cd "/Equipment/Galil/Monitor"

create BOOL "Read Thread Active"
create INT "Read Loop Index"
create INT "Buffer Load"
create DOUBLE "Motor Positions[3]"
create DOUBLE "Motor Velocities[3]"
create DOUBLE "Tensions[2]"

cd /
cp "/Equipment/Galil" "/Equipment/SimGalil"
cd "/Equipment/SimGalil/Common"
set "Frontend name" "Sim Galil Motion Control"
set "Frontend file name" "src/sim_galil.cxx"
set "Status" "Sim Galil Motion Control@localhost"
