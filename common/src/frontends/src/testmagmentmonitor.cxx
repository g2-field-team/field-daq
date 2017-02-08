/********************************************************************\

  Name:         frontend.c
  Created by:   Stefan Ritt

  Contents:     Experiment specific readout code (user part) of
                Midas frontend. This example simulates a "trigger
                event" and a "scaler event" which are filled with
                CAMAC or random data. The trigger event is filled
                with two banks (ADC0 and TDC0), the scaler event
                with one bank (SCLR).

  $Id$

\********************************************************************/

#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include <stdlib.h>

#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/timeb.h>

#include "midas.h"
#include "mcstd.h"
//#include "experim.h"

using namespace std;

/* make frontend functions callable from the C framework */
#ifdef __cplusplus
extern "C" {
#endif

/*-- Globals -------------------------------------------------------*/

/* The frontend name (client name) as seen by other MIDAS clients   */
char *frontend_name = "TestMagnetMonitor";
/* The frontend file name, don't change it */
char *frontend_file_name = __FILE__;

/* frontend_loop is called periodically if this variable is TRUE    */
BOOL frontend_call_loop = FALSE;

/* a frontend status page is displayed with this frequency in ms */
INT display_period = 3000;

/* maximum event size produced by this frontend */
INT max_event_size = 10000;

/* maximum event size for fragmented events (EQ_FRAGMENTED) */
INT max_event_size_frag = 5 * 1024 * 1024;

/* buffer size to hold events */
INT event_buffer_size = 100 * 10000;

//Terminal IO
int fSerialPort1_ptr;
int fSerialPort2_ptr;
int fSerialPort3_ptr;

//My file IO
ofstream output1_3;
ofstream output2;

//ODB
HNDLE hDB;

/*-- Function declarations -----------------------------------------*/

INT frontend_init();
INT frontend_exit();
INT begin_of_run(INT run_number, char *error);
INT end_of_run(INT run_number, char *error);
INT pause_run(INT run_number, char *error);
INT resume_run(INT run_number, char *error);
INT frontend_loop();

INT read_CompressorChiller_event(char *pevent, INT off);
INT read_HeLevel_event(char *pevent, INT off);

INT poll_event(INT source, INT count, BOOL test);
INT interrupt_configure(INT cmd, INT source, POINTER_T adr);
int SendWarning(string message);
int CompressorControl(int onoff);
int ChillerControl(int onoff);

/*-- Equipment list ------------------------------------------------*/

EQUIPMENT equipment[] = {

   {"CompressorChiller",                /* equipment name */
    {1, 0,                   /* event ID, trigger mask */
     "SYSTEM",               /* event buffer */
     EQ_PERIODIC,            /* equipment type */
     0,                      /* event source */
     "MIDAS",                /* format */
     TRUE,                   /* enabled */
     RO_RUNNING | RO_TRANSITIONS |   /* read when running and on transitions */
     RO_ODB,                 /* and update ODB */
     30000,                  /* read every 1 hour */
     0,                      /* stop run after this event limit */
     0,                      /* number of sub events */
     0,                      /* log history */
     "", "", "",},
    read_CompressorChiller_event,       /* readout routine */
    },

   {"HeLevel",                /* equipment name */
    {2, 0,                   /* event ID, trigger mask */
      "SYSTEM",               /* event buffer */
      EQ_PERIODIC,            /* equipment type */
      0,                      /* event source */
      "MIDAS",                /* format */
      TRUE,                   /* enabled */
      RO_RUNNING | RO_TRANSITIONS |   /* read when running and on transitions */
	RO_ODB,                 /* and update ODB */
      30000,                  /* read every 10 hour */
      0,                      /* stop run after this event limit */
      0,                      /* number of sub events */
      0,                      /* log history */
      "", "", "",},
    read_HeLevel_event,       /* readout routine */
   },

   {""}
};

#ifdef __cplusplus
}
#endif

/********************************************************************\
  Callback routines for system transitions

  These routines are called whenever a system transition like start/
  stop of a run occurs. The routines are called on the following
occations:

frontend_init:  When the frontend program is started. This routine
should initialize the hardware.

frontend_exit:  When the frontend program is shut down. Can be used
to releas any locked resources like memory, commu-
nications ports etc.

begin_of_run:   When a new run is started. Clear scalers, open
rungates, etc.

end_of_run:     Called on a request to stop a run. Can send
end-of-run event and close run gates.

pause_run:      When a run is paused. Should disable trigger events.

resume_run:     When a run is resumed. Should enable trigger events.
\********************************************************************/

/*-- Frontend Init -------------------------------------------------*/

INT frontend_init()
{
  char devname[100];
  struct termios options1;
  struct termios options2;
  struct termios options3;

  //Init port 1
  int fBufferSize = 256;
  char *fWriteBufferAg;
  char *fReadBufferAg;

  int c;
  char buf[2000];

  sprintf(devname, "/dev/ttyUSB0");

  //output1.open("/home/newg2/TestMagnetMonitor/Data/Port1DirectOutput.txt", ios::out);
  //cout << "About to connect to serial port 1" <<endl;
  
  fSerialPort1_ptr = open(devname, O_RDWR | O_NOCTTY | O_NDELAY);

  if(fSerialPort1_ptr < 0) {
    perror("Error when opening device port1");
    return -1;
  }
  fcntl(fSerialPort1_ptr, F_SETFL, 0); // return immediately if no data

  if(tcgetattr(fSerialPort1_ptr, &options1) < 0) {
    perror("Error tcgetattr port1");
    return -2;
  }

  cfsetospeed(&options1, B4800);
  cfsetispeed(&options1, B4800);

  options1.c_cflag     &=  ~PARENB;        // Make 8n1
  options1.c_cflag     &=  ~CSTOPB;
  options1.c_cflag     &=  ~CSIZE;
  options1.c_cflag     |=  CS8;

  if(tcsetattr(fSerialPort1_ptr, TCSANOW, &options1) < 0) {
    perror("Error tcsetattr port1");
    return -3;
  }
  tcflush( fSerialPort1_ptr, TCIOFLUSH );

  //Init port3
  sprintf(devname, "/dev/ttyUSB2");
  //output3.open("/home/newg2/TestMagnetMonitor/Data/Port3DirectOutput.txt", ios::out);
  //cout << "About to connect to serial port 3" <<endl;
  fSerialPort3_ptr = open(devname, O_RDWR | O_NOCTTY | O_NDELAY);

  if(fSerialPort3_ptr < 0) {
    perror("Error opening device port3");
    return -1;
  }
  fcntl(fSerialPort3_ptr, F_SETFL, 0); // return immediately if no data

  if(tcgetattr(fSerialPort3_ptr, &options3) < 0) {
    perror("Error tcgetattr port3");
    return -2;
  }

  cfsetospeed(&options3, B9600);
  cfsetispeed(&options3, B9600);

  options3.c_cflag     &=  ~PARENB;        // Make 8n1
  options3.c_cflag     &=  ~CSTOPB;
  options3.c_cflag     &=  ~CSIZE;
  options3.c_cflag     |=  CS8;

  if(tcsetattr(fSerialPort3_ptr, TCSANOW, &options3) < 0) {
    perror("Error tcsetattr port3");
    return -3;
  }
  tcflush( fSerialPort3_ptr, TCIOFLUSH );


  //Init Port 2
  sprintf(devname, "/dev/ttyUSB1");
  //output3.open("/home/newg2/TestMagnetMonitor/Data/Port3DirectOutput.txt", ios::out);
  //cout << "About to connect to serial port 3" <<endl;
  fSerialPort2_ptr = open(devname, O_RDWR | O_NOCTTY | O_NDELAY);

  if(fSerialPort2_ptr < 0) {
    perror("Error opening device port3");
    return -1;
  }
  fcntl(fSerialPort2_ptr, F_SETFL, 0); // return immediately if no data

  if(tcgetattr(fSerialPort2_ptr, &options3) < 0) {
    perror("Error tcgetattr port2");
    return -2;
  }

  cfsetospeed(&options3, B4800);
  cfsetispeed(&options3, B4800);

  options2.c_cflag     &=  ~PARENB;        // Make 8n1
  options2.c_cflag     &=  ~CSTOPB;
  options2.c_cflag     &=  ~CSIZE;
  options2.c_cflag     |=  CS8;
  options2.c_cflag     &=  ~CRTSCTS;       // no flow control

  tcflush( fSerialPort2_ptr, TCIFLUSH );

  if(tcsetattr(fSerialPort2_ptr, TCSANOW, &options2) < 0) {
    perror("Error tcsetattr port2");
    return -3;
  }
  return SUCCESS;
}

/*-- Frontend Exit -------------------------------------------------*/

INT frontend_exit()
{
   close(fSerialPort1_ptr);
   close(fSerialPort2_ptr);
   close(fSerialPort3_ptr);
   return SUCCESS;
}

/*-- Begin of Run --------------------------------------------------*/

INT begin_of_run(INT run_number, char *error)
{
   /* put here clear scalers etc. */
  INT RunNumber;
  INT RunNumber_size = sizeof(RunNumber);
  cm_get_experiment_database(&hDB, NULL);
  db_get_value(hDB,0,"/Runinfo/Run number",&RunNumber,&RunNumber_size,TID_INT, 0);
  char filename[1000];
  sprintf(filename,"/home/newg2/TestMagnetMonitor/Data/Monitor_Port1_3_%04d.txt",RunNumber);
  output1_3.open(filename,ios::out);

  sprintf(filename,"/home/newg2/TestMagnetMonitor/Data/Monitor_Port2_%04d.txt",RunNumber);
  output2.open(filename,ios::out);

   return SUCCESS;
}

/*-- End of Run ----------------------------------------------------*/

INT end_of_run(INT run_number, char *error)
{
  output1_3.close();
  output2.close();
   return SUCCESS;
}

/*-- Pause Run -----------------------------------------------------*/

INT pause_run(INT run_number, char *error)
{
   return SUCCESS;
}

/*-- Resume Run ----------------------------------------------------*/

INT resume_run(INT run_number, char *error)
{
   return SUCCESS;
}

/*-- Frontend Loop -------------------------------------------------*/

INT frontend_loop()
{
   /* if frontend_call_loop is true, this routine gets called when
      the frontend is idle or once between every event */
   return SUCCESS;
}

/*------------------------------------------------------------------*/

/********************************************************************\

  Readout routines for different events

\********************************************************************/

/*-- Trigger event routines ----------------------------------------*/

INT poll_event(INT source, INT count, BOOL test)
/* Polling routine for events. Returns TRUE if event
   is available. If test equals TRUE, don't return. The test
   flag is used to time the polling */
{
   return 0;
}

/*-- Interrupt configuration ---------------------------------------*/

INT interrupt_configure(INT cmd, INT source, POINTER_T adr)
{
   switch (cmd) {
   case CMD_INTERRUPT_ENABLE:
      break;
   case CMD_INTERRUPT_DISABLE:
      break;
   case CMD_INTERRUPT_ATTACH:
      break;
   case CMD_INTERRUPT_DETACH:
      break;
   }
   return SUCCESS;
}

/*-- Event readout -------------------------------------------------*/

INT read_CompressorChiller_event(char *pevent, INT off)
{
   WORD *pdata;
   float *pdata_f;
   //int comp_off_message_sent = 0;
   //int chill_off_message_sent = 0;
   //int high_temp_message_sent = 0;
   int comp_ctrl = 2;
   int chil_ctrl = 2;
   int comp_ctrl_size = sizeof(comp_ctrl);
   int chil_ctrl_size = sizeof(chil_ctrl);
   //int comp_off_size = sizeof(comp_off_message_sent);
   //int chill_off_size = sizeof(chill_off_message_sent);
   //int high_temp_size = sizeof(high_temp_message_sent);
   //float temp_threshold = 80;
   //int temp_threshold_size = sizeof(temp_threshold);


   HNDLE hDBcontrol;
   cm_get_experiment_database(&hDBcontrol, NULL);
   db_get_value(hDBwarning,0,"/Equipment/CompressorChiller/Settings/comp_ctrl",&comp_ctrl,&comp_ctrl_size,TID_INT, 0);
   db_get_value(hDBwarning,0,"/Equipment/CompressorChiller/Settings/chil_ctrl",&chil_ctrl,&chil_ctrl_size,TID_INT, 0);

   //HNDLE hDBwarning;
   //cm_get_experiment_database(&hDBwarning, NULL);
   //db_get_value(hDBwarning,0,"/EmailWarning/comp_off",&comp_off_message_sent,&comp_off_size,TID_INT, 0);
   //db_get_value(hDBwarning,0,"/EmailWarning/chill_off",&chill_off_message_sent,&chill_off_size,TID_INT, 0);
   //db_get_value(hDBwarning,0,"/EmailWarning/high_temp",&high_temp_message_sent,&high_temp_size,TID_INT, 0);
   //db_get_value(hDBwarning,0,"/EmailWarning/Temp_threshold",&temp_threshold,&temp_threshold_size,TID_FLOAT, 0);

   //Communicate with port1
   string command = "\x02";
   command+="DAT\r";
   char inbuf[100];
   char buf[100];
   sprintf(inbuf, command.c_str());
   int b;
   int sta1;
   b = write(fSerialPort1_ptr, &inbuf, 5);
   usleep(1000*500);
   b = read(fSerialPort1_ptr, &buf, 72);
   string received = buf;
   string status = received.substr(43,1);
   cout << "Received: "<<received<<endl;
   
   if (status.compare("1")==0){
     sta1 = 1;
   }else{
     sta1 = 0;
   }

   //Error Handling
   bool CompressorError = false;
   string numErrors = received.substr(49,2);
   if (numErrors != "00") {
      CompressorError = true;
   }
   //string CompressorErrorString = received.substr(52,16);
   string CompressorErrorMessage = "Errors: ";
   
   //Add error description to email message
   if (received.substr(52,1) == "1") {
      CompressorErrorMessage += "SYSTEM ERROR, ";
   }
   if (received.substr(53,1) == "1") {
      CompressorErrorMessage += "Compressor fail, ";
   }
   if (received.substr(54,1) == "1") {
      CompressorErrorMessage += "Locked rotor, ";
   }
   if (received.substr(55,1) == "1") {
      CompressorErrorMessage += "OVERLOAD, ";
   }
   if (received.substr(56,1) == "1") {
      CompressorErrorMessage += "Phase/fuse ERROR, ";
   }
   if (received.substr(57,1) == "1") {
      CompressorErrorMessage += "Pressure alarm, ";
   }
   if (received.substr(58,1) == "1") {
      CompressorErrorMessage += "Helium temp. fail, ";
   }
   if (received.substr(59,1) == "1") {
      CompressorErrorMessage += "Oil circuit fail, ";
   }
   if (received.substr(60,1) == "1") {
      CompressorErrorMessage += "RAM ERROR, ";
   }
   if (received.substr(61,1) == "1") {
      CompressorErrorMessage += "ROM ERROR, ";
   }
   if (received.substr(62,1) == "1") {
      CompressorErrorMessage += "EEPROM ERROR, ";
   }
   if (received.substr(63,1) == "1") {
      CompressorErrorMessage += "DC Voltage error, ";
   }
   if (received.substr(64,1) == "1") {
      CompressorErrorMessage += "MAINS LEVEL !!!!, ";
   }
   cout << CompressorErrorMessage << endl;   
                   
   if (status.compare("1") == 0){
     cout << "Compressor is ON."<<endl;
   }else{
     cout << "Compressor is OFF."<<endl;
   //  string outmessage = "Compressor is off or not read properly: "+status+CompressorErrorMessage;
   //  if(comp_off_message_sent==0){
   //    SendWarning(outmessage);
   //    comp_off_message_sent=1;
   //    db_set_value(hDBwarning,0,"/EmailWarning/comp_off",&comp_off_message_sent,comp_off_size,1,TID_INT);
   //  }
   }

   //Communicate with port3
   char inbuf3[100];
   char buf3[2000];
   char buf3_2[2000];
   char buf3_3[2000];
   char buf3_4[2000];
   
   // Get Chiller Temperature
   sprintf(inbuf3,"RT\r");
   b = write(fSerialPort3_ptr, &inbuf3,3);
   usleep(1000*400);
   b = read(fSerialPort3_ptr, &buf3, 7);
   received = buf3;
   received = received.substr(0,6);
   cout << "Received: "<<received<<endl;
   double temperature = strtod(buf3,NULL);
   float temp = float(temperature);

   // Get Chiller Status
   tcflush( fSerialPort3_ptr, TCIOFLUSH );
   sprintf(inbuf3,"RW\r");
   b=write(fSerialPort3_ptr, &inbuf3,3);
   usleep(1000*400);
   b=read(fSerialPort3_ptr, &buf3_2, 2);
   string status3 = buf3_2;
   status3 = status3.substr(0,1);
   cout << "Received: "<<status3<<endl;
   int sta3;
   if (status3.compare("1")==0){
     sta3 = 1;
   }else{
     sta3 = 0;
   }

   // Get Chiller Pressure
   tcflush( fSerialPort3_ptr, TCIOFLUSH );
   sprintf(inbuf3,"RK\r");
   b=write(fSerialPort3_ptr, &inbuf3,3);
   usleep(1000*400);
   b=read(fSerialPort3_ptr, &buf3_3, 7);
   string pressureStr = buf3_3;
   pressureStr = pressureStr.substr(0,6);
   cout << "Received: "<<pressureStr<<endl;
   double pressureD = strtod(buf3_3,NULL);
   float pressure = float(pressureD);
   
   // Get Chiller Flow
   tcflush( fSerialPort3_ptr, TCIOFLUSH );
   sprintf(inbuf3,"RL\r");
   b=write(fSerialPort3_ptr, &inbuf3,3);
   usleep(1000*400);
   b=read(fSerialPort3_ptr, &buf3_4, 7);
   string flowStr = buf3_4;
   flowStr = flowStr.substr(0,6);
   cout << "Received: "<<flowStr<<endl;
   double flowD = strtod(buf3_4,NULL);
   float flow = float(flowD);
   
   if (status3.compare("1")==0){
     cout << "Chiller is ON."<<endl;
   }else{
     cout << "Chiller is OFF."<<endl;
     //string outmessage = "Chiller is off or not read properly: "+status3;
     //if(chill_off_message_sent==0){
     //  SendWarning(outmessage);
     //  chill_off_message_sent=1;
     //  db_set_value(hDBwarning,0,"/EmailWarning/chill_off",&chill_off_message_sent,chill_off_size,1,TID_INT);
     //}
   }
   //cout <<"Temperature is "<<temperature<<endl;
   //if (temperature>temp_threshold){
   //  if(high_temp_message_sent==0){
   //   high_temp_message_sent=1;
   //    db_set_value(hDBwarning,0,"/EmailWarning/high_temp",&high_temp_message_sent,high_temp_size,1,TID_INT);
   //    SendWarning("Chiller temperature is too high.");
   //  }
   //}

   //Output to text file
   output1_3<<status<<" "<<status3<<" "<<temperature<<endl;
   //output1_3<<status<<" "<<status3<<" "<<temperature<<" "<<pressure<" "<<flow<<endl;

   // Turn Compressor and Chiller on and off
  
   if ((comp_ctrl == 1 || comp_ctrl == 0) && comp_ctrl != sta1) {
      CompressorControl(comp_ctrl);
   }
   if ((chil_ctrl == 1 || chil_ctrl == 0) && chil_ctrl != sta3) {
      ChillerControl(chil_ctrl);
   }

   /* init bank structure */
   bk_init(pevent);

   /* create structured status1 bank */
   bk_create(pevent, "Sta1", TID_WORD, (void **)&pdata);

   if (status.compare("1")==0){
     *pdata++ = 1;
   }else{
     *pdata++ = 0;
   }
   bk_close(pevent, pdata);

   /* create structured status3 bank */
   bk_create(pevent, "Sta3", TID_WORD, (void **)&pdata);

   if (status3.compare("1")==0){
     *pdata++ = 1;
   }else{
     *pdata++ = 0;
   }
   bk_close(pevent, pdata);

   /* create structured temperature bank */
   bk_create(pevent, "Temp", TID_FLOAT, (void **)&pdata_f);
   *pdata_f++=temp;
   bk_close(pevent, pdata_f);

   // create structured pressure bank 
   bk_create(pevent, "Pres", TID_FLOAT, (void **)&pdata_f);
   *pdata_f++=pressure;
   bk_close(pevent, pdata_f);

   // create structured flow rate bank 
   bk_create(pevent, "Flow", TID_FLOAT, (void **)&pdata_f);
   *pdata_f++=flow;
   bk_close(pevent, pdata_f);

   ss_sleep(10);

   return bk_size(pevent);
}

/*-- Scaler event --------------------------------------------------*/

INT read_HeLevel_event(char *pevent, INT off)
{  
  int i = 0;
  int k = 0;
  char line[100];
  int n;
  int b;
  char *ptr;
  int nbytes;
  float *pdata;
  char buf[2000];
  //Communicating to port2
  float Threshold = 74.0;

  double HeLevel=-1;
  float HeLevel_f;
  int nbyte;
  int err_read_message_sent = 0;
  int low_he_message_sent = 0;
  int err_read_size = sizeof(err_read_message_sent);
  int low_he_size = sizeof(low_he_message_sent);
  int ThresholdSize = sizeof(Threshold);
  
  //HNDLE hDBwarning;
  //cm_get_experiment_database(&hDBwarning, NULL);
  //db_get_value(hDBwarning,0,"/EmailWarning/err_read",&err_read_message_sent,&err_read_size,TID_INT, 0);
  //db_get_value(hDBwarning,0,"/EmailWarning/low_he",&low_he_message_sent,&low_he_size,TID_INT, 0);
  //db_get_value(hDBwarning,0,"/EmailWarning/He_threshold",&Threshold,&ThresholdSize,TID_FLOAT, 0);

  sprintf(line,"\x1B");
  nbyte=write(fSerialPort2_ptr, line, 1);
  sleep(5);

  tcflush( fSerialPort2_ptr, TCIFLUSH );

  sprintf(line,"R\r");
  nbyte=write(fSerialPort2_ptr, line, 2);
  sleep(5);
  n=0;
  while(1){
    b=read(fSerialPort2_ptr, &buf, 500);
    string teststring = buf;
    size_t found = teststring.find("6;41H");
    if (found!=string::npos){
      string substring = teststring.substr(found+5,4);
      HeLevel = strtod(substring.c_str(),NULL);
      break;
    }
    sleep(1);
    n++;
  }

  cout <<"He level is "<< HeLevel<<endl;

  //Output to text file
  output2<<HeLevel<<endl;

  if (HeLevel<0){
    cout << "Read out error!"<<endl;
    if(err_read_message_sent==0){
      //SendWarning("He level is not read properly.");
      err_read_message_sent=1;
      //db_set_value(hDBwarning,0,"/EmailWarning/err_read",&err_read_message_sent,err_read_size,1,TID_INT);
    }
  }else if (HeLevel>=0 && HeLevel<Threshold){
    cout << "He level is too low!";
    if(low_he_message_sent==0){
      //SendWarning("He level is too low.");
      low_he_message_sent=1;
      //db_set_value(hDBwarning,0,"/EmailWarning/low_he",&low_he_message_sent,low_he_size,1,TID_INT);
    }
  }else{
    cout << "He level is OK!"<<endl;
  }

  /* init bank structure */
  bk_init(pevent);

  /* create SCLR bank */
  bk_create(pevent, "HeLe", TID_FLOAT, (void **)&pdata);

  HeLevel_f = float(HeLevel);

  *pdata++=HeLevel;

  bk_close(pevent, pdata);


  return bk_size(pevent);
}

int SendWarning(string message){
  ofstream messagefile;
  messagefile.open("emailmessage.txt",ios::out);
  messagefile<<message<<endl;
  ifstream emaillist;
  emaillist.open("EmailList.txt",ios::in);
  string emailentry;
  while (1){
    emaillist>>emailentry;
    if(emaillist.eof())break;
    cout<<"Sending email to "<<emailentry<<endl;
    cout <<message<<endl;
    string command = "mail -s \"Warning from Test Magnet Monitor\" " + emailentry + " < emailmessage.txt";
    system(command.c_str());
    cout <<"Email sent."<<endl;
  }
  emaillist.close();

  return 0;
}

int CompressorControl(int onoff){
  string command = "\x02";
  command+="SYS";
  string s;
  if (onoff == 1)s = "1";
  if (onoff == 0)s = "0";
  command+=s;
  command+="\r";
  char inbuf[100];
  sprintf(inbuf, command.c_str());
  int b;
  b = write(fSerialPort_ptr, &inbuf, 6);
  usleep(1000*500);
  b = read(fSerialPort_ptr, &buf, 6);
  string received = buf;
  cout << "Received: " << received << endl;
  
  if (received.substr(4,1) == s )return 1;
  else return 0;
}

int ChillerControl(int onoff){
  char inbuf3[100];
  char buf3[2000];
  string command = "SO";
  if (onoff == 1)command+="1\r";
  if (onoff == 0)command+="0\r";

  sprintf(inbuf3,command);
  b = write(fSerialPort3_ptr, &inbuf3,4);
  usleep(1000*400);
  b = read(fSerialPort3_ptr, &buf3, 7);
  received = buf3;
  received = received.substr(0,6);
  cout << "Received: "<<received<<endl;

  return 0;
}
