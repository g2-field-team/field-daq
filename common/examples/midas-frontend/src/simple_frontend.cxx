/*****************************************************************************\

Name:   simple_frontend.cxx
Author: Matthias W. Smith
Email:  mwsmith2@uw.edu

About:  A simple example frontend that generates a random point.
        
\*****************************************************************************/


//--- std includes ----------------------------------------------------------//
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <random>
using std::string;

//--- other includes --------------------------------------------------------//
#include "midas.h"

//--- project includes ------------------------------------------------------//
#include "frontend_utils.hh"
#include "field_structs.hh"

//--- globals ---------------------------------------------------------------//

// @EXAMPLE BEGIN
#define FRONTEND_NAME "Simple Frontend" // Prefer capitalize with spaces
const char * const bank_name = "SMFE"; // 4 letters, try to make sensible

// Any structs need to be defined.
BANK_LIST trigger_bank_list[] = {
  {bank_name, TID_STRUCT, sizeof(g2::point_t), g2::point_str_midas}
  ,
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
  INT read_trigger_event(char *pevent, INT off);
  INT poll_event(INT source, INT count, BOOL test);
  INT interrupt_configure(INT cmd, INT source, PTYPE adr);

  // Equipment list @EXAMPLE BEGIN

  EQUIPMENT equipment[] = 
    {
      {FRONTEND_NAME,   // equipment name 
       { 10, 0,         // event ID, trigger mask 
         "SYSTEM",      // event buffer (used to be SYSTEM)
         EQ_POLLED,     // equipment type 
         0,             // not used 
         "MIDAS",       // format 
         TRUE,          // enabled 
         RO_RUNNING,    // read only when running 
         100,           // poll for 10ms 
         0,             // stop run after this event limit 
         0,             // number of sub events 
         1,             // don't log history 
         "", "", "",
       },
       read_trigger_event,      // readout routine 
      },
      
      {""}
    };

} //extern C

// Put necessary globals in an anonomous namespace here.
namespace {
boost::property_tree::ptree conf;
boost::property_tree::ptree global_conf;
double length_unit = 1.0; // meters
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
/*
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

  length_unit = conf.get<double>("lenght_unit");
*/
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

  // Check hardware for events, just random here.
  if (rand() > RAND_MAX / 2.0) {
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

INT read_trigger_event(char *pevent, INT off)
{
  g2::point_t *point;

  // And MIDAS output.
  bk_init32(pevent);

  // Let MIDAS allocate the struct.
  bk_create(pevent, bank_name, TID_STRUCT, &point);

  // Fill the struct.
  point->timestamp = clock() * 1.0 / CLOCKS_PER_SEC;
  point->x = 1.0 * rand() / RAND_MAX;
  point->y = 1.0 * rand() / RAND_MAX;
  point->z = 1.0 * rand() / RAND_MAX;

  // Need to increment pointer and close.
  bk_close(pevent, ++point); 

  return bk_size(pevent);
}
