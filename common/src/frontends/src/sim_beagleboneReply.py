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
print "Connected successfully"

#Receive the message
print "About to receive currents"
string = socket.recv_string()
print "Received currents"
print string
print "Did that work?"
if string.endswith('\0') : #Get rid of trailing null character 
    string = string[:-1]
setCurrent1, setCurrent2, setCurrent3, setCurrent4 = string.split()

print("The received values were %s %s %s %s" % (setCurrent1, setCurrent2, setCurrent3, setCurrent4))

rand1 = uniform(-0.1,0.1)
rand2 = uniform(-0.1,0.1)
rand3 = uniform(-0.1,0.1)
rand4 = uniform(-0.1,0.1)

currentVal1 = float(setCurrent1) + rand1
currentVal2 = float(setCurrent2) + rand2
currentVal3 = float(setCurrent3) + rand3
currentVal4 = float(setCurrent4) + rand4

#Do some "work"                                                                
time.sleep(1)

#Send reply back                                                               
socket.send_string("%f %f %f %f\0" % (currentVal1, currentVal2, currentVal3, currentVal4))
