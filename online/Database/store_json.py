import psycopg2
import sys
import os 


file_name = sys.argv[1];
dbname = sys.argv[2];

run_num =int(filter(str.isdigit,os.path.basename(file_name)));
print(run_num);
print(dbname);
print "Pouring Run No.{0} JSON File to Local Database [{1}]...".format(run_num,dbname),

json_text = open(file_name, "r").read();
conn = psycopg2.connect("host={0} dbname={1} port=5432".format('g2db-priv',dbname));
c = conn.cursor();
c.execute("insert into gm2field_daq(run_num, json_data) values(%s, %s); commit", (run_num, json_text));
print(' Done!');
