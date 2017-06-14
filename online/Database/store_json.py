import psycopg2
import sys
import os
import datetime

file_name = sys.argv[1];
username = sys.argv[2];
dbname = sys.argv[3];
hostname = sys.argv[4];
port = sys.argv[5];

run_num =int(filter(str.isdigit,os.path.basename(file_name)));
print "Pouring Run No.{0} JSON File to Local Database [{1}]...".format(run_num,dbname),

json_text = open(file_name, "r").read();
conn = psycopg2.connect("host={0} dbname={1} user={2} port={3}".format(hostname,dbname,username,port));
c = conn.cursor();
c.execute("insert into gm2field_daq(run_num, json_data) values(%s, %s); commit", (run_num, json_text));
print(' Done!');

#c.execute("insert into gm2field_daq(run_num, json_data, \"time\") values(%s, %s, %s); commit", (run_num, json_text, datetime.datetime.now()));
