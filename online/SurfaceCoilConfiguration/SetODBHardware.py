import json
import midas
import sys

#Read the configuration file in                                                                                         
#Access data as data["###"]                                                                                             
filename = str(sys.argv[1])
data = json.loads(open(filename).read())

#Set up to access odb
odb = midas.ODB('g2-field')

#Now we need to loop through all of the crates/boards
for j in range(1,7): #loop through crates
    crate_key = "/Settings/Hardware/Surface Coils/Crate " + str(j) + "/"
    
    for k in range (1,10): #loop through boards
        board_key = crate_key + "Driver Board " + str(k) + "/"

        for l in range (1,5): #loop through channels
            channel_key = board_key + "Channel " + str(l)
            ch_id = str(j) + str(k) + str(l) 
            if data.get(str(ch_id)): #Make sure that the key exists since we don't use all cards/channels
               odb.set_value(channel_key, data[str(ch_id)])

        

#odb.set_value(<key>, <value>)


