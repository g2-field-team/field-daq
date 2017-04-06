#                                                                              
# Beaglebone server                                                            
# Connects SUB socket to tcp://*:5550                                          
# Connects PUB socket to tcp://*:5551
# Gets current values from client and set currents.                         
# Publishes currents and temperatures   
#                                                                              

import time
import zmq
import subprocess
import json
import sys
import REOCurr
import REOTemp
import REOSetCurr

context = zmq.Context()
socketSub = context.socket(zmq.SUB)
print "Connecting to sub socket"
socketSub.connect("tcp://127.0.0.1:5550")
print "Connected successfully"

print "Connecting to pub socket"
socketPub = context.socket(zmq.PUB)
socketPub.connect("tcp://localhost:5551")
print "Connected successfully"

#subsribe to all incoming messages                       
socketSub.setsockopt(zmq.SUBSCRIBE, '')

while True:
    if socketSub.poll(500)!=0: #Check if there is data. Times out in 500ms
        message = socketSub.recv_json()
        
        for i in range (1,10): #9 boards
            #Set up the board
            card = i+1 #Hardware uses cards 2-10 instead of 1-9
            subprocess.call(['SelCard.sh', str(card)])
                       
            for j in range (1,5): #4 channels/board
                hw_id = '1' + str(i) + str(j)
                if j == 1:
                    chan = 1
                if j == 2:
                    chan = 2
                if j == 3:
                    chan = 4
                if j == 4:
                    chan = 8
                REOSetCurr.SetCurrent(chan, message[hw_id])
                
    data = {}
    for i in range (1,10): #9 driver boards
        card = i
        subprocess.call(['SelCard.sh', str(card)])
       
        for j in range (1,5): #4 channels/board                                
            hw_id = '1'+str(i)+str(j)
            if j == 1:
                chan = 1
            if j == 2:
                chan = 2
            if j == 3:
                chan = 4
            if j == 4:
                chan = 8
            current = REOCurr.GetCurr(chan)
            temperature = REOTemp.GetTemp(chan)
            data[hw_id] = [float(current),float(temperature)]

    socketPub.send_json(data)
    print "Sent a message, clearing dictionary"
    data.clear()
    time.sleep(0.5)
