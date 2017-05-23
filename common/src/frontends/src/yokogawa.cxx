/********************************************************************\

Name:     yokogawa.cxx
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

#include "g2field/YokogawaInterface.hh"
#include "g2field/core/field_structs.hh"
#include "g2field/core/field_constants.hh"

#include "TTree.h"
#include "TFile.h"

#define FRONTEND_NAME "PS Feedback" // Prefer capitalize with spaces

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


    {FRONTEND_NAME,               /* equipment name */
      {EVENTID_YOKOGAWA, 0,       /* event ID, trigger mask */
	"SYSTEM",                 /* event buffer */
	EQ_PERIODIC,              /* equipment type; periodic readout */ 
	0,                        /* interrupt source */
	"MIDAS",                  /* format */
	TRUE,                     /* enabled */
	RO_RUNNING,               /* read when running and on odb */
	1000,                     /* period (read every 1000 ms) */
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
mutex mlock_data;
// for MIDAS
BOOL RunActive;
// for ROOT 
BOOL write_root = false;
TFile *pf;
TTree *pt_norm;
// my data structures and variables  
g2field::yokogawa_t YokoCurrent;            // current value of yokogawa data 
vector<g2field::yokogawa_t> YokoBuffer;     // vector of yokogawa data 
BOOL gSimMode = false;
// hardware limits 
double gLowerLimit = -200E-3; 
double gUpperLimit =  200E-3; 
// P, I, D coefficients 
double gP_coeff=0;
double gI_coeff=0;
double gD_coeff=0;
// P, I, D terms 
double gP_term=0;
double gI_term=0;
double gD_term=0;
// other terms we need to keep track of 
double gSetpoint=0;
double gLastErr=0;
double gIntErr=0;
double gWindupGuard=20.;  
double gSampleTime=10E-3; // 10 ms  
double gScaleFactor=1.0;  // in Amps/Hz 
// time variables 
unsigned long gCurrentTime=0;
unsigned long gLastTime=0; 

// my functions 
void read_from_device();                               // pull data from the Yokogawa 
void update_p_term(double err,double dt,double derr);  // update P term 
void update_i_term(double err,double dt,double derr);  // update I term 
void update_d_term(double err,double dt,double derr);  // update D term 

double get_new_current(double meas_value);             // get new current based on PID  

int update_current();                                  // update the current on the Yokogawa 
unsigned long get_utc_time();                          // UTC time in milliseconds 

const char * const yoko_bank_name = "PSFB";     // 4 letters, try to make sensible
const char * const SETTINGS_DIR   = "/Equipment/Yokogawa/Settings";
const char * const MONITORS_DIR   = "/Equipment/Yokogawa/Monitors";

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
   // set to current mode 
   // set to maximum current range (0,200 mA) 
   // set device to 0 amps and 0 volts 

   //Check if it is a simulation
   int size_Bool = sizeof(gSimMode);
   const int SIZE = 500; 
   char *sim_sw_path = (char *)malloc( sizeof(char)*(SIZE+1) ); 
   sprintf(sim_sw_path,"%s/Simulation Mode",SETTINGS_DIR); 
   db_get_value(hDB,0,sim_sw_path,&gSimMode,&size_Bool,TID_BOOL,0);
  
   // IP addr
   char *ip_addr_path = (char *)malloc( sizeof(char)*(SIZE+1) ); 
   sprintf(ip_addr_path,"%s/IP address",SETTINGS_DIR); 

   char ip_addr[256];

   int ip_addr_size = sizeof(ip_addr);

   int rc=0;
   if (!gSimMode) {
      // taking real data, grab the IP address  
      db_get_value(hDB,0,ip_addr_path,&ip_addr,&ip_addr_size,TID_STRING,0);
      // connect to the yokogawa
      rc = yokogawa_interface::open_connection(ip_addr);  
      if (rc==0) {
         cm_msg(MINFO,"init","Yokogawa is connected.");
         rc = yokogawa_interface::set_mode(yokogawa_interface::kCURRENT); 
         cm_msg(MINFO,"init","Yokogawa set to CURRENT mode.");
         rc = yokogawa_interface::set_range_max(); 
         cm_msg(MINFO,"init","Yokogawa set to maximum range.");
         rc = yokogawa_interface::set_level(0.000); 
         cm_msg(MINFO,"init","Yokogawa current set to 0 mA.");
         rc = yokogawa_interface::set_output_state(yokogawa_interface::kENABLED); 
         cm_msg(MINFO,"init","Yokogawa output ENABLED.");
      } else {
         cm_msg(MERROR,"init","Yokogawa connection FAILED. Error code: %d",rc);
         return FE_ERR_HW; 
      }
   } else { 
      cm_msg(MINFO,"init","Yokogawa is in SIMULATION MODE.");
   }

   free(sim_sw_path); 
   free(ip_addr_path); 
   cm_msg(MINFO,"init","Initialization complete."); 

   return SUCCESS;
}
//______________________________________________________________________________
INT frontend_exit(){
   // Disconnect from Yokogawa 
   // set back to zero amps and volts
   // disable output  

   int rc=0;
 
   if (!gSimMode) { 
      // set to zero mA 
      rc = yokogawa_interface::set_level(0.0); 
      // disable output 
      rc = yokogawa_interface::set_output_state(yokogawa_interface::kDISABLED); 
      // close connection  
      rc = yokogawa_interface::close_connection();
      if (rc==0) {
	 cm_msg(MINFO,"exit","Yokogawa disconnected successfully.");
      } else {
	 cm_msg(MERROR,"exit","Yokogawa disconnection failed. Error code: %d",rc);
      }
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
      pt_norm->Branch(yoko_bank_name, &YokoCurrent, g2field::yokogawa_str);
   }

   // clear data buffers 
   mlock.lock(); 
   YokoBuffer.clear(); 
   mlock.unlock();
   cm_msg(MINFO,"begin_of_run","Data buffer is emptied at the beginning of the run.");

   //Start reading thread
   RunActive = true;
   // read_thread = thread(read_from_device);

   return SUCCESS;
}
//______________________________________________________________________________
INT end_of_run(INT run_number, char *error){

   mlock.lock();
   RunActive = false;
   mlock.unlock();
   // cm_msg(MINFO,"end_of_run","Trying to join threads.");
   // read_thread.join();
   cm_msg(MINFO,"exit","All threads joined.");
   cm_msg(MINFO,"exit","Data buffer is emptied before exit.");

   if(write_root){
      pt_norm->Write();
      pf->Write();
      pf->Close();
   }

   int rc=0; 

 
   if (!gSimMode) { 
      // set to zero mA 
      rc = yokogawa_interface::set_level(0.0); 
      if (rc!=0) { 
	 cm_msg(MERROR,"exit","Cannot set Yokogawa current to 0 mA!");
      }
      cm_msg(MINFO,"exit","Yokogawa set to 0 mA.");
      // disable output 
      rc = yokogawa_interface::set_output_state(yokogawa_interface::kDISABLED); 
      if (rc!=0) { 
	 cm_msg(MERROR,"exit","Cannot disable Yokogawa output!");
      }
      cm_msg(MINFO,"exit","Yokogawa output DISABLED.");

	   ;
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

//   cm_msg(MINFO,"read_yoko_event","Trying to read an event...");
//   cm_msg(MINFO,"read_yoko_event","Updating the current...");
   // first update the current based on the ODB value for the average field  
   int rc = update_current();
   if (rc!=0) { 
      cm_msg(MERROR,"read_yoko_event","Cannot update the current!");
   }

   //cm_msg(MINFO,"read_yoko_event","Reading data from device...");
   // read the current  
   read_from_device(); 
   //cm_msg(MINFO,"read_yoko_event","Done.");

   // now write everything to MIDAS banks 
   //cm_msg(MINFO,"read_yoko_event","Writing to MIDAS bank");

   static unsigned int num_events = 0; 
   DWORD *pYokoData; 

   INT BufferLoad; 
   INT BufferLoad_size = sizeof(BufferLoad); 

   // ROOT output 
   if (write_root) {
      mlock_data.lock();
      YokoCurrent = YokoBuffer[0];
      mlock_data.unlock();
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
   mlock_data.lock(); 

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
   mlock_data.unlock();  

   const int SIZE = 200; 
   char *buf_load_path = (char *)malloc( sizeof(char)*(SIZE+1) ); 
   sprintf(buf_load_path,"%s/Buffer Load",MONITORS_DIR); 

   //update buffer load in ODB
   db_set_value(hDB,0,buf_load_path,&BufferLoad,BufferLoad_size,1,TID_INT); 
   free(buf_load_path);  

   return bk_size(pevent); 
}
//______________________________________________________________________________
void read_from_device(){
   // read data from yokogawa 

   int i=0,is_enabled=-1,mode=-1;
   double lvl=0; 

   const int SIZE = 200; 
   char *read_path = (char *)malloc( sizeof(char)*(SIZE+1) ); 
   sprintf(read_path,"%s/Read Thread Active",MONITORS_DIR); 

   int ReadThreadActive = 1;
   mlock.lock();
   db_set_value(hDB,0,read_path,&ReadThreadActive,sizeof(ReadThreadActive),1,TID_BOOL);
   mlock.unlock();

   // create a data structure    
   g2field::yokogawa_t *yoko_data = new g2field::yokogawa_t; 

   // grab the data 
   if (!gSimMode) { 
      // real data 
      is_enabled = yokogawa_interface::get_output_state(); 
      mode       = yokogawa_interface::get_mode(); 
      lvl        = yokogawa_interface::get_level(); 
      // fill the data structure  
      yoko_data->sys_clock  = gCurrentTime;  // not sure of the difference here... 
      yoko_data->gps_clock  = gCurrentTime;  // not sure of the difference here... 
      yoko_data->mode       = mode;  
      yoko_data->is_enabled = is_enabled;  
      if (mode==yokogawa_interface::kVOLTAGE) {
	 yoko_data->current = 0.; 
	 yoko_data->voltage = lvl; 
      } else if (mode==yokogawa_interface::kCURRENT) {
	 yoko_data->current = lvl; 
	 yoko_data->voltage = 0.; 
      }
   } else { 
      // this is a simulation, fill with random numbers
      yoko_data->sys_clock  = gCurrentTime;
      yoko_data->gps_clock  = gCurrentTime;
      yoko_data->current    = (double)(rand() % 100);   // random number between 0 and 100 
      yoko_data->voltage    = 0.; 
      yoko_data->mode       = -1;  
      yoko_data->is_enabled = 0;  
   } 
   // fill buffer 
   mlock_data.lock(); 
   YokoBuffer.push_back(*yoko_data); 
   mlock_data.unlock(); 

   // Update ODB
   char current_read_path[512];
   sprintf(current_read_path,"%s/Current Value (mA)",MONITORS_DIR);
   double current_val = lvl/1E-3;
   db_set_value(hDB,0,current_read_path,&current_val,sizeof(current_val),1,TID_DOUBLE);

   // clean up for next read 
   delete yoko_data;

   // update read thread flag 
   ReadThreadActive = 0;
   mlock.lock();
   db_set_value(hDB,0,read_path,&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL);
   free(read_path); 
   mlock.unlock();

}
//______________________________________________________________________________
int update_current(){
   // update the current on the yokogawa based on the ODB
   int rc=0;

   double avg_field = 0;
   int SIZE_DOUBLE  = sizeof(avg_field);  

   const int SIZE = 100; 
   char *freq_path = (char *)malloc( sizeof(char)*(SIZE+1) ); 
   sprintf(freq_path,"%s/Average Field",MONITORS_DIR);

   db_get_value(hDB,0,freq_path,&avg_field,&SIZE_DOUBLE,TID_DOUBLE, 0);
   free(freq_path);
 
   char current_set_path[512];
   sprintf(current_set_path,"%s/Current Setpoint (mA)",SETTINGS_DIR);
   double current_set;
   db_get_value(hDB,0,current_set_path,&current_set,&SIZE_DOUBLE,TID_DOUBLE, 0);
   current_set *= 1E-3; // convert to amps! 

   char switch_path[512];
   sprintf(switch_path,"%s/Feedback Active",SETTINGS_DIR);
   BOOL IsFeedbackOn = FALSE;
   int SIZE_BOOL = sizeof(IsFeedbackOn);
   db_get_value(hDB,0,switch_path,&IsFeedbackOn,&SIZE_BOOL,TID_BOOL, 0);

   char pc_path[512];
   sprintf(pc_path,"%s/P Coefficient",SETTINGS_DIR);
   double P_coeff = 0;
   db_get_value(hDB,0,pc_path,&P_coeff,&SIZE_DOUBLE,TID_DOUBLE, 0);

   char ic_path[512];
   sprintf(ic_path,"%s/I Coefficient",SETTINGS_DIR);
   double I_coeff = 0;
   db_get_value(hDB,0,ic_path,&I_coeff,&SIZE_DOUBLE,TID_DOUBLE, 0);

   char dc_path[512];
   sprintf(dc_path,"%s/D Coefficient",SETTINGS_DIR);
   double D_coeff = 0;
   db_get_value(hDB,0,dc_path,&D_coeff,&SIZE_DOUBLE,TID_DOUBLE, 0);

   char sf_path[512];
   sprintf(sf_path,"%s/Scale Factor (Amps/Hz)",SETTINGS_DIR);
   double sf = 0;
   db_get_value(hDB,0,sf_path,&sf,&SIZE_DOUBLE,TID_DOUBLE, 0);

   // FIXME: 1. get the right time
   //        2. pass the correct current into the PID loop (based on fixed probe data)     
   double lvl   = 0;
   gCurrentTime = get_utc_time();

   gP_coeff     = P_coeff; 
   gI_coeff     = I_coeff; 
   gD_coeff     = D_coeff; 
   gSetpoint    = current_set;
   gScaleFactor = sf; 

   if (IsFeedbackOn) {
     lvl = yokogawa_interface::get_level(); 
     lvl = get_new_current(lvl);   
   } else {
     lvl = gSetpoint;
   }

   if (!gSimMode) { 
      // operational mode, check the level first  
      if (lvl>gLowerLimit && lvl<gUpperLimit) {  
        // the value looks good, do nothing 
      } else {
        if (lvl<0) lvl = 0.8*gLowerLimit; 
        if (lvl>0) lvl = 0.8*gUpperLimit; 
      } 
      // checks are finished, set the current 
      rc = yokogawa_interface::set_level(lvl);
   } else {
      // simulation, do nothing  
   }
   return rc;  
}
//_____________________________________________________________________________
double get_new_current(double meas_value){ 
   double err  = gSetpoint - meas_value;
   double dt   = (double)(gCurrentTime - gLastTime);
   double derr = err - gLastErr;
   update_p_term(err,dt,derr);
   update_i_term(err,dt,derr);
   update_d_term(err,dt,derr);
   double output = gScaleFactor*( gP_term + gI_coeff*gI_term + gD_coeff*gD_term );
   // remember values for next calculation 
   gLastTime     = gCurrentTime;
   gLastErr      = err;
   return output;
}
//______________________________________________________________________________
void update_p_term(double err,double dtime,double derror){
   if (dtime >= gSampleTime) {
      gP_term = gP_coeff*err;
   }
}
//______________________________________________________________________________
void update_i_term(double err,double dtime,double derror){
   if (dtime >= gSampleTime) {
      gI_term += err*dtime;
      if (gI_term< (-1.)*gWindupGuard) {
         gI_term = (-1.)*gWindupGuard;
      } else if (gI_term>gWindupGuard) {
         gI_term = gWindupGuard;
      }
   }
}
//______________________________________________________________________________
void update_d_term(double err,double dtime,double derror){
   gD_term = 0.;
   if (dtime >= gSampleTime) {
      if (dtime>0) gD_term = derror/dtime;
   }
}
//______________________________________________________________________________
unsigned long get_utc_time(){
   struct timeb now; 
   int rc = ftime(&now); 
   unsigned long utc = now.time + now.millitm;
   return utc; 
} 

