mkdir "/Experiment/Edit on Start"
cd "/Experiment/Edit on Start"

create STRING "Comment[1][256]"
create STRING "Quality[1][256]"
create STRING "Shift crew[1][256]"
create STRING "Shift[1][256]"
create BOOL "Trolley ON"
create BOOL "Fixed Probe ON"
create BOOL "Plunging Probe ON"
create BOOL "Surface Coil ON"
create BOOL "Flux Gate ON"


cd "/Experiment/Status items"
ln "/Experiment/Edit on Start/Trolley ON" "Trolley Status"

mkdir "/Experiment/Source code git hash"
cd "/Experiment/Source code git hash"
create STRING "daq git hash[1][256]"
create STRING "core git hash[1][256]"
create STRING "analysis git hash[1][256]"
create STRING "signal lib git hash[1][256]"
