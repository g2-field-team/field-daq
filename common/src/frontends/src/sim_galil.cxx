/********************************************************************\

Name:         gm2GalilFeSim.cxx
Created by:   Matteo Bartolini
Modified by:  Joe Grange, Ran Hong

Contents:     readout code to talk to Galil motion control

$Id$

\********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "midas.h"
#include "mcstd.h"
#include "gclib.h"
#include "gclibo.h"
#include <iostream>
#include <string>
#include <iomanip>
#include <vector>
#include <unistd.h>
#include <sys/timeb.h>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>

#define GALIL_EXAMPLE_OK G_NO_ERROR //return code for correct code execution
#define GALIL_EXAMPLE_ERROR -100
using namespace std;

/* make frontend functions callable from the C framework */
#ifdef __cplusplus
extern "C" {
#endif

  // i am defining some Galil libraries variables

  //----------------------------------------------------------
  /*-- Globals -------------------------------------------------------*/

  /* The frontend name (client name) as seen by other MIDAS clients   */
  char *frontend_name = "gm2GalilFeSim";
  /* The frontend file name, don't change it */
  char *frontend_file_name = __FILE__;

  /* frontend_loop is called periodically if this variable is TRUE    */
  BOOL frontend_call_loop = FALSE;

  /* a frontend status page is displayed with this frequency in ms */
  INT display_period = 3000;

  /* maximum event size produced by this frontend */
  INT max_event_size = 100000;

  /* maximum event size for fragmented events (EQ_FRAGMENTED) */
  INT max_event_size_frag = 5 * 1024 * 1024;

  /* buffer size to hold events */
  INT event_buffer_size = 100 * 10000;


  /*-- Function declarations -----------------------------------------*/
  INT frontend_init();
  INT frontend_exit();
  INT begin_of_run(INT run_number, char *error);
  INT end_of_run(INT run_number, char *error);
  INT pause_run(INT run_number, char *error);
  INT resume_run(INT run_number, char *error);
  INT frontend_loop();

  INT read_galil_event(char *pevent, INT off);

  INT poll_event(INT source, INT count, BOOL test);
  INT interrupt_configure(INT cmd, INT source, POINTER_T adr);
  INT db_set_value(HNDLE hDB, HNDLE hKeyRoot, const char *key_name, const void *data, INT data_size, INT num_values, DWORD type); 
  INT db_find_key(HNDLE hdB, HNDLE hKey, const char *key_name, HNDLE *subhkey);
  INT cm_get_experiment_database(HNDLE *hDB, HNDLE *hKeyClient);


  /* device driver list */
  /*DEVICE_DRIVER hv_driver[] = {
    {"Dummy Device", nulldev, 16, null},
    {""}
    };*/


  /*-- Equipment list ------------------------------------------------*/


  EQUIPMENT equipment[] = {


    {"Galil",                /* equipment name */
      {1, 0,                   /* event ID, trigger mask */
	"SYSTEM",               /* event buffer */
	EQ_POLLED,            /* equipment type */
	0,                      /* event source */
	"MIDAS",                /* format */
	TRUE,                   /* enabled */
	RO_RUNNING|   /* read when running and on transitions */
	RO_ODB,
	100,                  /* poll every 0.1 sec */
	0,                      /* stop run after this event limit */
	0,                      /* number of sub events */
	60,                      /* log history, logged once per minute */
	"", "", "",},
      read_galil_event,       /* readout routine */
    },

    {""}
  };


#ifdef __cplusplus
}
#endif

HNDLE hDB, hkeyclient;

typedef struct GalilDataStruct{
  double TimeStamp;
  double TensionArray[2];
  double VelocityArray[3];
  double PositionArray[3];
  double OutputVArray[3];
}GalilDataStruct;

vector<GalilDataStruct> GalilDataBuffer;

thread read_thread;
mutex mlock;

void GetGalilMessage(const GCon &g);
bool RunActive;
int ReadGroupSize = 50;

GCon g = 0; //var used to refer to a unique connection. A valid connection is nonzero.

ofstream GalilOutFile;
//ofstream GalilOutFileDirect;

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
  /*
  char buf[1023]; //traffic buffer
  GReturn b = G_NO_ERROR;
  b=GOpen("192.168.1.13 -s ALL -t 1000 -d",&g);
  GInfo(g, buf, sizeof(buf)); //grab connection string
  cout << "connection string is" << " "<<  buf << "\n";
  if (b==G_NO_ERROR){
    cout << "connection successful\n";
  }
  else {cout << "connection failed \n";}

  GTimeout(g,2000);//adjust timeout
*/
  //cout << "This is a fake front-end simulating the Galil system"<<endl;
  cm_msg(MINFO,"init","This is a fake front-end simulating the Galil system");
  //-------------end code to communicate with Galil------------------

  return SUCCESS;
}

/*-- Frontend Exit -------------------------------------------------*/

INT frontend_exit()
{
  return SUCCESS;
}

/*-- Begin of Run --------------------------------------------------*/

INT begin_of_run(INT run_number, char *error)
{
  //Get run number
  INT RunNumber;
  INT RunNumber_size = sizeof(RunNumber);
  cm_get_experiment_database(&hDB, NULL);
  db_get_value(hDB,0,"/Runinfo/Run number",&RunNumber,&RunNumber_size,TID_INT, 0);
  char filename[1000];
  sprintf(filename,"/home/rhong/gm2/g2-field-team/field-daq/resources/GalilTextOut/GalilOutput%04d.txt",RunNumber);
  GalilOutFile.open(filename,ios::out);
/*  sprintf(filename,"/Users/rhong/gm2software/experiments/gm2Trolley/DirectGalilOutput%04d.txt",RunNumber);
  GalilOutFileDirect.open(filename,ios::out);
  */
  //Load script
  char ScriptName[500];
  INT ScriptName_size = sizeof(ScriptName);
  db_get_value(hDB,0,"/Equipment/Galil/Settings/CmdScript",ScriptName,&ScriptName_size,TID_STRING,0);
  string FullScriptName = string("/home/rhong/gm2/g2-field-team/field-daq/resources/GalilMotionScripts/")+string(ScriptName)+string(".dmc");
//  cout <<"Galil Script to load: " << FullScriptName<<endl;
  cm_msg(MINFO,"begin_of_run","Galil Script to load: %s",FullScriptName.c_str());
//Get ReadGroupSize from odb
  INT ReadGroupSize_size = sizeof(ReadGroupSize);
  db_get_value(hDB,0,"/Equipment/Galil/Settings/ReadGroupSize",&ReadGroupSize,&ReadGroupSize_size,TID_INT, 0);
//  cout <<"ReadGroup size: " << ReadGroupSize<<endl;
  cm_msg(MINFO,"begin_of_run","ReadGroup size: %d",ReadGroupSize);
/*
  GReturn b = G_NO_ERROR;
  GProgramDownload(g,"",0); //to erase prevoius programs
  //dump the buffer
  int rc = GALIL_EXAMPLE_OK; //return code
  char buffer[5000000];
  rc = GMessage(g, buffer, sizeof(buffer));
  //b=GProgramDownloadFile(g,"/Users/rhong/gm2software/experiments/gm2Trolley/feedbacktestCurrent.dmc",0);
  b=GProgramDownloadFile(g,FullScriptName.c_str(),0);
  GCmd(g, "XQ #TH1,0");*/
  RunActive=true;
  //Start thread
  read_thread = thread(GetGalilMessage,g);
  return SUCCESS;
}

/*-- End of Run ----------------------------------------------------*/

INT end_of_run(INT run_number, char *error)

{
 // GCmd(g,"HX");
 // GCmd(g,"AB");
  mlock.lock();
  RunActive=false;
  mlock.unlock();
  GalilDataBuffer.clear();
  GalilOutFile.close();
//  GalilOutFileDirect.close();
  read_thread.join();
  return SUCCESS;
}

/*-- Pause Run -----------------------------------------------------*/

INT pause_run(INT run_number, char *error)
{
//  GCmd(g,"STA");
  return SUCCESS;
}

/*-- Resuem Run ----------------------------------------------------*/

INT resume_run(INT run_number, char *error)
{ 
//  GCmd(g,"SHA");
//  GCmd(g,"BGA"); 
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

  mlock.lock();
  bool check = GalilDataBuffer.size()>ReadGroupSize;
//  cout <<"poll "<<GalilDataBuffer.size()<<" "<<int(check)<<endl;
  mlock.unlock();
  if (check)return 1;
  else return 0;
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






INT read_galil_event(char *pevent, INT off){
  double *pdata;
  double *pdatab;

  //Init bank
  bk_init32(pevent);

  mlock.lock();
  //Write data to banks
  bk_create(pevent, "GALI", TID_DOUBLE, (void **)&pdata);
 // cout <<"Size "<<GalilDataBuffer.size()<<endl;
  
  for (int jj=0;jj<ReadGroupSize;jj++){
    *pdata++ = GalilDataBuffer[jj].TimeStamp;
    for (int j=0;j<2;j++){
      *pdata++ = GalilDataBuffer[jj].TensionArray[j];
    }
    for (int j=0;j<2;j++){
      *pdata++ = GalilDataBuffer[jj].PositionArray[j];
    }
    for (int j=0;j<2;j++){
      *pdata++ = GalilDataBuffer[jj].VelocityArray[j];
    }
    for (int j=0;j<2;j++){
      *pdata++ = GalilDataBuffer[jj].OutputVArray[j];
    }
  }
  GalilDataBuffer.erase(GalilDataBuffer.begin(),GalilDataBuffer.begin()+ReadGroupSize);
  mlock.unlock();
  bk_close(pevent,pdata);

  return bk_size(pevent);
}

//GetGalilMessage
void GetGalilMessage(const GCon &g){
  char buffer[5000000];
  hkeyclient=0;
  string Device;
  int  rc = GALIL_EXAMPLE_OK; //return code
  /* residule string */
  string ResidualString = string("");

  int position =0;
  timeb starttime,currenttime;
  ftime(&starttime);
 
  //Readout loop
  while (1){
    bool localRunActive;
    mlock.lock();
    localRunActive=RunActive;
    mlock.unlock();
    if (!localRunActive)break;
    //Read Message to buffer
    //rc = GMessage(g, buffer, sizeof(buffer));
    ftime(&currenttime);
    double time = (currenttime.time-starttime.time)*1000 + (currenttime.millitm - starttime.millitm);
    position=time*0.1;
    sprintf(buffer,"Trolley %f 1.0 -1.1 %d %d 100 -100 3.0 -3.0\n",time,position,-position);
//    cout<<buffer<<endl;

//    GalilOutFileDirect<<buffer;
    string BufString = string(buffer);
    //Add the residual from last read
    if (ResidualString.size()!=0)BufString = ResidualString+BufString;
    ResidualString.clear();

    size_t foundnewline = BufString.find("\n");
    //  static  bool flag = false;

    int iGalil = 0;
    GalilDataStruct GalilDataUnit;
    while (foundnewline!=string::npos){
      stringstream iss (BufString.substr(0,foundnewline-1));
      // output returned by Galil is stored in the following variables

      iss >> Device;
      if(Device.compare("Trolley")==0){
	iss >> GalilDataUnit.TimeStamp;
	for (int j=0;j<2;j++){
	  iss >> GalilDataUnit.TensionArray[j];
	}
	for (int j=0;j<2;j++){
	  iss >> GalilDataUnit.PositionArray[j];
	}
	for (int j=0;j<2;j++){
	  iss >> GalilDataUnit.VelocityArray[j];
	}
	for (int j=0;j<2;j++){
	  iss >> GalilDataUnit.OutputVArray[j];
	}

	mlock.lock();
	GalilDataBuffer.push_back(GalilDataUnit);
	mlock.unlock();

	iGalil++;
	//Write to txt output
	GalilOutFile<<"Trolley "<<int(time)<<" "<<GalilDataUnit.TensionArray[0]<<" "<<GalilDataUnit.TensionArray[1]<<" "<<GalilDataUnit.PositionArray[0]<<" "<<GalilDataUnit.PositionArray[1]<<" "<<GalilDataUnit.VelocityArray[0]<<" "<<GalilDataUnit.VelocityArray[1]<<" "<<GalilDataUnit.OutputVArray[0]<<" "<<GalilDataUnit.OutputVArray[1]<<endl;
      }

      BufString = BufString.substr(foundnewline+1,string::npos);
      foundnewline = BufString.find("\n");
    }
    if (BufString.size()!=0){
      //  GalilOutFile << "Remaining string: ";
      ResidualString = BufString;
      //    cout <<ResidualString<<endl;
    }
    usleep(10000);
  }
}
