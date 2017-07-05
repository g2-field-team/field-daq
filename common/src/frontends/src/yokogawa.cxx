/********************************************************************\

Name:     yokogawa.cxx
Author :  David Flay (flay@umass.edu)  
Contents: Code to talk to the Yokogawa  

$Id$

\********************************************************************/

#include <stdio.h>
#include <stdlib.h>
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

#include "midas.h"
#include "mcstd.h"

#include "g2field/common.hh"
#include "g2field/YokogawaInterface.hh"
#include "g2field/core/field_structs.hh"
#include "g2field/core/field_constants.hh"

#include "PID.hh"

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
	1000,                     /* period (read every 4000 ms) */
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
g2field::psfeedback_t PSFBCurrent;            // current value of yokogawa data 
g2field::PID *pidLoop; 
vector<g2field::psfeedback_t> PSFBBuffer;     // vector of yokogawa data 
BOOL gSimMode = false;
// test data
BOOL gWriteTestData = false; 
// hardware limits 
double gLowerLimit = -200E-3; // in Amps  
double gUpperLimit =  200E-3; // in Amps  
// field change limit  
double gFieldLimit = 0;        // in Hz; if the change in the field is above this value, we ignore updating the current    
double gSmallFieldLimit = 1.5; // in Hz; apply a small correction of 1.5*ScaleFactor if bigger than this 
// PID Terms 
int    gCounter=0;
double gWindupGuard=20.;  
double gSampleTime=10E-3; // 10 ms  
// other terms we need to keep track of 
unsigned long int gEventCounter=0; 
int gProbeNum = -1;
BOOL gUseSingleProbe = false; 
BOOL gIsFeedbackOn = false; 
double gTotalCurrent=0;
double gLastCurrent=0;
double gLastAvgField=-100;  
// time variables 
unsigned long gCurrentTime=0;
unsigned long gLastTime=0; 
double gFieldUpdateTime=0; 

// my functions 
void read_from_device();                                   // pull data from the Yokogawa 

int update_current(BOOL IsFieldUpdated,double current_setpoint,double avg_field);  // update current on the yokogawa 
int update_parameters_from_ODB(double &current_setpoint,double &avg_field);       // update pars from ODB  
int check_average_field_ODB(double &avg_field);            // update average field from ODB 
int check_yokogawa_comms(int rc,const char *func);         // check on the yokogawa communication; run error check if necessary 
int write_to_file(const char *tag,unsigned long time,double x,double y,double z);                    // print test data to file 
int write_to_file(const char *tag,unsigned long time,double x);   // another print function           

unsigned long get_utc_time();                              // UTC time in milliseconds 

const char * const psfb_bank_name = "PSFB";     // 4 letters, try to make sensible
const char * const SETTINGS_DIR   = "/Equipment/PS Feedback/Settings";
const char * const MONITORS_DIR   = "/Equipment/PS Feedback/Monitors";
const char * const SHARED_DIR     = "/Shared/Variables/PS Feedback";
const char * const TEST_DIR       = "/home/newg2/Workspace/dflay_root_ana/input/"; 

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

   int rc=0,err_code;
   double lvl=0;
   char yoko_read_msg[512],err_msg[512]; 

   // set current accumulator to zero 
   gTotalCurrent = 0;  
   gLastCurrent  = 0;
   // reset bad correction counter to zero 
   gEventCounter = 0;
   gCounter      = 0;
   // reset last avg field 
   gLastAvgField = -100; 
   gFieldLimit   = 0;
   // reset feedback boolean  
   gIsFeedbackOn = false;  

   pidLoop = new g2field::PID(); 
   pidLoop->SetPID(0,0,0); 
   pidLoop->SetIAltCoeff(0); 
   pidLoop->SetSampleTime(gSampleTime);  
   pidLoop->SetWindupGuard(gWindupGuard);  

   if (!gSimMode) {
      // taking real data, grab the IP address  
      db_get_value(hDB,0,ip_addr_path,&ip_addr,&ip_addr_size,TID_STRING,0);
      // connect to the yokogawa
      rc = yokogawa_interface::open_connection(ip_addr);  
      if (rc==0) {
         cm_msg(MINFO,"init","Yokogawa is connected.");
         rc = yokogawa_interface::set_mode(yokogawa_interface::kCURRENT); 
         rc = check_yokogawa_comms(rc,"init");  
         if (rc==0) {
	    cm_msg(MINFO,"init","Yokogawa set to CURRENT mode.");
         } else { 
            return FE_ERR_HW; 
         }
         rc = yokogawa_interface::set_range_max(); 
         rc = check_yokogawa_comms(rc,"init");  
         if (rc==0) { 
            cm_msg(MINFO,"init","Yokogawa set to maximum range.");
         } else { 
            return FE_ERR_HW; 
         }
         rc = yokogawa_interface::set_level(0.000); 
         rc = check_yokogawa_comms(rc,"init"); 
         if (rc==0) { 
	    // sanity check to make sure we did what we thought we did 
	    lvl = yokogawa_interface::get_level(); 
	    sprintf(yoko_read_msg,"Yokogawa set to %.3lf mA",lvl/1E-3);  
	    cm_msg(MINFO,"init",yoko_read_msg);
         } else { 
            return FE_ERR_HW; 
         }
         rc = yokogawa_interface::set_output_state(yokogawa_interface::kENABLED); 
         rc = check_yokogawa_comms(rc,"init"); 
         if (rc==0) {
            cm_msg(MINFO,"init","Yokogawa output ENABLED.");
         } else { 
            return FE_ERR_HW; 
         }
      } else {
         cm_msg(MERROR,"init","Cannot connect to the Yokogawa.");
         err_code = check_yokogawa_comms(rc,"init");
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
      rc = check_yokogawa_comms(rc,"exit"); 
      if (rc!=0) { 
          return FE_ERR_HW; 
      } 
      // disable output 
      rc = yokogawa_interface::set_output_state(yokogawa_interface::kDISABLED); 
      rc = check_yokogawa_comms(rc,"exit"); 
      if (rc!=0) { 
          return FE_ERR_HW; 
      } 
      // close connection  
      rc = yokogawa_interface::close_connection();
      if (rc==0) {
	 cm_msg(MINFO,"exit","Yokogawa disconnected successfully.");
      } else {
	 cm_msg(MERROR,"exit","Yokogawa disconnection failed.");
         rc = check_yokogawa_comms(rc,"exit"); 
         if (rc!=0) { 
             return FE_ERR_HW; 
         }
      }
   }

   delete pidLoop; 

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
   const int SIZE = 200; 
   char *root_sw = (char *)malloc( sizeof(char)*(SIZE+1) ); 
   sprintf(root_sw,"%s/Root Output",SETTINGS_DIR); 
   int write_root_size = sizeof(write_root);
   db_get_value(hDB,0,root_sw,&write_root,&write_root_size,TID_BOOL, 0);
   free(root_sw); 

   char *root_outpath = (char *)malloc( sizeof(char)*(SIZE+1) ); 
   sprintf(root_outpath,"%s/Root Dir",SETTINGS_DIR); 
   //Get Data dir
   string DataDir;
   char str[500];
   int str_size = sizeof(str);
   db_get_value(hDB,0,root_outpath,&str,&str_size,TID_STRING, 0);
   DataDir = string(str);
   free(root_outpath); 

   //Root File Name
   sprintf(str,"ps-feedback_%05d.root",RunNumber);
   string RootFileName = DataDir + string(str);

   if(write_root){
      cm_msg(MINFO,"begin_of_run","Writing to root file %s",RootFileName.c_str());
      pf      = new TFile(RootFileName.c_str(), "recreate");
      pt_norm = new TTree("t_psfb", "PSFeedbackData");
      pt_norm->SetAutoSave(5);
      pt_norm->SetAutoFlush(20);

      // string psfb_br_name("PSFB");
      pt_norm->Branch(psfb_bank_name, &PSFBCurrent, g2field::psfb_str);
   }

   // clear data buffers 
   mlock.lock(); 
   PSFBBuffer.clear(); 
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
   double lvl=0; 
 
   char yoko_read_msg[512];  

   if (!gSimMode) { 
      // set to zero mA 
      // rc = yokogawa_interface::set_level(0.0); 
      // if (rc!=0) { 
      //    cm_msg(MERROR,"exit","Cannot set Yokogawa current to 0 mA!");
      //    rc = check_yokogawa_comms(rc,"exit"); 
      //    return FE_ERR_HW; 
      // }
      // sanity check to make sure we did what we thought we did 
      // message the end_of_run current.
      lvl = yokogawa_interface::get_level(); 
      sprintf(yoko_read_msg,"End of run.  Yokogawa is set to %.3lf mA",lvl/1E-3);  
      cm_msg(MINFO,"end_of_run",yoko_read_msg);
      // disable output 
      // rc = yokogawa_interface::set_output_state(yokogawa_interface::kDISABLED); 
      // if (rc!=0) { 
      //    cm_msg(MERROR,"exit","Cannot disable Yokogawa output!"); 
      //    rc = check_yokogawa_comms(rc,"exit"); 
      //    return FE_ERR_HW; 
      // }
      // cm_msg(MINFO,"exit","Yokogawa output DISABLED.");
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

   int rc=1; 

   // update parameters from ODB 
   double current_setpoint=0,avg_field=0; 

   BOOL IsFieldUpdated = false;

   rc = update_parameters_from_ODB(current_setpoint,avg_field);
   if (rc>1) { 
      cm_msg(MERROR,"read_yoko_event","Cannot read parameters from ODB!");
      gIsFeedbackOn = false; 
   }

   // check to see if the field was updated 
   if (rc==1) IsFieldUpdated = true; 

   // char msg[512]; 
   // sprintf(msg,"The field is now %.3lf kHz.",avg_field/1E+3); 
   // cm_msg(MINFO,"read_yoko_event",msg);

   // determine the new current to set on the Yokogawa 
   rc = update_current(IsFieldUpdated,current_setpoint,avg_field);
   if (rc!=0) { 
      cm_msg(MERROR,"read_yoko_event","Cannot update the current!");
   }

   // read the current on the Yokogawa   
   read_from_device(); 

   // now write everything to MIDAS banks 

   static unsigned int num_events = 0; 
   DWORD *pPSFBData; 

   INT BufferLoad; 
   INT BufferLoad_size = sizeof(BufferLoad); 

   // ROOT output 
   if (write_root) {
      mlock_data.lock();
      PSFBCurrent = PSFBBuffer[0];
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
   bk_create(pevent,psfb_bank_name,TID_WORD,(void **)&pPSFBData);
   // copy data into pPSFBData 
   memcpy(pPSFBData, &(PSFBBuffer[0]), sizeof(g2field::psfeedback_t));
   // increment the pointer 
   pPSFBData += sizeof(g2field::psfeedback_t)/sizeof(WORD);
   // close the event 
   bk_close(pevent,pPSFBData);

   // some type of cleanup 
   PSFBBuffer.erase( PSFBBuffer.begin() ); 
   // check size of readout buffer 
   BufferLoad = PSFBBuffer.size();

   // unlock the thread  
   mlock_data.unlock();  

   const int SIZE = 200; 
   char *buf_load_path = (char *)malloc( sizeof(char)*(SIZE+1) ); 
   sprintf(buf_load_path,"%s/Buffer Load",MONITORS_DIR); 

   //update buffer load in ODB
   db_set_value(hDB,0,buf_load_path,&BufferLoad,BufferLoad_size,1,TID_INT); 
   free(buf_load_path);  

   // increment event counter 
   gEventCounter++; 

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
   g2field::psfeedback_t *psfb_data = new g2field::psfeedback_t; 

   int rc=0;

   // grab the data 
   if (!gSimMode) { 
      // real data 
      mode = yokogawa_interface::get_mode(); 
     // cm_msg(MINFO,"read","Yokogawa mode: %d",mode);
      if (mode==-1) {
         // something is wrong
         rc         = check_yokogawa_comms(mode,"read_from_device"); 
         is_enabled = -1; 
         lvl        = -500E-3;   // unrealistic value  
      } else { 
         is_enabled = yokogawa_interface::get_output_state(); 
         lvl        = yokogawa_interface::get_level(); 
      } 
      // fill the data structure  
      psfb_data->sys_clock  = gCurrentTime;  // not sure of the difference here... 
      psfb_data->mode       = mode;  
      psfb_data->is_enabled = is_enabled;  
      psfb_data->p_fdbk     = pidLoop->GetPCoeff(); // gP_coeff; 
      psfb_data->i_fdbk     = pidLoop->GetICoeff(); // gI_coeff; 
      psfb_data->d_fdbk     = pidLoop->GetDCoeff(); // gD_coeff; 
      if (mode==yokogawa_interface::kVOLTAGE) {
	 psfb_data->current = 0.; 
	 psfb_data->voltage = lvl; 
      } else if (mode==yokogawa_interface::kCURRENT) {
	 psfb_data->current = lvl; 
	 psfb_data->voltage = 0.; 
      }
      psfb_data->fdbk_state = (int)gIsFeedbackOn; 
   } else { 
      // this is a simulation, fill with random numbers
      psfb_data->sys_clock  = gCurrentTime;
      psfb_data->current    = (double)(rand() % 100);   // random number between 0 and 100 
      psfb_data->voltage    = 0.;
      psfb_data->p_fdbk     = pidLoop->GetPCoeff(); // gP_coeff; 
      psfb_data->i_fdbk     = pidLoop->GetICoeff(); // gI_coeff; 
      psfb_data->d_fdbk     = pidLoop->GetDCoeff(); // gD_coeff; 
      psfb_data->mode       = -1;  
      psfb_data->is_enabled = 0;  
      psfb_data->fdbk_state = 0; 
   } 
   // fill buffer 
   mlock_data.lock(); 
   PSFBBuffer.push_back(*psfb_data); 
   mlock_data.unlock(); 

   // Update ODB
   char current_read_path[512];
   sprintf(current_read_path,"%s/Current Value (mA)",MONITORS_DIR);
   double current_val = lvl/1E-3;
   db_set_value(hDB,0,current_read_path,&current_val,sizeof(current_val),1,TID_DOUBLE);

   if (gWriteTestData) write_to_file("yoko-readout",gCurrentTime,lvl); 
 
   // clean up for next read 
   delete psfb_data;

   // update read thread flag 
   ReadThreadActive = 0;
   mlock.lock();
   db_set_value(hDB,0,read_path,&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL);
   free(read_path); 
   mlock.unlock();

}
//______________________________________________________________________________
int update_parameters_from_ODB(double &current_setpoint,double &avg_field){
   // update values from the ODB 
   char msg[512]; 

   char sf_path[512];
   sprintf(sf_path,"%s/Scale Factor (kHz per mA)",SETTINGS_DIR);
   double sf = 0;
   int SIZE_DOUBLE  = sizeof(sf);  
   db_get_value(hDB,0,sf_path,&sf,&SIZE_DOUBLE,TID_DOUBLE, 0);
   double sf_Amps_per_Hz = 1./(sf*1E+6);   
   pidLoop->SetScaleFactor(sf_Amps_per_Hz); 

   double CURRENT=0; 
   char current_set_path[512];
   sprintf(current_set_path,"%s/Current Setpoint (mA)",SETTINGS_DIR);
   db_get_value(hDB,0,current_set_path,&CURRENT,&SIZE_DOUBLE,TID_DOUBLE, 0);
   current_setpoint = CURRENT*1E-3; // convert to amps! 

   char field_set_path[512];
   sprintf(field_set_path,"%s/Field Setpoint (kHz)",SETTINGS_DIR);
   double field_setpoint=0;
   db_get_value(hDB,0,field_set_path,&field_setpoint,&SIZE_DOUBLE,TID_DOUBLE, 0);
   field_setpoint *= 1E+3;              // convert from kHz -> Hz! 
   // pidLoop->SetSetpoint(field_setpoint); 

   char switch_path[512];
   sprintf(switch_path,"%s/Feedback Active",SETTINGS_DIR);
   int SIZE_BOOL = sizeof(gIsFeedbackOn);
   db_get_value(hDB,0,switch_path,&gIsFeedbackOn,&SIZE_BOOL,TID_BOOL, 0);

   char test_flag_path[512];
   sprintf(test_flag_path,"%s/Write Test File",SETTINGS_DIR);
   db_get_value(hDB,0,test_flag_path,&gWriteTestData,&SIZE_BOOL,TID_BOOL, 0);

   char pr_path[512];
   sprintf(pr_path,"%s/Use Single Probe for Field Avg",SETTINGS_DIR);
   db_get_value(hDB,0,pr_path,&gUseSingleProbe,&SIZE_BOOL,TID_BOOL, 0);

   char pn_path[512];
   int probeNum = -1;
   int SIZE_INT = sizeof(probeNum); 
   sprintf(pn_path,"%s/Probe Number for Field Avg",SETTINGS_DIR);
   db_get_value(hDB,0,pn_path,&probeNum,&SIZE_INT,TID_INT, 0);
   gProbeNum = probeNum;     

   char thr_path[512]; 
   sprintf(thr_path,"%s/Feedback Threshold (Hz)",SETTINGS_DIR); 
   db_get_value(hDB,0,thr_path,&gFieldLimit,&SIZE_DOUBLE,TID_DOUBLE,0); 
   pidLoop->SetMaxError(gFieldLimit); 

   char pc_path[512];
   sprintf(pc_path,"%s/P Coefficient",SETTINGS_DIR);
   double P_coeff = 0;
   db_get_value(hDB,0,pc_path,&P_coeff,&SIZE_DOUBLE,TID_DOUBLE, 0);
   pidLoop->SetPCoeff(P_coeff);  

   char ic_path[512];
   sprintf(ic_path,"%s/I Coefficient",SETTINGS_DIR);
   double I_coeff = 0;
   db_get_value(hDB,0,ic_path,&I_coeff,&SIZE_DOUBLE,TID_DOUBLE, 0);
   pidLoop->SetICoeff(I_coeff);  

   char dc_path[512];
   sprintf(dc_path,"%s/D Coefficient",SETTINGS_DIR);
   double D_coeff = 0;
   db_get_value(hDB,0,dc_path,&D_coeff,&SIZE_DOUBLE,TID_DOUBLE, 0);
   pidLoop->SetDCoeff(D_coeff);  

   char ic_alt_path[512];
   sprintf(ic_alt_path,"%s/I Alt Coefficient",SETTINGS_DIR);
   double I_alt_coeff = 0;
   db_get_value(hDB,0,ic_alt_path,&I_alt_coeff,&SIZE_DOUBLE,TID_DOUBLE, 0);
   pidLoop->SetIAltCoeff(I_alt_coeff);  

   char corr_path[512];
   sprintf(corr_path,"%s/Maximum Correction Size (Hz)",SETTINGS_DIR);
   double corr_size = 0;
   db_get_value(hDB,0,corr_path,&corr_size,&SIZE_DOUBLE,TID_DOUBLE, 0);
   pidLoop->SetMaxCorrSize(corr_size);  

   char max_path[512];
   sprintf(max_path,"%s/Maximum PID Output (Hz)",SETTINGS_DIR);
   double maxPID = 0;
   db_get_value(hDB,0,max_path,&maxPID,&SIZE_DOUBLE,TID_DOUBLE, 0);
   pidLoop->SetMaxPIDOutput(maxPID);  

   char max_i_path[512];
   sprintf(max_i_path,"%s/Maximum I Term Output (Hz)",SETTINGS_DIR);
   double maxITerm = 0;
   db_get_value(hDB,0,max_i_path,&maxITerm,&SIZE_DOUBLE,TID_DOUBLE, 0);
   pidLoop->SetMaxITermOutput(maxITerm);  

   double AVG=0;
   int rc = check_average_field_ODB(AVG);
   avg_field = AVG;  

   // Use first event to set the field setpoint 
   if (gEventCounter==0) {
      pidLoop->SetSetpoint(avg_field); 
      sprintf(msg,"The setpoint is %.3lf kHz",avg_field/1E+3); 
      cm_msg(MINFO,"update_parameters_from_ODB",msg);
   } else { 
      // pidLoop->SetSetpoint(field_setpoint); 
   }

   return rc; 

}
//______________________________________________________________________________
int check_average_field_ODB(double &avg_field){
   // check to see if the average field has changed  
   int rc=0;  // no field change

   // get all probe frequencies  
   const int NPROBES = 378;
   double probeFreq[NPROBES];
   for(int i=0;i<NPROBES;i++) probeFreq[i] = 0.;  
   INT ARR_SIZE = sizeof(probeFreq); 

   // grab all probe frequencies 
   const int SIZE = 512; 
   char arr_path[SIZE]; 
   sprintf(arr_path,"%s/nmr_freq_array",SHARED_DIR); 
   db_get_value(hDB,0,arr_path,probeFreq,&ARR_SIZE,TID_DOUBLE,0);

   // get filtered mean frequency  
   char freq_path[SIZE]; 
   double FIELD_AVG=0;
   int SIZE_DOUBLE = sizeof(FIELD_AVG);  
   sprintf(freq_path,"%s/filtered_mean_nmr_freq",SHARED_DIR);
   db_get_value(hDB,0,freq_path,&FIELD_AVG,&SIZE_DOUBLE,TID_DOUBLE, 0);

   // get time of updated frequency  
   char time_path[SIZE]; 
   double TIME=0;
   sprintf(time_path,"%s/time_of_update",SHARED_DIR);
   db_get_value(hDB,0,freq_path,&TIME,&SIZE_DOUBLE,TID_DOUBLE, 0);
   gFieldUpdateTime = TIME; 

   // are we using a single probe for the field average? 
   if (gUseSingleProbe) FIELD_AVG = probeFreq[gProbeNum]; 
   
   FIELD_AVG *= 1E+3;   // convert from kHz -> Hz 

   if (avg_field != gLastAvgField ) {
      // average field changed!  
      avg_field  = FIELD_AVG;  // convert from kHz -> Hz 
      rc = 1;
   } 
 
   return rc;  
}
//______________________________________________________________________________
int update_current(BOOL IsFieldUpdated,double current_setpoint,double avg_field){
   // update the current on the yokogawa based on the ODB
   int rc=-1;
   double lvl=0,eps=0; 
   char msg[200];  
   
   gCurrentTime = get_utc_time();
   double time_sec = gCurrentTime/1E+9; 

   // get the setpoint for printing to file
   double theSetpoint = pidLoop->GetSetpoint(); 

   // eps = get_new_current(avg_field);
   if(gWriteTestData && !gIsFeedbackOn){ 
      eps = pidLoop->Update(time_sec,avg_field);    
      rc  = write_to_file("fdbk-off",gCurrentTime,avg_field,theSetpoint,eps);   
      // reset before we do any real calculation... 
      eps = 0.;
   }

   // write the field data regardless of feedback being on or off  
   unsigned long time_of_update = (unsigned long)gFieldUpdateTime;
   if (gWriteTestData) rc = write_to_file("field-update",time_of_update,avg_field);   

   double field_change     = avg_field - gLastAvgField; 
   double abs_field_change = fabs(field_change); 
   
   // double err_term     = theSetpoint - avg_field; 
   // double abs_err_term = fabs(err_term); 

   // bool IsSmallFieldCorr = false;  
   // double delta=0;    
   // if (abs_err_term>gSmallFieldLimit) {
   //    // if the change in the field is bigger than the small field limit
   //    // apply a small correction that is the small field limit (with the correct sign)  
   //    delta = (err_term/abs_err_term)*gSmallFieldLimit*pidLoop->GetScaleFactor();   // scale factor puts us in Amps 
   //    IsSmallFieldCorr = true;  
   // } 

   // Update the current to set 
   // check if feedback is on and the average field value changed
   if (gIsFeedbackOn) {
      if (IsFieldUpdated) {
         // the field was updated; so we try to change the current
         // otherwise, leave the current as is for now  
	 // if( (abs_field_change>=gFieldLimit) && gCounter>1 ) { 
	 //    // change in field is too large and it's not the first time we try to change 
	 //    // the current on the yokogawa. 
	 //    sprintf(msg,"The field changed by %.3lf Hz!  Last field = %.3lf kHz, current field = %.3lf kHz.",
         //            field_change,avg_field/1E+3,gLastAvgField/1E+3); 
	 //    cm_msg(MERROR,"update_current",msg);
	 //    // eps = 0.; 
	 // } else { 
            // ok, the field change is smaller than the field limit.  let's use the PID loop. 
	    // send in the time (in sec) and the average field (in Hz); compares to setpoint 
	    eps = pidLoop->Update(time_sec,avg_field);    
	 // }
	 gTotalCurrent = eps;
         // if(IsSmallFieldCorr){
         //    // strcpy(msg,""); 
	 //    // sprintf(msg,"Accumulating small change of %.3lf A",delta); 
	 //    // cm_msg(MINFO,"update_current",msg);
	 //    gTotalCurrent += delta;
         // } 
	 gCounter++;                    // count the update since we possibly changed the current 
      } 
      lvl = gTotalCurrent;              // assign the total current to the level we'll set  
      if (gWriteTestData) rc = write_to_file("fdbk-on",gCurrentTime,avg_field,theSetpoint,lvl);   
   } else {
      lvl = current_setpoint;		// If not running feedback, use the current_setpoint  
   }
   
   // now check the current we'll set
   // does it meet the hardware limits? 
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
      if (rc!=0) { 
         rc = check_yokogawa_comms(rc,"update_current");
         return FE_ERR_HW;  
      }
   } else {
      // simulation, do nothing  
   }
   
   // keep track of the last current and field if we ever need it... 
   gLastCurrent  = gTotalCurrent; 
   gLastAvgField = avg_field;

   return rc;  
}
//______________________________________________________________________________
unsigned long get_utc_time(){
   unsigned long utc = hw::systime_us()*1E+3; 
   // struct timeb now; 
   // int rc = ftime(&now); 
   // unsigned long utc = now.time + now.millitm;
   return utc; 
}
//______________________________________________________________________________
int check_yokogawa_comms(int rc,const char *func){
   int RC=-1,RC2=-1;
   int err_code=0;
   char err_msg[512],myStr[512]; 
   if (rc!=0) {
      err_code = yokogawa_interface::error_check(err_msg); 
      sprintf(myStr,"Yokogawa communication FAILED.  Error code: %d, message: %s \n",err_code,err_msg); 
      cm_msg(MERROR,func,myStr);
      RC2 = yokogawa_interface::clear_errors();
      RC  = err_code; 
   } else {
      RC = rc;
   }
   return RC; 
} 
//______________________________________________________________________________
int write_to_file(const char *tag,unsigned long time,double x,double y,double z){
   char myStr[512],filepath[512],write_str[512];
   int RunNumber=0;
   int RunNumber_size = sizeof(RunNumber); 
   db_get_value(hDB,0,"/Runinfo/Run number",&RunNumber,&RunNumber_size,TID_INT, 0);
   sprintf(filepath,"%s/%s_%05d.csv",TEST_DIR,tag,RunNumber);   
   ofstream outfile;
   outfile.open(filepath,ios::app); 
   if ( outfile.fail() ){
      sprintf(myStr,"Cannot write to the file: %s.",filepath);
      cm_msg(MERROR,"write_to_file",myStr); 
      return 1; 
   } else {
      sprintf(write_str,"%lu,%.10E,%.10E,%.10E",time,x,y,z); 
      outfile << write_str << std::endl;
   } 
   return 0; 
}
//______________________________________________________________________________
int write_to_file(const char *tag,unsigned long time,double x){
   char myStr[512],filepath[512],write_str[512];
   int RunNumber=0;
   int RunNumber_size = sizeof(RunNumber); 
   db_get_value(hDB,0,"/Runinfo/Run number",&RunNumber,&RunNumber_size,TID_INT, 0);
   sprintf(filepath,"%s/%s_%05d.csv",TEST_DIR,tag,RunNumber);   
   ofstream outfile;
   outfile.open(filepath,ios::app); 
   if ( outfile.fail() ){
      sprintf(myStr,"Cannot write to the file: %s.",filepath);
      cm_msg(MERROR,"write_to_file",myStr); 
      return 1; 
   } else {
      sprintf(write_str,"%lu,%.10E",time,x); 
      outfile << write_str << std::endl;
   } 
   return 0; 
}

