cd "/Equipment/Surface Coils/Settings"
create BOOL "Root Output"
set "Root Output" false
create STRING "Root Dir[1][256]"
set "Root Dir" "/home/newg2/Applications/field-daq/resources/"

cd
mkdir "/Equipment/Surface Coils/Settings/Set Points"
cd "/Equipment/Surface Coils/Settings/Set Points"

create DOUBLE "Allowed Difference"
set "Allowed Difference" .0053
create DOUBLE "Allowed Temperature"
set "Allowed Temperature" 50.0
create DOUBLE "Bottom Set Currents[100]"
create DOUBLE "Top Set Currents[100]"

cd
mkdir "/Equipment/Surface Coils/Settings/Calibration"
cd "/Equipment/Surface Coils/Settings/Calibration"

create DOUBLE "Bottom Intercepts[100]"
create DOUBLE "Top Intercepts[100]"
create DOUBLE "Bottom Slopes[100]"
create DOUBLE "Top Slopes[100]"

cd
mkdir "/Equipment/Surface Coils/Settings/Monitoring"
cd "/Equipment/Surface Coils/Settings/Monitoring"

create BOOL "Current Health"
set "Current Health" true
create BOOL "Temp Health"
set "Temp Health" true
create STRING "Problem Channel"

cd
mkdir "/Equipment/Surface Coils/Settings/Monitoring/Currents"
cd "/Equipment/Surface Coils/Settings/Monitoring/Currents"

create DOUBLE "Bottom Currents[100]"
create DOUBLE "Top Currents[100]"

cd 
mkdir "/Equipment/Surface Coils/Settings/Monitoring/Temperatures"
cd "/Equipment/Surface Coils/Settings/Monitoring/Temperatures"

create DOUBLE "Bottom Temps[100]"
create DOUBLE "Top Temps[100]"

cd

mkdir "/Settings/Hardware/Surface Coils/crate_template"
cd "/Settings/Hardware/Surface Coils/crate_template"

mkdir "Driver Board 1"
cd "Driver Board 1"
create STRING "Channel 1"
create STRING "Channel 2"
create STRING "Channel 3"
create STRING "Channel 4"

cd ..
mkdir "Driver Board 2"
cd "Driver Board 2"
create STRING "Channel 1"
create STRING "Channel 2"
create STRING "Channel 3"
create STRING "Channel 4"

cd ..
mkdir "Driver Board 3"
cd "Driver Board 3"
create STRING "Channel 1"
create STRING "Channel 2"
create STRING "Channel 3"
create STRING "Channel 4"

cd ..
mkdir "Driver Board 4"
cd "Driver Board 4"
create STRING "Channel 1"
create STRING "Channel 2"
create STRING "Channel 3"
create STRING "Channel 4"

cd ..
mkdir "Driver Board 5"
cd "Driver Board 5"
create STRING "Channel 1"
create STRING "Channel 2"
create STRING "Channel 3"
create STRING "Channel 4"

cd ..
mkdir "Driver Board 6"
cd "Driver Board 6"
create STRING "Channel 1"
create STRING "Channel 2"
create STRING "Channel 3"
create STRING "Channel 4"

cd ..
mkdir "Driver Board 7"
cd "Driver Board 7"
create STRING "Channel 1"
create STRING "Channel 2"
create STRING "Channel 3"
create STRING "Channel 4"

cd ..
mkdir "Driver Board 8"
cd "Driver Board 8"
create STRING "Channel 1"
create STRING "Channel 2"
create STRING "Channel 3"
create STRING "Channel 4"

cd ..
mkdir "Driver Board 9"
cd "Driver Board 9"
create STRING "Channel 1"
create STRING "Channel 2"
create STRING "Channel 3"
create STRING "Channel 4"


cd
cd "/Settings/Hardware/Surface Coils"

cp crate_template "Crate 1"
cp crate_template "Crate 2"
cp crate_template "Crate 3"
cp crate_template "Crate 4"
cp crate_template "Crate 5"
cp crate_template "Crate 6"
