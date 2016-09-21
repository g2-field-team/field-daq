/********************************************************************\

Name:         gm2GalilFe.cxx
Created by:   Matteo Bartolini
Modified by:  Joe Grange, Ran Hong

Contents:     readout code to talk to Galil motion control

$Id$

\********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "midas.h"
#include "mcstd.h"
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

#include "TrolleyInterface.h"

using namespace std;
using namespace TrolleyInterface;

/* make frontend functions callable from the C framework */
#ifdef __cplusplus
extern "C" {
#endif

  // i am defining some Galil libraries variables

  //----------------------------------------------------------
  /*-- Globals -------------------------------------------------------*/

  /* The frontend name (client name) as seen by other MIDAS clients   */
  char *frontend_name = "gm2TrolleyFe";
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

  INT read_trly_event(char *pevent, INT off);

  INT poll_event(INT source, INT count, BOOL test);
  INT interrupt_configure(INT cmd, INT source, POINTER_T adr);


  /*-- Equipment list ------------------------------------------------*/


  EQUIPMENT equipment[] = {


    {"TrolleyInterface",                /* equipment name */
      {1, 0,                   /* event ID, trigger mask */
	"SYSTEM",               /* event buffer */
	EQ_POLLED,            /* equipment type */
	0,                      /* event source */
	"MIDAS",                /* format */
	TRUE,                   /* enabled */
	RO_RUNNING|   /* read when running and on odb */
	RO_ODB,
	100,                  /* poll every 0.1 sec */
	0,                      /* stop run after this event limit */
	0,                      /* number of sub events */
	0,                      /* log history, logged once per minute */
	"", "", "",},
      read_trly_event,       /* readout routine */
    },

    {""}
  };


#ifdef __cplusplus
}
#endif

HNDLE hDB;

typedef struct _TrlyDataStruct{
  //NMR
  long int NMRTimeStamp;
  short ProbeNumber;
  short NSamlpe_NMR;
  short NMRSamples[MAX_NMR_SAMPLES];

  //Barcode
  long int BarcodeTimeStamp;
  short NSample_Barcode_PerCh;
  short BarcodeSamples[MAX_BARCODE_SAMPLES];

  //Monitor
  bool BarcodeError;
  short BCToNMROffset;
  short VMonitor1;
  short VMonitor2;
  short TMonitorIn;
  short TMonitorExt1;
  short TMonitorExt2;
  short TMonitorExt3;
  short PressorSensorCal[7];
  int PMonitorVal;
  int PMonitorTemp;

  //Register Readbacks
  short BarcodeRegisters[5];
  short TrlyRegisters[16];

}TrlyDataStruct;

vector<TrlyDataStruct> TrlyDataBuffer;

thread read_thread;
mutex mlock;

void ReadFromDevice();
bool FEActive;
//int ReadGroupSize = 17;

ofstream TrolleyOutFile;

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
  //Connect to trolley interface
  int err = DeviceConnect("192.168.1.123");  

  if (err==0){
    //cout << "connection successful\n";
    cm_msg(MINFO,"init","Trolley Interface connection successful");
  }
  else {
    //   cout << "connection failed \n";
    cm_msg(MERROR,"init","Trolley Interface connection failed. Error code: %d",err);
  }

  //Start reading thread
  FEActive=true;
  read_thread = thread(ReadFromDevice);

  return SUCCESS;
}

/*-- Frontend Exit -------------------------------------------------*/

INT frontend_exit()
{
  mlock.lock();
  FEActive=false;
  mlock.unlock();
//  cm_msg(MINFO,"end_of_run","Trying to join threads.");
  read_thread.join();
  cm_msg(MINFO,"exit","All threads joined.");
  TrlyDataBuffer.clear();
  cm_msg(MINFO,"exit","Data buffer is emptied before exit.");

  //Disconnect from Trolley interface
  int err = DeviceDisconnect();
  if (err==0){
    //cout << "connection successful\n";
    cm_msg(MINFO,"exit","Trolley Interface disconnection successful");
  }
  else {
    //   cout << "connection failed \n";
    cm_msg(MERROR,"exit","Trolley Interface disconnection failed. Error code: %d",err);
  }
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
  sprintf(filename,"/home/rhong/gm2/g2-field-team/field-daq/resources/TrolleyTextOut/TrolleyOutput%04d.txt",RunNumber);
  TrolleyOutFile.open(filename,ios::out);
  
  //Load script
/*  GReturn b = G_NO_ERROR;
  char ScriptName[500];
  INT ScriptName_size = sizeof(ScriptName);
  db_get_value(hDB,0,"/Equipment/Galil/Settings/CmdScript",ScriptName,&ScriptName_size,TID_STRING,0);
  string FullScriptName = string("/home/rhong/gm2/g2-field-team/field-daq/resources/GalilMotionScripts/")+string(ScriptName)+string(".dmc");
//  cout <<"Galil Script to load: " << FullScriptName<<endl;
  cm_msg(MINFO,"begin_of_run","Galil Script to load: %s",FullScriptName.c_str());
  */

  mlock.lock();
  TrlyDataBuffer.clear();
  mlock.unlock();
  cm_msg(MINFO,"begin_of_run","Data buffer is emptied at the beginning of the run.");

  return SUCCESS;
}

/*-- End of Run ----------------------------------------------------*/

INT end_of_run(INT run_number, char *error)
{
  TrolleyOutFile.close();
  return SUCCESS;
}

/*-- Pause Run -----------------------------------------------------*/

INT pause_run(INT run_number, char *error)
{
  return SUCCESS;
}

/*-- Resuem Run ----------------------------------------------------*/

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

  mlock.lock();
  bool check = (TrlyDataBuffer.size()>0);
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


INT read_trly_event(char *pevent, INT off){
  INT *pdata;
  double *pdatab;

  //Init bank
  bk_init32(pevent);

  //Write data to banks
  bk_create(pevent, "GALI", TID_DWORD, (void **)&pdata);
  
  mlock.lock();
  for (int i=0;i<ReadGroupSize;i++){
    *pdata++ = TrlyDataBuffer[i].TimeStamp;
    for (int j=0;j<2;j++){
      *pdata++ = TrlyDataBuffer[i].TensionArray[j];
    }
    for (int j=0;j<2;j++){
      *pdata++ = TrlyDataBuffer[i].PositionArray[j];
    }
    for (int j=0;j<2;j++){
      *pdata++ = TrlyDataBuffer[i].VelocityArray[j];
    }
    for (int j=0;j<2;j++){
      *pdata++ = TrlyDataBuffer[i].OutputVArray[j];
    }
  }
  TrlyDataBuffer.erase(TrlyDataBuffer.begin(),TrlyDataBuffer.begin()+ReadGroupSize);
  mlock.unlock();
  bk_close(pevent,pdata);

  //Write barcode data to banks
//  bk_create(pevent, "BARC", TID_DOUBLE, (void **)&pdatab);
/*  for (int i=0;i<ReadGroupSize;i++){
    *pdatab++ = BarcodeDataBuffer[i].TimeStamp;
    for (int j=0;j<6;j++){
      *pdatab++ = BarcodeDataBuffer[i].BarcodeArray[j];
    }
  }*/
//  bk_close(pevent,pdatab);

  return bk_size(pevent);
}

//ReadFromDevice
void ReadFromDevice(){
  int ReadThreadActive = 1;
  int LastFrameNumber = 0;
  //Frame buffer
  short* Frame = new short[MAX_PAYLOAD_DATA];

  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/ReadThreadActive",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 

//  timeb starttime,currenttime;
//  ftime(&starttime);
  //Read first frame and sync
  int rc = DataReceive((void *)Frame);
  if (rc<0){
    ReadThreadActive = 0;
    db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/ReadThreadActive",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
    return;
  }
 
  //Readout loop
  int i=0;
  while (1){
    db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/DataFrameIndex",&i,sizeof(i), 1 ,TID_INT); 
    //Check if the front-end is active
    bool localFEActive;
    mlock.lock();
    localFEActive = FEActive;
    mlock.unlock();
    if (!localFEActive)break;

    //Read Message to buffer
//    mlock.lock();
    rc = DataReceive((void *)Frame);
    if (rc<0){
      ReadThreadActive = 0;
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/ReadThreadActive",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
      return;
    }
    //    mlock.unlock();

    //Translate buffer into TrlyDataStruct
    TrlyDataStruct* TrlyDataUnit = new TrlyDataStruct;

    TrlyDataUnit->NMRTimeStamp = *((long int *)(&(Frame[32])));
    TrlyDataUnit->ProbeNumber = short(0x1F & Frame[11]);
    TrlyDataUnit->NSamlpe_NMR = Frame[12];
    short NSamNMR = Frame[12];
    //Check if this is larger than the MAX
    if (NSamNMR>MAX_NMR_SAMPLES){
      ReadThreadActive = 0;
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/ReadThreadActive",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
      return;
    }
    for (short ii=0;ii<NSamNMR;i++){
      TrlyDataUnit->NMRSamples[ii] = Frame[64+ii];
    }
    TrlyDataUnit->BarcodeTimeStamp = *((long int *)(&(Frame[36]))) ;
    TrlyDataUnit->NSample_Barcode_PerCh = Frame[13];
    short NSamBarcode = Frame[13]*BARCODE_CH_NUM;
    //Check if this is larger than the MAX
    if (NSamBarcode>MAX_BARCODE_SAMPLES){
      ReadThreadActive = 0;
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/ReadThreadActive",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
      return;
    }
    for (short ii=0;ii<NSamBarcode;i++){
      TrlyDataUnit->BarcodeSamples[ii] = Frame[64+NSamNMR+ii];
    }
    TrlyDataUnit->BarcodeError = bool(0x100 & Frame[11]);
    TrlyDataUnit->BCToNMROffset = Frame[14];
    TrlyDataUnit->VMonitor1 = Frame[15];
    TrlyDataUnit->VMonitor2 = Frame[16];
    TrlyDataUnit->TMonitorIn = Frame[17];
    TrlyDataUnit->TMonitorExt1 = Frame[18];
    TrlyDataUnit->TMonitorExt2 = Frame[19];
    TrlyDataUnit->TMonitorExt3 = Frame[20];
    for (short ii=0;ii<7;ii++){
      TrlyDataUnit->PressorSensorCal[ii] = Frame[21+ii];;
    }
    TrlyDataUnit->PMonitorVal = *((int *)(&(Frame[28]))) ;
    TrlyDataUnit->PMonitorTemp = *((int *)(&(Frame[30])));
    for (short ii=0;ii<5;ii++){
      TrlyDataUnit->BarcodeRegisters[ii] = Frame[43+ii];
    }
    for (short ii=0;ii<16;ii++){
      TrlyDataUnit->TrlyRegisters[ii] = Frame[48+ii];
    }

    //Push to buffer
    mlock.lock();
    TrlyDataBuffer.push_back(*TrlyDataUnit);
    mlock.unlock();
    delete TrlyDataUnit;
    //Text Output
    //TrolleyOutFile<<"Trolley "<<int(Time)<<" "<<TrlyDataUnit.TensionArray[0]<<" "<<TrlyDataUnit.TensionArray[1]<<" "<<TrlyDataUnit.PositionArray[0]<<" "<<TrlyDataUnit.PositionArray[1]<<" "<<TrlyDataUnit.VelocityArray[0]<<" "<<TrlyDataUnit.VelocityArray[1]<<" "<<TrlyDataUnit.OutputVArray[0]<<" "<<TrlyDataUnit.OutputVArray[1]<<endl;

    /*db_set_value(hDB,0,"/Equipment/Galil/Monitor/MotorPositions",&GalilDataUnitD.PositionArray,sizeof(GalilDataUnitD.PositionArray), 3 ,TID_DOUBLE); 
      db_set_value(hDB,0,"/Equipment/Galil/Monitor/MotorVelocities",&GalilDataUnitD.VelocityArray,sizeof(GalilDataUnitD.VelocityArray), 3 ,TID_DOUBLE); 
      db_set_value(hDB,0,"/Equipment/Galil/Monitor/Tensions",&GalilDataUnitD.TensionArray,sizeof(GalilDataUnitD.TensionArray), 2 ,TID_DOUBLE); 
     */
    i++;
  }
  ReadThreadActive = 0;
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/ReadThreadActive",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
  delete []Frame;
}
