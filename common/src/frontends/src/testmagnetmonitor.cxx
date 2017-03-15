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
#include <stdarg.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

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

//Timeout for Helium Read
sig_atomic_t alarm_counter;
bool timeout;

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
INT read_MagnetCoils_event(char *pevent, INT off);

INT poll_event(INT source, INT count, BOOL test);
INT interrupt_configure(INT cmd, INT source, POINTER_T adr);

int CompressorControl(int onoff);
int ChillerControl(int onoff);

void alarm_handler(int signal);
void setup_alarm_handler();

/*-- Equipment list ------------------------------------------------*/

EQUIPMENT equipment[] = {

   {"CompressorChiller",                /* equipment name */
    {1, 0,                   /* event ID, trigger mask */
     "SYSTEM",               /* event buffer */
     EQ_PERIODIC,            /* equipment type */
     0,                      /* event source */
     "MIDAS",                /* format */
     TRUE,                   /* enabled */
     RO_ALWAYS |   /* read when running and on transitions */
     RO_ODB,                 /* and update ODB */
     300000,                  /* read every 5 minutes */
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
      RO_ALWAYS |   /* read when running and on transitions */
	RO_ODB,                 /* and update ODB */
      300000,                  /* read every 5 minutes */
      0,                      /* stop run after this event limit */
      0,                      /* number of sub events */
      0,                      /* log history */
      "", "", "",},
    read_HeLevel_event,       /* readout routine */
   },

   {"MagnetCoils",     /* equipment name */
    {3, 0,                   /* event ID, trigger mask */
     "SYSTEM",               /* event buffer */
     EQ_PERIODIC,            /* equipment type */
     0,                      /* event source */
     "MIDAS",                /* format */
     TRUE,                   /* enabled */
     RO_ALWAYS |             /* read always */
     RO_ODB,                 /* and update ODB */
     5000,                 /* read every 30 seconds */
     0,                      /* stop run after this event limit */
     0,                      /* number of sub events */
     0,                      /* log history */
     "", "", "",},
    read_MagnetCoils_event,       /* readout routine */
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
    perror("Error opening device port2");
    return -1;
  }
  fcntl(fSerialPort2_ptr, F_SETFL, 0); // return immediately if no data

  if(tcgetattr(fSerialPort2_ptr, &options2) < 0) {
    perror("Error tcgetattr port2");
    return -2;
  }

  cfsetospeed(&options2, B4800);
  cfsetispeed(&options2, B4800);

  options2.c_cflag     &=  ~PARENB;        // Make 8n1
  options2.c_cflag     &=  ~CSTOPB;
  options2.c_cflag     &=  ~CSIZE;
  options2.c_cflag     |=  CS8;
  options2.c_cflag     &=  ~CRTSCTS;       // no flow control

  //Set Non canonical and timeout
  options2.c_lflag     &= ~(ICANON | ECHO | ECHOE | ISIG);
  options2.c_cc[VTIME] = 0;
  options2.c_cc[VMIN]  = 0;

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
  sprintf(filename,"/home/newg2/Applications/field-daq/resources/data/Monitor_Port1_3_%04d.txt",RunNumber);
  output1_3.open(filename,ios::out);

  sprintf(filename,"/home/newg2/Applications/field-daq/resources/data/Monitor_Port2_%04d.txt",RunNumber);
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
  static unsigned int i;
  if (test) {
    for (i = 0; i < count; i++) {
      usleep(10);
    }
    return 0;
  }

  INT update = 0;
  INT update_size = sizeof(update);
  
  HNDLE hDBupdate;
  cm_get_experiment_database(&hDBupdate, NULL);
  db_get_value(hDBupdate,0,"/Equipment/MagnetCoils/Settings/update",&update,&update_size,TID_INT, 0);
   
  return update;
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
   int *pdata_i;
   INT comp_ctrl = 2;
   INT chil_ctrl = 2;
   INT comp_ctrl_size = sizeof(comp_ctrl);
   INT chil_ctrl_size = sizeof(chil_ctrl);

   HNDLE hDBcontrol;
   cm_get_experiment_database(&hDBcontrol, NULL);
   db_get_value(hDBcontrol,0,"/Equipment/CompressorChiller/Settings/comp_ctrl",&comp_ctrl,&comp_ctrl_size,TID_INT, 0);
   db_get_value(hDBcontrol,0,"/Equipment/CompressorChiller/Settings/chil_ctrl",&chil_ctrl,&chil_ctrl_size,TID_INT, 0);

   //Communicate with port1
   string command = "\x02";
   command+="DAT\r";
   char inbuf[100];
   char buf[100];
   sprintf(inbuf, command.c_str());
   int b;
   int sta1;
   string status = "0";
   char hoursStr[5];
   b = write(fSerialPort1_ptr, &inbuf, 5);
   ss_sleep(5000);
   b = read(fSerialPort1_ptr, &buf, 72);
   string received = buf;
   size_t found = received.find("\x02");
   //cout<<found<<endl;
   status = received.substr(found+43,1);
   strncpy(hoursStr, buf + found + 9, 6);
   long int hours = strtol(hoursStr, NULL, 10); 
   cout << "Received: "<<status<<endl;
   cout << "Hours: " << hoursStr << endl;
    
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
   char CompressorErrorString[16];
   strncpy(CompressorErrorString, buf + found + 52, 16);

  string CompressorErrorMessage = "Errors: ";
   
   //Add error description to email message
   if (received.substr(found+52,1) == "1") {
      CompressorErrorMessage += "SYSTEM ERROR, ";
   }
   if (received.substr(found+53,1) == "1") {
      CompressorErrorMessage += "Compressor fail, ";
   }
   if (received.substr(found+54,1) == "1") {
      CompressorErrorMessage += "Locked rotor, ";
   }
   if (received.substr(found+55,1) == "1") {
      CompressorErrorMessage += "OVERLOAD, ";
   }
   if (received.substr(found+56,1) == "1") {
      CompressorErrorMessage += "Phase/fuse ERROR, ";
   }
   if (received.substr(found+57,1) == "1") {
      CompressorErrorMessage += "Pressure alarm, ";
   }
   if (received.substr(found+58,1) == "1") {
      CompressorErrorMessage += "Helium temp. fail, ";
   }
   if (received.substr(found+59,1) == "1") {
      CompressorErrorMessage += "Oil circuit fail, ";
   }
   if (received.substr(found+60,1) == "1") {
      CompressorErrorMessage += "RAM ERROR, ";
   }
   if (received.substr(found+61,1) == "1") {
      CompressorErrorMessage += "ROM ERROR, ";
   }
   if (received.substr(found+62,1) == "1") {
      CompressorErrorMessage += "EEPROM ERROR, ";
   }
   if (received.substr(found+63,1) == "1") {
      CompressorErrorMessage += "DC Voltage error, ";
   }
   if (received.substr(found+64,1) == "1") {
      CompressorErrorMessage += "MAINS LEVEL !!!!, ";
   }
   cout << CompressorErrorMessage << endl;   
                   
   if (status.compare("1") == 0){
     cout << "Compressor is ON."<<endl;
   }else{
     cout << "Compressor is OFF."<<endl;
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
   ss_sleep(400);
   b = read(fSerialPort3_ptr, &buf3, 7);
   received = buf3;
   received = received.substr(0,6);
   cout << "Received: "<<received<<endl;
   double temperature = strtod(buf3,NULL);
   float temp = float(temperature);

   // Get Chiller Status
   string status3 = "0";
   tcflush( fSerialPort3_ptr, TCIOFLUSH );
   sprintf(inbuf3,"RW\r");
   b=write(fSerialPort3_ptr, &inbuf3,3);
   ss_sleep(400);
   b=read(fSerialPort3_ptr, &buf3_2, 2);
   status3 = buf3_2;
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
   ss_sleep(400);
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
   ss_sleep(400);
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
   }

   //Output to text file
   output1_3<<status<<" "<<hours<<" "<<CompressorErrorString<<" "<<status3<<" "<<temperature<<" "<<pressure<<" "<<flow<<endl;

   // Turn Compressor and Chiller on and off
   if ((comp_ctrl == 1 || comp_ctrl == 0) && comp_ctrl != sta1) {
      if (CompressorControl(comp_ctrl) == 1){
         comp_ctrl = 2;
	 db_set_value(hDBcontrol,0,"/Equipment/CompressorChiller/Settings/comp_ctrl",&comp_ctrl,comp_ctrl_size,1,TID_INT);
      }
   }
   if ((chil_ctrl == 1 || chil_ctrl == 0) && chil_ctrl != sta3) {
      if (ChillerControl(chil_ctrl) == 1){
	chil_ctrl = 2;
	db_set_value(hDBcontrol,0,"/Equipment/CompressorChiller/Settings/chil_ctrl",&chil_ctrl,chil_ctrl_size,1,TID_INT);
      }
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

   // Compressor hours counter bank
   bk_create(pevent, "Hour", TID_INT, (void **)&pdata_i);
   *pdata_i++=hours;
   bk_close(pevent, pdata_i);

   // Compressor errors bank
   bk_create(pevent, "Errs", TID_WORD, (void **)&pdata);
   for (int i = 0; i < 16; i++){
      if (CompressorErrorString[i] == '1'){
         *pdata++ = 1;
      }else{
         *pdata++ = 0;
      }
   }
   bk_close(pevent, pdata);

   ss_sleep(10);

   return bk_size(pevent);
}

/*-- Scaler event --------------------------------------------------*/

INT read_HeLevel_event(char *pevent, INT off)
{  
  int i = 0;
  int k = 0;
  char line[1];
  int n;
  int b;
  char *ptr;
  int nbytes;
  float *pdata;
  char buf[2000];

  //Communicating to port2
  float Threshold = 74.0;

  double HeLevel=102;
  double shieldTemp = -1;
  float HeLevel_f;
  float shieldTemp_f;
  int nbyte;
  float prevHe;
  int alarm;
  int prevHe_size = sizeof(prevHe);
  int alarm_size = sizeof(alarm);  

  HNDLE hDBhe;
  cm_get_experiment_database(&hDBhe, NULL);
  db_get_value(hDBhe,0,"/Equipment/HeLevel/Variables/HeLe",&prevHe,&prevHe_size,TID_FLOAT, 0);
  db_get_value(hDBhe,0,"/Equipment/HeLevel/Settings/Alarm",&alarm,&alarm_size,TID_INT, 0);

  sprintf(line,"\x1B");
  nbyte=write(fSerialPort2_ptr, line, 1);
  
  ss_sleep(1000);
  b = 1;
  while (b > 0){
    b = read(fSerialPort2_ptr, &buf, 500);
    //cout<<b<< " bytes"<<endl;
    ss_sleep(200);
  }
  tcflush( fSerialPort2_ptr, TCIFLUSH );

  sprintf(line,"R");
  nbyte=write(fSerialPort2_ptr, line, 1);
  ss_sleep(50);
  sprintf(line,"\r");
  nbyte=write(fSerialPort2_ptr, line, 1);
  ss_sleep(2000);
  n = 0;
  b = 2;
  while(b>1 && n<=20 && (shieldTemp==-1 || HeLevel==102)){
    b=read(fSerialPort2_ptr, &buf, 500);
    string teststring = buf;
    size_t found2 = teststring.find("11;43H");
    if (found2!=string::npos){
      string substring2 = teststring.substr(found2+6,3);
      shieldTemp = strtod(substring2.c_str(),NULL);
    }

    size_t found = teststring.find("6;41H");
    if (found!=string::npos){
      string substring = teststring.substr(found+5,4);
      HeLevel = strtod(substring.c_str(),NULL);
    }

    ss_sleep(1000);
    n++;
  }

  cout<<"He level is "<< HeLevel<<endl;
  cout<<"Shield Temp is "<<shieldTemp<<endl;

  //Output to text file
  output2<<HeLevel<<" "<<shieldTemp<<endl;

  if (HeLevel>101){
    cout << "Read out error!"<<endl;
  }else if (HeLevel<Threshold){
    cout << "He level is too low!";
  }else{
    cout << "He level is OK!"<<endl;
  }

  if (HeLevel == prevHe) {
    alarm++;
    cout<<"Alarm is at: "<<alarm<<endl;
  }


  // init bank structure 
  // Only write if Helium was read correctly

  bk_init(pevent);

  // create SCLR bank */
  bk_create(pevent, "HeLe", TID_FLOAT, (void **)&pdata);

  HeLevel_f = float(HeLevel);
  
  if (HeLevel != 102){
    *pdata++=HeLevel;
  } else {
    *pdata++=prevHe;
    alarm = 100; 
  }
  
  db_set_value(hDBhe,0,"/Equipment/HeLevel/Settings/Alarm",&alarm,alarm_size,1,TID_INT);

  bk_close(pevent, pdata);
 
  //Create shield Temperature bank
  bk_create(pevent, "ShTe", TID_FLOAT, (void **)&pdata);

  shieldTemp_f = float(shieldTemp);

  *pdata++=shieldTemp;

  bk_close(pevent, pdata);
 
  return bk_size(pevent);
}

INT read_MagnetCoils_event(char *pevent, INT off)
{
  int n = 0;
  int nbyte;
  int b = 2;
  char buf[2000];
  char line[1];
  float *pdata;
  double CoilX = -11;
  double CoilY = -11;
  double CoilZ = -11;
  int xsize = 10;
  int ysize = 10;
  int zsize = 10;
  string teststring;
  string substring;
  string setXstr;
  string setYstr;
  string setZstr;

  INT update = 0; 
  INT setX = -11000;
  INT setY = -11000;
  INT setZ = -11000;
  INT update_size = sizeof(update);
  INT setX_size = sizeof(setX);
  INT setY_size = sizeof(setY);
  INT setZ_size = sizeof(setZ);

  //See if coil currents should be changed

  HNDLE hDBupdate;
  cm_get_experiment_database(&hDBupdate, NULL);
  db_get_value(hDBupdate,0,"/Equipment/MagnetCoils/Settings/update",&update,&update_size,TID_INT, 0);
  
  if (update == 1)
  {
    db_get_value(hDBupdate,0,"/Equipment/MagnetCoils/Settings/setX",&setX,&setX_size,TID_INT, 0);
    db_get_value(hDBupdate,0,"/Equipment/MagnetCoils/Settings/setY",&setY,&setY_size,TID_INT, 0);
    db_get_value(hDBupdate,0,"/Equipment/MagnetCoils/Settings/setZ",&setZ,&setZ_size,TID_INT, 0);
    
    if (abs(setX) > 10000 || abs(setY) > 10000 || abs(setZ) > 1000) {
      cout<<"Current values out of range (-10A to 10A)"<<endl;
    } else if (setX == -11000 || setY == -11000 || setZ == -11000){
      cout<<"Error getting set values"<<endl;
    } else {
      if (setX >= 0){setXstr = "DEM2+";}
      else{setXstr = "DEM2-";}
      if (setY >= 0){setYstr = "DEM0+";}
      else{setYstr = "DEM0-";}
      if (setZ >= 0){setZstr = "DEM1+";}
      else{setZstr = "DEM1-";}

      setX = abs(setX);
      setY = abs(setY);
      setZ = abs(setZ);

      if (setX < 10){setXstr.append("000");}
      else if (setX < 100) {setXstr.append("00");}
      else if (setX < 1000) {setXstr.append("0");}
      else if (setX == 10000) {xsize++;}
      if (setY < 10){setYstr.append("000");} 
      else if (setY < 100) {setYstr.append("00");} 
      else if (setY < 1000) {setYstr.append("0");}
      else if (setY == 10000) {ysize++;}
      if (setZ < 10){setZstr.append("000");} 
      else if (setZ < 100) {setZstr.append("00");} 
      else if (setZ < 1000) {setZstr.append("0");}
      else if (setZ == 10000) {zsize++;}

      setXstr.append(to_string(setX));
      setYstr.append(to_string(setY));
      setZstr.append(to_string(setZ));
      setXstr.append("\r");
      setYstr.append("\r");
      setZstr.append("\r");

      sprintf(line,"\x1B");
      nbyte=write(fSerialPort2_ptr, line, 1);
      ss_sleep(1000);
      b = 1;
      while (b > 0){
        b = read(fSerialPort2_ptr, &buf, 500);
        //cout<<b<< " bytes"<<endl;
        ss_sleep(200);
      }
      tcflush( fSerialPort2_ptr, TCIFLUSH );

      for (int i = 0; i < xsize; i++){
	char temp[1] = {setXstr[i]};
	nbyte = write(fSerialPort2_ptr, temp, 1);
	//cout<<temp[0]<<endl;
	ss_sleep(50);
      }
      for (int i = 0; i < ysize; i++){
        char temp[1] = {setYstr[i]};
        nbyte = write(fSerialPort2_ptr, temp, 1);
        //cout<<temp[0]<<endl;
        ss_sleep(50);
      }
      for (int i = 0; i < zsize; i++){
        char temp[1] = {setZstr[i]};
        nbyte = write(fSerialPort2_ptr, temp, 1);
        //cout<<temp[0]<<endl;
        ss_sleep(50);
      }
        
      //sprintf(line,setXc);
      //nbyte=write(fSerialPort2_ptr, line, xsize);
      //ss_sleep(2000);
      //sprintf(line,setYc);
      //nbyte=write(fSerialPort2_ptr, line, ysize);
      //ss_sleep(2000);      
      //sprintf(line,setZc);
      //nbyte=write(fSerialPort2_ptr, line, zsize);
      //ss_sleep(2000);

      sprintf(line,"R");
      nbyte=write(fSerialPort2_ptr, line, 1);
      ss_sleep(50);
      sprintf(line,"\r");
      nbyte=write(fSerialPort2_ptr, line, 1);
      ss_sleep(3000);
    }
  }
  update = 0;
  db_set_value(hDBupdate,0,"/Equipment/MagnetCoils/Settings/update",&update,update_size,1,TID_INT);


  while((CoilX==-11 || CoilY==-11 || CoilZ==-11) && n < 10){
    b = read(fSerialPort2_ptr, &buf, 500);
    teststring = buf;
    //Find X current
    size_t foundx = teststring.find("20;42H");
    if (foundx!=string::npos){
      substring = teststring.substr(foundx+6,5);
      CoilX = strtod(substring.c_str(),NULL);
      cout<<"Read X: "<<CoilX<<endl;
    }
    //Find Y current
    size_t foundy = teststring.find("18;42H");
    if (foundy!=string::npos){
      substring = teststring.substr(foundy+6,5);
      CoilY = strtod(substring.c_str(),NULL);
      cout<<"Read Y: "<<CoilY<<endl;
    }
    //Find Z current
    size_t foundz = teststring.find("19;42H");
    if (foundz!=string::npos){
      substring = teststring.substr(foundz+6,5);
      CoilZ = strtod(substring.c_str(),NULL);
      cout<<"Read Z: "<<CoilZ<<endl;
    }
    
    //cout<<teststring<<endl;
    ss_sleep(1000);
    n++;
  }

  //create structured coil x (2) event
  bk_init(pevent);
  bk_create(pevent, "CoiX", TID_FLOAT, (void **)&pdata);
  *pdata++=CoilX;
  bk_close(pevent, pdata);
  
  //create structured coil y (0) event
  bk_create(pevent, "CoiY", TID_FLOAT, (void **)&pdata);
  *pdata++=CoilY;
  bk_close(pevent, pdata);
  
  //create structured coil z (1) event
  bk_create(pevent, "CoiZ", TID_FLOAT, (void **)&pdata);
  *pdata++=CoilZ;
  bk_close(pevent, pdata);
  
  return bk_size(pevent);
}

int CompressorControl(int onoff){
  string command = "\x02";
  command+="SYS";
  string s;
  if (onoff == 1)s = "1";
  if (onoff == 0)s = "0";
  command+=s;
  command+="\r";
  /*char inbuf[100];
  char buf[2000];
  sprintf(inbuf, command.c_str());
  int b;
  b = write(fSerialPort1_ptr, &inbuf, 6);
  usleep(1000*500);
  b = read(fSerialPort1_ptr, &buf, 6);
  string received = buf;
  cout << "Received: " << received << endl;
  
  if (received.substr(4,1) == s )return 1;
  else return 0;*/
  cout<<"Sending command: "<<command<<endl;;
  return 1;
}

int ChillerControl(int onoff){
  char inbuf3[100];
  char buf3[2000];
  int b;

  /*if (onoff == 1)sprintf(inbuf3,"SO1\r");
  if (onoff == 0)sprintf(inbuf3,"SO0\r");  
  b = write(fSerialPort3_ptr, &inbuf3,4);
  usleep(1000*400);
  b = read(fSerialPort3_ptr, &buf3, 7);
  string received = buf3;
  received = received.substr(0,6);
  cout << "Received: "<<received<<endl;*/
 
  if (onoff == 1)cout<<"Sending command: "<<"SO1\r"<<endl;
  if (onoff == 0)cout<<"Sending command: "<<"SO0\r"<<endl;

  return 1;
}
