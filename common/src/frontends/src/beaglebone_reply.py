#
#Reply server
#connects reply socket to tcp://127.0.0.1:5555
#Gets current values from client
#Sets currents and sends back the new values
#

import time
import zmq
import RealCurr
import SetRealCurr
import sys

#Get set up with zmq
context = zmq.Context()
socket = context.socket(zmq.REP)
print "Binding to server"
socket.connect("tcp://127.0.0.1:5555")

#Receive the message                                                           
string = socket.recv_string()
if string.endswith('\0') : #Get rid of trailing null character                 
    string = string[:-1]
setCurrent1, setCurrent2, setCurrent3, setCurrent4 = string.split()

print("The received values were %s %s %s %s" % (setCurrent1, setCurrent2, setCurrent3, setCurrent4))

#Set the currents of the 4 channels
SetRealCurr.SetCurrents(setCurrent1, setCurrent2, setCurrent3, setCurrent4)

#Read the just set currents and send them back
#if len(sys.argv) <= 1:                                                        
#    t = 15 #If channels not specified, read from all 4 channels               
#else:                                                                         
#    t = int(sys.argv[1])                                                      
t = 15 #Read from all channels  

currString = RealCurr.GetCurr(t)
curr1, curr2, curr3, curr4 = currString.split()

#Send reply back                                                               
socket.send_string("%f %f %f %f\0" % (float(curr1), float(curr2), float(curr3), float(curr4)))
