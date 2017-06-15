mkdir "/Equipment/Fluxgate"

mkdir "/Equipment/Fluxgate/Settings"
cd "/Equipment/Fluxgate/Settings"
create FLOAT aqRate
set aqRate 8000
create FLOAT aqTime
set aqTime 60
create INT sampsPerChanToAcquire
set sampsPerChanToAcquire 480000
create STRING physicalChannelDC
set physicalChannelDC "Dev1/ai12:23"
create STRING physicalChannelAC
set physicalChannelAC "Dev1/ai12:23"
create FLOAT minVolDC
set minVolDC -10
create FLOAT maxVolDC
set maxVolDC 10
create FLOAT minVolAC
set minVolAC -1
create FLOAT maxVolAC
set maxVolAC 1
	
mkdir "/Equipment/Fluxgate/Alerts"
cd "/Equipment/Fluxgate/Alerts"
create FLOAT DCSetPointMag
set DCSetPointMag 10.0
create FLOAT ACSetPointMag
set ACSetPointMag 1.0
create BOOL DCRails[24]
set DCRails[*] n
create BOOL ACRails[24]
set ACRails[*] n
create BOOL DCSetPoint[24]
set DCSetPoint[*] n
create BOOL ACSetPoint[24]
set DCSetPoint[*] n
create BOOL FFTon
set FFTon y
