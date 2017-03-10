/*****************************************************************************\

Name:   monitor.cxx
Author: Matthias W. Smith
Email:  mwsmith2@uw.edu

About:  A frontend that triggers alarms and ships all experimental flags
        as a single MIDAS event for processing later down the pipeline.
        
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

//--- project includes ------------------------------------------------------//
#include "frontend_utils.hh"

//--- globals ---------------------------------------------------------------//

// Define front-end and bank names
#define FRONTEND_NAME "Monitor" // Prefer capitalize with spaces
const char * const bank_name = "FLAG"; // 4 letters, try to make sensible

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
  INT read_expt_flags(char *pevent, INT off);
  INT poll_event(INT source, INT count, BOOL test);
  INT interrupt_configure(INT cmd, INT source, PTYPE adr);

  // Equipment list @EXAMPLE BEGIN

  EQUIPMENT equipment[] = 
    {
      {FRONTEND_NAME,   // equipment name 
       { 1, 0,          // event ID, trigger mask 
         "SYSTEM",      // event buffer (used to be SYSTEM)
         EQ_PERIODIC,   // equipment type 
         0,             // not used 
         "MIDAS",       // format 
         TRUE,          // enabled 
         RO_RUNNING,    // read only when running 
         5,             // poll for 5 ms 
         0,             // stop run after this event limit 
         0,             // number of sub events 
         0,             // don't log history 
         "", "", "",
       },
       read_expt_flags,      // readout routine 
       NULL,
       NULL,
       NULL, 
      },

      {""}
    };

} //extern C

// Put necessary globals in an anonomous namespace here.
namespace {
boost::property_tree::ptree conf;
boost::property_tree::ptree global_conf;
} 

//--- Frontend Init ---------------------------------------------------------//
INT frontend_init() 
{
  return SUCCESS;
}

//--- Frontend Exit ---------------------------------------------------------//
INT frontend_exit()
{
  return SUCCESS;
}

//--- Begin of Run ----------------------------------------------------------//
INT begin_of_run(INT run_number, char *error)
{
  return SUCCESS;
}

//--- End of Run ------------------------------------------------------------//
INT end_of_run(INT run_number, char *error)
{
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

    return 0;
  }

  return 0;
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

INT read_expt_flags(char *pevent, INT off)
{
  // Allocate a pointer to attached to MIDAS's internal buffer.
  BYTE *pdata;
  boost::property_tree::ptree pt;

  // And initialize MIDAS output.
  bk_init32(pevent);

  // Let MIDAS allocate the struct.
  bk_create(pevent, bank_name, TID_BYTE, (void **)&pdata);

  // Load all flags to monitor (probably sequester out later).
  HNDLE hDB, hkey;
  char str[256], logdir[256];
  int size = 0;
  int bytes_written = 0;
  int rc = 0;

  char *json_buf = new char[0x8000];
  std::stringstream ss;

  // Get the experiment database handle.
  cm_get_experiment_database(&hDB, NULL);

  snprintf(str, 256, "/Shared/Monitor");
  db_find_key(hDB, 0, str, &hkey);

  if (hkey) {
    db_copy_json_values(hDB, hkey, &json_buf, &size, &bytes_written, 1, 0, 1);

  } else {

    cm_msg(MERROR, frontend_name, "No flag monitor subtree in ODB");
    return FE_ERR_ODB;
  }

  // Store it in a property tree.
  ss << json_buf;
  boost::property_tree::read_json(ss, pt);
  ss.str("");

  // Map elements to a new ptree.
  boost::property_tree::ptree flags;

  // Take care of general case first.
  for (auto &flag: pt) {
    boost::property_tree::ptree temp;

    temp.put("flag", flag.second.get<bool>("flag"));
    temp.put("timestamp", flag.second.get<int>("flag/last_written"));
    temp.put("alarm_level", flag.second.get<int>("alarm_level"));
    temp.put("alarm_msg", flag.second.get<std::string>("alarm_msg"));

    flags.add_child(flag.first, temp);
  }

  // Write a string and copy.
  boost::property_tree::write_json(ss, flags);
  
  memcpy(pdata, ss.str().c_str(), ss.str().size());
  pdata += ss.str().size();

  // Need to increment pointer and close.
  bk_close(pevent, pdata);

  return bk_size(pevent);
}



