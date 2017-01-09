mkdir "/Settings/Analysis/online_fid_params/fid"
cd "/Settings/Analysis/online_fid_params/fid"

create INT num_samples 1000000
set num_samples 1000000

create DOUBLE start_time
set start_time 0.0

create DOUBLE delta_time
set delta_time 0.0001

mkdir params
cd params

create INT fit_width
set fit_width 50

create INT zc_width
set zc_width 200

create INT edge_ignore
set edge_ignore 100

create DOUBLE start_thresh
set start_thresh 0.37

create DOUBLE max_phase_jump
set max_phase_jump 1.0

create DOUBLE low_pass_freq
set low_pass_freq 1000

create INT centroid_thresh
set centroid_thresh 0x0

create DOUBLE hyst_thresh
set hyst_thresh 0.3



