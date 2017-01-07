mkdir "/Settings/Hardware/sis_3302/template"
cd "/Settings/Hardware/sis_3302/template"
create INT base_address
set base_address 0x60000000

create BOOL user_led_on
set user_led_on y

create BOOL invert_ext_lemo
set invert_ext_lemo n

create BOOL enable_int_stop
set enable_int_stop y

create BOOL enable_ext_lemo
set enable_ext_lemo y

create BOOL enable_ext_clk
set enable_ext_clk y

create INT int_clk_setting_MHz
set int_clk_setting_MHz 10

create INT start_delay
create INT stop_delay

create BOOL enable_event_length_stop
set enable_event_length_stop y

create INT pretrigger_samples
set pretrigger_samples 0xfff

cd ..
cp template 0
