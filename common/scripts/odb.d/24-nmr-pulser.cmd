mkdir "/Settings/Hardware/nmr_pulser/template"
cd "/Settings/Hardware/nmr_pulser/template"

create CHAR dio_board_id
set dio_board_id b

create INT dio_port_num
set dio_port_num 0x3

create INT dio_trg_mask
set dio_trg_mask 0xf

cd ..
cp template 0
