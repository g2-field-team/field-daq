mkdir "/Equipment/Fixed Probes/Settings/output"
cd "/Equipment/Fixed Probes/Settings/output"

create BOOL write_midas
set write_midas y

create BOOL write_root
set write_root y

create STRING root_path
set root_path "/home/newg2/Applications/field-daq/resources/root"

create STRING root_file
set root_file "fixed_probe_run_%05i.root"

create BOOL write_full_waveforms
set write_full_waveforms false

create INT full_waveform_subsampling
set full_waveform_subsampling 100

mkdir "/Equipment/Fixed Probes/Settings/devices/sis_3302"
mkdir "/Equipment/Fixed Probes/Settings/devices/sis_3316"
mkdir "/Equipment/Fixed Probes/Settings/devices/dio_triggers"
cd "/Equipment/Fixed Probes/Settings/devices"

ln "/Settings/Hardware/sis_3316/0" "sis_3316/sis_3316_0"
ln "/Settings/Hardware/sis_3302/0" "sis_3316/sis_3302_0"
ln "/Settings/Hardware/dio_trigger/0" "dio_triggers/trg_0"
ln "/Settings/Hardware/dio_trigger/1" "dio_triggers/trg_1"
ln "/Settings/Hardware/dio_trigger/2" "dio_triggers/trg_2"

mkdir "/Equipment/Fixed Probes/Settings/config"
cd "/Equipment/Fixed Probes/Settings/config"

ln "/Settings/NMR Sequence/Fixed Probes" mux_sequence
ln "/Settings/Hardware/mux_connections" mux_connections
ln "/Settings/Analysis/online_fid_params" fid_analysis

cd "/Equipment/Fixed Probes/Settings"

create INT max_event_time
set max_event_time 150000

create INT min_event_time
set max_event_time 50000

create INT mux_switch_time
set mux_switch_time 10000

create BOOL analyze_fids_online
set analyze_fids_online y

create BOOL use_fast_fids_class
set use_fast_fids_class n

create BOOL simulation_mode
set simulation_mode false

create BOOL generate_software_triggers
set generate_software_triggers false

create STRING logfile
set logfile "/var/log/g2field/fixed-probes.log"


