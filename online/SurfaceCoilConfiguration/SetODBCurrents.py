#Takes json file of current settings
import sys
import json
import midas

#Read the configuration file in                                                
#Access data as data["###"]         
filename = str(sys.argv[1])                                           
data = json.loads(open(filename).read())

#Set up to access odb                                                          
odb = midas.ODB('g2-field')

for i in range (1,101):
    bot_coil_str = "/Equipment/Surface Coils/Settings/Set Points/Bottom Set Currents[" + str(i-1) + "]"
    top_coil_str = "/Equipment/Surface Coils/Settings/Set Points/Top Set Currents[" + str(i-1) + "]"
    
    if i < 10:
        num = "00" + str(i)
    if 9 < i < 100:
        num = "0" + str(i)
    if i == 100:
        num = str(i)

    bot_str = "B-" + num
    top_str = "T-" + num
                                           
    odb.set_value(bot_coil_str, data[str(bot_str)])
    odb.set_value(top_coil_str, data[str(top_str)])
