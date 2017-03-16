mkdir "/Equipment/Surface Coils/Settings"
cd "/Equipment/Surface Coils/Settings"

create DOUBLE "Allowed Difference"
create DOUBLE "Bottom Set Currents[100]"
create DOUBLE "Top Set Currents[100]"

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
