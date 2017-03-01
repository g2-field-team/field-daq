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
#include <json.hpp>

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
  
  int event_number = 0;
  bool write_midas = true;
  bool write_root = true;

  //for zmq
  zmq::context_t context(1);
  zmq::socket_t requester1(context, ZMQ_REQ); //for sending set point currents
  zmq::socket_t requester2(context, ZMQ_REQ);
  zmq::socket_t requester3(context, ZMQ_REQ);
  zmq::socket_t requester4(context, ZMQ_REQ);
  zmq::socket_t requester5(context, ZMQ_REQ);
  zmq::socket_t requester6(context, ZMQ_REQ);
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

  //Grab the database handle
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

  db_get_data(hDB, hkey, &bot_set_values, &bot_size, TID_DOUBLE);

  //Top                                                                                      
  db_find_key(hDB, 0, "/Equipment/Surface Coils/Settings/Top Set Currents", &hkey);

  if(hkey == NULL){
    cm_msg(MERROR, "begin_of_run", "unable to find Top Set Currents key");
  }

  int top_size = sizeof(top_set_values);

  db_get_data(hDB, hkey, &top_set_values, &top_size, TID_DOUBLE);

  //Get the allowable difference                                                            
  db_find_key(hDB, 0, "/Equipment/Surface Coils/Settings/Allowed Difference", &hkey);

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
    request[coil_map[botString]] = bot_set_values[i-1];
    request[coil_map[topString]] = top_set_values[i-1];
  }
  
  //bind to server                                                         
  cm_msg(MINFO, "init", "Binding to servers");                                
  requester1.connect("tcp://127.0.0.1:5551");                                               
  requester2.connect("tcp://127.0.0.1:5552");
  requester3.connect("tcp://127.0.0.1:5553");
  requester4.connect("tcp://127.0.0.1:5554");
  requester5.connect("tcp://127.0.0.1:5555");
  requester6.connect("tcp://127.0.0.1:5556");

  //send data to driver boards                                                 
  std::string buffer = request.dump();
  zmq::message_t message1 (buffer.size());
  std::copy(buffer.begin(), buffer.end(), (char *)message1.data());
  zmq::message_t message2 (buffer.size());
  std::copy(buffer.begin(), buffer.end(), (char *)message2.data());
  zmq::message_t message3 (buffer.size());
  std::copy(buffer.begin(), buffer.end(), (char *)message3.data());
  zmq::message_t message4 (buffer.size());
  std::copy(buffer.begin(), buffer.end(), (char *)message4.data());
  zmq::message_t message5 (buffer.size());
  std::copy(buffer.begin(), buffer.end(), (char *)message5.data());
  zmq::message_t message6 (buffer.size());
  std::copy(buffer.begin(), buffer.end(), (char *)message6.data());
  
  zmq::message_t reply1(4096);
  zmq::message_t reply2(4096);
  zmq::message_t reply3(4096);
  zmq::message_t reply4(4096);
  zmq::message_t reply5(4096);
  zmq::message_t reply6(4096);

  bool st1 = false; //status of request/reply for crate 1
  bool st2 = false;
  bool st3 = false;
  bool st4 = false;
  bool st5 = false;
  bool st6 = false;

  do{
    try{
      st1 = requester1.send(message1, ZMQ_DONTWAIT);
      usleep(200);
    } catch(const zmq::error_t e1) {std::cout << "Problem sending to crate 1" << std::endl;};
    try{
      st2 = requester2.send(message2, ZMQ_DONTWAIT);
      usleep(200);
    } catch(const zmq::error_t e2) {std::cout << "Problem sending to crate 2" << std::endl;};
    try{
      st3 = requester3.send(message3, ZMQ_DONTWAIT);
      usleep(200);
    } catch(const zmq::error_t e3) {std::cout << "Problem sending to crate 3" << std::endl;};
    try{
      st4 = requester4.send(message4, ZMQ_DONTWAIT);
      usleep(200);
    } catch(const zmq::error_t e4) {std::cout << "Problem sending to crate 4" << std::endl;};
    try{
      st5 = requester5.send(message5, ZMQ_DONTWAIT);
      usleep(200);
    } catch(const zmq::error_t e5) {std::cout << "Problem sending to crate 5" << std::endl;};
    try{
      st6 = requester6.send(message6, ZMQ_DONTWAIT);
      usleep(200);
    } catch(const zmq::error_t e6) {std::cout << "Problem sending to crate 6" << std::endl;};

    cm_msg(MINFO, "begin_of_run", "Current values sent to beaglebones");         
   
    if(st1 == true){                                                            
      do{	
	try{
	  st1 = requester1.recv(&reply1, ZMQ_DONTWAIT | ZMQ_NOBLOCK);
	  usleep(200);
	} catch(const zmq::error_t e11) {std::cout << "Prob response crate 1" << std::endl;};
      } while(!st1);                                                            
    }
    if(st2 == true){
      do{
        try{
          st2 = requester2.recv(&reply2, ZMQ_DONTWAIT | ZMQ_NOBLOCK);
          usleep(200);
        } catch(const zmq::error_t e12) {std::cout << "Prob response crate 2" << std::endl;};
      } while(!st2);
    }
    if(st3 == true){
      do{
        try{
          st3 = requester3.recv(&reply3, ZMQ_DONTWAIT | ZMQ_NOBLOCK);
          usleep(200);
        } catch(const zmq::error_t e13) {std::cout << "Prob response crate 3" << std::endl;};
      } while(!st3);
    }
    if(st4 == true){
      do{
        try{
          st4 = requester4.recv(&reply4, ZMQ_DONTWAIT | ZMQ_NOBLOCK);
          usleep(200);
        } catch(const zmq::error_t e14) {std::cout << "Prob response crate 4" << std::endl;};
      } while(!st4);
    }
    if(st5 == true){
      do{
        try{
          st5 = requester5.recv(&reply5, ZMQ_DONTWAIT | ZMQ_NOBLOCK);
          usleep(200);
        } catch(const zmq::error_t e15) {std::cout << "Prob response crate 5" << std::endl;};
      } while(!st5);
    }
    if(st6 == true){
      do{
        try{
          st6 = requester6.recv(&reply6, ZMQ_DONTWAIT | ZMQ_NOBLOCK);
          usleep(200);
        } catch(const zmq::error_t e16) {std::cout << "Prob response crate 6" << std::endl;};
      } while(!st6);
    }
                                                                          
    } while(!st1 && !st2 && !st3 && !st4 && !st5 && !st6);

  cm_msg(MINFO, "begin_of_run", "Received replies from beaglebones");
  //Wait for response that currents were sent                       
  //Expects back the read out currents of the channels that were set
  string s1(static_cast<char*>(reply1.data()));
  s1.resize(reply1.size());
  string s2(static_cast<char*>(reply2.data()));
  s2.resize(reply2.size());
  string s3(static_cast<char*>(reply3.data()));
  s3.resize(reply3.size());
  string s4(static_cast<char*>(reply4.data()));
  s4.resize(reply4.size());
  string s5(static_cast<char*>(reply5.data()));
  s5.resize(reply5.size());
  string s6(static_cast<char*>(reply6.data()));
  s6.resize(reply6.size());

  //Want to combine the strings into one json object
  //Need to get rid of brackets that would otherwise be in middle of combined string
  s1.replace(s1.size()-1,1,","); //replace ending "}" with ","
  s2.replace(0,1,""); //remove starting "{"
  s2.replace(s2.size()-1,1,",");
  s3.replace(0,1,"");
  s3.replace(s3.size()-1,1,",");
  s4.replace(0,1,"");
  s4.replace(s4.size()-1,1,",");
  s5.replace(0,1,"");
  s5.replace(s5.size()-1,1,",");
  s6.replace(0,1,"");

  string com_str = s1+s2+s3+s4+s5+s6;
  json reply_data = json::parse(com_str);
  
  //Check that the initial currents match the set points.
  //Data we have now is ordered as hw_id : current
  //To check vs setpoint need to use reverse_coil_map to get sc_id : current
  /*  std::cout << "Starting my check vs. setpoint" << std::endl;
  for(int i=1;i<nCoils+1;i++){
    string newCoilNum;
    if(i >= 0 && i <= 9) newCoilNum = "00" + std::to_string(i);
    if(i > 9 && i <= 99) newCoilNum = "0" + std::to_string(i);
    if(i==100) newCoilNum = std::to_string(i);
    string newTopString = "T-"+newCoilNum;
    string newBotString = "B-"+newCoilNum;
    std::cout << newBotString << std::endl;
    std::cout << coil_map[newBotString] << std::endl;
    //std::cout << reply_data[coil_map[newBotString]] << std::endl;
    //float botVal = float(reply_data[coil_map[newBotString]]);
    //float topVal = float(reply_data[coil_map[newTopString]]);
    std::cout << "AM I WORKING HERE" << std::endl;
    //if(std::abs(botVal - bot_set_values[i-1]) > setPoint) std::cout << "Problem with " << botString << std::endl;
    //if(std::abs(topVal - top_set_values[i-1]) > setPoint) std::cout << "Problem with " << topString << std::endl;
    }*/



  //Now bind subscriber to receive data being pushed by beaglebones         
  std::cout << "Binding to subscribe socket" << std::endl;
  subscriber.bind("tcp://127.0.0.1:5557");    
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
    pt_norm = new TTree("t_scc", "Sim Surface Coil Data");
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
  requester1.disconnect("tcp://127.0.0.1:5551");
  requester2.disconnect("tcp://127.0.0.1:5552");
  requester3.disconnect("tcp://127.0.0.1:5553");
  requester4.disconnect("tcp://127.0.0.1:5554");
  requester5.disconnect("tcp://127.0.0.1:5555");
  requester6.disconnect("tcp://127.0.0.1:5556");
  subscriber.disconnect("tcp://127.0.0.1:5557");
  
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
  zmq::pollitem_t poller;
  poller.socket = (void *)subscriber; //Attaches pointer of socket to poller
  poller.events = ZMQ_POLLIN;
  std::vector<json> dataVector;
  bool st = false; //status of request/reply

  if(zmq::poll(&poller, 1, 1) == 0){return 0;} //if no data, exit loop

  while(zmq::poll(&poller, 1, 1) > 0){ //if we have data, process it
    zmq::message_t bbVals(4096);
    do{
      try{
	st = subscriber.recv(&bbVals, ZMQ_DONTWAIT | ZMQ_NOBLOCK);
	usleep(200);
      } catch(const zmq::error_t e) {};
   } while(!st);
   string s(static_cast<char*>(bbVals.data()));
   s.resize(bbVals.size());
   json reply_data = json::parse(s);
   dataVector.push_back(reply_data);
  }
  cm_msg(MINFO, "read_surface_coils", "Received reply from beaglebone 1");
 
  //Process the data in the vector. Loops through all json objects. 
  //In the end, only most recent data is stored in array
  for (auto &reply_data : dataVector) { 
    for(json::iterator it = reply_data.begin(); it != reply_data.end(); ++it){ 
      //loop through all entries in json object
      //First need to turn hw_id into sc_id. 
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

 
