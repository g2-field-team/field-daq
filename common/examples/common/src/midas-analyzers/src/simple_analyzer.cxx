/**************************************************************************** \

  file:    simple_analyzer.cxx
  author:  Matthias W. Smith
  email:   mwsmith2@uw.edu

  about:   A compliment to the Simple Frontend.

\*****************************************************************************/

//--- std includes ----------------------------------------------------------//
#include <string>
#include <ctime>
#include <cstdio>
#include <sstream>
#include <iostream>
#include <iomanip>
using std::string;
using std::cout;
using std::endl;

//--- other includes --------------------------------------------------------//
#include "midas.h"
#include "TTree.h"
#include "TFile.h"

//--- project includes ------------------------------------------------------//
#include "field_structs.hh"

//--- global variables ------------------------------------------------------//

// The analyzer name (client name) as seen by other MIDAS clients
char *analyzer_name = (char *)"Simple Analyzer";

// analyzer_loop is called with this interval in ms (0 to disable)  
INT analyzer_loop_period = 0;

// default ODB size 
INT odb_size = DEFAULT_ODB_SIZE;

//--- module declaration ----------------------------------------------------//

BANK_LIST analyzer_bank_list[] = {
  {"SMFE", TID_BYTE, sizeof(g2::point_t), NULL},
  {""}
};

//-- event request list -----------------------------------------------------//
INT ana_trigger_event(EVENT_HEADER *pheader, void *pevent);
INT ana_begin_of_run(INT run_number, char *error);
INT ana_end_of_run(INT run_number, char *error);

ANALYZE_REQUEST analyze_request[] = {
  {"Simple Analyzer",     // equipment name 
    {10,                     // event ID 
     TRIGGER_ALL,           // trigger mask 
     GET_NONBLOCKING,       // get events without blocking producer 
     "SYSTEM",                // event buffer 
     TRUE,                  // enabled 
     "", "",}
    ,
    ana_trigger_event,      // analyzer routine 
    NULL,                   // module list 
    analyzer_bank_list,     // bank list  
   },
   {""}
};



//--- init routine ----------------------------------------------------------//

INT analyzer_init()
{
  return SUCCESS;
}


//--- exit routine ----------------------------------------------------------//

INT analyzer_exit(void)
{
   return SUCCESS;
}

//--- BOR routine -----------------------------------------------------------//

INT ana_begin_of_run(INT run_number, char *error)
{
   return SUCCESS;
}

//--- EOR routine -----------------------------------------------------------//

INT ana_end_of_run(INT run_number, char *error)
{
   return SUCCESS;
}

//--- pause run -------------------------------------------------------------//

INT ana_pause_run(INT run_number, char *error)
{
   return CM_SUCCESS;
}
 
//--- resume run ------------------------------------------------------------//

INT ana_resume_run(INT run_number, char *error)
{
   return CM_SUCCESS;
}

//--- analyzer loop ---------------------------------------------------------//

INT analyzer_loop()
{
   return CM_SUCCESS;
}

//--- event routine ---------------------------------------------------------//

INT ana_trigger_event(EVENT_HEADER *pheader, void *pevent)
{
   INT end, start, n;
   BYTE *pdata;
   static int count = 0;
   static g2::point_t point;

   // Look for shim platform data bank
   n = bk_locate(pevent, "SMFE", &pdata);

   memcpy(&point.timestamp, pdata, sizeof(point));

   // Return if we didn't find anything.
   if (n == 0) {

     cout << "Couldn't find SMFE bank." << endl;
     cout << "pevent: " << std::hex << pevent << std::dec << endl;
     return 1;

   } else {
     
     cout << "Found SMFE bank." << endl;
     printf("Event %05i: %llus, <%.3f, %.3f, %.3f>\n", 
	    count++, 
	    point.timestamp,
	    point.x,
	    point.y,
	    point.z);
   }

   return SUCCESS;
}
