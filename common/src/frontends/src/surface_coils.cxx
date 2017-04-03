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
#include "field_structs.hh"

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
       { 1, 0,         //event ID, trigger mask
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

//Anonymous namespace for "globals"
namespace{
  
  signed int event_number = 0;
  bool write_midas = true;
  bool write_root = true;

  //for zmq
  zmq::context_t context(1);
  zmq::socket_t publisher(context, ZMQ_PUB); //for sending set point currents
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

  Double_t bot_offsets[nCoils];
  Double_t top_offsets[nCoils];

  Double_t bot_currents[nCoils];
  Double_t top_currents[nCoils];

  Double_t bot_temps[nCoils];
  Double_t top_temps[nCoils];
  
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
 
  //Get the offset values
  db_find_key(hDB, 0, "/Equipment/Surface Coils/Settings/Offset Values/Bottom Offset Values", &hkey);
 
   if(hkey == NULL){
    cm_msg(MERROR, "begin_of_run", "unable to find Bottom Offset Values key");
  }

  int bot_off_size = sizeof(bot_offsets);
  db_get_data(hDB, hkey, &bot_offsets, &bot_off_size, TID_DOUBLE); 

  db_find_key(hDB, 0, "/Equipment/Surface Coils/Settings/Offset Values/Top Offset Values", &hkey);

   if(hkey == NULL){
    cm_msg(MERROR, "begin_of_run", "unable to find Top Offset Values key");
  }

  int top_off_size = sizeof(top_offsets);
  db_get_data(hDB, hkey, &top_offsets, &top_off_size, TID_DOUBLE);


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

  //Top                                                                                      
  db_find_key(hDB, 0, "/Equipment/Surface Coils/Settings/Set Points/Top Set Currents", &hkey);

  if(hkey == NULL){
    cm_msg(MERROR, "begin_of_run", "unable to find Top Set Currents key");
  }

  int top_size = sizeof(top_set_values);

  db_get_data(hDB, hkey, &top_set_values, &top_size, TID_DOUBLE);

  std::cout << "TESTTESTTEST" << std::endl;
  for(int i=0;i<10;i++) std::cout << bot_set_values[i] << " " << top_set_values[i] << std::endl;
  //Get the allowable difference                                                            
  db_find_key(hDB, 0, "/Equipment/Surface Coils/Settings/Set Points/Allowed Difference", &hkey);

  setPoint = 0;
  int setpt_size = sizeof(setPoint);

  db_get_data(hDB, hkey, &setPoint, &setpt_size, TID_DOUBLE);


  //Now that we have the data, package it into json object
  if(coil_map.size()!=200) cm_msg(MERROR, "begin_of_run", "Coil map is of wrong size");
  for(int i=1;i<nCoils+1;i++){
    string coilNum;
    if(i >= 0 && i <= 9) coilNum = "00" + std::to_string(i);
    if(i > 9 && i <= 99) coilNum = "0" + std::to_string(i);
    if(i==100) coilNum = std::to_string(i);
    string topString = "T-"+coilNum;
    string botString = "B-"+coilNum;
    request[coil_map[botString]] = bot_set_values[i-1]-bot_offsets[i-1];
    request[coil_map[topString]] = top_set_values[i-1]-top_offsets[i-1];
  }
  
  //bind to server
  cm_msg(MINFO, "init", "Binding to server");
  publisher.bind("tcp://127.0.0.1:5550");
  usleep(200);

  //send data to driver boards                                                 
  for(int i=0;i<5;i++){
    //bind to server                           
    //cm_msg(MINFO, "init", "Binding to server");
    //publisher.bind("tcp://127.0.0.1:5550");      
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

    /*do{
      try{
	st = publisher.send(message, ZMQ_DONTWAIT);
	usleep(200);
	std::cout << "Sent a message" << std::endl;
      } catch(const zmq::error_t e1) {std::cout << "Problem publishing" << std::endl;};
    } while(!st); */
    
    usleep(500000);
    //publisher.disconnect("tcp://127.0.0.1:5550");
  }//end for loop

  cm_msg(MINFO, "begin_of_run", "Current values sent to beaglebones");            
 
  //Now bind subscriber to receive data being pushed by beaglebones         
  std::cout << "Binding to subscribe socket" << std::endl;
  subscriber.bind("tcp://127.0.0.1:5551");    
  cm_msg(MINFO, "info", "Bound to subscribe socket");
  std::cout << "Bound" << std::endl;
                                     
  //Subscribe to all incoming data                                             
  subscriber.setsockopt(ZMQ_SUBSCRIBE, "", 0);        

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

  return SUCCESS;
}

//--- End of Run -------------------------------------------------------------//
INT end_of_run(INT run_number, char *error)
{
  //Disconnect the sockets. Necessary so that next run starts properly
  publisher.disconnect("tcp://127.0.0.1:5550");
  subscriber.disconnect("tcp://127.0.0.1:5551");
  
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
  std::cout << "Made it into readout routine" << std::endl;
  static unsigned long long num_events;
  static unsigned long long events_written;

 // std::cout << "Binding to subscribe socket" << std::endl;
 // subscriber.bind("tcp://127.0.0.1:5551");    
 // cm_msg(MINFO, "info", "Bound to subscribe socket");
 // std::cout << "Bound" << std::endl;
                                     
  //Subscribe to all incoming data                                             
  //subscriber.setsockopt(ZMQ_SUBSCRIBE, "", 0);


  HNDLE hDB, hkey;
  char bk_name[10]; //bank name
  DWORD *pdata; //place to store data
  
  //initialize MIDAS bank
  bk_init32(pevent);

  sprintf(bk_name, "SCCS");
  bk_create(pevent, bk_name, TID_DOUBLE, (void **)&pdata);

  //reset arrays to hold currents
  for(int i=0;i<nCoils;i++){
    bot_currents[i]=0;
    top_currents[i]=0;
    bot_temps[i] = 0;
    top_temps[i] = 0;
  }

  //Receive values from beaglebones
  //zmq::pollitem_t poller;
  //poller.socket = (void *)subscriber; //Attaches pointer of socket to poller
  //poller.events = ZMQ_POLLIN;
  std::vector<json> dataVector;
  bool st = false; //status of request/reply
  zmq::message_t bbVals;

  while (!st) {
   try {
    st = subscriber.recv(&bbVals);
   } catch (zmq::error_t& err) {
    if (err.num() != EINTR) {
      throw;
    }
   }	
  }

  string s(static_cast<char*>(bbVals.data()));
  s.resize(bbVals.size());
  std::cout << s << std::endl;
  json reply_data = json::parse(s);
  dataVector.push_back(reply_data);

  //if(zmq::poll(&poller, 1, 1) == 0){return 0;} //if no data, exit loop

  /*while(zmq::poll(&poller, 1, 1) > 0){ //if we have data, process it
    zmq::message_t bbVals(4096);
    do{
      try{
	st = subscriber.recv(&bbVals, ZMQ_DONTWAIT | ZMQ_NOBLOCK);
	usleep(200);
      } catch(const zmq::error_t e) {};
   } while((!st) && (zmq_errno() == EINTR));
   string s(static_cast<char*>(bbVals.data()));
   s.resize(bbVals.size());
   std::cout << s << std::endl;
   json reply_data = json::parse(s);
   dataVector.push_back(reply_data);
  }*/

	
  cm_msg(MINFO, "read_surface_coils", "Received reply from beaglebones");
 
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
	bot_temps[coil_index] = it.value()[1];
      }
      else std::cout << "PROBLEM! Neither a T nor a B!" << std::endl;

      }

    }
 

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
  /*
  //Check values vs set points. Set a flag if something drifts out of range
  if(event_number % 10 == 0){
    //json reset_request;
    
    for(int i=0;i<nCoils;i++){ //Check all the values. Fill json with problem channels

      string coilNum;
      if(i >= 0 && i <= 8) coilNum = "00" + std::to_string(i+1);
      if(i > 8 && i <= 98) coilNum = "0" + std::to_string(i+1);
      if(i==99) coilNum = std::to_string(i+1);
      string topString = "T-"+coilNum;
      string botString = "B-"+coilNum;

      if(abs(bot_currents[i]- bot_set_values[i]) > setPoint){
	cm_msg(MINFO, "read_surface_coils", "Problem  with %s, out of range", botString.c_str());
	//reset_request[coil_map[botString]] = bot_set_values[i];
      }
      if(abs(top_currents[i]- top_set_values[i]) > setPoint){
	cm_msg(MINFO, "read_surface_coils", "Problem  with %s, out of range", topString.c_str());
        //reset_request[coil_map[topString]] = top_set_values[i];
      }

    }
    }*/
    /*
    //Publish json of problem channels
    for(int i=0;i<5;i++){
      std::string buffer = reset_request.dump();
      zmq::message_t message (buffer.size());
      std::copy(buffer.begin(), buffer.end(), (char *)message.data());

      bool st = false; //status of publishing                                   

      do{
	try{
	  st = publisher.send(message, ZMQ_DONTWAIT);
	  usleep(200);
	} catch(const zmq::error_t e1) {std::cout << "Problem publishing" << std::endl;};
      } while(!st);

    }
  }
  */
  event_number++;

  //subscriber.disconnect("tcp://127.0.0.1:5551");

  cm_msg(MINFO, "read_surface_coils", "Finished generating event");
  return bk_size(pevent);

}

 
