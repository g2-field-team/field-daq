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
#include <sstream>
#include <string>
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
#include <zmq.hpp>

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
  bool write_midas = true;
  bool write_root = true;

  TFile *pf;
  TTree * pt_norm;

  std::atomic<bool> run_in_progress;

  boost::property_tree::ptree conf;
  
  g2field::surface_coil_t data; //Surface coil data type defined in field_structs.hh

  const int nCoils = g2field::kNumSCoils; //Defined in field_constants.hh
  Double_t setPoint; //If difference between value and set point is larger than this, need to change the current. Value stored in odb

  Double_t bot_set_values[nCoils];
  Double_t top_set_values[nCoils];

  Double_t bot_currents[nCoils];
  Double_t top_currents[nCoils];
}

int read_coil_currents(Double_t val_array[]);
int set_coil_currents(Double_t val_array[], Double_t set_array[], int index);

int read_coil_currents(Double_t val_array[]){
  for(int i=0;i<nCoils;i++){
    val_array[i] = 0.0; //This is where the actual readout routine will go
  }

  return SUCCESS;
}

int set_coil_currents(Double_t val_array[], Double_t set_array[], int index){
  val_array[index] = set_array[index];

  return SUCCESS;
  }

//--- Frontend Init ---------------------------------------------------------//
INT frontend_init()
{
  INT rc = load_settings(frontend_name, conf);
  if(rc != SUCCESS){
    return rc;
  }
  
  //Get the set currents
  HNDLE hDB, hkey;

  cm_get_experiment_database(&hDB, NULL);

  //Get bottom set currents                                                               
  db_find_key(hDB, 0, "/Equipment/Surface Coils/Settings/Bottom Set Currents", &hkey);

  if(hkey == NULL){
    cm_msg(MERROR, "begin_of_run", "unable to find Bottom Set Currents key");
  }

  for(int i=0;i<nCoils;i++){
    bot_set_values[i] = 0;
    top_set_values[i] = 0;
  }

  int bot_size = sizeof(bot_set_values);

  db_get_data(hDB, hkey, &bot_set_values, &bot_size, TID_FLOAT);

  //Top
  db_find_key(hDB, 0, "/Equipment/Surface Coils/Settings/Top Set Currents", &hkey);

  if(hkey == NULL){
    cm_msg(MERROR, "begin_of_run", "unable to find Top Set Currents key");
  }

  int top_size = sizeof(top_set_values);

  db_get_data(hDB, hkey, &top_set_values, &top_size, TID_FLOAT);

  //Send the data to the driver boards
  //Prepare context and socket
  zmq::context_t context(1);
  zmq::socket_t socket(context, ZMQ_REQ);

  cm_msg(MINFO, "init", "Connecting to server");
  socket.connect("tcp://localhost:5555");

  //send message
  zmq::message_t message(18);
  snprintf((char *) message.data(), 18, "%f %f", bot_set_values[0], bot_set_values[1]);
  socket.send(message);
  cm_msg(MINFO, "init", "Current values sent to beaglebone");

  //Wait for response that currents were sent
  //Expects back the read out currents of the channels that were set
  zmq::message_t reply;
  socket.recv(&reply);
  double Ch0Set, Ch1Set;
  std::istringstream iss(static_cast<char*>(reply.data()));
  iss >> Ch0Set >> Ch1Set;
  cm_msg(MINFO, "info", "Received values %f %f", Ch0Set, Ch1Set);

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

  //set up the data
  std::string datadir;
  std::string filename;

  //Grab the database handle
  cm_get_experiment_database(&hDB, NULL);

  // Get the run info out of the ODB.                                                     
  db_find_key(hDB, 0, "/Runinfo", &hkey);
  if (db_open_record(hDB, hkey, &runinfo, sizeof(runinfo), MODE_READ,
                     NULL, NULL) != DB_SUCCESS) {
    cm_msg(MERROR, "begin_of_run", "Can't open \"/Runinfo\" in ODB");
    return CM_DB_ERROR;
  }

  // Get the data directory from the ODB.                                                 
  snprintf(str, sizeof(str), "/Logger/Data dir");
  db_find_key(hDB, 0, str, &hkey);

  if (hkey) {
    size = sizeof(str);
    db_get_data(hDB, hkey, str, &size, TID_STRING);
    datadir = std::string(str);
  }

  // Set the filename                                                                     
  snprintf(str, sizeof(str), "root/fixed_run_%05d.root", runinfo.run_number);

  // Join the directory and filename using boost filesystem.                              
  filename = (filesystem::path(datadir) / filesystem::path(str)).string();

  // Get the parameter for root output.                                                   
  db_find_key(hDB, 0, "/Experiment/Run Parameters/Root Output", &hkey);

  if (hkey) {
    size = sizeof(flag);
    db_get_data(hDB, hkey, &flag, &size, TID_BOOL);

    write_root = flag;
  } else {

    write_root = true;
  }

  if (write_root) {
    // Set up the ROOT data output.                                                       
    pf = new TFile(filename.c_str(), "recreate");
    pt_norm = new TTree("t_scc", "Sim Surface Coil Data");
    pt_norm->SetAutoSave(5);
    pt_norm->SetAutoFlush(20);

    std::string br_name("surface_coils");

    pt_norm->Branch(br_name.c_str(), &data.bot_sys_clock[0], g2field::sc_str);
  }



  //Get the allowable difference
  db_find_key(hDB, 0, "/Equipment/Surface Coils/Settings/Allowed Difference", &hkey);

  setPoint = 0;
  int setpt_size = sizeof(setPoint);

  db_get_data(hDB, hkey, &setPoint, &setpt_size, TID_FLOAT);

  for(int i=0; i<nCoils;i++){
    bot_currents[i] = 0;
    top_currents[i] = 0;
  }
  //Read coil currents
  read_coil_currents(bot_currents);
  read_coil_currents(top_currents);

  //Check that the initial currents match the set points. If not, reset any necessary channels 
  for(int i=0;i<nCoils;i++){
    if(abs(bot_currents[i]-bot_set_values[i])>setPoint){
       set_coil_currents(bot_currents,bot_set_values,i);
    }
    if(abs(top_currents[i]-top_set_values[i])>setPoint){
      set_coil_currents(top_currents,top_set_values,i);
    }
  }

//ADD ALARM IF CHANNEL CAN'T BE SET

  event_number = 0;
  run_in_progress = true;
 	      
  cm_msg(MLOG, "begin of run", "Completed successfully");

  return SUCCESS;
}

//--- End of Run -------------------------------------------------------------//
INT end_of_run(INT run_number, char *error)
{
  //DISABLE COMMUNICATION?

  // Make sure we write the ROOT data.                                                    
  if (run_in_progress && write_root) {

    pt_norm->Write();

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
      usleep(10);
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
  static unsigned long long num_events;
  static unsigned long long events_written;

  HNDLE hDB, hkey;
  char bk_name[10]; //bank name
  DWORD *pdata; //place to store data
  
  //initialize MIDAS bank
  bk_init32(pevent);

  sprintf(bk_name, "SCCS");
  bk_create(pevent, bk_name, TID_DOUBLE, (void **)&pdata);

  //Read the data
  for(int i=0;i<nCoils;i++){
    bot_currents[i]=0;
    top_currents[i]=0;
  }

  read_coil_currents(bot_currents);
  read_coil_currents(top_currents);

  for(int idx = 0; idx < nCoils; ++idx){
    data.bot_sys_clock[idx] = hw::systime_us();
    data.top_sys_clock[idx] = hw::systime_us();
    data.bot_coil_currents[idx] = bot_currents[idx]; 
    data.top_coil_currents[idx] = top_currents[idx];
    data.bot_coil_temps[idx] = 0.0;
    data.top_coil_temps[idx] = 0.0;
    //Add a check here to see if the currents match set point
    //Alarm if not? Reset current?
  }

  //write root output
  if (write_root && run_in_progress) {
    cm_msg(MINFO, "read_fixed_event", "Filling TTree");
    // Now that we have a copy of the latest event, fill the tree.                        
    pt_norm->Fill();

    num_events++;

    if (num_events % 10 == 1) {

      cm_msg(MINFO, frontend_name, "flushing TTree.");
      pt_norm->AutoSave("SaveSelf,FlushBaskets");

      pf->Flush();
    }
  }

  //Write midas output
  memcpy(pdata, &data, sizeof(data));
  pdata += sizeof(data) / sizeof(DWORD);

  bk_close(pevent, pdata);

  cm_msg(MINFO, "read_surface_coils", "Finished generating event");
  return bk_size(pevent);

}

 
