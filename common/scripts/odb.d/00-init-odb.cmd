create STRING "Experiment/Menu Buttons"
set "Experiment/Menu Buttons" "Status, ODB, Messages, Alarms, Programs, History, MSCB, Help"

create DWORD "/Experiment/MAX_EVENT_SIZE"
set "/Experiment/MAX_EVENT_SIZE" 0x10000000

mkdir "Equipment"
cd "Equipment"
load EquipmentInit.xml

cd "/"
mkdir "Logger"
cd "Logger"
load LoggerInit.xml

cd  "/"
mkdir "Programs"
cd "Programs"
load ProgramsInit.xml

cd  "/"
mkdir "Lazy"
cd "Lazy"
load LazyloggerInit.xml

cd "/"
mkdir "Settings"
