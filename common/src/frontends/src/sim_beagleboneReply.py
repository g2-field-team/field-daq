#
# Reply server
# Binds REPLY socket to tcp://*:5555
# Gets current values from client.
# "Sets" currents and sends back the new values
#

import time
import zmq
from random import uniform

context = zmq.Context()
socket = context.socket(zmq.REP)
print "Binding to server"
socket.bind("tcp://*:5555")

#Receive the message
string = socket.recv_string()
if string.endswith('\0') : #Get rid of trailing null character 
    string = string[:-1]
setCurrent1, setCurrent2 = string.split()

print("The received values were %s %s" % (setCurrent1, setCurrent2))

rand1 = uniform(-0.1,0.1)
rand2 = uniform(-0.1,0.1)

currentVal1 = float(setCurrent1) + rand1
currentVal2 = float(setCurrent2) + rand2

#Do some "work"                                                                
time.sleep(1)

#Send reply back                                                               
socket.send_string("%f %f\0" % (currentVal1, currentVal2))
