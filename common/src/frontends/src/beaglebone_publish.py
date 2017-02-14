#Test program
#Reads temperature and currents from specified channels
#Connects PUB socket to tcp://127.0.0.1:5556

import RealTemp
import RealCurr
import sys
import zmq
import time

#Get set up with zmq 
context = zmq.Context()
socket = context.socket(zmq.PUB)
socket.connect("tcp://127.0.0.1:5556")

#Set up for reading data
if len(sys.argv) <= 1:
    t = 15 #If channels not specified, read from all 4 channels
else:
    t = int(sys.argv[1])

while True:
    tempString = RealTemp.GetTemp(t)
    temp1, temp2, temp3, temp4 = tempString.split()
    currString = RealCurr.GetCurr(t)
    curr1, curr2, curr3, curr4 = currString.split()
    socket.send_string("%f %f %f %f %f %f %f %f" % (float(curr1), float(temp1), float(curr2), float(temp2), float(curr3), float(temp3), float(curr4), float(temp4)))
    print "Sent a message"
    time.sleep(1)

