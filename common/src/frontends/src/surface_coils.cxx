/******************************************************************************

Name: surface_coils.cxx
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
#include <iomanip>
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
#include <json.hpp>
#include <boost/property_tree/json_parser.hpp>

//--- project includes -----------------------------------------------------//
#include "frontend_utils.hh"
#include "core/field_structs.hh"

//--- globals --------------------------------------------------------------//
#define FRONTEND_NAME "Surface Coils"

using json = nlohmann::json;

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
       { EVENTID_SURFACE_COIL, 0,         //event ID, trigger mask
	 "SYSTEM",     //event bugger (use to be SYSTEM)
	 EQ_PERIODIC,  //equipment type
	 0,            //not used
	 "MIDAS",      //format
	 TRUE,         //enabled
	 RO_RUNNING,   //read only when running
	 1000,           //poll every 1000 ms
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

std::string coil_string(string loc, int i);
std::string coil_string(string loc, int i){
  string beg;
  string end;
  if(loc == "bot") beg = "B-";
  if(loc == "top") beg = "T-";
  if (0 <= i < 9) end = "00" + std::to_string(i+1);
  if (9 <= i <= 98) end = "0" + std::to_string(i+1);
  if(i == 99) end = "100";
  string result = beg + end;
  return result;
}

//Anonymous namespace for "globals"
namespace{
  
  signed int event_number = 0;
  bool write_midas = true;
  bool write_root = true;

  //for zmq
  zmq::context_t context(1);
  //  zmq::socket_t publisher(context, ZMQ_PUB); //for sending set point currents
  zmq::socket_t requester(context, ZMQ_REQ);
  zmq::socket_t subscriber(context, ZMQ_SUB); //subscribe to data being sent back from beaglebones

  TFile *pf;
  TTree * pt_norm;

  std::atomic<bool> run_in_progress;

  boost::property_tree::ptree conf;
  
  g2field::surface_coil_t data; //Surface coil data type defined in field_structs.hh

  const int nCoils = g2field::kNumSCoils; //Defined in field_constants.hh
  Double_t setPoint; //If difference between value and set point is larger than this, need to change the current. Value stored in odb

  Double_t bot_set_values[nCoils];
  Double_t top_set_values[nCoils];

  Double_t bot_intercepts[nCoils];
  Double_t top_intercepts[nCoils];
  Double_t bot_slopes[nCoils];
  Double_t top_slopes[nCoils];

  Double_t bot_currents[nCoils];
  Double_t top_currents[nCoils];

  Double_t bot_temps[nCoils];
  Double_t top_temps[nCoils];
  
  Double_t high_temp;

  int current_health;
  int temp_health;

  std::map<std::string, std::string> coil_map; //sc_id, hw_id
  std::map<std::string, std::string> rev_coil_map;//hw_id, sc_id
  json request; //json object to hold set currents
 }

//--- Frontend Init ---------------------------------------------------------//
INT frontend_init()
{
  INT rc = load_settings(frontend_name, conf);
  if(rc != SUCCESS){
    return rc;
  }
  
  //Get the channel mapping
  HNDLE hDB, hkey;

  //Grab the database handle                                        
  cm_get_experiment_database(&hDB, NULL);
  
  for(int i=0;i<2*nCoils;i++){
    string crate_key;
    string board_key;
    string channel_key;

    char buf[32];
    string sc_id;
    string hw_id;

    for(int j=1;j<7;j++){ //loop through the crates
      crate_key = "/Settings/Hardware/Surface Coils/Crate "+std::to_string(j)+"/";
      for(int k=1;k<10;k++){ //loop through the driver boards
	board_key = crate_key + "Driver Board " + std::to_string(k) + "/";
	for(int l=1;l<5;l++){ //loop through the channels
	  channel_key = board_key + "Channel " + std::to_string(l);
	  db_find_key(hDB, 0, channel_key.c_str(), &hkey);
	  
	  if(hkey == NULL){
	    cm_msg(MERROR, "frontend_init", "unable to find %s key", channel_key.c_str());
	  }
	  
	  int buf_size = sizeof(buf);
	  db_get_data(hDB, hkey, &buf, &buf_size, TID_STRING);
	  sc_id = string(buf);
	  hw_id = std::to_string(j) + std::to_string(k) + std::to_string(l); 
	  if(sc_id.size() > 2) coil_map[sc_id] = hw_id; //Don't include unused channels 
	  if(sc_id.size() > 2) rev_coil_map[hw_id] = sc_id; //Don't include unused channels
	  //Access values as coil_map["T-###"] (returns value ###) 
	  //or rev_coil_map["###"] (returns value T-###)
	}
      }
    }
  }
 
  //Get the calibration values (intercepts and slopes)
  db_find_key(hDB, 0, "/Equipment/Surface Coils/Settings/Calibration/Bottom Intercepts", &hkey);
 
   if(hkey == NULL){
    cm_msg(MERROR, "begin_of_run", "unable to find Bottom Intercepts key");
  }

  int bot_int_size = sizeof(bot_intercepts);
  db_get_data(hDB, hkey, &bot_intercepts, &bot_int_size, TID_DOUBLE); 
  
  ///
  db_find_key(hDB, 0, "/Equipment/Surface Coils/Settings/Calibration/Top Intercepts", &hkey);

  if(hkey == NULL){
    cm_msg(MERROR, "begin_of_run", "unable to find Top Intercepts key");
  }

  int top_int_size = sizeof(top_intercepts);
  db_get_data(hDB, hkey, &top_intercepts, &top_int_size, TID_DOUBLE);

  ///
  db_find_key(hDB, 0, "/Equipment/Surface Coils/Settings/Calibration/Bottom Slopes", &hkey);

  if(hkey == NULL){
    cm_msg(MERROR, "begin_of_run", "unable to find Bottom Slopess key");
  }

  int bot_sl_size = sizeof(bot_slopes);
  db_get_data(hDB, hkey, &bot_slopes, &bot_sl_size, TID_DOUBLE);

  ///
  db_find_key(hDB, 0, "/Equipment/Surface Coils/Settings/Calibration/Top Slopes", &hkey);

  if(hkey == NULL){
    cm_msg(MERROR, "begin_of_run", "unable to find Top Slopess key");
  }

  int top_sl_size = sizeof(top_slopes);
  db_get_data(hDB, hkey, &top_slopes, &top_sl_size, TID_DOUBLE);

  //bind to server                                           
  cm_msg(MINFO, "init", "Binding to server");
  //publisher.bind("tcp://127.0.0.1:5550");                              
  //publisher.setsockopt(ZMQ_LINGER, 0);
  //publisher.bind("tcp://*:5550");
  requester.setsockopt(ZMQ_LINGER, 0);
  requester.bind("tcp://*:5550");

  //Now bind subscriber to receive data being pushed by beaglebones             
  std::cout << "Binding to subscribe socket" << std::endl;
  //subscriber.bind("tcp://127.0.0.1:5551");                                    
  //Subscribe to all incoming data                                             
  subscriber.setsockopt(ZMQ_SUBSCRIBE, "", 0);
  subscriber.setsockopt(ZMQ_LINGER, 0);
  subscriber.setsockopt(ZMQ_RCVTIMEO, 5000);
  subscriber.bind("tcp://*:5551");
  std::cout << "Bound to subscribe socket" << std::endl;

  run_in_progress = false;
  
  cm_msg(MINFO, "init","Surface Coils initialization complete");
  return SUCCESS;

}

//--- Frontend Exit ---------------------------------------------------------//
INT frontend_exit()
{
  run_in_progress = false;

  cm_msg(MINFO, "exit", "Surface Coils teardown complete");
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

  //Grab the database handle
  cm_get_experiment_database(&hDB, NULL);

  //Start with all healths being good
  current_health = 1;
  temp_health = 1;
  std::string blank = "";

  db_set_value(hDB, hkey, "/Equipment/Surface Coils/Settings/Monitoring/Current Health", &current_health, sizeof(current_health), 1, TID_BOOL);
  db_set_value(hDB, hkey, "/Equipment/Surface Coils/Settings/Monitoring/Temp Health", &temp_health, sizeof(temp_health), 1, TID_BOOL);
  db_set_value(hDB, hkey, "/Equipment/Surface Coils/Settings/Monitoring/Problem Channel", blank.c_str(), sizeof(blank.c_str()), 1, TID_STRING);


  //Get bottom set currents                                        
  db_find_key(hDB, 0, "/Equipment/Surface Coils/Settings/Set Points/Bottom Set Currents", &hkey);

  if(hkey == NULL){
    cm_msg(MERROR, "begin_of_run", "unable to find Bottom Set Currents key");
  }

  for(int i=0;i<nCoils;i++){
    bot_set_values[i] = 0;
    top_set_values[i] = 0;
  }

  int bot_size = sizeof(bot_set_values);

  db_get_data(hDB, hkey, &bot_set_values, &bot_size, TID_DOUBLE);

  //Top set currents        
  db_find_key(hDB, 0, "/Equipment/Surface Coils/Settings/Set Points/Top Set Currents", &hkey);

  if(hkey == NULL){
    cm_msg(MERROR, "begin_of_run", "unable to find Top Set Currents key");
  }

  int top_size = sizeof(top_set_values);

  db_get_data(hDB, hkey, &top_set_values, &top_size, TID_DOUBLE);

  //Get the allowable difference     
  db_find_key(hDB, 0, "/Equipment/Surface Coils/Settings/Set Points/Allowed Difference", &hkey);
  setPoint = 0;
  int setpt_size = sizeof(setPoint);
  db_get_data(hDB, hkey, &setPoint, &setpt_size, TID_DOUBLE);

  //Get the highest allowed temp
  db_find_key(hDB, 0, "/Equipment/Surface Coils/Settings/Set Points/Allowed Temperature", &hkey);
  high_temp = 0;
  int temp_size = sizeof(high_temp);
  db_get_data(hDB, hkey, &high_temp, &temp_size, TID_DOUBLE);

  //Now that we have the current set points, package them into json object
  if(coil_map.size()!=200) cm_msg(MERROR, "begin_of_run", "Coil map is of wrong size");
  for(int i=1;i<nCoils+1;i++){
    string coilNum;
    if(i >= 0 && i <= 9) coilNum = "00" + std::to_string(i);
    if(i > 9 && i <= 99) coilNum = "0" + std::to_string(i);
    if(i==100) coilNum = std::to_string(i);
    string topString = "T-"+coilNum;
    string botString = "B-"+coilNum;

    if(bot_slopes[i-1] == 0) bot_slopes[i-1] = 1;
    if(top_slopes[i-1] == 0) top_slopes[i-1] = 1;

    Double_t bot_val = (bot_set_values[i-1]-bot_intercepts[i-1])/bot_slopes[i-1];
    Double_t top_val = (top_set_values[i-1]-top_intercepts[i-1])/top_slopes[i-1];
    request[coil_map[botString]] = bot_val;
    request[coil_map[topString]] = top_val;
  }

  /*
  //send data to driver boards             
  for(int i=0;i<5;i++){
    std::string buffer = request.dump();
    zmq::message_t message (buffer.size());
    std::copy(buffer.begin(), buffer.end(), (char *)message.data());
  
    bool st = false; //status of publishing

    while(!st){
     try{
      st = publisher.send(message, ZMQ_DONTWAIT);
      usleep(200);
      std::cout << "Sent a message" << std::endl;	
     } catch(zmq::error_t &err){
	if(err.num() != EINTR) throw;	
       }	
    }
    usleep(500000);
  }//end for loop
  */

  //send data to driver boards
  std::string buffer = request.dump();                                        
  zmq::message_t message (buffer.size());                                     
  std::copy(buffer.begin(), buffer.end(), (char *)message.data());
  requester.send(message);
  std::cout << "Sent the set points" << std::endl;
  zmq::message_t reply;
  requester.recv (&reply);
  std::cout << "set Points were received" << std::endl;

  cm_msg(MINFO, "begin_of_run", "Current values sent to beaglebones"); 
 
  //set up the data
  std::string datadir;
  std::string filename;

  // Get the run info out of the ODB.          
  db_find_key(hDB, 0, "/Runinfo", &hkey);
  if (db_open_record(hDB, hkey, &runinfo, sizeof(runinfo), MODE_READ, NULL, NULL) != DB_SUCCESS) {
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
  snprintf(str, sizeof(str), "root/surface_coil_run_%05d.root", runinfo.run_number);

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
    pt_norm = new TTree("t_scc", "Surface Coil Data");
    pt_norm->SetAutoSave(5);
    pt_norm->SetAutoFlush(20);

    std::string br_name("surface_coils");

    pt_norm->Branch(br_name.c_str(), &data.bot_sys_clock[0], g2field::sc_str);
  }


  event_number = 0;
  run_in_progress = true;

  cm_msg(MLOG, "begin of run", "Completed successfully");

  std::cout << "Finished begin of run routine" << std::endl;

  return SUCCESS;
}

//--- End of Run -------------------------------------------------------------//
INT end_of_run(INT run_number, char *error)
{
  // Make sure we write the ROOT data.       
  if (run_in_progress && write_root) {

    pt_norm->Write();

    pf->Write();
    pf->Close();

    delete pf;
  }

  run_in_progress = false;

  std::cout << "Finished end of run routine" << std::endl;
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

  //Grab the database handle                             
  cm_get_experiment_database(&hDB, NULL);

  //initialize MIDAS bank
  bk_init32(pevent);

  sprintf(bk_name, "SCCS");
  bk_create(pevent, bk_name, TID_DOUBLE, (void **)&pdata);

  //reset arrays to hold data sent from beaglebones
  for(int i=0;i<nCoils;i++){
    bot_currents[i]=0;
    top_currents[i]=0;
    bot_temps[i] = 0;
    top_temps[i] = 0;
  }

  //Receive values from beaglebones
  std::vector<json> dataVector;
  bool st = false; //status of request/reply
  zmq::message_t bbVals;

  while (!st) {
   try {
     if (!subscriber.recv(&bbVals)) {
       //std::cout << "timeout" << std::endl;
       return 0;
     } else {
       st = true;
     }
   } catch (zmq::error_t& err) {
    if (err.num() != EINTR) {
      throw;
    }
    std::cout << "interrupted by midas, or something else" << std::endl;
   }
  }
  //   //    break;
  //   std::cout << "no message, returning" << std::endl;
  //   return 0;
  //  }	
  // }

  string s(static_cast<char*>(bbVals.data()));
  s.resize(bbVals.size());
  //std::cout << s << std::endl;
  json reply_data = json::parse(s);
  dataVector.push_back(reply_data);
	
  //cm_msg(MINFO, "read_surface_coils", "Received reply from beaglebones");
 
  //Process the data in the vector. Loops through all json objects. 
  //In the end, only most recent data is stored in array
  for (auto &reply_data : dataVector) { 

    for(json::iterator it = reply_data.begin(); it != reply_data.end(); ++it){

      //loop through all entries in json object
      //First need to turn hw_id into sc_id 
      //Get element index (get ###-1 from A-### where A = T or B)
      //Figure out if it is top or bottom
      string coil_id = rev_coil_map[it.key()];
      int coil_index = stoi(coil_id.substr(2,3).erase(0,coil_id.substr(2,3).find_first_not_of('0')))-1; //Get coil index without leading zeros
      string tb = coil_id.substr(0,1); //Top or bottom?
     
      if(tb == "B"){
	bot_currents[coil_index] = it.value()[0];
	bot_temps[coil_index] = it.value()[1];
      }
      else if(tb == "T"){
	top_currents[coil_index] = it.value()[0];
	top_temps[coil_index] = it.value()[1];
      }
      else std::cout << "PROBLEM! Neither a T nor a B!" << std::endl;

      }

    }
 
  //Get data ready for midas banks
  for(int idx = 0; idx < nCoils; ++idx){
    data.bot_sys_clock[idx] = hw::systime_us();
    data.top_sys_clock[idx] = hw::systime_us();
    data.bot_coil_currents[idx] = bot_currents[idx]; 
    data.top_coil_currents[idx] = top_currents[idx];
    data.bot_coil_temps[idx] = bot_temps[idx];
    data.top_coil_temps[idx] = top_temps[idx];
    data.bot_coil_temps[idx] = 27.0;
    data.top_coil_temps[idx] = 27.0;
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

  event_number++;

  //Update values in odb
  db_set_value(hDB, hkey, "/Equipment/Surface Coils/Settings/Monitoring/Currents/Bottom Currents", &bot_currents, sizeof(bot_currents), 100, TID_DOUBLE);
  db_set_value(hDB, hkey, "/Equipment/Surface Coils/Settings/Monitoring/Currents/Top Currents", &top_currents, sizeof(top_currents), 100, TID_DOUBLE);
  db_set_value(hDB, hkey, "/Equipment/Surface Coils/Settings/Monitoring/Temperatures/Bottom Temps", &bot_temps, sizeof(bot_temps), 100, TID_DOUBLE);
  db_set_value(hDB, hkey, "/Equipment/Surface Coils/Settings/Monitoring/Temperatures/Top Temps", &top_temps, sizeof(top_temps), 100, TID_DOUBLE);

  //Check values vs. set points/allowed difference and temperature
  for(int i=0;i<nCoils;i++){
    //bottom currents
    if(std::abs(bot_currents[i]-bot_set_values[i]) >= setPoint){
      current_health = 0;
      string bad_curr_string = coil_string("bot", i);
      db_set_value(hDB, hkey, "/Equipment/Surface Coils/Settings/Monitoring/Current Health", &current_health, sizeof(current_health), 1, TID_BOOL);
      db_set_value(hDB, hkey, "/Equipment/Surface Coils/Settings/Monitoring/Problem Channel", bad_curr_string.c_str(), sizeof(bad_curr_string.c_str()), 1, TID_STRING);
      cm_msg(MINFO, "read_surface_coils", "Bottom current out of spec");
    }
    
    //top currents                                                          
    if(std::abs(top_currents[i]-top_set_values[i]) >= setPoint){
      current_health = 0;
      string bad_curr_string = coil_string("top", i);
      db_set_value(hDB, hkey, "/Equipment/Surface Coils/Settings/Monitoring/Current Health", &current_health, sizeof(current_health), 1, TID_BOOL);
      db_set_value(hDB, hkey, "/Equipment/Surface Coils/Settings/Monitoring/Problem Channel", bad_curr_string.c_str(), sizeof(bad_curr_string.c_str()), 1, TID_STRING);
      cm_msg(MINFO, "read_surface_coils", "Top current out of spec");
    }

    //bottom temps                                                           
    if(bot_temps[i] > high_temp){
      temp_health = 0;
      string bad_temp_string = coil_string("bot", i);
      db_set_value(hDB, hkey, "/Equipment/Surface Coils/Settings/Monitoring/Temp Health", &temp_health, sizeof(temp_health), 1, TID_BOOL);
      db_set_value(hDB, hkey, "/Equipment/Surface Coils/Settings/Monitoring/Problem Channel", bad_temp_string.c_str(), sizeof(bad_temp_string.c_str()), 1, TID_STRING);
      cm_msg(MINFO, "read_surface_coils", "A bottom temp is too high!");
    }

    //top temps    
    if(top_temps[i] > high_temp){
      temp_health = 0;
      string bad_temp_string = coil_string("top", i);
      db_set_value(hDB, hkey, "/Equipment/Surface Coils/Settings/Monitoring/Temp Health", &temp_health, sizeof(temp_health), 1, TID_BOOL);
      db_set_value(hDB, hkey, "/Equipment/Surface Coils/Settings/Monitoring/Problem Channel", bad_temp_string.c_str(), sizeof(bad_temp_string.c_str()), 1, TID_STRING);
      cm_msg(MINFO, "read_surface_coils", "A top temp is too high!");
    }
  }
  
  cm_msg(MINFO, "read_surface_coils", "Finished generating event");
  return bk_size(pevent);

}

 
