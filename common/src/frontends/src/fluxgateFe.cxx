/*****************************************************************************\

Name:   fluxgateFe.cxx
Author: Alec Tewsley-Booth
Email:  aetb@umich.edu

About:  Addresses NI PCIe daq card, reads multiple channels of data, performs simple manipulations on the data, saves a SQL header, and .mid file.

To do: Alter make file to include NI driver header, alter field_structs.hh to include fluxgate structures, find SQL database management
        
\*****************************************************************************/


//--- std includes ----------------------------------------------------------//
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <random>
#include <sys/time.h>
using std::string;

//--- other includes --------------------------------------------------------//
#include "midas.h"
#include "NIDAQmxBase.h"
#include <NIDAQmx.h> //NI drivers for daq card, add to make file
//#include something that addresses sql databases

//--- project includes ------------------------------------------------------//
#include "frontend_utils.hh"
#include "field_structs.hh" //need to add fluxgate data structure

//--- globals ---------------------------------------------------------------//

// @EXAMPLE BEGIN
#define FRONTEND_NAME "Fluxgate Frontend" // Prefer capitalize with spaces
const char * const bank_name = "FLUX"; // 4 letters, try to make sensible
#define DAQMXERRCHECK(functionCall) error=(functionCall); // Error checking for NI DAQmx

// Any structs need to be defined. NEED TO ALTER FOR FLUXGATE
BANK_LIST trigger_bank_list[] = {
  {bank_name, TID_CHAR, sizeof(g2::point_t), NULL},
  {""}
};
// @EXAMPLE END

extern "C" {
  
  // The frontend name (client name) as seen by other MIDAS clients
  char *frontend_name = (char*)FRONTEND_NAME;

  // The frontend file name, don't change it.
  char *frontend_file_name = (char*)__FILE__;
  
  // frontend_loop is called periodically if this variable is TRUE
  BOOL frontend_call_loop = FALSE;
  
  // A frontend status page is displayed with this frequency in ms.
  INT display_period = 1000;
  
  // maximum event size produced by this frontend
  INT max_event_size = 0x8000; // 32 kB @EXAMPLE - adjust if neeeded

  // maximum event size for fragmented events (EQ_FRAGMENTED)
  INT max_event_size_frag = 0x800000; // DEPRECATED
  
  // buffer size to hold events
  INT event_buffer_size = 0x800000;
  
  // Function declarations
  INT frontend_init();
  INT frontend_exit();
  INT begin_of_run(INT run_number, char *error);
  INT end_of_run(INT run_number, char *error);
  INT pause_run(INT run_number, char *error);
  INT resume_run(INT run_number, char *error);

  INT frontend_loop();
  INT read_event(char *pevent, INT off); //renamed from read_trigger_event
  INT poll_event(INT source, INT count, BOOL test); //do I actually need this?
  INT interrupt_configure(INT cmd, INT source, PTYPE adr);

  // Equipment list for fluxgate, changed from POLLED to PERIODIC

  EQUIPMENT equipment[] = 
    {
      {FRONTEND_NAME,   // equipment name 
       { 10, 0,         // event ID, trigger mask (need to figure these out)
         "SYSTEM",      // event buffer (used to be SYSTEM)
         EQ_PERIODIC,     // equipment type 
         0,             // event source, not used 
         "MIDAS",       // format 
         TRUE,          // enabled 
         RO_RUNNING,    // read only when running 
         60000,           // read every 60 seconds 
         0,             // stop run after this event limit 
         0,             // number of sub events 
         1,             // don't log history 
         "", "", "",
       },
       read_event,      // readout routine 
       NULL,
       NULL,
       NULL, 
      },

      {""}
    };

} //extern C

// Put necessary globals in an anonymous namespace here.
// NEED TO LEARN MORE ABOUT THESE
namespace {
boost::property_tree::ptree conf;
boost::property_tree::ptree global_conf;
double point_min = 0.0;
double point_max = 1.0;
long long last_event;
long long next_event;
//--- NI DAQ globals --------------------------------------------------------//
//---------------------------------------------------------------------------//
int32 error=0;
TaskHandle taskHandle=0;
int32 read;
char errBuff[2048]={'\0'};
int32 numChannels = 12;
//--- Channel Parameters ----------------------------------------------------//
//--- Timing Parameters -----------------------------------------------------//
float64 rate = 8000.0;
float64 aqTime = 60.0; //time to acquire in seconds
uInt64 sampsPerChanToAcquire = static_cast<uInt64>(rate*aqTime);
size_t dataSize = static_cast<size_t>(sampsPerChanToAcquire*numChannels)
std::vector<float64> data(dataSize);
//--- DC Coupled Channels ---------------------------------------------------//
const char physicalChannelDC[] = "Dev1/ai0:11"; //creates AI channels 0-11
float64 minVolDC = -10.0;
float64 maxVolDC = 10.0;
//--- AC Coupled Channels ---------------------------------------------------//
}

//--- Frontend Init ---------------------------------------------------------//
INT frontend_init() 
{
  // this is where it should connect to the DAQ
  DAQMXERRCHECK(DAQmxCreateTask("",&taskHandle));
  
  
  // DAQmxCreateTask("",&taskHandle);
  // DAQmxCreateAIVoltageChan(taskHandle, "Dev1/ai0", "Voltage", DAQmx_Val_Cfg_Default, -10.0, 10.0, DAQmx_Val_Volts, NULL);
  
  if( DAQmxFailed(error) ) {
	  DAQmxGetExtendedErrorInfo(errBuff,2048);
	  cm_msg(MERROR,"frontend_init",errBuff);
  } else {cm_msg(MERROR,"frontend_init","DAQ task created");}
  
  return SUCCESS;
}

//--- Frontend Exit ---------------------------------------------------------//
INT frontend_exit()
{
  //clear task
  DAQmxClearTask(taskHandle);
  return SUCCESS;
}

//--- Begin of Run ----------------------------------------------------------//
INT begin_of_run(INT run_number, char *error)
{

  int rc = load_global_settings(global_conf);
  if (rc != 0) {
    cm_msg(MERROR, "begin_of_run", "could not load global settings");
    return rc;
  }
 
  rc = load_settings(frontend_name, global_conf);
  if (rc != 0) {
    cm_msg(MERROR, "begin_of_run", "could not load equipment settings");
    return rc;
  }

  point_min = conf.get<double>("min");
  point_max = conf.get<double>("max");
  
  //setup channel and timing parameters
  //create DC channels
  DAQMXERRCHECK (DAQmxCreateAIVoltageChan(taskHandle,physicalChannelDC[],"",DAQmx_Val_RSE,minVolDC,maxVolDC,DAQmx_Val_Volts,NULL));
  cm_msg(MERROR,"begin_of_run","DAQ AI voltage channels created");
  //create AC channels
  //setup timing
  DAQMXERRCHECK (DAQmxCfgSampClkTiming(taskHandle,"",rate,DAQmx_Val_Rising,DAQmx_Val_ContSamps,sampsPerChanToAcquire));
  cm_msg(MERROR,"begin_of_run","DAQ timing parameters set");
  //start task
  DAQMXERRCHECK (DAQmxStartTask(taskHandle));
  cm_msg(MERROR,"begin_of_run","DAQ task began");

  return SUCCESS;
}

//--- End of Run ------------------------------------------------------------//
INT end_of_run(INT run_number, char *error)
{
  //stop task
  //DAQmxStopTask(taskHandle);
  cm_msg(MERROR,"end_of_run","DAQ task stopped");
  return SUCCESS;
}

//--- Pause Run -------------------------------------------------------------//
INT pause_run(INT run_number, char *error)
{
  return SUCCESS;
}

//--- Resume Run ------------------------------------------------------------//
INT resume_run(INT run_number, char *error)
{
  return SUCCESS;
}

//--- Frontend Loop ---------------------------------------------------------//

INT frontend_loop()
{
  // If frontend_call_loop is true, this routine gets called when
  // the frontend is idle or once between every event
  // IS THIS WHERE AVERAGING SHOULD GO?
  return SUCCESS;
}

//---------------------------------------------------------------------------//

/****************************************************************************\
  
  Readout routines for different events

\*****************************************************************************/

//--- Trigger event routines ------------------------------------------------//

INT poll_event(INT source, INT count, BOOL test) {

  static unsigned int i;

  // fake calibration
  if (test) {
    for (i = 0; i < count; i++) {
      usleep(10);
    }

    last_event = steadyclock_us();

    return 0;
  }

  next_event = steadyclock_us();

  // Check hardware for events, just random here.
  if (next_event - last_event > 100000) {
    return 1;
  } else {
    return 0;
  }
}

//--- Interrupt configuration ---------------------------------------*/

INT interrupt_configure(INT cmd, INT source, PTYPE adr)
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

//--- Event readout -------------------------------------------------*/

INT read_event(char *pevent, INT off)
{
  BYTE *pdata;
  g2::point_t point;

  // And MIDAS output.
  bk_init32(pevent);

  // Let MIDAS allocate the struct.
  bk_create(pevent, bank_name, TID_BYTE, &pdata);

  // Grab a timestamp.
  last_event = steadyclock_us();

  // Fill the struct.
  point.timestamp = last_event;
  point.x = (double)rand() / RAND_MAX * (point_max - point_min) + point_min;
  point.y = (double)rand() / RAND_MAX * (point_max - point_min) + point_min;
  point.z = (double)rand() / RAND_MAX * (point_max - point_min) + point_min;

  // Copy the struct
  memcpy(pdata, &point.timestamp, sizeof(point));
  pdata += sizeof(point);

  // Need to increment pointer and close.
  bk_close(pevent, pdata);

  return bk_size(pevent);
}