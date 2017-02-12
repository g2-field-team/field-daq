/********************************************************************\

Name:    yokogawa.cxx
Author :  David Flay (flay@umass.edu)  
Contents: Code to talk to the Yokogawa  

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

  INT poll_event(INT source, INT count, BOOL test);
  INT interrupt_configure(INT cmd, INT source, POINTER_T adr);
  
  // user-defined functions 
  INT read_yoko_event(char *pevent, INT off);


  /*-- Equipment list ------------------------------------------------*/


  EQUIPMENT equipment[] = {


    {"Yokogawa",                  /* equipment name */
      {1, 0,                      /* event ID, trigger mask */
	"SYSTEM",                 /* event buffer */
	EQ_PERIODIC,              /* equipment type; periodic readout */ 
	0,                        /* interrupt source */
	"MIDAS",                  /* format */
	TRUE,                     /* enabled */
	RO_RUNNING,               /* read when running and on odb */
	1E+3,                     /* period (read every 1000 ms) */
	0,                        /* stop run after this event limit */
	0,                        /* number of sub events */
	0,                        /* log history, logged once per minute */
	"", "", "",},
      read_yoko_event,            /* readout routine */
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
g2field::yokogawa_t YokogawaCurrent;            // current value of yokogawa data 
vector<g2field::yokogawa_t> YokogawaBuffer;     // vector of yokogawa data 
// my functions 
void read_from_device();                        // pull data from the Yokogawa  

const char * const yoko_bank_name = "YOKO";     // 4 letters, try to make sensible
const char * const SETTINGS_DIR   = "/Equipment/Yokogawa/Settings"
const char * const MONITOR_DIR    = "/Equipment/Yokogawa/Monitor"

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

  // Get IP addr
  const int SIZE = 200; 
  char *ip_addr_path = (char *)malloc( sizeof(char)*(SIZE+1) ); 
  sprintf(ip_addr_path,"%s/IP address",SETTINGS_DIR); 

  string ip_addr;
  char ip_str[SIZE];
  int ip_str_size = sizeof(ip_str);
  db_get_value(hDB,0,ip_addr_path,&ip_str,&ip_str_size,TID_STRING,0);
  ip_addr = string(ip_str);

  free(ip_addr_path); 

  // connect to the yokogawa
  int rc = yokogawa::open_connection(ip_addr);  

  if (rc==0) {
    cm_msg(MINFO,"init","Yokogawa is connected");
  } else {
    cm_msg(MERROR,"init","Yokogawa connection FAILED. Error code: %d",rc);
    return FE_ERR_HW; 
  }

  rc = yokogawa::set_mode(yokogawa::kCURRENT); 
  cm_msg(MINFO,"init","Yokogawa set to CURRENT mode.");
  rc = yokogawa::set_range_max(); 
  cm_msg(MINFO,"init","Yokogawa set to maximum range.");
  rc = yokogawa::set_level(0.0); 
  cm_msg(MINFO,"init","Yokogawa current set to 0 mA.");
  rc = yokogawa::set_output_state(yokogawa::kENABLED); 
  cm_msg(MINFO,"init","Yokogawa output ENABLED.");

  return SUCCESS;
}
//______________________________________________________________________________
INT frontend_exit(){
  // Disconnect from Yokogawa 
  // set back to zero amps and volts
  // disable output  

  int rc=0;

  // set to zero mA 
  rc = yokogawa::set_level(0.0); 
  // disable output 
  rc = yokogawa::set_output_state(yokogawa::kDISABLED); 
  // close connection  
  rc = yokogawa::close_connection();
  
  if (rc==0) {
    cm_msg(MINFO,"exit","Yokogawa disconnected successfully.");
  } else {
    cm_msg(MERROR,"exit","Yokogawa disconnection failed. Error code: %d",rc);
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

  const int SIZE = 200; 
  char *root_sw = (char *)malloc( sizeof(char)*(SIZE+1) ); 
  sprintf(root_sw,"%s/Root Output",SETTINGS_DIR); 

  //Get Root output switch
  int write_root_size = sizeof(write_root);
  db_get_value(hDB,0,root_sw,&write_root,&write_root_size,TID_BOOL, 0);

  free(root_sw); 

  char *root_outpath = (char *)malloc( sizeof(char)*(SIZE+1) ); 
  sprintf(root_outpath,"%s/Root dir",SETTINGS_DIR); 

  //Get Data dir
  string DataDir;
  char str[500];
  int str_size = sizeof(str);
  db_get_value(hDB,0,root_outpath,&str,&str_size,TID_STRING, 0);
  DataDir = string(str);

  free(root_outpath); 
 
  //Root File Name
  sprintf(str,"Root/Yokogawa_%05d.root",RunNumber);
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
  YokogawaBuffer.clear(); 
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

  int rc=0; 

  // set to zero mA 
  rc = yokogawa::set_level(0.0); 
  if (rc!=0) { 
     cm_msg(MERROR,"exit","Cannot set Yokogawa current to 0 mA!");
  }

  // disable output 
  rc = yokogawa::set_output_state(yokogawa::kDISABLED); 
  if (rc!=0) { 
     cm_msg(MERROR,"exit","Cannot disable Yokogawa output!");
  }

  cm_msg(MINFO,"exit","Yokogawa set to 0 mA.");
  cm_msg(MINFO,"exit","Yokogawa output DISABLED.");

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

  // bool check = true; 
 
  // if(check){
  //    return 1;
  // }else{ 
  //    return 0;
  // }

  return 0; 

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
   static unsigned int num_events = 0; 
   DWORD *pYokoData; 

   INT BufferLoad; 
   INT BufferLoad_size = sizeof(BufferLoad); 

   // ROOT output 
   if (write_root) {
      mlockdata.lock();
      YokoCurrent = YokoBuffer[0];
      mlockdata.unlock();
      pt_norm->Fill();
      num_events++;
      if (num_events % 10 == 0) {
	 pt_norm->AutoSave("SaveSelf,FlushBaskets");
	 pf->Flush();
	 num_events = 0;
      }
   }

   // write data to bank 
   // initialize the bank structure 
   bk_init32(pevent);

   // multi-thread lock 
   mlockdata.lock(); 

   // create the bank 
   bk_create(pevent,yoko_bank_name,TID_WORD,(void **)&pYokoData);
   // copy data into pYokoData 
   memcpy(pYokoData, &(YokoBuffer[0]), sizeof(g2field::yokogawa_t));
   // increment the pointer 
   pYokoData += sizeof(g2field::yokogawa_t)/sizeof(WORD);
   // close the event 
   bk_close(pevent,pYokoData);

   // some type of cleanup 
   YokoBuffer.erase( YokoBuffer.begin() ); 
   // check size of readout buffer 
   BufferLoad = YokoBuffer.size();

   // unlock the thread  
   mlockdata.unlock();  

   const int SIZE = 200; 
   char *buf_load_path = (char *)malloc( sizeof(char)*(SIZE+1) ); 
   sprintf(buf_load_path,"%s/Buffer Load",MONITOR_DIR); 

   //update buffer load in ODB
   db_set_value(hDB,0,buf_load_path,&BufferLoad,BufferLoad_size,1,TID_INT); 

   free(buf_load_path);  

   return bk_size(pevent); 
}
//______________________________________________________________________________
void read_from_device(){
   // read data from yokogawa 

   BOOL localRunActive = false;
   int i=0,is_enabled=-1,mode=-1;
   double lvl=0; 

   const int SIZE = 200; 
   char *read_path = (char *)malloc( sizeof(char)*(SIZE+1) ); 
   sprintf(read_path,"%s/Read Thread Active",MONITOR_DIR); 

   int ReadThreadActive = 1;
   mlock.lock();
   db_set_value(hDB,0,read_path,&ReadThreadActive,sizeof(ReadThreadActive),1,TID_BOOL);
   mlock.unlock();

   while (1) { 
      // create a data structure    
      g2field::yokogawa_t *yoko_data = new g2field::yokogawa_t; 
      //Check if the front-end is active
      mlock.lock();
      localRunActive = RunActive;
      mlock.unlock();
      if (!localRunActive) break;
      // grab the data 
      is_enabled = yokogawa::get_output_state(); 
      mode       = yokogawa::get_mode(); 
      lvl        = yokogawa::get_level(); 
      // fill the data structure  
      yoko_data->sys_clock  = 0;
      yoko_data->gps_clock  = 0;
      yoko_data->mode       = mode;  
      yoko_data->is_enabled = is_enabled;  
      if (mode==yokogawa::kVOLTAGE) {
	 yoko_data->current = 0.; 
	 yoko_data->voltage = lvl; 
      } else if (mode==yokogawa::kCURRENT) {
	 yoko_data->current = lvl; 
	 yoko_data->voltage = 0.; 
      }
      // fill buffer 
      mlockdata.lock(); 
      YokogawaBuffer.push_back(*yoko_data); 
      mlockdata.unlock(); 
      // clean up for next read 
      delete yoko_data;
      i++;
   }

   // update read thread flag 
   ReadThreadActive = 0;
   mlock.lock();
   db_set_value(hDB,0,read_path,&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL);
   free(read_path); 
   mlock.unlock();

}

