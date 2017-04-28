import psycopg2
import sys
import os 
import midas

#Set up to access odb                                                          
odb = midas.ODB('g2-field')

# Start collecting information from ODB first
Run_number = odb.get_value('/RunInfo/Run number')
Data_dir = odb.get_value('/Logger/Data dir')
filename = Data_dir.strip() + 'run' + '{0:05d}'.format(int(Run_number)) + '.json'

dbname = sys.argv[1];

run_num =int(filter(str.isdigit,os.path.basename(file_name)));
print(run_num);
print(dbname);
print "Pouring Run No.{0} JSON File to Local Database [{1}]...".format(run_num,dbname),

json_text = open(file_name, "r").read();
conn = psycopg2.connect("host={0} dbname={1} port=5432".format('g2db-priv',dbname));
c = conn.cursor();
c.execute("insert into gm2field_daq(run_num, json_data) values(%s, %s); commit", (run_num, json_text));
print(' Done!');
