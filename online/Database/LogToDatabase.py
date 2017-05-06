#Takes json file of current settings
import sys
import midas

#Set up to access odb                                                          
odb = midas.ODB('g2-field')

x = odb.get_value('/RunInfo/Run number')
print x
