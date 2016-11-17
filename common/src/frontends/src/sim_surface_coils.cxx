/******************************************************************************

Name: sim_surface_coils.cxx
Author: Rachel E. Osofsky
Email: osofskyr@uw.edu

About: Implements a MIDAS frontend for the surface coils. It checks
       what the current settings should be and makes sure that the
       currents are set correctly. Then collects data continuously about
       the output currents.

\*****************************************************************************/

//--- std includes ----------------------------------------------------------//
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <ctime>
using std::string;

//--- other includes --------------------------------------------------------//
#include "midas.h"
#include "TFile.h"
#include "TTree.h"
#include "g2field/common.hh"

//--- project includes -----------------------------------------------------//
#include "frontend_utils.hh"
#include "field_structs.hh"

//--- globals --------------------------------------------------------------//
#define FRONTEND_NAME "Surface Coils"

extern "C" {
  //The frontend name (client name) as seen by other MIDAS clients
  char *frontend_name = (char *)FRONTEND_NAME;

  //The frontend file name, don't change it
  char *frontend_file_name = (char *)__FILE__;

  //frontend_loop is called periodically if this variable is TRUE
  BOOL frontend_call_loop = FALSE;

  //A frontend status page is displaced with this frequency in ms
  INT display_period = 1000;

  //maximum event size produced by this frontend
  INT max_event_size = 0x800000; // 8 MB

  //maximum event size for fragmented events (EQ_FRAGMENTED)
  INT max_event_size_frag = 0x100000;

  //buffer size to hold events
  INT event_buffer_size = 0x1000000; // 64 MB

  //Function declarations
  INT frontend_init();
  INT frontend_exit();
  INT begin_of_run(INT run_number, char *error);
  INT end_of_run(INT run_number, char *error);
  INT pause_run(INT run_number, char *error);
  INT resume_run(INT run_number, char *error);

  INT frontend_loop();
  INT read_surface_coils(char *pevent, INT c);//NEED TO DEFINE THIS BETTER LATER
  INT poll_event(INT source, INT count, BOOL test);
  INT interrupt_configure(INT cmd, INT source, POINTER_T adr);

  //Equipment list

  EQUIPMENT equipment[] = 
    {
      {FRONTEND_NAME,  //equipment name
       { 1, 0,         //event ID, trigger mask
	 "SYSTEM",     //event bugger (use to be SYSTEM)
	 EQ_PERIODIC,  //equipment type
	 0,            //not used
	 "MIDAS",      //format
	 TRUE,         //enabled
	 RO_RUNNING,   //read only when running
	 10,           //poll for 10 ms
	 0,            //stop run after this event limit
	 0,            //number of sub events
	 0,            //don't log history
	 "","","",
       },
       read_surface_coils, //readout routine
      },

      {""}
    };
} //extern C

RUNINFO runinfo;

//Anonymous namespace for "globals"
namespace{
  
  int event_number = 0;
  bool write_root = false;
  bool write_midas = true;
  
  TFile *pf;
  TTree *pt;

  std::atomic<bool> run_in_progress;

  boost::property_tree::ptree conf;
  
  g2field::surface_coil_t data; //Surface coil data type defined in field_structs.hh

  const int ncoils = g2field::kNumSCoils; //Defined in field_constants.hh
}

//void trigger_loop();

//int load_device_class();

//--- Frontend Init ---------------------------------------------------------//
INT frontent_init()
{
  run_in_progress = false;
  
  cm_msg(MINFO, "init","Sim Surface Coils initialization complete");
  return SUCCESS;

}

//--- Frontend Exit ---------------------------------------------------------//
INT frontend_exit()
{
  run_in_progress = false;

  cm_msg(MINFO, "exit", "Sim Surface Coils teardown complete");
  return SUCCESS;
}

//--- Begin of Run ----------------------------------------------------------//
INT begin_of_run(INT run_number, char *error)
{
  using namespace boost;

  //ODB parameters
  HNDLE hDB, hkey;
  char str[256];
  int size;
  BOOL flag;

  //Set up the data
  std::string datadir;
  std::string filename;

  //Grab the database handle
  cm_get_experiment_database(&hDB, NULL);

  //Get the run info out of the ODB
  db_find_key(hDB, 0, "/Runinfo", &hkey);
  if (db_open_record(hDB, hkey, &runinfo, sizeof(runinfo), MODE_READ,
		     NULL, NULL) != DB_SUCCESS) {
    cm_msg(MERROR,"begin_of_run","Can't open \"/Runinfo\" in ODB");
    return CM_DB_ERROR;
  }

  //Get the data directory from the ODB
  snprintf(str, sizeof(str), "/Logger/Data dir");
  db_find_key(hDB, 0, str, &hkey);

  if(hkey){
    size = sizeof(str);
    db_get_data(hDB, hkey, str, &size, TID_STRING);
    datadir = std::string(str);
  }

  //Set the filename
  snprintf(str, sizeof(str), "root.surface_coil_run_%05d.root",runinfo.run_number);

  //Join the directory and filename using boost filesystem
  filename = (filesystem::path(datadir) / filesystem::path(str)).string();

  //Get the parameter for root output
  db_find_key(hDB, 0, "Experiment/Run Parameters/Root Output", &hkey);	      

   if(hkey) {
     size = sizeof(flag);
     db_get_data(hDB, hkey, &flag, &size, TID_BOOL);

     write_root = flag;
   } else {
     
     write_root = true;
   }

  if (write_root){
    
    //Set up the ROOT data output.
    pf = new TFile(filename.c_str(), "recreate");
    pt = new TTree("t_srfccl","Sim Surface Coil Data");
    pt->SetAutoSave(5);
    pt->SetAutoFlush(20);

    std::string br_name("surface_coils");

    pt->Branch(br_name.c_str(), &data.sys_clock[0], g2field::sc_str);
  }

  event_number = 0;
  run_in_progress = true;
 	      
  cm_msg(MLOG, "begin of run", "Completed successfully");

  return SUCCESS;
}

//--- End of Run -------------------------------------------------------------//
INT end_of_run(INT run_number, char *error)
{
  //Make sure we write the ROOT data
  if (run_in_progress && write_root){
    
    pt->Write();
    pf->Write();
    pf->Close();

    delete pf;
  }

  run_in_progress = false;

  cm_msg(MLOG, "end_of_run","Completed successfully");
  return SUCCESS;
}

//--- Pause Run --------------------------------------------------------------//
INT pause_run(INT run_number, char *error)
{
  return SUCCESS;
}

//--- Resume Run -------------------------------------------------------------//
INT resume_run(INT run_number, char *error)
{
  return SUCCESS;
}

//--- Frontend Loop ----------------------------------------------------------//
INT frontend_loop()
{
  //If frontend call_loop is true, this routine gets called when
  //the frontend is idle or once between every event
  return SUCCESS;
}

//----------------------------------------------------------------------------//

/*****************************************************************************\

   Readout routines for different events

\******************************************************************************/

//--- Trigger event routines -------------------------------------------------//
INT poll_event(INT source, INT count, BOOL test)
{
  static bool triggered = false;

  //fake calibration
  if(test) {
    for(int i=0;i<count;i++){
      usleep(1000);
    }
    return 0;
  }
  
  return 0;
}

//--- Interrupt configuration ------------------------------------------------//
INT interrupt_configure(INT cmd, INT source, PTYPE adr)
{
  switch (cmd){
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

//--- Event Readout ----------------------------------------------------------//
INT read_surface_coils(char *pevent, INT c)
{
  cm_msg(MINFO, "read_surface_coils", "Finished generating event");
  return bk_size(pevent);

}

 
