mkdir "/Settings/Hardware/dio_trigger/template"
cd "/Settings/Hardware/dio_trigger/template"

create CHAR dio_board_id
set dio_board_id d

create INT dio_port_num
set dio_port_num 0

create INT dio_trg_mask
set dio_trg_mask 0xff

cd ..
cp template 0
cp template 1
cp template 2
