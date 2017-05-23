mkdir "/Equipment/Fluxgate/Parameters"
cd "/Equipment/Fluxgate/Parameters"

create FLOAT "Minimum Voltage DC"
set "Minimum Voltage DC" -10.0
create FLOAT "Maximum Voltage DC"
set "Minimum Voltage DC" 10.0

create FLOAT "Minimum Voltage AC"
set "Minimum Voltage DC" -1.0
create FLOAT "Maximum Voltage AC"
set "Minimum Voltage DC" 1.0

create FLOAT "Sample Rate"
set "Sample Rate" 2000.0
create FLOAT "Acquisition Time"
set "Acquisition Time" 60.0

mkdir "/Equipment/Fluxgate/Alarms"
cd "/Equipment/Fluxgate/Alarms"

create INT DCRails[24]
set DCRails[*] 0
create INT ACRails[24]
set ACRails[*] 0

create INT DCSetPoint[24]
set DCSetPoint[*] 0
create INT ACSetPoint[24]
set DCSetPoint[*] 0


mkdir "/Equipment/Fluxgate/Settings"
cd "/Equipment/Fluxgate/Settings"

create FLOAT "DC Setpoint Magnitude"
set "DC Setpoint Magnitude" 10.0
create FLOAT "AC Setpoint Magnitude"
set "AC Setpoint Magnitude" 1.0
create INT "FFT on"
set "FFT on" 1
