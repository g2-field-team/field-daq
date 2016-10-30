/********************************************************************\

Name:     gm2TrolleyFeSim.cxx
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
#include "field_structs.hh"
#include "field_constants.hh"

#include "TTree.h"
#include "TFile.h"

#define FRONTEND_NAME "Trolley Interface Simulate" // Prefer capitalize with spaces

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
  char *frontend_name = (char *)FRONTEND_NAME;
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
	RO_RUNNING,   /* read when running and on odb */
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

vector<g2field::trolley_nmr_t> TrlyNMRBuffer;
vector<g2field::trolley_barcode_t> TrlyBarcodeBuffer;
vector<g2field::trolley_monitor_t> TrlyMonitorBuffer;

g2field::trolley_nmr_t TrlyNMRCurrent;
g2field::trolley_barcode_t TrlyBarcodeCurrent;
g2field::trolley_monitor_t TrlyMonitorCurrent;

thread read_thread;
mutex mlock;
mutex mlockdata;

void ReadFromDevice();
bool RunActive;

bool write_root = false;
TFile *pf;
TTree *pt_norm;

const char * const nmr_bank_name = "TLNP"; // 4 letters, try to make sensible
const char * const barcode_bank_name = "TLBC"; // 4 letters, try to make sensible
const char * const monitor_bank_name = "TLMN"; // 4 letters, try to make sensible


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
  //Connect to fake trolley interface
  //const char * filename = "/home/rhong/gm2/g2-field-team/field-daq/resources/NMRDataTemp/data_NMR_61682000Hz_11.70dbm-2016-10-27_19-36-42.dat";
  const char * filename = "/home/newg2/Applications/field-daq/resources/NMRDataTemp/data_NMR_61682000Hz_11.70dbm-2016-10-27_19-36-42.dat";
  int err = FileOpen(filename); 

  if (err==0){
    //cout << "connection successful\n";
    cm_msg(MINFO,"init","Trolley Interface Simulation is initialized successful with file %s",filename);
  }
  else {
    //   cout << "connection failed \n";
    cm_msg(MERROR,"init","Trolley Interface connection failed. Error code: %d",err);
  }

  return SUCCESS;
}

/*-- Frontend Exit -------------------------------------------------*/

INT frontend_exit()
{
  //Disconnect from fake Trolley interface
  int err = FileClose();
  if (err==0){
    //cout << "connection successful\n";
    cm_msg(MINFO,"exit","Trolley Interface Simulation is disconnected");
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

  //Get Root output switch
  int write_root_size = sizeof(write_root);
  db_get_value(hDB,0,"/Experiment/Run Parameters/Root Output",&write_root,&write_root_size,TID_BOOL, 0);

  //Get Data dir
  string DataDir;
  char str[500];
  int str_size = sizeof(str);
  db_get_value(hDB,0,"/Logger/Data dir",&str,&str_size,TID_STRING, 0);
  DataDir=string(str);

  //Root File Name
  sprintf(str,"Root/TrolleyInterface_%05d.root",RunNumber);
  string RootFileName = DataDir + string(str);

  if(write_root){
    cm_msg(MINFO,"begin_of_run","Writing to root file %s",RootFileName.c_str());
    pf = new TFile(RootFileName.c_str(), "recreate");
    pt_norm = new TTree("t_TrolleySim", "Sim Trolley Interface Data");
    pt_norm->SetAutoSave(5);
    pt_norm->SetAutoFlush(20);

    string nrm_br_name("TLNP");
    string barcode_br_name("TLBC");
    string monitor_br_name("TLMN");

    pt_norm->Branch(nmr_bank_name, &TrlyNMRCurrent, g2field::trolley_nmr_str);
    pt_norm->Branch(barcode_bank_name, &TrlyBarcodeCurrent, g2field::trolley_barcode_str);
    pt_norm->Branch(monitor_bank_name, &TrlyMonitorCurrent, g2field::trolley_monitor_str);
  }

  mlock.lock();
  TrlyNMRBuffer.clear();
  TrlyBarcodeBuffer.clear();
  TrlyMonitorBuffer.clear();
  mlock.unlock();
  cm_msg(MINFO,"begin_of_run","Data buffer is emptied at the beginning of the run.");

  //Start reading thread
  RunActive=true;
  read_thread = thread(ReadFromDevice);
  

  return SUCCESS;
}

/*-- End of Run ----------------------------------------------------*/

INT end_of_run(INT run_number, char *error)
{
  mlock.lock();
  RunActive=false;
  mlock.unlock();
//  cm_msg(MINFO,"end_of_run","Trying to join threads.");
  read_thread.join();
  cm_msg(MINFO,"exit","All threads joined.");
  TrlyNMRBuffer.clear();
  TrlyBarcodeBuffer.clear();
  TrlyMonitorBuffer.clear();
  cm_msg(MINFO,"exit","Data buffer is emptied before exit.");

  pt_norm->Write();

  pf->Write();
  pf->Close();

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
  bool check = (TrlyNMRBuffer.size()>0 && TrlyBarcodeBuffer.size()>0 && TrlyMonitorBuffer.size()>0);
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
  static unsigned int num_events = 0;
  WORD *pNMRdata;
  WORD *pBarcodedata;
  WORD *pMonitordata;


  //Check consistency
/*  BarcodeError;
  BCToNMROffset;
  PressureSensorCal[7];
  BarcodeRegisters[5];
  TrlyRegisters[16];
*/
  //Root output
  if (write_root) {
    TrlyNMRCurrent = TrlyNMRBuffer[0];
    TrlyBarcodeCurrent = TrlyBarcodeBuffer[0];
    TrlyMonitorCurrent = TrlyMonitorBuffer[0];
    pt_norm->Fill();
    num_events++;
    if (num_events % 10 == 0) {
      pt_norm->AutoSave("SaveSelf,FlushBaskets");
      pf->Flush();
      num_events = 0;
    }
  }
  //Init bank
  bk_init32(pevent);

  //Write data to banks
  mlockdata.lock();
  bk_create(pevent, nmr_bank_name, TID_WORD, (void **)&pNMRdata);
  memcpy(pNMRdata, &(TrlyNMRBuffer[0]), sizeof(g2field::trolley_nmr_t));
  pNMRdata += sizeof(g2field::trolley_nmr_t)/sizeof(WORD);
  bk_close(pevent,pNMRdata);

  bk_create(pevent, barcode_bank_name, TID_WORD, (void **)&pBarcodedata);
  memcpy(pBarcodedata, &(TrlyBarcodeBuffer[0]), sizeof(g2field::trolley_barcode_t));
  pBarcodedata += sizeof(g2field::trolley_barcode_t)/sizeof(WORD);
  bk_close(pevent,pBarcodedata);

  bk_create(pevent, monitor_bank_name, TID_WORD, (void **)&pMonitordata);
  memcpy(pMonitordata, &(TrlyMonitorBuffer[0]), sizeof(g2field::trolley_monitor_t));
  pMonitordata += sizeof(g2field::trolley_monitor_t)/sizeof(WORD);
  bk_close(pevent,pMonitordata);

  TrlyNMRBuffer.erase(TrlyNMRBuffer.begin());
  TrlyBarcodeBuffer.erase(TrlyBarcodeBuffer.begin());
  TrlyMonitorBuffer.erase(TrlyMonitorBuffer.begin());
  mlockdata.unlock();

  return bk_size(pevent);
}

//ReadFromDevice
void ReadFromDevice(){
  int LastFrameNumber = 0;
  int FrameNumber = 0;
 
  //Monitor data will be synced to ODB
  bool BarcodeError;
  bool PowersupplyStatus[3];
  bool TemperatureInterupt;
  unsigned short PressureSensorCal[7];
  unsigned int NMRCheckSum;
  unsigned int FrameCheckSum;
  bool NMRCheckSumPassed;
  bool FrameCheckSumPassed;

  int FrameSize = 0;
  //Frame buffer
  unsigned short* Frame = new unsigned short[MAX_PAYLOAD_DATA/sizeof(unsigned short)];

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
    cm_msg(MINFO,"ReadFromDevice","Reading error code %d",rc);
    mlock.unlock();
    return;
  }
  memcpy(&LastFrameNumber,&(Frame[9]),sizeof(int));
  memcpy(&FrameSize,&(Frame[7]),sizeof(int));
 
  //Readout loop
  int i=0;
  while (1){
    //Check if the front-end is active
    bool localRunActive;
    mlock.lock();
    localRunActive = RunActive;
    mlock.unlock();
    if (!localRunActive)break;

    //Read Frame
    rc = DataReceive((void *)Frame);
    usleep(1000);
    if (rc<0){
      ReadThreadActive = 0;
      mlock.lock();
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/ReadThreadActive",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
      mlock.unlock();
      //Change to a gental warning
      return;
    }
    memcpy(&FrameNumber,&(Frame[9]),sizeof(int));
    memcpy(&FrameSize,&(Frame[7]),sizeof(int));

    mlock.lock();
    db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/DataFrameIndex",&FrameNumber,sizeof(FrameNumber), 1 ,TID_INT); 
    mlock.unlock();

    if (FrameNumber!=(LastFrameNumber+1)){
      ReadThreadActive = 0;
      mlock.lock();
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/ReadThreadActive",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
      mlock.unlock();
      //Change to a gental warning
      return;
    }
    LastFrameNumber=FrameNumber;

    //Translate buffer into TrlyDataStruct
    g2field::trolley_nmr_t* TrlyNMRDataUnit = new g2field::trolley_nmr_t;
    g2field::trolley_barcode_t* TrlyBarcodeDataUnit = new g2field::trolley_barcode_t;
    g2field::trolley_monitor_t* TrlyMonitorDataUnit = new g2field::trolley_monitor_t;

    memcpy(&(TrlyNMRDataUnit->gps_clock),&(Frame[38]),sizeof(unsigned long int));
    TrlyNMRDataUnit->probe_index = (0x1F & Frame[11]);
    TrlyNMRDataUnit->length = Frame[12];
    unsigned short NSamNMR = Frame[12];
    //Check if this is larger than the MAX
    if (NSamNMR>TRLY_NMR_LENGTH){
      NSamNMR = TRLY_NMR_LENGTH;
      ReadThreadActive = 0;
      mlock.lock();
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/ReadThreadActive",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
      mlock.unlock();
      //Change to a gental warning
      return;
    }
    for (unsigned short ii=0;ii<NSamNMR;ii++){
      TrlyNMRDataUnit->trace[ii] = (short)Frame[64+ii];
    }

    memcpy(&(TrlyBarcodeDataUnit->gps_clock),&(Frame[42]),sizeof(unsigned long int));
    TrlyBarcodeDataUnit->length_per_ch = Frame[13];
    unsigned short NSamBarcode = Frame[13]*TRLY_BARCODE_CHANNELS;
    //Check if this is larger than the MAX
    if (NSamBarcode>TRLY_BARCODE_LENGTH){
      NSamBarcode = TRLY_BARCODE_LENGTH;
      ReadThreadActive = 0;
      mlock.lock();
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/ReadThreadActive",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
      mlock.unlock();
      //Change to a gental warning
      return;
    }
    for (unsigned short ii=0;ii<NSamBarcode;ii++){
      TrlyBarcodeDataUnit->traces[ii] = Frame[64+Frame[12]+ii];
    }

    memcpy(&(TrlyMonitorDataUnit->gps_clock_cycle_start),&(Frame[34]),sizeof(unsigned long int));
    memcpy(&(TrlyMonitorDataUnit->PMonitorVal),&(Frame[30]),sizeof(int));
    memcpy(&(TrlyMonitorDataUnit->PMonitorTemp),&(Frame[32]),sizeof(int));
    memcpy(&(TrlyMonitorDataUnit->RFPower1),&(Frame[60]),sizeof(int));
    memcpy(&(TrlyMonitorDataUnit->RFPower2),&(Frame[62]),sizeof(int));
    TrlyMonitorDataUnit->TMonitorIn = Frame[19];
    TrlyMonitorDataUnit->TMonitorExt1 = Frame[20];
    TrlyMonitorDataUnit->TMonitorExt2 = Frame[21];
    TrlyMonitorDataUnit->TMonitorExt3 = Frame[22];
    TrlyMonitorDataUnit->V1Min = Frame[15];
    TrlyMonitorDataUnit->V1Max = Frame[16];
    TrlyMonitorDataUnit->V2Min = Frame[17];
    TrlyMonitorDataUnit->V2Max = Frame[18];
    TrlyMonitorDataUnit->length_per_ch = Frame[13];
    for (unsigned short ii=0;ii<Frame[13];ii++){
      TrlyMonitorDataUnit->trace_VMonitor1[ii] = Frame[64+Frame[12]+Frame[13]*TRLY_BARCODE_CHANNELS+ii];
    }
    for (unsigned short ii=0;ii<Frame[13];ii++){
      TrlyMonitorDataUnit->trace_VMonitor1[ii] = Frame[64+NSamNMR+NSamBarcode+Frame[13]+ii];
    }

    //Data quality monitoring
    BarcodeError = bool(0x100 & Frame[11]);
    TemperatureInterupt = bool(0x200 & Frame[11]);
    PowersupplyStatus[0] = bool(0x400 & Frame[11]);
    PowersupplyStatus[1] = bool(0x800 & Frame[11]);
    PowersupplyStatus[2] = bool(0x1000 & Frame[11]);
    memcpy(&(NMRCheckSum),&(Frame[46]),sizeof(int));
    memcpy(&(FrameCheckSum),&(Frame[64+NSamNMR+NSamBarcode+Frame[13]*2]),sizeof(int));
    for (short ii=0;ii<7;ii++){
      PressureSensorCal[ii] = Frame[23+ii];;
    }

    //Push to buffer
    mlockdata.lock();
    TrlyNMRBuffer.push_back(*TrlyNMRDataUnit);
    TrlyBarcodeBuffer.push_back(*TrlyBarcodeDataUnit);
    TrlyMonitorBuffer.push_back(*TrlyMonitorDataUnit);
    mlockdata.unlock();

    delete TrlyNMRDataUnit;
    delete TrlyBarcodeDataUnit;
    delete TrlyMonitorDataUnit;
    i++;
  }
  ReadThreadActive = 0;
  mlock.lock();
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/ReadThreadActive",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
  mlock.unlock();
  delete []Frame;
}
