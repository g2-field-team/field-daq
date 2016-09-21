/********************************************************************\

Name:     gm2TrolleyFe.cxx
Author :  Ran Hong

Contents:     readout code to talk to Trolley Interface

$Id$

\********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "midas.h"
#include "mcstd.h"
#include <iostream>
#include <string>
#include <cstring>
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
  short NSample_NMR;
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
  short PressureSensorCal[7];
  int PMonitorVal;
  int PMonitorTemp;

  //Register Readbacks
  short BarcodeRegisters[5];
  short TrlyRegisters[16];

  //Constructor
  _TrlyDataStruct(){}
  _TrlyDataStruct(const _TrlyDataStruct& obj)
  {
    //NMR
    NMRTimeStamp = obj.NMRTimeStamp;
    ProbeNumber = obj.ProbeNumber;
    NSample_NMR = obj.NSample_NMR;
    for (int i=0;i<MAX_NMR_SAMPLES;i++){
      NMRSamples[i] = obj.NMRSamples[i];
    }

    //Barcode
    BarcodeTimeStamp = obj.BarcodeTimeStamp;
    NSample_Barcode_PerCh = obj.NSample_Barcode_PerCh;
    for (int i=0;i<MAX_BARCODE_SAMPLES;i++){
      BarcodeSamples[i] = obj.BarcodeSamples[i];
    }

    //Monitor
    BarcodeError = obj.BarcodeError;
    BCToNMROffset = obj.BCToNMROffset;
    VMonitor1 = obj.VMonitor1;
    VMonitor2 = obj.VMonitor2;
    TMonitorIn = obj.TMonitorIn;
    TMonitorExt1 = obj.TMonitorExt1;
    TMonitorExt2 = obj.TMonitorExt2;
    TMonitorExt3 = obj.TMonitorExt3;
    for(int i=0;i<7;i++){
      PressureSensorCal[i] = obj.PressureSensorCal[i];
    }
    PMonitorVal = obj.PMonitorVal;
    PMonitorTemp = obj.PMonitorTemp;
    for(int i=0;i<5;i++){
      BarcodeRegisters[i] = obj.BarcodeRegisters[i];
    }
    for(int i=0;i<16;i++){
      TrlyRegisters[i] = obj.TrlyRegisters[i];
    }
  }
}TrlyDataStruct;

vector<TrlyDataStruct> TrlyDataBuffer;

thread read_thread;
mutex mlock;
mutex mlockdata;

void ReadFromDevice();
bool FEActive;
//int ReadGroupSize = 17;

ofstream NMROutFile;

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
  NMROutFile.open(filename,ios::out);
  
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
  NMROutFile.close();
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
  WORD *pNMRdata;
  WORD *pBarcodedata;
  WORD *pMonitordata;

  mlockdata.lock();
  TrlyDataStruct* TrlyDataRead = new TrlyDataStruct(TrlyDataBuffer[0]);
  TrlyDataBuffer.erase(TrlyDataBuffer.begin());
  mlockdata.unlock();

  //Check consistency
/*  BarcodeError;
  BCToNMROffset;
  PressureSensorCal[7];
  BarcodeRegisters[5];
  TrlyRegisters[16];
*/
  //Init bank
  bk_init32(pevent);

  //Write data to banks
  bk_create(pevent, "NMRT", TID_WORD, (void **)&pNMRdata);
  memcpy(pNMRdata, &(TrlyDataRead->NMRTimeStamp), sizeof(long int));
  pNMRdata += sizeof(long int)/sizeof(WORD);
  memcpy(pNMRdata, &(TrlyDataRead->ProbeNumber), sizeof(short));
  pNMRdata += sizeof(short)/sizeof(WORD);
  memcpy(pNMRdata, &(TrlyDataRead->NSample_NMR), sizeof(short));
  pNMRdata += sizeof(short)/sizeof(WORD);
  memcpy(pNMRdata, &(TrlyDataRead->NMRSamples), sizeof(short)*TrlyDataRead->NSample_NMR);
  pNMRdata += sizeof(short)*TrlyDataRead->NSample_NMR/sizeof(WORD);
  bk_close(pevent,pNMRdata);

  bk_create(pevent, "BARC", TID_WORD, (void **)&pBarcodedata);
  memcpy(pBarcodedata, &(TrlyDataRead->BarcodeTimeStamp), sizeof(long int));
  pBarcodedata += sizeof(long int)/sizeof(WORD);
  memcpy(pBarcodedata, &(TrlyDataRead->NSample_Barcode_PerCh), sizeof(short));
  pBarcodedata += sizeof(short)/sizeof(WORD);
  memcpy(pBarcodedata, &(TrlyDataRead->BarcodeSamples), sizeof(short)*TrlyDataRead->NSample_Barcode_PerCh*BARCODE_CH_NUM);
  pBarcodedata += sizeof(short)*TrlyDataRead->NSample_Barcode_PerCh*BARCODE_CH_NUM/sizeof(WORD);
  bk_close(pevent,pBarcodedata);

  bk_create(pevent, "MONT", TID_WORD, (void **)&pMonitordata);
  memcpy(pMonitordata, &(TrlyDataRead->VMonitor1), sizeof(short));
  pMonitordata += sizeof(short)/sizeof(WORD);
  memcpy(pMonitordata, &(TrlyDataRead->VMonitor2), sizeof(short));
  pMonitordata += sizeof(short)/sizeof(WORD);
  memcpy(pMonitordata, &(TrlyDataRead->TMonitorIn), sizeof(short));
  pMonitordata += sizeof(short)/sizeof(WORD);
  memcpy(pMonitordata, &(TrlyDataRead->TMonitorExt1), sizeof(short));
  pMonitordata += sizeof(short)/sizeof(WORD);
  memcpy(pMonitordata, &(TrlyDataRead->TMonitorExt2), sizeof(short));
  pMonitordata += sizeof(short)/sizeof(WORD);
  memcpy(pMonitordata, &(TrlyDataRead->TMonitorExt3), sizeof(short));
  pMonitordata += sizeof(short)/sizeof(WORD);
  memcpy(pMonitordata, &(TrlyDataRead->PMonitorVal), sizeof(int));
  pMonitordata += sizeof(int)/sizeof(WORD);
  memcpy(pMonitordata, &(TrlyDataRead->PMonitorTemp), sizeof(int));
  pMonitordata += sizeof(int)/sizeof(WORD);
  bk_close(pevent,pMonitordata);

  delete TrlyDataRead;
  return bk_size(pevent);
}

//ReadFromDevice
void ReadFromDevice(){
  int LastFrameNumber = 0;
  int FrameNumber = 0;
  int FrameSize = 0;
  //Frame buffer
  short* Frame = new short[MAX_PAYLOAD_DATA];

  int ReadThreadActive = 1;
  mlock.lock();
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/ReadThreadActive",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
  mlock.unlock();

  //Read first frame and sync
  int rc = DataReceive((void *)Frame);
  if (rc<0){
    ReadThreadActive = 0;
    mlock.lock();
    db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/ReadThreadActive",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
    mlock.unlock();
    return;
    mlock.unlock();
  }
  LastFrameNumber = *((int *)(&(Frame[7])));
  FrameSize = *((int *)(&(Frame[9])));
 
  //Readout loop
  int i=0;
  while (1){
    //Check if the front-end is active
    bool localFEActive;
    mlock.lock();
    localFEActive = FEActive;
    mlock.unlock();
    if (!localFEActive)break;

    //Read Frame
    rc = DataReceive((void *)Frame);
    if (rc<0){
      ReadThreadActive = 0;
      mlock.lock();
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/ReadThreadActive",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
      mlock.unlock();
      return;
    }
    FrameNumber = *((int *)(&(Frame[7])));
    FrameSize = *((int *)(&(Frame[9])));
    mlock.lock();
    db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/DataFrameIndex",&FrameNumber,sizeof(FrameNumber), 1 ,TID_INT); 
    mlock.unlock();

    if (FrameNumber!=(LastFrameNumber+1)){
      ReadThreadActive = 0;
      mlock.lock();
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/ReadThreadActive",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
      mlock.unlock();
      return;
    }
    LastFrameNumber=FrameNumber;

    //Translate buffer into TrlyDataStruct
    TrlyDataStruct* TrlyDataUnit = new TrlyDataStruct;

    TrlyDataUnit->NMRTimeStamp = *((long int *)(&(Frame[32])));
    TrlyDataUnit->ProbeNumber = short(0x1F & Frame[11]);
    TrlyDataUnit->NSample_NMR = Frame[12];
    short NSamNMR = Frame[12];
    //Check if this is larger than the MAX
    if (NSamNMR>MAX_NMR_SAMPLES){
      ReadThreadActive = 0;
      mlock.lock();
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/ReadThreadActive",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
      mlock.unlock();
      return;
    }
    for (int ii=0;ii<NSamNMR;i++){
      TrlyDataUnit->NMRSamples[ii] = Frame[64+ii];
    }
    TrlyDataUnit->BarcodeTimeStamp = *((long int *)(&(Frame[36]))) ;
    TrlyDataUnit->NSample_Barcode_PerCh = Frame[13];
    short NSamBarcode = Frame[13]*BARCODE_CH_NUM;
    //Check if this is larger than the MAX
    if (NSamBarcode>MAX_BARCODE_SAMPLES){
      ReadThreadActive = 0;
      mlock.lock();
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/ReadThreadActive",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
      mlock.unlock();
      return;
    }
    for (int ii=0;ii<NSamBarcode;i++){
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
      TrlyDataUnit->PressureSensorCal[ii] = Frame[21+ii];;
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
    mlockdata.lock();
    TrlyDataBuffer.push_back(*TrlyDataUnit);
    mlockdata.unlock();
    //Text Output
    if (NMROutFile.is_open()){
      NMROutFile<<TrlyDataUnit->NMRTimeStamp<<" "<<TrlyDataUnit->ProbeNumber<<" "<<TrlyDataUnit->NSample_NMR<<endl;
      for (int ii=0;ii<NSamNMR;i++){
	NMROutFile<<TrlyDataUnit->NMRSamples[ii]<<endl;
      }
    }
    delete TrlyDataUnit;
    i++;
  }
  ReadThreadActive = 0;
  mlock.lock();
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/ReadThreadActive",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
  mlock.unlock();
  delete []Frame;
}
