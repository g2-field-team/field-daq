create STRING "Experiment/Menu Buttons"
set "Experiment/Menu Buttons" "Status, ODB, Messages, Alarms, Programs, History, MSCB, Help"
create BOOL "/Experiment/Run Parameters/Root Output"
set "/Experiment/Run Parameters/Root Output" n

create DWORD "/Experiment/MAX_EVENT_SIZE"
set "/Experiment/MAX_EVENT_SIZE" 0x1000000

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

cd "/"
mkdir "Settings"
