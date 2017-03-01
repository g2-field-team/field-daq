/********************************************************************\

Name:     trolley.cxx
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

#include "g2field/TrolleyInterface.h"
#include "g2field/Sg382Interface.h"
#include "g2field/core/field_constants.hh"
#include "g2field/core/field_structs.hh"

#include "TTree.h"
#include "TFile.h"

#define FRONTEND_NAME "Trolley Interface" // Prefer capitalize with spaces

using namespace std;
using namespace TrolleyInterface;
using namespace Sg382Interface;

/* make frontend functions callable from the C framework */
#ifdef __cplusplus
extern "C" {
#endif

  // i am defining some Galil libraries variables

  //----------------------------------------------------------
  /*-- Globals -------------------------------------------------------*/

  /* The frontend name (client name) as seen by other MIDAS clients   */
  const char *frontend_name = FRONTEND_NAME;
  /* The frontend file name, don't change it */
  const char *frontend_file_name = __FILE__;

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
      {EVENTID_TROLLEY, 0,                   /* event ID, trigger mask */
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
thread control_thread;
mutex mlock;
mutex mlockdata;

void LoadGeneralSettingsToInterface();
void UpdateGeneralOdbSettings();
void ReadFromDevice();
void ControlDevice();
int RampTrolleyVoltage(int InitialVoltage,int TargetVoltage);
int LoadProbeSettings();
BOOL FrontendActive;
BOOL RunActiveForRead;
BOOL RunActiveForControl;
string CurrentMode;
BOOL SimSwitch = false;
INT DebugLevel;

BOOL write_root = false;
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
  //Check if it is a simulation
  int size_Bool = sizeof(SimSwitch);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Simulation Switch",&SimSwitch,&size_Bool,TID_BOOL,0);

  if (SimSwitch){
    //Connect to fake trolley interface
    char filename[500];
    INT filename_size = sizeof(filename);
    db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Simulation/Data Source",filename,&filename_size,TID_STRING,0);

    int err = FileOpen(filename);

    if (err==0){
      //cout << "connection successful\n";
      cm_msg(MINFO,"init","Trolley Interface Simulation is initialized successful with file %s",filename);
    }
    else {
      //   cout << "connection failed \n";
      cm_msg(MERROR,"init","Trolley Interface connection failed. Error code: %d",err);
    }
    CurrentMode = string("Simulation");
    //update odb CurrentMode
    char ModeName[32];
    sprintf(ModeName,"%s",CurrentMode.c_str());
    db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Current Mode",&ModeName,sizeof(ModeName), 1 ,TID_STRING); 
    //Set FrontendActive to True, then the read/control loop will keep active
    mlock.lock();
    FrontendActive=true;
    RunActiveForRead=false;
    RunActiveForControl=false;
    mlock.unlock();
  }else{
    //Connect to trolley interface
    int err = DeviceConnect("192.168.1.123");  

    if (err==0){
      //cout << "connection successful\n";
      cm_msg(MINFO,"init","Trolley Interface connection successful");
    }
    else {
      //   cout << "connection failed \n";
      cm_msg(MERROR,"init","Trolley Interface connection failed. Error code: %d",err);
      return FE_ERR_HW;
    }

    //Connect to Sg382
/*    err = Sg382Connect("192.168.1.122");
    if (err==0){
      //cout << "connection successful\n";
      cm_msg(MINFO,"init","Sg382 Interface connection successful");
    }
    else {
      //   cout << "connection failed \n";
      cm_msg(MERROR,"init","Sg382 Interface connection failed. Error code: %d",err);
      //    return FE_ERR_HW;
    }
*/
    //Set RF frequency and Amplitude
    double RF_Freq;
    double RF_Amp;
    INT Size_DOUBLE = sizeof(RF_Freq);
    db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Sg382/RF Frequency",&RF_Freq,&Size_DOUBLE,TID_DOUBLE, 0);
    db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Sg382/RF Amplitude",&RF_Amp,&Size_DOUBLE,TID_DOUBLE, 0);
    //  SetFrequency(RF_Freq);
    //  SetAmplitude(RF_Amp);
    //Enable RF On sg382
    //EnableRF();

    //Disable Data flow
    DeviceWriteMask(reg_event_data_control,0x00000001,0x00000001);
    //Purge Data
    DevicePurgeData();

    //Configure Timing
    DeviceWrite(reg_timing_control,0x00003000);

    //Enable Data flow
    DeviceWriteMask(reg_event_data_control,0x00000001,0x00000000);

    //Ramp up the trolley voltage
    unsigned int readback;
    DeviceRead(reg_trolley_ldo_status, &readback);
    INT InitialVoltage = readback & 0xFF;
    INT SetTrolleyVoltage = 0;
    INT size_INT = sizeof(SetTrolleyVoltage);
    db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Trolley Power/Voltage",&SetTrolleyVoltage,&size_INT,TID_INT, 0);
    RampTrolleyVoltage(InitialVoltage,SetTrolleyVoltage);
    DeviceRead(reg_trolley_ldo_status, &readback);
    cm_msg(MINFO,"init","Trolley Power : %d",readback & 0xFF);

    //Makesure the measurements are stopped
    DeviceWrite(reg_command,0x0000);
    //Load Odb settings to interface, probe settings are loaded separately
    LoadGeneralSettingsToInterface();
    //To initialize, ask the trolley doing nothing
    DeviceWrite(reg_command,0x0000);

    //Set FrontendActive to True, then the read/control loop will keep active
    mlock.lock();
    FrontendActive=true;
    RunActiveForRead=false;
    RunActiveForControl=false;
    mlock.unlock();

    //Start control thread
    control_thread = thread(ControlDevice);
  }

  //Start read thread
  read_thread = thread(ReadFromDevice);

  return SUCCESS;
}

/*-- Frontend Exit -------------------------------------------------*/

INT frontend_exit()
{
  //Set FrontendActive to False, then the read/control loop will react to it
  mlock.lock();
  FrontendActive=false;
  RunActiveForRead=false;
  RunActiveForControl=false;
  mlock.unlock();
  //Join read/control threads
  read_thread.join();
  if (!SimSwitch){
    control_thread.join();
  }
  cm_msg(MINFO,"exit","All threads joined.");

  if (SimSwitch){
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
  }else{
    //Send Trolley interface command to stop data taking
    DeviceWrite(reg_command,0x0000);
    DeviceWriteMask(reg_event_data_control,0x00000001,0x00000001);

    //Clear buffer
    TrlyNMRBuffer.clear();
    TrlyBarcodeBuffer.clear();
    TrlyMonitorBuffer.clear();
    cm_msg(MINFO,"exit","Data buffer is emptied before exit.");

    //Ramp down the trolley power
    unsigned int readback;
    DeviceRead(reg_trolley_ldo_status, &readback);
    INT InitialVoltage = readback & 0xFF;
    RampTrolleyVoltage(InitialVoltage,0);

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

    //Disable RF On sg382
    //DisableRF();
    //Disconnect from Trolley interface
    /*
       err = Sg382Disconnect();
       if (err==0){
    //cout << "connection successful\n";
    cm_msg(MINFO,"exit","Sg382 Interface disconnection successful");
    }
    else {
    //   cout << "connection failed \n";
    cm_msg(MERROR,"exit","Sg382 Interface disconnection failed. Error code: %d",err);
    }
    */
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
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Root Output",&write_root,&write_root_size,TID_BOOL, 0);

  //Get Data dir
  string DataDir;
  char str[500];
  int str_size = sizeof(str);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Root Dir",&str,&str_size,TID_STRING, 0);
  DataDir=string(str);

  //Root File Name
  sprintf(str,"TrolleyInterface_%05d.root",RunNumber);
  string RootFileName = DataDir + string(str);

  if(write_root){
    cm_msg(MINFO,"begin_of_run","Writing to root file %s",RootFileName.c_str());
    pf = new TFile(RootFileName.c_str(), "recreate");
    pt_norm = new TTree("t_Trolley", "Trolley Interface Data");
    pt_norm->SetAutoSave(5);
    pt_norm->SetAutoFlush(20);

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

  //Set RunActiveControl to True, then the control loop will keep active
  mlock.lock();
  RunActiveForControl=true;
  mlock.unlock();
  //Send command to the control loop through odb
  INT Cmd = 2; //Start-run command
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cmd",&Cmd,sizeof(Cmd), 1 ,TID_INT); 

  usleep(100000);
  //Set RunActiveRead to True, then the read loop will keep active
  mlock.lock();
  RunActiveForRead=true;
  mlock.unlock();

  return SUCCESS;
}

/*-- End of Run ----------------------------------------------------*/

INT end_of_run(INT run_number, char *error)
{
  //Set RunActiveForRead to false, then the read loop will stop packing data
  mlock.lock();
  RunActiveForRead=false;
  mlock.unlock();

  //Set RunActiveControl to false, then the control loop will react
  mlock.lock();
  RunActiveForControl=false;
  mlock.unlock();
  //Send command to the control loop through odb
  INT Cmd = 3; //Stop-run command
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cmd",&Cmd,sizeof(Cmd), 1 ,TID_INT); 
//  cm_msg(MINFO,"end_of_run","Trying to join threads.");
  TrlyNMRBuffer.clear();
  TrlyBarcodeBuffer.clear();
  TrlyMonitorBuffer.clear();
  cm_msg(MINFO,"end_of_run","Data buffer is emptied before exit.");

  if(write_root){
    pt_norm->Write();

    pf->Write();
    pf->Close();
  }

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
  BOOL check = (TrlyNMRBuffer.size()>0 && TrlyBarcodeBuffer.size()>0 && TrlyMonitorBuffer.size()>0);
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
  
  INT BufferLoad;
  INT BufferLoad_size = sizeof(BufferLoad);

  //Check consistency
  /*  BarcodeError;
      BCToNMROffset;
      BarcodeRegisters[5];
      TrlyRegisters[16];
      */

  if (write_root) {
    mlockdata.lock();
    TrlyNMRCurrent = TrlyNMRBuffer[0];
    TrlyBarcodeCurrent = TrlyBarcodeBuffer[0];
    TrlyMonitorCurrent = TrlyMonitorBuffer[0];
    mlockdata.unlock();
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
  
  //Check current size of the readout buffer
  BufferLoad = TrlyNMRBuffer.size();
  mlockdata.unlock();

  //update buffer load in odb
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Buffer Load",&BufferLoad,BufferLoad_size, 1 ,TID_INT);

  return bk_size(pevent);
}

//ReadFromDevice
void ReadFromDevice(){
  int LastFrameANumber = 0;
  int FrameANumber = 0;
 
  //Monitor data will be synced to ODB
  BOOL BarcodeError = TRUE;
  BOOL PowersupplyStatus[3];
  BOOL VMStatus = FALSE;
  BOOL TemperatureInterrupt = TRUE;
  unsigned short PressureSensorCal[7];
  unsigned int NMRCheckSum;
  unsigned int ConfigCheckSum;
  unsigned int FrameCheckSum;
  unsigned int NFlashWords;
  BOOL NMRCheckSumPassed = FALSE;
  BOOL ConfigCheckSumPassed = FALSE;
  BOOL FrameCheckSumPassed = FALSE;
  PowersupplyStatus[0] = FALSE;
  PowersupplyStatus[1] = FALSE;
  PowersupplyStatus[2] = FALSE;
  //Monitor values from the data
  float PowerFactor;
  float Temperature1;
  float PressureTemperature;
  float Pressure;
  float Vmin1;
  float Vmax1;
  float Vmin2;
  float Vmax2;

  BOOL BarcodeErrorOld;
  BOOL TemperatureInterruptOld;
  BOOL PowersupplyStatusOld[3];
  BOOL VMStatusOld = FALSE;
  BOOL NMRCheckSumPassedOld;
  BOOL ConfigCheckSumPassedOld;
  BOOL FrameCheckSumPassedOld;


  unsigned int FrameASize = 0;
  unsigned int FrameBSize = 0;
  //Frame buffer, A and B
  unsigned short* FrameA = new unsigned short[MAX_PAYLOAD_DATA/sizeof(unsigned short)];
  unsigned short* FrameB = new unsigned short[MAX_PAYLOAD_DATA/sizeof(unsigned short)];

  int ReadThreadActive = 1;
  mlock.lock();
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Read Thread Active",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
  mlock.unlock();

  //Read first frame and sync
  int rc = DataReceive((void *)FrameA, (void *)FrameB, &FrameASize, &FrameBSize);
  if (rc<0){
    ReadThreadActive = 0;
    mlock.lock();
    db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Read Thread Active",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
    if (rc==errorEOF){
      cm_msg(MINFO,"ReadFromDevice","End of fake file frontend");
    }else{
      cm_msg(MERROR,"ReadFromDevice","Fail from first reading, error code = %d",rc);
    }

    mlock.unlock();
    return;
  }
 
  //Readout loop
  int i=0;
  bool FrameACountingStart = true;
  while (1){
    BOOL localFrontendActive;
    mlock.lock();
    localFrontendActive = FrontendActive;
    mlock.unlock();
    if (!localFrontendActive)break;

    //For simulatin, do not read file until run starts
    if (SimSwitch && !RunActiveForRead)continue;

    //Read Frame
    rc = DataReceive((void *)FrameA, (void *)FrameB, &FrameASize, &FrameBSize);
//    cm_msg(MINFO,"ReadFromDevice","FrameA size = %d , FrameB size = %d",FrameASize,FrameBSize);
/*    unsigned int command;
    DeviceRead(reg_command, &command);
    cm_msg(MINFO,"ReadFromDevice","command = %d",command);
*/
    if (rc<0){
      ReadThreadActive = 0;
      mlock.lock();
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Read Thread Active",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
      if (rc==errorEOF){
	cm_msg(MINFO,"ReadFromDevice","End of fake file frontend");
      }else{
	cm_msg(MERROR,"ReadFromDevice","Fail from regular reading, error code = %d",rc);
      }
      mlock.unlock();
      return;
    }
    if (FrameASize!=0){
      memcpy(&FrameANumber,&(FrameA[9]),sizeof(int));
    //  cm_msg(MINFO,"ReadFromDevice","Frame number %d",FrameANumber);
      if (FrameACountingStart){
	LastFrameANumber = FrameANumber - 1;
	FrameACountingStart = false;
      }
      memcpy(&FrameASize,&(FrameA[7]),sizeof(int));
      mlock.lock();
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Data Frame Index",&FrameANumber,sizeof(FrameANumber), 1 ,TID_INT); 

      if (FrameANumber!=(LastFrameANumber+1)){
	cm_msg(MERROR,"ReadFromDevice","Skipping trolley data frame at iteration %d",i);
      }
      mlock.unlock();
    }
    LastFrameANumber=FrameANumber;

    //Translate buffer into TrlyDataStruct
    g2field::trolley_nmr_t* TrlyNMRDataUnit = new g2field::trolley_nmr_t;
    g2field::trolley_barcode_t* TrlyBarcodeDataUnit = new g2field::trolley_barcode_t;
    g2field::trolley_monitor_t* TrlyMonitorDataUnit = new g2field::trolley_monitor_t;

    if (CurrentMode.compare("Sleep")!=0 && FrameASize!=0){
      //Probe
      memcpy(&(TrlyNMRDataUnit->local_clock),&(FrameA[40]),sizeof(unsigned long int));
      TrlyNMRDataUnit->probe_index = (0x1F & FrameA[11]);
      TrlyNMRDataUnit->length = FrameA[12];
      unsigned short NSamNMR = FrameA[12];
   /*   if (NSamNMR>0){
	cm_msg(MINFO,"ReadFromDevice","Probe index %d",TrlyNMRDataUnit->probe_index);
	cm_msg(MINFO,"ReadFromDevice","NMR Samples %d",NSamNMR);
	cm_msg(MINFO,"ReadFromDevice","User Data %d",FrameA[92]);
      }*/
      //Check if this is larger than the MAX
      if (NSamNMR>TRLY_NMR_LENGTH){
	NSamNMR = TRLY_NMR_LENGTH;
	mlock.lock();
	cm_msg(MINFO,"ReadFromDevice","NMR sample overflow, length = %d",FrameA[12]);
	mlock.unlock();
      }
      for (unsigned short ii=0;ii<NSamNMR;ii++){
	TrlyNMRDataUnit->trace[ii] = (short)FrameA[96+ii];
      }
      //NMR related registry
      TrlyNMRDataUnit->TS_OffSet = 0x3F & FrameA[50];
      TrlyNMRDataUnit->RF_Prescale = FrameA[83];
      TrlyNMRDataUnit->Probe_Command = FrameA[84];
      TrlyNMRDataUnit->Preamp_Delay = FrameA[85];
      TrlyNMRDataUnit->Preamp_Period = FrameA[86];
      TrlyNMRDataUnit->ADC_Gate_Delay = FrameA[87];
      TrlyNMRDataUnit->ADC_Gate_Offset = FrameA[88];
      TrlyNMRDataUnit->ADC_Gate_Period = FrameA[89];
      TrlyNMRDataUnit->TX_Delay = FrameA[90];
      TrlyNMRDataUnit->TX_Period = FrameA[91];
      TrlyNMRDataUnit->UserDefinedData = FrameA[92];
      //Check TX Pulse On/Off
      if ((FrameA[81]&0x00001000) == 0)TrlyNMRDataUnit->TX_On = 1;
      else TrlyNMRDataUnit->TX_On = 0;

      //Barcode
      memcpy(&(TrlyBarcodeDataUnit->local_clock),&(FrameA[44]),sizeof(unsigned long int));
      TrlyBarcodeDataUnit->length_per_ch = FrameA[13];
      unsigned short NSamBarcode = FrameA[13]*TRLY_BARCODE_CHANNELS;
      //Check if this is larger than the MAX
      if (NSamBarcode>TRLY_BARCODE_LENGTH){
	NSamBarcode = TRLY_BARCODE_LENGTH;
	mlock.lock();
	cm_msg(MINFO,"ReadFromDevice","Barcode sample overflow, length = %d",FrameA[13]);
	mlock.unlock();
      }
      for (unsigned short ii=0;ii<NSamBarcode;ii++){
	TrlyBarcodeDataUnit->traces[ii] = FrameA[96+FrameA[12]+ii];
      }
      //Barcode related registry
      TrlyBarcodeDataUnit->Sampling_Period = FrameA[76];
      TrlyBarcodeDataUnit->Acquisition_Delay = FrameA[77];
      TrlyBarcodeDataUnit->DAC_1_Config = FrameA[78];
      TrlyBarcodeDataUnit->DAC_2_Config = FrameA[79];
      TrlyBarcodeDataUnit->Ref_CM = FrameA[80];

      //Monitor
      memcpy(&(TrlyMonitorDataUnit->local_clock_cycle_start),&(FrameA[36]),sizeof(unsigned long int));
      memcpy(&(TrlyMonitorDataUnit->PMonitorVal),&(FrameA[30]),sizeof(unsigned int));
      memcpy(&(TrlyMonitorDataUnit->PMonitorTemp),&(FrameA[32]),sizeof(unsigned int));
      memcpy(&(TrlyMonitorDataUnit->RFPower1),&(FrameA[60]),sizeof(unsigned int));
      memcpy(&(TrlyMonitorDataUnit->RFPower2),&(FrameA[62]),sizeof(unsigned int));
      //check sums are filled later
      memcpy(&(TrlyMonitorDataUnit->FrameIndex),&(FrameA[9]),sizeof(int));
      TrlyMonitorDataUnit->StatusBits = (FrameA[11]>>8);//Get rid of probe index by bit shift
      TrlyMonitorDataUnit->TMonitorIn = FrameA[19];
      TrlyMonitorDataUnit->TMonitorExt1 = FrameA[20];
      TrlyMonitorDataUnit->TMonitorExt2 = FrameA[21];
      TrlyMonitorDataUnit->TMonitorExt3 = FrameA[22];
      TrlyMonitorDataUnit->V1Min = FrameA[15];
      TrlyMonitorDataUnit->V1Max = FrameA[16];
      TrlyMonitorDataUnit->V2Min = FrameA[17];
      TrlyMonitorDataUnit->V2Max = FrameA[18];
      TrlyMonitorDataUnit->length_per_ch = FrameA[13];
      for (unsigned short ii=0;ii<FrameA[13];ii++){
	TrlyMonitorDataUnit->trace_VMonitor1[ii] = FrameA[96+FrameA[12]+FrameA[13]*TRLY_BARCODE_CHANNELS+ii];
      }
      for (unsigned short ii=0;ii<FrameA[13];ii++){
	TrlyMonitorDataUnit->trace_VMonitor1[ii] = FrameA[96+NSamNMR+NSamBarcode+FrameA[13]+ii];
      }
      //Monitor related registry
      TrlyMonitorDataUnit->Trolley_Command = FrameA[65];
      TrlyMonitorDataUnit->TIC_Stop = FrameA[66];
      TrlyMonitorDataUnit->TC_Start = FrameA[67];
      TrlyMonitorDataUnit->TD_Start = FrameA[68];
      TrlyMonitorDataUnit->TC_Stop = FrameA[69];
      TrlyMonitorDataUnit->Switch_RF = FrameA[70];
      TrlyMonitorDataUnit->PowerEnable = FrameA[71];
      TrlyMonitorDataUnit->RF_Enable = FrameA[72];
      TrlyMonitorDataUnit->Switch_Comm = FrameA[73];
      TrlyMonitorDataUnit->TIC_Start = FrameA[74];
      TrlyMonitorDataUnit->Cycle_Length = FrameA[75];
      TrlyMonitorDataUnit->Power_Control_1 = FrameA[81];
      TrlyMonitorDataUnit->Power_Control_2 = FrameA[82];

      //Data quality monitoring
      //keep previous conditions
      BarcodeErrorOld = BarcodeError;
      TemperatureInterruptOld = TemperatureInterrupt;
      PowersupplyStatusOld[0] = PowersupplyStatus[0];
      PowersupplyStatusOld[1] = PowersupplyStatus[1];
      PowersupplyStatusOld[2] = PowersupplyStatus[2];
      VMStatusOld = VMStatus;
      NMRCheckSumPassedOld = NMRCheckSumPassed;
      FrameCheckSumPassedOld = FrameCheckSumPassed;
      ConfigCheckSumPassedOld = ConfigCheckSumPassed;

      //Flash words
      NFlashWords = FrameA[14];

      //Get new monitor values
      BarcodeError = BOOL(0x100 & FrameA[11]);
      TemperatureInterrupt = !BOOL(0x200 & FrameA[11]);//Low active
      PowersupplyStatus[0] = BOOL(0x400 & FrameA[11]);
      PowersupplyStatus[1] = BOOL(0x800 & FrameA[11]);
      PowersupplyStatus[2] = BOOL(0x1000 & FrameA[11]);
      VMStatus = BOOL(0x2000 & FrameA[11]);
      memcpy(&(NMRCheckSum),&(FrameA[48]),sizeof(int));
      memcpy(&(ConfigCheckSum),&(FrameA[94]),sizeof(int));
      memcpy(&(FrameCheckSum),&(FrameA[96+NSamNMR+NSamBarcode+FrameA[13]*2+NFlashWords]),sizeof(int));
      for (short ii=0;ii<7;ii++){
	PressureSensorCal[ii] = FrameA[23+ii];;
      }
      //Set the pressure sensor calibration only once
      if (i==0){
	mlock.lock();
	db_set_value(hDB,0,"/Equipment/TrolleyInterface/Hardware/Pressure Sensor Calibration",&PressureSensorCal,sizeof(PressureSensorCal), 7 ,TID_SHORT);
	mlock.unlock();
      }

      //Checking sums
      unsigned int sum1=0;
      unsigned int sum2=0;
      unsigned int sum3=0;
      for (unsigned short ii=0;ii<FrameA[12];ii++){
	sum1+=(unsigned int)FrameA[96+ii];
      }
      int NWords = FrameASize/2-2;
      for (int ii=0;ii<NWords;ii++){
	sum2+=(unsigned int)FrameA[ii];
      }
      //Correction for 0x7FFF
      sum2+=0x7FFF;
      //////////////////////
      for(int ii=64;ii<94;ii++){
	sum3+=(unsigned int)FrameA[ii];
      }
      NMRCheckSumPassed = (sum1==NMRCheckSum);
      ConfigCheckSumPassed = (sum3==ConfigCheckSum);
      FrameCheckSumPassed = (sum2==FrameCheckSum);
      TrlyMonitorDataUnit->NMRCheckSum = NMRCheckSum;
      TrlyMonitorDataUnit->ConfigCheckSum = ConfigCheckSum;
      TrlyMonitorDataUnit->FrameCheckSum = FrameCheckSum;
      TrlyMonitorDataUnit->NMRFrameSum = sum1;
      TrlyMonitorDataUnit->ConfigFrameSum = sum3;
      TrlyMonitorDataUnit->FrameSum = sum2;

      //Check if the front-end is active
      if (TrlyMonitorDataUnit->RFPower1!=0){
	PowerFactor = TrlyMonitorDataUnit->RFPower2 / TrlyMonitorDataUnit->RFPower1;
      }else{
	PowerFactor = 0;
      }
      Temperature1 = TrlyMonitorDataUnit->TMonitorExt1;
      PressureTemperature = TrlyMonitorDataUnit->PMonitorTemp;
      Pressure = TrlyMonitorDataUnit->PMonitorVal;
      Vmin1 = TrlyMonitorDataUnit->V1Min;
      Vmax1 = TrlyMonitorDataUnit->V1Max;
      Vmin2 = TrlyMonitorDataUnit->V2Min;
      Vmax2 = TrlyMonitorDataUnit->V2Max;

      //Update odb error monitors and sending messages
      mlock.lock();
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Barcode Error",&BarcodeError,sizeof(BarcodeError), 1 ,TID_BOOL);
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Temperature Interrupt",&TemperatureInterrupt,sizeof(TemperatureInterrupt), 1 ,TID_BOOL);
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Power Supply Status",&PowersupplyStatus,sizeof(PowersupplyStatus), 3 ,TID_BOOL);
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/NMR Check Sum",&NMRCheckSumPassed,sizeof(NMRCheckSumPassed), 1 ,TID_BOOL);
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Config Check Sum",&ConfigCheckSumPassed,sizeof(ConfigCheckSumPassed), 1 ,TID_BOOL);
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Frame Check Sum",&FrameCheckSumPassed,sizeof(FrameCheckSumPassed), 1 ,TID_BOOL);
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Power Factor",&PowerFactor,sizeof(PowerFactor),1,TID_FLOAT);
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Temperature 1",&Temperature1,sizeof(Temperature1),1,TID_FLOAT);
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Pressure Temperature",&PressureTemperature,sizeof(PressureTemperature),1,TID_FLOAT);
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Pressure",&Pressure,sizeof(Pressure),1,TID_FLOAT);
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Vmin 1",&Vmin1,sizeof(Vmin1),1,TID_FLOAT);
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Vmax 1",&Vmax1,sizeof(Vmax1),1,TID_FLOAT);
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Vmin 2",&Vmin2,sizeof(Vmin2),1,TID_FLOAT);
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Vmax 2",&Vmax2,sizeof(Vmax2),1,TID_FLOAT);

      if(BarcodeError && (!BarcodeErrorOld || i==0))cm_msg(MERROR,"ReadFromDevice","Message from trolley interface: Barcode reading error. At iteration %d",i);
      if(TemperatureInterrupt && (!TemperatureInterruptOld || i==0))cm_msg(MERROR,"ReadFromDevice","Message from trolley interface: Temperature interrupt. At iteration %d",i);
      if(!PowersupplyStatus[0] && (PowersupplyStatusOld[0]) || i==0 )cm_msg(MERROR,"ReadFromDevice","Message from trolley interface: Power supply error 1.At iteration %d",i);
      if(!PowersupplyStatus[1] && (PowersupplyStatusOld[1] || i==0))cm_msg(MERROR,"ReadFromDevice","Message from trolley interface: Power supply error 2. At iteration %d",i);
      if(!PowersupplyStatus[2] && (PowersupplyStatusOld[2]) || i==0)cm_msg(MERROR,"ReadFromDevice","Message from trolley interface: Power supply error 3. At iteration %d",i);
//      if(!NMRCheckSumPassed && (NMRCheckSumPassedOld || i==0))cm_msg(MERROR,"ReadFromDevice","Message from trolley interface: NMR check sum failed. At iteration %d. Sum expected = %d. Diff %d",i,NMRCheckSum,NMRCheckSum-sum1);
//      if(!FrameCheckSumPassed && (FrameCheckSumPassedOld || i==0))cm_msg(MERROR,"ReadFromDevice","Message from trolley interface: Frame check sum failed. At iteration %d. Sum expected = %d. Diff %d",i,FrameCheckSum,FrameCheckSum-sum2);
 /*     if(!NMRCheckSumPassed )cm_msg(MERROR,"ReadFromDevice","Message from trolley interface: NMR check sum failed. At iteration %d. Sum expected = %d. Diff %d",i,NMRCheckSum,NMRCheckSum-sum1);
      if(!ConfigCheckSumPassed )cm_msg(MERROR,"ReadFromDevice","Message from trolley interface: Config check sum failed. At iteration %d. Sum expected = %d. Diff %d",i,ConfigCheckSum,ConfigCheckSum-sum3);
      if(!FrameCheckSumPassed )cm_msg(MERROR,"ReadFromDevice","Message from trolley interface: Frame check sum failed. At iteration %d. Sum expected = %d. Diff %d",i,FrameCheckSum,FrameCheckSum-sum2);
      */
      mlock.unlock();

    }

    //Push to buffer
    mlockdata.lock();
    if (RunActiveForRead && CurrentMode.compare("Sleep")!=0 && FrameASize!=0){
      TrlyNMRBuffer.push_back(*TrlyNMRDataUnit);
      TrlyBarcodeBuffer.push_back(*TrlyBarcodeDataUnit);
      TrlyMonitorBuffer.push_back(*TrlyMonitorDataUnit);
    }
    mlockdata.unlock();

    delete TrlyNMRDataUnit;
    delete TrlyBarcodeDataUnit;
    delete TrlyMonitorDataUnit;
    i++;
  }
  ReadThreadActive = 0;
  mlock.lock();
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Read Thread Active",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
  mlock.unlock();
  delete []FrameA;
  delete []FrameB;
}

//Control Device
void ControlDevice(){
  //Configs for each mode
  INT BaselineCycles;
  INT Repeat;
  BOOL ReadBarcode;
  BOOL PowerDown;
  int size_INT = sizeof(BaselineCycles);
  int size_BOOL = sizeof(ReadBarcode);

  int StartUpCount = 0;
  int RepeatCount = 0;

  //Trigger and Monitor for Interactive Mode
  INT Trigger = 0;
  INT Executing = 0;

  //Startup Mode: Sleep
  CurrentMode = string("Sleep");
  DeviceWrite(reg_command,0x0000);
  //Clear FIFO
  DeviceWriteMask(reg_nmr_control,0x00000001, 0x00000001);
  DeviceWriteMask(reg_nmr_control,0x00000001, 0x00000000);

  while (1){
    int ControlThreadActive = 1;
    mlock.lock();
    db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Control Thread Active",&ControlThreadActive,sizeof(ControlThreadActive), 1 ,TID_BOOL); 
    mlock.unlock();

    //Check if the front-end is active
    BOOL localFrontendActive;
    mlock.lock();
    localFrontendActive = FrontendActive;
    mlock.unlock();
    if (!localFrontendActive)break;
    INT Cmd = 0;
    mlock.lock();
    db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cmd",&Cmd,&size_INT,TID_INT, 0);
    mlock.unlock();
    if (Cmd != 0){
      //Get Debug level
      mlock.lock();
      db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Run Config/Debug Level",&DebugLevel,&size_INT,TID_INT, 0);
      mlock.unlock();
      //For general command, cmd==1, do nothing if DebugLevel==0 
      if (Cmd==1 && DebugLevel==0){
	Cmd = 0;
	mlock.lock();
	db_set_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cmd",&Cmd,size_INT, 1 ,TID_INT); 
	mlock.unlock();
	continue;
      }

      mlock.lock();
      //Check Current mode
      char buffer[500];
      int buffer_size = sizeof(buffer);
      if (RunActiveForControl){
	db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Run Config/Mode",&buffer,&buffer_size,TID_STRING, 0);
	CurrentMode = string(buffer);
	if (CurrentMode.compare("Continuous")==0 || CurrentMode.compare("Interactive")==0 || CurrentMode.compare("Idle")==0 || CurrentMode.compare("Sleep")==0){
	  cm_msg(MINFO,"control loop","Current Mode: %s",CurrentMode.c_str());
	}else{
	  cm_msg(MERROR,"control loop","Invalid mode for running: %s",CurrentMode.c_str());
	  CurrentMode = string("Continuous");
	  cm_msg(MINFO,"control loop","Current Mode: %s",CurrentMode.c_str());
	}
      }else{
	db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Run Config/Idle Mode",&buffer,&buffer_size,TID_STRING, 0);
	CurrentMode = string(buffer);
	if (CurrentMode.compare("Idle")==0 || CurrentMode.compare("Sleep")==0){
	  cm_msg(MINFO,"control loop","Current Mode: %s",CurrentMode.c_str());
	}else{
	  cm_msg(MERROR,"control loop","Invalid mode for idle state: %s",CurrentMode.c_str());
	  CurrentMode = string("Idle");
	  cm_msg(MINFO,"control loop","Current Mode: %s",CurrentMode.c_str());
	}
      }
      //Get settings for each mode
      db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Run Config/Continuous/Baseline Cycles",&BaselineCycles,&size_INT,TID_INT, 0);
      db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Run Config/Interactive/Repeat",&Repeat,&size_INT,TID_INT, 0);
      db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Run Config/Idle/Read Barcode",&ReadBarcode,&size_BOOL,TID_BOOL, 0);
      db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Run Config/Sleep/Power Down",&PowerDown,&size_BOOL,TID_BOOL, 0);

      //Update trolley voltage
      unsigned int readback;
      DeviceRead(reg_trolley_ldo_status, &readback);
      INT InitialVoltage = readback & 0xFF;
      INT SetTrolleyVoltage = 0;
      db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Trolley Power/Voltage",&SetTrolleyVoltage,&size_INT,TID_INT, 0);
      int reg_value = 0;
      //In Sleep mode, if PowerDown==true, turn trolley off
      if (CurrentMode.compare("Sleep")==0 && PowerDown){
	RampTrolleyVoltage(InitialVoltage,0);
      }else{
	RampTrolleyVoltage(InitialVoltage,SetTrolleyVoltage);
      }
      DeviceRead(reg_trolley_ldo_status, &readback);
      cm_msg(MINFO,"control loop","Trolley Power : %d",readback & 0xFF);

      //Set RF frequency and Amplitude
      double RF_Freq;
      double RF_Amp;
      INT Size_DOUBLE = sizeof(RF_Freq);
      db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Sg382/RF Frequency",&RF_Freq,&Size_DOUBLE,TID_DOUBLE, 0);
      db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Sg382/RF Amplitude",&RF_Amp,&Size_DOUBLE,TID_DOUBLE, 0);
//      SetFrequency(RF_Freq);
//      SetAmplitude(RF_Amp);

      //Send trolley command based on Mode
      if (CurrentMode.compare("Continuous")==0){ 
	DeviceWrite(reg_command,0x0021);
	if (Cmd==1){
	  //Set FIFO to circular, if cmd==1
	  DeviceWriteMask(reg_nmr_control,0x00000002, 0x00000000);
	  //Reload FIFO and do not do startup cycles, if cmd==1
	  StartUpCount=0;
	  LoadProbeSettings();
	  //Start FIFO
	  DeviceWriteMask(reg_nmr_control,0x00000001, 0x00000001);
	  DeviceWriteMask(reg_nmr_control,0x00000001, 0x00000000);
	}
      }else if (CurrentMode.compare("Interactive")==0 ){
	//Set FIFO to single shot
	DeviceWriteMask(reg_nmr_control,0x00000002, 0x00000002);
	//Clear FIFO  (No probes are loaded)
	DeviceWriteMask(reg_nmr_control,0x00000001, 0x00000001);
	DeviceWriteMask(reg_nmr_control,0x00000001, 0x00000000);
	DeviceWrite(reg_command,0x0021);
	RepeatCount = Repeat;
	Executing = 0;
      }else if (CurrentMode.compare("Sleep")==0){
	DeviceWrite(reg_command,0x0000);
	//Clear FIFO
	DeviceWriteMask(reg_nmr_control,0x00000001, 0x00000001);
	DeviceWriteMask(reg_nmr_control,0x00000001, 0x00000000);
      }else if (CurrentMode.compare("Idle")==0){
	if(ReadBarcode)DeviceWrite(reg_command,0x0021);
	else DeviceWrite(reg_command,0x0000);
	//Clear FIFO
	DeviceWriteMask(reg_nmr_control,0x00000001, 0x00000001);
	DeviceWriteMask(reg_nmr_control,0x00000001, 0x00000000);
      }
      
      //Load general settings to trolley interface, no probe settings
      LoadGeneralSettingsToInterface();

      //Handling Cmd==2 and Cmd==3, for run start/stop
      if (Cmd==2){
	if (CurrentMode.compare("Continuous")==0){
	  StartUpCount = BaselineCycles;
	  if (StartUpCount>0){
	    DeviceWriteMask(reg_power_control1,0x00001000, 0x00001000);//Disable TX
	    //Single shot
	    DeviceWriteMask(reg_nmr_control,0x00000002, 0x00000002);
	  }else{
	    DeviceWriteMask(reg_power_control1,0x00001000, 0x00000000);//Enable TX
	    //Set Fifo to be circular
	    DeviceWriteMask(reg_nmr_control,0x00000002, 0x00000000);
	  }
	  //Load FIFO
	  LoadProbeSettings();
	  //Start FIFO
	  DeviceWriteMask(reg_nmr_control,0x00000001, 0x00000001);
	  DeviceWriteMask(reg_nmr_control,0x00000001, 0x00000000);
	}else if (CurrentMode.compare("Interactive")==0){
	  RepeatCount = Repeat;
	  Executing = 0;
	}
	//Read back registries to odb
	UpdateGeneralOdbSettings();
      }else if (Cmd==3){
	//Read back registries to odb
	UpdateGeneralOdbSettings();
      }
      mlock.unlock();

      //When command finishes, set it back to 0
      Cmd = 0;
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cmd",&Cmd,size_INT, 1 ,TID_INT); 
    }
    //Here read back from interface, load to odb, handling interactive mode, startup of continuous mode
    if (DebugLevel>0){
      mlock.lock();
      UpdateGeneralOdbSettings();
      mlock.unlock();
    }
    mlock.lock();
    //update odb CurrentMode
    char ModeName[32];
    sprintf(ModeName,"%s",CurrentMode.c_str());
    db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Current Mode",&ModeName,sizeof(ModeName), 1 ,TID_STRING); 

    unsigned int FIFORunning = 0;
    //Check FIFO status
    DeviceReadMask(reg_nmr_status,0x00000001,&FIFORunning);
    db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/FIFO Status",(INT *)&FIFORunning,sizeof(FIFORunning), 1 ,TID_INT); 
    if (CurrentMode.compare("Continuous")==0 && StartUpCount>0){
      if (FIFORunning==0){
	StartUpCount--;
	if (StartUpCount<=0){
	  DeviceWriteMask(reg_power_control1,0x00001000, 0x00000000);//Enable TX
	  //Set Fifo to be circular
	  DeviceWriteMask(reg_nmr_control,0x00000002, 0x00000000);
	}
	//Load Probe settings
	LoadProbeSettings();
	//Start FIFO
	DeviceWriteMask(reg_nmr_control,0x00000001, 0x00000001);
	DeviceWriteMask(reg_nmr_control,0x00000001, 0x00000000);
      }
    }else if (CurrentMode.compare("Interactive")==0){
      if (Executing == 1){
	if (FIFORunning==0){
	  RepeatCount--;
	  if (RepeatCount<=0){
	    Executing = 0;
	  }else{
	    //Load Probe settings
	    LoadProbeSettings();
	    //Start FIFO
	    DeviceWriteMask(reg_nmr_control,0x00000001, 0x00000001);
	    DeviceWriteMask(reg_nmr_control,0x00000001, 0x00000000);
	  }
	}
      }else{
	db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Run Config/Interactive/Trigger",&Trigger,&size_INT,TID_INT, 0);
	if (Trigger==1){
	  Executing = 1;
	  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/FIFO Status",&Executing,size_INT, 1 ,TID_INT); 
	  //Load Probe settings
	  LoadProbeSettings();
	  //Start FIFO
	  DeviceWriteMask(reg_nmr_control,0x00000001, 0x00000001);
	  DeviceWriteMask(reg_nmr_control,0x00000001, 0x00000000);
	  RepeatCount = Repeat;
	  //Reset Trigger
	  Trigger = 0;
	  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Run Config/Interactive/Trigger",&Trigger,size_INT, 1 ,TID_INT); 
	}
      }
    }
    mlock.unlock();

    //Sleep for some time
    usleep(100000);
  }
  int ControlThreadActive = 0;
  mlock.lock();
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Control Thread Active",&ControlThreadActive,sizeof(ControlThreadActive), 1 ,TID_BOOL); 
  mlock.unlock();
}

void LoadGeneralSettingsToInterface()
{
  //Cycle
  INT Interface_Comm_Stop;
  INT Trolley_Comm_Start;
  INT Trolley_Comm_Data_Start;
  INT Trolley_Comm_Stop;
  INT Switch_To_RF;
  INT Power_ON;
  INT RF_Enable;
  INT Switch_To_Comm;
  INT Interface_Comm_Start;
  INT Cycle_Length;
  INT RF_Prescale;
  INT Switch_RF_Offset;
  INT Switch_Comm_Offset;

  //Barcode
  INT BC_LED_Voltage;
  INT BC_Sample_Period;
  INT BC_Acq_Delay;

  INT size_INT = sizeof(RF_Prescale);//Size for all ODB registies here

  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/Interface Comm Stop",&Interface_Comm_Stop,&size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/Trolley Comm Start",&Trolley_Comm_Start,&size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/Trolley Comm Data Start",&Trolley_Comm_Data_Start,&size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/Trolley Comm Stop",&Trolley_Comm_Stop,&size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/Switch To RF",&Switch_To_RF,&size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/Power ON",&Power_ON,&size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/RF Enable",&RF_Enable,&size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/Switch To Comm",&Switch_To_Comm,&size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/Interface Comm Start",&Interface_Comm_Start,&size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/Cycle Length",&Cycle_Length,&size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/RF Prescale",&RF_Prescale,&size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/Switch RF Offset",&Switch_RF_Offset,&size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/Switch Comm Offset",&Switch_Comm_Offset,&size_INT,TID_INT, 0);

  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Barcode/LED Voltage",&BC_LED_Voltage,&size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Barcode/Sample Period",&BC_Sample_Period,&size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Barcode/Acq Delay",&BC_Acq_Delay,&size_INT,TID_INT, 0);

  //Load registries to Trolley Interface
  DeviceWrite(reg_comm_t_tid_stop,static_cast<unsigned int>(Interface_Comm_Stop));
  DeviceWrite(reg_comm_t_td_start,static_cast<unsigned int>(Trolley_Comm_Start));
  DeviceWrite(reg_comm_t_td,static_cast<unsigned int>(Trolley_Comm_Data_Start));     
  DeviceWrite(reg_comm_t_td_stop,static_cast<unsigned int>(Trolley_Comm_Stop));  
  DeviceWrite(reg_comm_t_switch_rf,static_cast<unsigned int>(Switch_To_RF));
  DeviceWrite(reg_comm_t_power_on,static_cast<unsigned int>(Power_ON)); 
  DeviceWrite(reg_comm_t_rf_on,static_cast<unsigned int>(RF_Enable));   
  DeviceWrite(reg_comm_t_switch_comm,static_cast<unsigned int>(Switch_To_Comm));
  DeviceWrite(reg_comm_t_tid_start,static_cast<unsigned int>(Interface_Comm_Start));
  DeviceWrite(reg_comm_t_cycle_length,static_cast<unsigned int>(Cycle_Length));
  DeviceWrite(reg_nmr_rf_prescale,static_cast<unsigned int>(RF_Prescale));
  DeviceWrite(reg_ti_switch_rf_offset,static_cast<unsigned int>(Switch_RF_Offset));
  DeviceWrite(reg_ti_switch_comm_offset,static_cast<unsigned int>(Switch_Comm_Offset));

  DeviceWrite(reg_bc_sample_period,static_cast<unsigned int>(BC_Sample_Period));
  DeviceWrite(reg_bc_t_acq,static_cast<unsigned int>(BC_Acq_Delay));
  DeviceWrite(reg_bc_refdac2,static_cast<unsigned int>(BC_LED_Voltage));
}

void UpdateGeneralOdbSettings()
{
  //Cycle
  INT Interface_Comm_Stop;
  INT Trolley_Comm_Start;
  INT Trolley_Comm_Data_Start;
  INT Trolley_Comm_Stop;
  INT Switch_To_RF;
  INT Power_ON;
  INT RF_Enable;
  INT Switch_To_Comm;
  INT Interface_Comm_Start;
  INT Cycle_Length;
  INT RF_Prescale;
  INT Switch_RF_Offset;
  INT Switch_Comm_Offset;

  //Barcode
  INT BC_LED_Voltage;
  INT BC_Sample_Period;
  INT BC_Acq_Delay;

  INT size_INT = sizeof(RF_Prescale);//Size for all ODB registies here

  //Buffer Load
  INT Buffer_Load;

  //Read registries from Trolley Interface
  DeviceRead(reg_comm_t_tid_stop,(unsigned int *)(& Interface_Comm_Stop));
  DeviceRead(reg_comm_t_td_start,(unsigned int *)(& Trolley_Comm_Start));
  DeviceRead(reg_comm_t_td,(unsigned int *)(& Trolley_Comm_Data_Start));     
  DeviceRead(reg_comm_t_td_stop,(unsigned int *)(& Trolley_Comm_Stop));  
  DeviceRead(reg_comm_t_switch_rf,(unsigned int *)(& Switch_To_RF));
  DeviceRead(reg_comm_t_power_on,(unsigned int *)(& Power_ON)); 
  DeviceRead(reg_comm_t_rf_on,(unsigned int *)(& RF_Enable));   
  DeviceRead(reg_comm_t_switch_comm,(unsigned int *)(& Switch_To_Comm));
  DeviceRead(reg_comm_t_tid_start,(unsigned int *)(& Interface_Comm_Start));
  DeviceRead(reg_comm_t_cycle_length,(unsigned int *)(& Cycle_Length));
  DeviceRead(reg_nmr_rf_prescale,(unsigned int *)(& RF_Prescale));
  DeviceRead(reg_ti_switch_rf_offset,(unsigned int *)(& Switch_RF_Offset));
  DeviceRead(reg_ti_switch_comm_offset,(unsigned int *)(& Switch_Comm_Offset));

  DeviceRead(reg_bc_sample_period,(unsigned int *)(& BC_Sample_Period));
  DeviceRead(reg_bc_t_acq,(unsigned int *)(& BC_Acq_Delay));
  DeviceRead(reg_bc_refdac2,(unsigned int *)(& BC_LED_Voltage));

  DeviceRead(reg_free_event_memory,(unsigned int *)(& Buffer_Load));

  //Load values back to odb
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/Interface Comm Stop",&Interface_Comm_Stop,size_INT, 1,TID_INT);
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/Trolley Comm Start",&Trolley_Comm_Start,size_INT, 1,TID_INT);
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/Trolley Comm Data Start",&Trolley_Comm_Data_Start,size_INT, 1,TID_INT);
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/Trolley Comm Stop",&Trolley_Comm_Stop,size_INT, 1,TID_INT);
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/Switch To RF",&Switch_To_RF,size_INT, 1,TID_INT);
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/Power ON",&Power_ON,size_INT, 1,TID_INT);
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/RF Enable",&RF_Enable,size_INT, 1,TID_INT);
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/Switch To Comm",&Switch_To_Comm,size_INT, 1,TID_INT);
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/Interface Comm Start",&Interface_Comm_Start,size_INT, 1,TID_INT);
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/Cycle Length",&Cycle_Length,size_INT, 1,TID_INT);
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/RF Prescale",&RF_Prescale,size_INT, 1,TID_INT);
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/Switch RF Offset",&Switch_RF_Offset,size_INT, 1,TID_INT);
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cycle/Switch Comm Offset",&Switch_Comm_Offset,size_INT, 1,TID_INT);

  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Barcode/LED Voltage",&BC_LED_Voltage,size_INT, 1,TID_INT);
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Barcode/Sample Period",&BC_Sample_Period,size_INT, 1,TID_INT);
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Barcode/Acq Delay",&BC_Acq_Delay,size_INT, 1,TID_INT);

  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Interface Buffer Load",&Buffer_Load,size_INT, 1,TID_INT);
}

int RampTrolleyVoltage(int InitialVoltage,int TargetVoltage)
{
  int reg_value = 0;
  if (InitialVoltage<TargetVoltage){//Ramp Up
    for (int i=InitialVoltage;i<=TargetVoltage;i++){
      reg_value = i; 
      reg_value &= 0xFF;
      reg_value |= (reg_value << 8);
      if (reg_value !=0){
	reg_value |= 0x00220000; //Enable both potentiometers
      }
      DeviceWrite(reg_trolley_ldo_config, reg_value);                              // Set the Trolley Voltage Control Register
      DeviceWrite(reg_trolley_ldo_config_load, 0x00000001);                 // Load the new voltage setting.
      usleep(10000);
    }
  }else if(InitialVoltage>TargetVoltage){//Ramp Down
    for (int i=InitialVoltage;i>=TargetVoltage;i--){
      reg_value = i; 
      reg_value &= 0xFF;
      reg_value |= (reg_value << 8);
      if (reg_value !=0){
	reg_value |= 0x00220000; //Enable both potentiometers
      }
      DeviceWrite(reg_trolley_ldo_config, reg_value);                              // Set the Trolley Voltage Control Register
      DeviceWrite(reg_trolley_ldo_config_load, 0x00000001);                 // Load the new voltage setting.
      usleep(10000);
    }
  }else{
    ;//No need to do anything
  }
  return 0;
}

int LoadProbeSettings()
{
  //Get Load Method, and script filename
  string ScriptDir;
  string ScriptName;
  string ScriptFileName;
  string Source;
  char buffer1[500];
  char buffer2[500];
  char buffer3[500];
  int buffer1_size = sizeof(buffer1);
  int buffer2_size = sizeof(buffer2);
  int buffer3_size = sizeof(buffer3);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Probe/Source",&buffer1,&buffer1_size,TID_STRING, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Probe/Script Dir",&buffer2,&buffer2_size,TID_STRING, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Probe/Script",&buffer3,&buffer3_size,TID_STRING, 0);
  Source=string(buffer1);
  ScriptDir=string(buffer2);
  ScriptName=string(buffer3);
  ScriptFileName = ScriptDir + ScriptName + string(".scr");
  int ConfigCount = 0;
  //Odb keys
  INT ProbeID;
  INT ProbeEnable;
  INT PreampDelay;
  INT PreampPeriod;
  INT ADCGateDelay;
  INT ADCGateOffset;
  INT ADCGatePeriod;
  INT TXDelay;
  INT TXPeriod;
  INT UserData;
  int size_INT = sizeof(ProbeID);
  //Create a NMR_Config struct
  NMR_Config NMR_Setting;
  if (Source.compare("Odb")==0){
    for (int i=0;i<17;i++){
      char key[500];
      sprintf(key,"/Equipment/TrolleyInterface/Settings/Probe/Probe%d/Probe ID",i);
      db_get_value(hDB,0,key,&ProbeID,&size_INT,TID_INT, 0);
      sprintf(key,"/Equipment/TrolleyInterface/Settings/Probe/Probe%d/Probe Enable",i);
      db_get_value(hDB,0,key,&ProbeEnable,&size_INT,TID_INT, 0);
      sprintf(key,"/Equipment/TrolleyInterface/Settings/Probe/Probe%d/Probe Delay",i);
      db_get_value(hDB,0,key,&PreampDelay,&size_INT,TID_INT, 0);
      sprintf(key,"/Equipment/TrolleyInterface/Settings/Probe/Probe%d/Preamp Period",i);
      db_get_value(hDB,0,key,&PreampPeriod,&size_INT,TID_INT, 0);
      sprintf(key,"/Equipment/TrolleyInterface/Settings/Probe/Probe%d/ADC Gate Delay",i);
      db_get_value(hDB,0,key,&ADCGateDelay,&size_INT,TID_INT, 0);
      sprintf(key,"/Equipment/TrolleyInterface/Settings/Probe/Probe%d/ADC Gate Offset",i);
      db_get_value(hDB,0,key,&ADCGateOffset,&size_INT,TID_INT, 0);
      sprintf(key,"/Equipment/TrolleyInterface/Settings/Probe/Probe%d/ADC Gate Period",i);
      db_get_value(hDB,0,key,&ADCGatePeriod,&size_INT,TID_INT, 0);
      sprintf(key,"/Equipment/TrolleyInterface/Settings/Probe/Probe%d/TX Delay",i);
      db_get_value(hDB,0,key,&TXDelay,&size_INT,TID_INT, 0);
      sprintf(key,"/Equipment/TrolleyInterface/Settings/Probe/Probe%d/TX Period",i);
      db_get_value(hDB,0,key,&TXPeriod,&size_INT,TID_INT, 0);
      sprintf(key,"/Equipment/TrolleyInterface/Settings/Probe/Probe%d/User Data",i);
      db_get_value(hDB,0,key,&UserData,&size_INT,TID_INT, 0);
      NMR_Setting.NMR_Command = static_cast<unsigned int>(ProbeID) | (static_cast<unsigned int>(ProbeEnable) <<15); 
      NMR_Setting.NMR_Preamp_Delay = static_cast<unsigned int>(PreampDelay);
      NMR_Setting.NMR_Preamp_Period = static_cast<unsigned int>(PreampPeriod);
      NMR_Setting.NMR_ADC_Gate_Delay = static_cast<unsigned int>(ADCGateDelay);
      NMR_Setting.NMR_ADC_Gate_Offset = static_cast<unsigned int>(ADCGateOffset);
      NMR_Setting.NMR_ADC_Gate_Period = static_cast<unsigned int>(ADCGatePeriod);
      NMR_Setting.NMR_TX_Delay = static_cast<unsigned int>(TXDelay);
      NMR_Setting.NMR_TX_Period = static_cast<unsigned int>(TXPeriod);
      NMR_Setting.User_Defined_Data = static_cast<unsigned int>(UserData);
      if (DebugLevel>0){
	cm_msg(MINFO,"LoadProbeSettings","NMR_Command: %x",NMR_Setting.NMR_Command);
	cm_msg(MINFO,"LoadProbeSettings","NMR_TX_Period: %d",NMR_Setting.NMR_TX_Period);
      }
      //Write to FIFO
      DeviceLoadNMRSetting(NMR_Setting);
      ConfigCount++;
    }
  }else if (Source.compare("Script")){
    ifstream ScriptInput;
    ScriptInput.open(ScriptFileName.c_str(),ios::in);
    string RegistryName;
    while(1){
      //Make sure the order is correct!
      //To do : Make sure the loop end at the right position.
      ScriptInput>>RegistryName>>NMR_Setting.NMR_Command;
      if (RegistryName.compare("NMR_Command")!=0)return -1;
      if (ScriptInput.eof())break;
      ScriptInput>>RegistryName>>NMR_Setting.NMR_Preamp_Delay;
      if (RegistryName.compare("NMR_Preamp_Delay")!=0)return -1;
      if (ScriptInput.eof())break;
      ScriptInput>>RegistryName>>NMR_Setting.NMR_Preamp_Period;
      if (RegistryName.compare("NMR_Preamp_Period")!=0)return -1;
      if (ScriptInput.eof())break;
      ScriptInput>>RegistryName>>NMR_Setting.NMR_ADC_Gate_Delay;
      if (RegistryName.compare("NMR_ADC_Gate_Delay")!=0)return -1;
      if (ScriptInput.eof())break;
      ScriptInput>>RegistryName>>NMR_Setting.NMR_ADC_Gate_Offset;
      if (RegistryName.compare("NMR_ADC_Gate_Offset")!=0)return -1;
      if (ScriptInput.eof())break;
      ScriptInput>>RegistryName>>NMR_Setting.NMR_ADC_Gate_Period;
      if (RegistryName.compare("NMR_ADC_Gate_Period")!=0)return -1;
      if (ScriptInput.eof())break;
      ScriptInput>>RegistryName>>NMR_Setting.NMR_TX_Delay;
      if (RegistryName.compare("NMR_TX_Delay")!=0)return -1;
      if (ScriptInput.eof())break;
      ScriptInput>>RegistryName>>NMR_Setting.NMR_TX_Period;
      if (RegistryName.compare("NMR_TX_Period")!=0)return -1;
      if (ScriptInput.eof())break;
      ScriptInput>>RegistryName>>NMR_Setting.User_Defined_Data;
      if (RegistryName.compare("User_Defined_Data")!=0)return -1;
      if (ScriptInput.eof())break;
      //Write to FIFO
      DeviceLoadNMRSetting(NMR_Setting);
      ConfigCount++;
    }
    ScriptInput.close();
  }
  return ConfigCount;
}


