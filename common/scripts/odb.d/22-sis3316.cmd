mkdir "/Settings/Hardware/sis_3316/template"
cd "/Settings/Hardware/sis_3316/template"

create INT base_address
set base_address 0x20000000

create BOOL enable_ext_trg
set enable_ext_trg y

create BOOL enable_int_trg
set enable_int_trg y

create BOOL invert_ext_trg
set invert_ext_trg n

create BOOL enable_ext_clk
set enable_ext_clk y

create INT oscillator_hs
set oscillator_hs 5

create INT oscillator_n1
set oscillator_n1 8

create INT io_tap_delay
set io_tap_delay 0x1020

create BOOL set_voltage_offset
set set_voltage_offset y

create INT dac_voltage_offset
set dac_voltage_offset 0x8000

create INT pretrigger_samples
set pretrigger_samples 0x1000

cd ..
cp template 0
