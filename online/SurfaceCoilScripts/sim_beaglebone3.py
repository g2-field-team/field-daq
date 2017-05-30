#                                                                              
# Beaglebone server                                                            
# Connects REP socket to tcp://*:5553                                          
# Connects PUB socket to tcp://*:5550
# Gets current values from client and set currents.                         
# Publishes currents and temperatures   
#                                                                              

import time
import zmq
from random import uniform
import json

context = zmq.Context()
socketRep = context.socket(zmq.REP)
print "Connecting to rep socket"
socketRep.connect("tcp://127.0.0.1:5553")
print "Connected successfully"

print "Connecting to pub socket"
socketPub = context.socket(zmq.PUB)
socketPub.connect("tcp://127.0.0.1:5550")
print "Connected successfully"

#subsribe to all incoming messages                       
#socketSub.setsockopt(zmq.SUBSCRIBE, '')

while True:
    if socketRep.poll(500)!=0: #Check if there is data. Times out in 500ms
        print "poll returned true"
        message = socketRep.recv_json()
        reply = socketRep.send("Completed")
        print "Message: ", message
    data = {}
    for i in range (1,6): #9 driver boards                                    
        for j in range (1,5): #4 channels/board                                
            hw_id = '3'+str(i)+str(j)
            current = uniform(-0.001,0.001)
            temperature = 40.0 + uniform(-10.0,10.0)
            data[hw_id] = [current,temperature]
    socketPub.send_json(data)
    print "Sent a message, clearing dictionary"
    #print data
    data.clear()
    time.sleep(3)
