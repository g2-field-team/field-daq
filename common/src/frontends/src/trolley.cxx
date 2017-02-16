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

#include "g2field/TrolleyInterface.h"
#include "g2field/core/field_constants.hh"
#include "g2field/core/field_structs.hh"

#include "TTree.h"
#include "TFile.h"

#define FRONTEND_NAME "Trolley Interface" // Prefer capitalize with spaces

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

void LoadOdbToInterface();
void ReadFromDevice();
void ControlDevice();
BOOL RunActive;

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

  //Makesure the measurements are stopped
  DeviceWrite(reg_command,0x0000);
  //Send Trolley interface command to stop data taking
  DeviceWriteMask(reg_event_data_control,0x00000001,0x00000000);
  //Ramp up the trolley voltage
  INT SetTrolleyVoltage = 0;
  INT Size_INT = sizeof(SetTrolleyVoltage);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Trolley Power Registry/Voltage",&SetTrolleyVoltage,&Size_INT,TID_INT, 0);
  int reg_value = 0;
  for (int i=1;i<=SetTrolleyVoltage;i++){
    reg_value = i; 
    reg_value &= 0xFF;
    reg_value |= (reg_value << 8);
    if (reg_value !=0){
      reg_value |= 0x00220000; //Enable both potentiometers
    }
    DeviceWrite(reg_trolley_power, reg_value);                              // Set the Trolley Voltage Control Register
    DeviceWrite(reg_trolley_power_set, 0x00000001);                 // Load the new voltage setting.
    usleep(10000);
  }
  
  unsigned int readback;
  DeviceRead(reg_trolley_power, &readback);
  cm_msg(MINFO,"init","Trolley Power : %d",readback & 0xFF);

  //Configure power
 // DeviceWrite(reg_power_control2,0x0004);
  //Configure Probe
  DeviceWrite(0x400004C0,17);
  DeviceWrite(reg_nmr_rf_probe_select,0);
  DeviceWrite(reg_bc_refdac2,1024);
  DeviceWrite(reg_command,0x0100);
  DeviceWrite(reg_command,0x0200);
  sleep(1);
  DeviceWrite(reg_command,0x0000);
  DevicePurgeData();

  return SUCCESS;
}

/*-- Frontend Exit -------------------------------------------------*/

INT frontend_exit()
{
  //Send Trolley interface command to stop data taking
  DeviceWriteMask(reg_event_data_control,0x00000001,0x00000001);
  DeviceWrite(reg_command,0x0000);
  //Ramp down the trolley power
  INT SetTrolleyVoltage = 0;
  INT Size_INT = sizeof(SetTrolleyVoltage);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Trolley Power Registry/Voltage",&SetTrolleyVoltage,&Size_INT,TID_INT, 0);
  int reg_value = 0;
  for (int i=SetTrolleyVoltage;i>=0;i--){
    reg_value = i; 
    reg_value &= 0xFF;
    reg_value |= (reg_value << 8);
    if (reg_value !=0){
      reg_value |= 0x00220000;; //Enable both potentiometers
    }
    DeviceWrite(reg_trolley_power, reg_value);                              // Set the Trolley Voltage Control Register
    DeviceWrite(reg_trolley_power_set, 0x00000001);                 // Load the new voltage setting.
    usleep(10000);
  }

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

  LoadOdbToInterface();

  //Send Trolley interface command to start data taking
  DeviceWrite(reg_command,0x0031);
  //  DeviceWriteMask(0x40000944,0x00000001,0x00000000);

  DeviceWriteMask(reg_command,0x00000100,0x00000100);
  sleep(1);
  DeviceWriteMask(reg_command,0x00000100,0x00000000);

  //Start reading thread
  RunActive=true;
  read_thread = thread(ReadFromDevice);

  //Create control thread
  control_thread = thread(ControlDevice);
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
  control_thread.join();
  cm_msg(MINFO,"exit","All threads joined.");
  TrlyNMRBuffer.clear();
  TrlyBarcodeBuffer.clear();
  TrlyMonitorBuffer.clear();
  cm_msg(MINFO,"exit","Data buffer is emptied before exit.");

  //Turn off LED
  DeviceWrite(reg_bc_refdac2,1024);
  DeviceWriteMask(reg_command,0x00000100,0x00000100);
  sleep(1);
  DeviceWriteMask(reg_command,0x00000100,0x00000000);
  //Send Trolley interface command to stop data taking
  DeviceWriteMask(reg_command,0x00000001,0x00000001);
  //Clear buffer
  DeviceWrite(reg_command,0x0200);
  sleep(1);
  DeviceWrite(reg_command,0x0000);
  DevicePurgeData();

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
      PressureSensorCal[7];
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
  int LastFrameNumber = 0;
  int FrameNumber = 0;
 
  //Monitor data will be synced to ODB
  BOOL BarcodeError = TRUE;
  BOOL PowersupplyStatus[3];
  BOOL TemperatureInterupt = TRUE;
  unsigned short PressureSensorCal[7];
  unsigned int NMRCheckSum;
  unsigned int FrameCheckSum;
  BOOL NMRCheckSumPassed = FALSE;
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
  BOOL TemperatureInteruptOld;
  BOOL PowersupplyStatusOld[3];
  BOOL NMRCheckSumPassedOld;
  BOOL FrameCheckSumPassedOld;


  int FrameSize = 0;
  //Frame buffer
  unsigned short* Frame = new unsigned short[MAX_PAYLOAD_DATA/sizeof(unsigned short)];

  int ReadThreadActive = 1;
  mlock.lock();
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Read Thread Active",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
  mlock.unlock();

  //Read first frame and sync
  int rc = DataReceive((void *)Frame);
  if (rc<0){
    ReadThreadActive = 0;
    mlock.lock();
    db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Read Thread Active",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
    cm_msg(MERROR,"ReadFromDevice","Fail from first reading, error code = %d",rc);
    mlock.unlock();
    return;
  }
  memcpy(&LastFrameNumber,&(Frame[9]),sizeof(int));
  memcpy(&FrameSize,&(Frame[7]),sizeof(int));
 
  //Readout loop
  int i=0;
  while (1){
    //Check if the front-end is active
    BOOL localRunActive;
    mlock.lock();
    localRunActive = RunActive;
    mlock.unlock();
    if (!localRunActive)break;

    //Read Frame
    rc = DataReceive((void *)Frame);
    if (rc<0){
      ReadThreadActive = 0;
      mlock.lock();
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Read Thread Active",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
      cm_msg(MERROR,"ReadFromDevice","Fail from regular reading, error code = %d",rc);
      mlock.unlock();
      return;
    }
    memcpy(&FrameNumber,&(Frame[9]),sizeof(int));
    memcpy(&FrameSize,&(Frame[7]),sizeof(int));
    mlock.lock();
    db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Data Frame Index",&FrameNumber,sizeof(FrameNumber), 1 ,TID_INT); 
    mlock.unlock();

    if (FrameNumber!=(LastFrameNumber+1)){
      mlock.lock();
      cm_msg(MERROR,"ReadFromDevice","Skipping frame at iteration %d",i);
      mlock.unlock();
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
      mlock.lock();
      cm_msg(MINFO,"ReadFromDevice","NMR sample overflow, length = %d",Frame[12]);
      mlock.unlock();
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
      mlock.lock();
      cm_msg(MINFO,"ReadFromDevice","Barcode sample overflow, length = %d",Frame[13]);
      mlock.unlock();
    }
    for (unsigned short ii=0;ii<NSamBarcode;ii++){
      TrlyBarcodeDataUnit->traces[ii] = Frame[64+Frame[12]+ii];
    }

    memcpy(&(TrlyMonitorDataUnit->gps_clock_cycle_start),&(Frame[34]),sizeof(unsigned long int));
    memcpy(&(TrlyMonitorDataUnit->PMonitorVal),&(Frame[30]),sizeof(int));
    memcpy(&(TrlyMonitorDataUnit->PMonitorTemp),&(Frame[32]),sizeof(int));
    memcpy(&(TrlyMonitorDataUnit->RFPower1),&(Frame[60]),sizeof(int));
    memcpy(&(TrlyMonitorDataUnit->RFPower2),&(Frame[62]),sizeof(int));
    //check sums are filled later
    memcpy(&(TrlyMonitorDataUnit->FrameIndex),&(Frame[9]),sizeof(int));
    TrlyMonitorDataUnit->StatusBits = (Frame[11]>>8);//Get rid of probe index by bit shift
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
    //keep previous conditions
    BarcodeErrorOld = BarcodeError;
    TemperatureInteruptOld = TemperatureInterupt;
    PowersupplyStatusOld[0] = PowersupplyStatus[0];
    PowersupplyStatusOld[1] = PowersupplyStatus[1];
    PowersupplyStatusOld[2] = PowersupplyStatus[2];
    NMRCheckSumPassedOld = NMRCheckSumPassed;
    FrameCheckSumPassedOld = FrameCheckSumPassed;

    //Get new monitor values
    BarcodeError = BOOL(0x100 & Frame[11]);
    TemperatureInterupt = !BOOL(0x200 & Frame[11]);//Low active
    PowersupplyStatus[0] = BOOL(0x400 & Frame[11]);
    PowersupplyStatus[1] = BOOL(0x800 & Frame[11]);
    PowersupplyStatus[2] = BOOL(0x1000 & Frame[11]);
    memcpy(&(NMRCheckSum),&(Frame[46]),sizeof(int));
    memcpy(&(FrameCheckSum),&(Frame[64+NSamNMR+NSamBarcode+Frame[13]*2]),sizeof(int));
    for (short ii=0;ii<7;ii++){
      PressureSensorCal[ii] = Frame[23+ii];;
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
    for (unsigned short ii=0;ii<Frame[12];ii++){
      sum1+=(unsigned int)Frame[64+ii];
    }
    int NWords = FrameSize/2-2;
    for (int ii=0;ii<NWords;ii++){
      sum2+=(unsigned int)Frame[ii];
    }
    //Correction for 0x7FFF
    sum2+=0x7FFF;
    //////////////////////
    NMRCheckSumPassed = (sum1==NMRCheckSum);
    FrameCheckSumPassed = (sum2==FrameCheckSum);
    TrlyMonitorDataUnit->NMRCheckSum = NMRCheckSum;
    TrlyMonitorDataUnit->FrameCheckSum = FrameCheckSum;
    TrlyMonitorDataUnit->FrameSum = sum2;

    PowerFactor = TrlyMonitorDataUnit->RFPower2 / TrlyMonitorDataUnit->RFPower1;
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
    db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Temperature Interupt",&TemperatureInterupt,sizeof(TemperatureInterupt), 1 ,TID_BOOL);
    db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Power Supply Status",&PowersupplyStatus,sizeof(PowersupplyStatus), 3 ,TID_BOOL);
    db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/NMR Check Sum",&NMRCheckSumPassed,sizeof(NMRCheckSumPassed), 1 ,TID_BOOL);
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
    if(TemperatureInterupt && (!TemperatureInteruptOld || i==0))cm_msg(MERROR,"ReadFromDevice","Message from trolley interface: Temperature interupt. At iteration %d",i);
    if(!PowersupplyStatus[0] && (PowersupplyStatusOld[0]) || i==0 )cm_msg(MERROR,"ReadFromDevice","Message from trolley interface: Power supply error 1.At iteration %d",i);
    if(!PowersupplyStatus[1] && (PowersupplyStatusOld[1] || i==0))cm_msg(MERROR,"ReadFromDevice","Message from trolley interface: Power supply error 2. At iteration %d",i);
    if(!PowersupplyStatus[2] && (PowersupplyStatusOld[2]) || i==0)cm_msg(MERROR,"ReadFromDevice","Message from trolley interface: Power supply error 3. At iteration %d",i);
    if(!NMRCheckSumPassed && (NMRCheckSumPassedOld || i==0))cm_msg(MERROR,"ReadFromDevice","Message from trolley interface: NMR check sum failed. At iteration %d. Sum expected = %d. Diff %d",i,NMRCheckSum,NMRCheckSum-sum1);
    if(!FrameCheckSumPassed && (FrameCheckSumPassedOld || i==0))cm_msg(MERROR,"ReadFromDevice","Message from trolley interface: Frame check sum failed. At iteration %d. Sum expected = %d. Diff %d",i,FrameCheckSum,FrameCheckSum-sum2);
    //if(!NMRCheckSumPassed )cm_msg(MERROR,"ReadFromDevice","Message from trolley interface: NMR check sum failed. At iteration %d. Sum expected = %d. Diff %d",i,NMRCheckSum,NMRCheckSum-sum1);
    //if(!FrameCheckSumPassed )cm_msg(MERROR,"ReadFromDevice","Message from trolley interface: Frame check sum failed. At iteration %d. Sum expected = %d. Diff %d",i,FrameCheckSum,FrameCheckSum-sum2);
    mlock.unlock();


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
  db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitor/Read Thread Active",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
  mlock.unlock();
  delete []Frame;
}

//Control Device
void ControlDevice(){
  while (1){
    //Check if the front-end is active
    BOOL localRunActive;
    mlock.lock();
    localRunActive = RunActive;
    mlock.unlock();
    if (!localRunActive)break;
    INT SetTrolleyVoltage = 0;
    INT Cmd = 0;
    INT Size_INT = sizeof(SetTrolleyVoltage);
    mlock.lock();
    db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Trolley Power Registry/Voltage",&SetTrolleyVoltage,&Size_INT,TID_INT, 0);
    db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cmd",&Cmd,&Size_INT,TID_INT, 0);
    if (Cmd == 1){
      int reg_value = 0;
      reg_value = SetTrolleyVoltage; 
      reg_value &= 0xFF;
      reg_value |= (reg_value << 8);
      if (reg_value !=0){
	reg_value |= 0x00220000;; //Enable both potentiometers
      }
      DeviceWrite(reg_trolley_power, reg_value);                              // Set the Trolley Voltage Control Register
      DeviceWrite(reg_trolley_power_set, 0x00000001);                 // Load the new voltage setting.
      usleep(10000);
      unsigned int readback;
      DeviceRead(reg_trolley_power, &readback);
      cm_msg(MINFO,"init","Trolley Power : %d",readback & 0xFF);
      LoadOdbToInterface();
      Cmd = 0;
      db_set_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Cmd",&Cmd,Size_INT, 1 ,TID_INT); 
    }
    mlock.unlock();
    sleep(1);
  }
}

void LoadOdbToInterface()
{
  //Get ODB Values for registies
  INT Interface_Comm_Start;
  INT Interface_Comm_Data_Start;
  INT Interface_Comm_Stop;
  INT Trolley_Comm_Start;
  INT Trolley_Comm_Data_Start;
  INT Trolley_Comm_Stop;
  INT Switch_To_RF;
  INT Power_ON;
  INT RF_Enable;
  INT RF_Disable;
  INT Power_OFF;
  INT Switch_To_Comm;
  INT Cycle_Length;

  INT NMR_RF_Prescale;
  INT NMR_Probe_Select;
  INT NMR_Probe_Delay;
  INT NMR_Probe_Period;
  INT NMR_Preamp_Delay;
  INT NMR_Preamp_Period;
  INT NMR_Gate_Delay;
  INT NMR_Gate_Period;
  INT NMR_Transmit_Delay;
  INT NMR_Transmit_Period;

  INT BC_Sample_Period;
  INT BC_Acq_Delay;

  INT Size_INT = sizeof(NMR_RF_Prescale);//Size for all ODB registies here

  double BC_LED_Voltage = 0.0;
  int BC_LED_V_size = sizeof(BC_LED_Voltage);

  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Communication Registry/Interface Comm Start",&Interface_Comm_Start,&Size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Communication Registry/Interface Comm Data Start",&Interface_Comm_Data_Start,&Size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Communication Registry/Interface Comm Stop",&Interface_Comm_Stop,&Size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Communication Registry/Trolley Comm Start",&Trolley_Comm_Start,&Size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Communication Registry/Trolley Comm Data Start",&Trolley_Comm_Data_Start,&Size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Communication Registry/Trolley Comm Stop",&Trolley_Comm_Stop,&Size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Communication Registry/Switch To RF",&Switch_To_RF,&Size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Communication Registry/Power ON",&Power_ON,&Size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Communication Registry/RF Enable",&RF_Enable,&Size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Communication Registry/RF Disable",&RF_Disable,&Size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Communication Registry/Power OFF",&Power_OFF,&Size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Communication Registry/Switch To Comm",&Switch_To_Comm,&Size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Communication Registry/Cycle Length",&Cycle_Length,&Size_INT,TID_INT, 0);

  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/NMR Registry/RF Prescale",&NMR_RF_Prescale,&Size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/NMR Registry/Probe Select",&NMR_Probe_Select,&Size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/NMR Registry/Probe Delay",&NMR_Probe_Delay,&Size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/NMR Registry/Probe Period",&NMR_Probe_Period,&Size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/NMR Registry/Preamp Delay",&NMR_Preamp_Delay,&Size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/NMR Registry/Preamp Period",&NMR_Preamp_Period,&Size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/NMR Registry/Gate Delay",&NMR_Gate_Delay,&Size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/NMR Registry/Gate Period",&NMR_Gate_Period,&Size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/NMR Registry/Transmit Delay",&NMR_Transmit_Delay,&Size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/NMR Registry/Transmit Period",&NMR_Transmit_Period,&Size_INT,TID_INT, 0);

  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Barcode Registry/Sample Period",&BC_Sample_Period,&Size_INT,TID_INT, 0);
  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Barcode Registry/Acq Delay",&BC_Acq_Delay,&Size_INT,TID_INT, 0);

  db_get_value(hDB,0,"/Equipment/TrolleyInterface/Settings/Barcode Registry/LED Voltage",&BC_LED_Voltage,&BC_LED_V_size,TID_DOUBLE, 0);

  //Load registries to Trolley Interface
  DeviceWrite(reg_comm_t_tid_start,static_cast<unsigned int>(Interface_Comm_Start));
  DeviceWrite(reg_comm_t_tid,static_cast<unsigned int>(Interface_Comm_Data_Start));   
  DeviceWrite(reg_comm_t_tid_stop,static_cast<unsigned int>(Interface_Comm_Stop));
  DeviceWrite(reg_comm_t_td_start,static_cast<unsigned int>(Trolley_Comm_Start));
  DeviceWrite(reg_comm_t_td,static_cast<unsigned int>(Trolley_Comm_Data_Start));     
  DeviceWrite(reg_comm_t_td_stop,static_cast<unsigned int>(Trolley_Comm_Stop));  
  DeviceWrite(reg_comm_t_switch_rf,static_cast<unsigned int>(Switch_To_RF));
  DeviceWrite(reg_comm_t_power_on,static_cast<unsigned int>(Power_ON)); 
  DeviceWrite(reg_comm_t_rf_on,static_cast<unsigned int>(RF_Enable));   
  DeviceWrite(reg_comm_t_rf_off,static_cast<unsigned int>(RF_Disable)); 
  DeviceWrite(reg_comm_t_power_off,static_cast<unsigned int>(Power_OFF));
  DeviceWrite(reg_comm_t_switch_comm,static_cast<unsigned int>(Switch_To_Comm));
  DeviceWrite(reg_comm_t_cycle_length,static_cast<unsigned int>(Cycle_Length));

  DeviceWrite(reg_nmr_rf_prescale,static_cast<unsigned int>(NMR_RF_Prescale));
  DeviceWrite(reg_nmr_rf_probe_select,static_cast<unsigned int>(NMR_Probe_Select));
  DeviceWrite(reg_nmr_rf_probe_delay,static_cast<unsigned int>(NMR_Probe_Delay));
  DeviceWrite(reg_nmr_rf_probe_period,static_cast<unsigned int>(NMR_Probe_Period));
  DeviceWrite(reg_nmr_rf_preamp_delay,static_cast<unsigned int>(NMR_Preamp_Delay));
  DeviceWrite(reg_nmr_rf_preamp_period,static_cast<unsigned int>(NMR_Preamp_Period));
  DeviceWrite(reg_nmr_rf_gate_delay,static_cast<unsigned int>(NMR_Gate_Delay));
  DeviceWrite(reg_nmr_rf_gate_period,static_cast<unsigned int>(NMR_Gate_Period));
  DeviceWrite(reg_nmr_rf_transmit_delay,static_cast<unsigned int>(NMR_Transmit_Delay));
  DeviceWrite(reg_nmr_rf_transmit_period,static_cast<unsigned int>(NMR_Transmit_Period));

  DeviceWrite(reg_bc_sample_period,static_cast<unsigned int>(BC_Sample_Period/50));
  DeviceWrite(reg_bc_t_acq,static_cast<unsigned int>(BC_Acq_Delay/50));

  //Turn on LED, V=1V
  unsigned int led_v =static_cast<unsigned int>(1024.0*BC_LED_Voltage/2.5);
  DeviceWrite(reg_bc_refdac2,led_v);
}
