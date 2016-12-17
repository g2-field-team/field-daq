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

#include "YokogawaInterface.hh"
#include "field_structs.hh"
#include "field_constants.hh"

#include "TTree.h"
#include "TFile.h"

#define FRONTEND_NAME "Yokogawa" // Prefer capitalize with spaces

using namespace std;

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

  INT read_yoko_event(char *pevent, INT off);

  INT poll_event(INT source, INT count, BOOL test);
  INT interrupt_configure(INT cmd, INT source, POINTER_T adr);


  /*-- Equipment list ------------------------------------------------*/


  EQUIPMENT equipment[] = {


    {"YokogawaSim",               /* equipment name */
      {1, 0,                      /* event ID, trigger mask */
	"SYSTEM",                 /* event buffer */
	EQ_POLLED,                /* equipment type */
	0,                        /* event source */
	"MIDAS",                  /* format */
	TRUE,                     /* enabled */
	RO_RUNNING,               /* read when running and on odb */
	100,                      /* poll every 0.1 sec */
	0,                        /* stop run after this event limit */
	0,                        /* number of sub events */
	0,                        /* log history, logged once per minute */
	"", "", "",},
      read_trly_event,            /* readout routine */
    },

    {""}
  };


#ifdef __cplusplus
}
#endif

// a hook into the ODB 
HNDLE hDB;
// multi-thread data types 
thread read_thread;
mutex mlock;
mutex mlockdata;
// for MIDAS
BOOL RunActive;
// for ROOT 
BOOL write_root = false;
TFile *pf;
TTree *pt_norm;
// my data structures 
gm2field::yokogawa_t YokogawaCurrent;            // current value of yokogawa data 
vector<gm2field::yokogawa_t> YokogawaBuffer;     // vector of yokogawa data 
// my functions 
void read_from_device();                         // pull data from the Yokogawa  

const char * const yoko_bank_name = "YOKO"; // 4 letters, try to make sensible

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

//______________________________________________________________________________
INT frontend_init(){ 
  // initialize Yokogawa hardware 
  // set device to 0 amps and 0 volts 
  // set to current mode 
  // set to maximum current range (0,200 mA) 

  int err=0;  
  if (err==0){
    //cout << "connection successful\n";
    cm_msg(MINFO,"init","Yokogawa is initialized");
  }
  else {
    //   cout << "connection failed \n";
    cm_msg(MERROR,"init","Yokogawa initialization failed. Error code: %d",err);
  }

  //For simulation open the input file at begin_of_run
  return SUCCESS;
}
//______________________________________________________________________________
INT frontend_exit(){
  // Disconnect from Yokogawa 
  // set back to zero amps and volts
  // disable output  
   
  int err=0;  
  if (err==0){
    //cout << "connection successful\n";
    cm_msg(MINFO,"exit","Yokogawa is disconnected");
  }
  else {
    //   cout << "connection failed \n";
    cm_msg(MERROR,"exit","Yokogawa disconnection failed. Error code: %d",err);
  }
  return SUCCESS;
}
//______________________________________________________________________________
INT begin_of_run(INT run_number, char *error){
  // set up for the run 

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
  sprintf(str,"Root/YokogawaSim_%05d.root",RunNumber);
  string RootFileName = DataDir + string(str);

  if(write_root){
    cm_msg(MINFO,"begin_of_run","Writing to root file %s",RootFileName.c_str());
    pf      = new TFile(RootFileName.c_str(), "recreate");
    pt_norm = new TTree("t_yoko", "Yokogawa Data");
    pt_norm->SetAutoSave(5);
    pt_norm->SetAutoFlush(20);

    string yoko_br_name("YOKO");
    pt_norm->Branch(yoko_bank_name, &YokogawaCurrent, g2field::yokogawa_str);
  }

  // clear data buffers 
  mlock.lock(); 
  YokogawaBuffer.clear()
  mlock.unlock();
  cm_msg(MINFO,"begin_of_run","Data buffer is emptied at the beginning of the run.");
  
  //Start reading thread
  RunActive = true;
  read_thread = thread(read_from_device);
  
  return SUCCESS;
}
//______________________________________________________________________________
INT end_of_run(INT run_number, char *error){

  mlock.lock();
  RunActive = false;
  mlock.unlock();
  // cm_msg(MINFO,"end_of_run","Trying to join threads.");
  read_thread.join();
  cm_msg(MINFO,"exit","All threads joined.");

  cm_msg(MINFO,"exit","Data buffer is emptied before exit.");

  if(write_root){
    pt_norm->Write();
    pf->Write();
    pf->Close();
  }

  return SUCCESS;
}
//______________________________________________________________________________
INT pause_run(INT run_number, char *error){
  return SUCCESS;
}
//______________________________________________________________________________
INT resume_run(INT run_number, char *error){ 
  return SUCCESS;
}
//______________________________________________________________________________
INT frontend_loop(){
  // if frontend_call_loop is true, this routine gets called when
  // the frontend is idle or once between every event 
  return SUCCESS;
}
//______________________________________________________________________________
INT poll_event(INT source, INT count, BOOL test){

   // Polling routine for events. Returns TRUE if event
   // is available. If test equals TRUE, don't return. The test
   // flag is used to time the polling 

  static unsigned int i;
  if (test) {
    for (i = 0; i < count; i++) {
      usleep(10);
    }
    return 0;
  }

  bool check = true; 
 
  if(check){
     return 1;
  }else{ 
     return 0;
  }
}
//______________________________________________________________________________
INT interrupt_configure(INT cmd, INT source, POINTER_T adr){
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
//______________________________________________________________________________
INT read_yoko_event(char *pevent,INT off){
   return SUCCESS; 
}
//______________________________________________________________________________
void read_from_device(){
   // does nothing yet 
}

