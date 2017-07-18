/******************************************************************************

Name: surface_coils.cxx
Author: Rachel E. Osofsky
Email: osofskyr@uw.edu

About: Implements a MIDAS frontend for the surface coils. During initialization 
       gets the current set points from the ODB and sends a message to each
       beaglebone, which in turn sets the currents. Then, in a thread, routinely 
       checks the current set points and determines whether they have changed. 
       If so, sends a new message to all beaglebones with new currents. Otherwise 
       listens for read-back values of currents and temperatures, records these 
       values, and checks versus set points.

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
#include <thread>
#include <mutex>
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

//definition used for json objects
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
  INT read_surface_coils(char *pevent, INT c);
  INT poll_event(INT source, INT count, BOOL test);
  INT interrupt_configure(INT cmd, INT source, POINTER_T adr);

  void ReadCurrents(); //Thread function
  std::string coil_string(string loc, int i); //Return a string like T-###

  //Equipment list

  EQUIPMENT equipment[] = 
    {
      {FRONTEND_NAME,  //equipment name
       { EVENTID_SURFACE_COIL, 0,         //event ID, trigger mask
	 "SYSTEM",     //event bugger (use to be SYSTEM)
	 EQ_POLLED,  //equipment type
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

/*
Function: std::string coil_string(string loc, int i)

Inputs: Takes a string "bot" or "top" to determine whether the coil is a top or bottom coil.
        Takes an int i, where i is the array index

Outputs: Returns a string like B-### or T-### where ###=i+1 padded by zeros
*/

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

  //ZMQ setup
  //Requesters are part of a request-reply pair. Send set points to beaglebones, expect
  //a response that message was received
  //Subscriber listens for messages from all beaglebones. Message are sent by publishers
  //on each beaglebone
  zmq::context_t context(1);
  zmq::socket_t requester1(context, ZMQ_REQ);
  zmq::socket_t requester2(context, ZMQ_REQ);
  zmq::socket_t requester3(context, ZMQ_REQ);
  zmq::socket_t requester4(context, ZMQ_REQ);
  zmq::socket_t requester5(context, ZMQ_REQ);
  zmq::socket_t requester6(context, ZMQ_REQ);
  zmq::socket_t subscriber(context, ZMQ_SUB);

  std::thread read_thread;
  std::mutex mlock;
  std::mutex globalLock;
  bool FrontendActive;

  TFile *pf;
  TTree * pt_norm;

  std::atomic<bool> run_in_progress;

  boost::property_tree::ptree conf;
  
  g2field::surface_coil_t data; //Surface coil data type defined in field_structs.hh

  const int nCoils = g2field::kNumSCoils; //Defined in field_constants.hh
  Double_t setPoint; //If difference between value and set point is larger than this, need to change the current. Value stored in odb
  Double_t compSetPoint; //For checking if setPoint has changed

  Double_t bot_set_values[nCoils];
  Double_t top_set_values[nCoils];
  Double_t bot_comp_values[nCoils]; //for comparison with set points
  Double_t top_comp_values[nCoils]; //for comparison with set points

  //Struct to store data sent back by beaglebones
  struct unpacked_data{
    Double_t bot_currents[nCoils];
    Double_t top_currents[nCoils];
    Double_t bot_temps[nCoils];
    Double_t top_temps[nCoils];
  };

  unpacked_data dataUnit;

  //Holds the data until it is written to file
  std::vector<unpacked_data> dataBuffer;

  Double_t high_temp; //Highest allowed temperature

  int current_health;
  int temp_health;

  //Maps between hardware labeling (###, crate-board-channel) and array labeling (Top or bottom, which index)
  std::map<std::string, std::string> coil_map; //sc_id, hw_id
  std::map<std::string, std::string> rev_coil_map;//hw_id, sc_id
  json request; //json object to hold set currents
 }

//--- Frontend Init ---------------------------------------------------------//
/*
Frontend init first resets all of the health parameters in the ODB so that everything starts in a 
healthy status. It then gets the hardware map from the ODB, as well as reads in the set points. It sets
up all of the zmq sockets and send out the initial messages to the beaglebones. At the end of the function
the thread is started.
*/

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

  //Start with all healths being good, no problem channels      
  mlock.lock();

  current_health = 1;
  temp_health = 1;
  std::string blank = "";

  db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Current Health", &current_health, sizeof(current_health), 1, TID_BOOL);
  db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Temp Health", &temp_health, sizeof(temp_health), 1, TID_BOOL);
  db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Problem Current Channel", blank.c_str(), sizeof(blank.c_str()), 1, TID_STRING);
  db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Problem Temp Channel", blank.c_str(), sizeof(blank.c_str()), 1, TID_STRING);

  mlock.unlock();

  //Get the hardware/array map from ODB
  mlock.lock();
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
  mlock.unlock();

  mlock.lock();
  const int SIZE = 512; 

  //Get bottom set currents
  char arr_path_bot[SIZE]; 
  int bot_size = sizeof(bot_set_values);
  sprintf(arr_path_bot,"/Equipment/Surface Coils/Settings/Set Points/Bottom Set Currents");
  db_get_value(hDB,0,arr_path_bot,bot_set_values,&bot_size,TID_DOUBLE,0);

  //Get top set currents
  char arr_path_top[SIZE];
  int top_size = sizeof(top_set_values);
  sprintf(arr_path_top,"/Equipment/Surface Coils/Settings/Set Points/Top Set Currents");
  db_get_value(hDB,0,arr_path_top,top_set_values,&top_size,TID_DOUBLE,0);

  //Get the allowable difference
  char path_dif[SIZE];    
  int dif_size = sizeof(setPoint);                                                                        
  sprintf(path_dif,"/Equipment/Surface Coils/Settings/Set Points/Allowed Difference");               
  db_get_value(hDB,0,path_dif,&setPoint,&dif_size,TID_DOUBLE,0); 
       
  //Get the highest allowed temp       
  char path_temp[SIZE];
  int temp_size = sizeof(high_temp);
  sprintf(path_temp,"/Equipment/Surface Coils/Settings/Set Points/Allowed Temperature");
  db_get_value(hDB,0,path_temp,&high_temp,&temp_size,TID_DOUBLE,0);

  mlock.unlock();
 
  
  //Now that we have the current set points, package them into a json object to be sent to beaglebones  
  mlock.lock();
  request["000"] = setPoint;            
  if(coil_map.size()!=200) cm_msg(MERROR, "begin_of_run", "Coil map is of wrong size");                                                                      

  for(int i=1;i<nCoils+1;i++){                                

    string coilNum;                                             
    if(i >= 0 && i <= 9) coilNum = "00" + std::to_string(i);        
    if(i > 9 && i <= 99) coilNum = "0" + std::to_string(i);               
    if(i==100) coilNum = std::to_string(i);                        
 
    string topString = "T-"+coilNum;                           
    string botString = "B-"+coilNum;                                 
                              
    Double_t bot_val = bot_set_values[i-1];                     
    Double_t top_val = top_set_values[i-1];                         

    request[coil_map[botString]] = bot_val;                           
    request[coil_map[topString]] = top_val;         

  }  
  mlock.unlock();

  //Set all requesters to have no linger time
  //Set all requesters to time out after 2 seconds
  //Bind to the correct ports
  std::cout << "Binding to beaglebone 1" << std::endl;
  requester1.setsockopt(ZMQ_LINGER, 0);     
  requester1.setsockopt(ZMQ_RCVTIMEO, 60000);           
  requester1.bind("tcp://*:5551");               
  cm_msg(MINFO, "init", "Bound to beaglebone 1"); 
  std::cout << "Bound to beaglebone 1" << std::endl;

  requester2.setsockopt(ZMQ_LINGER, 0);
  requester2.setsockopt(ZMQ_RCVTIMEO, 60000);
  requester2.bind("tcp://*:5552");
  cm_msg(MINFO, "init", "Bound to beaglebone 2");
  std::cout << "Bound to beaglebone 2" << std::endl;

  requester3.setsockopt(ZMQ_LINGER, 0);
  requester3.setsockopt(ZMQ_RCVTIMEO, 60000);
  requester3.bind("tcp://*:5553");
  cm_msg(MINFO, "init", "Bound to beaglebone 3");
  std::cout << "Bound to beaglebone 3" << std::endl;

  requester4.setsockopt(ZMQ_LINGER, 0);  
  requester4.setsockopt(ZMQ_RCVTIMEO, 60000);         
  requester4.bind("tcp://*:5554");                
  cm_msg(MINFO, "init", "Bound to beaglebone 4");
  std::cout << "Bound to beaglebone 4" << std::endl;

  requester5.setsockopt(ZMQ_LINGER, 0); 
  requester5.setsockopt(ZMQ_RCVTIMEO, 60000);      
  requester5.bind("tcp://*:5555");             
  cm_msg(MINFO, "init", "Bound to beaglebone 5");
  std::cout << "Bound to beaglebone 5 " << std::endl;
  
  requester6.setsockopt(ZMQ_LINGER, 0);      
  requester6.setsockopt(ZMQ_RCVTIMEO, 60000);     
  requester6.bind("tcp://*:5556");               
  cm_msg(MINFO, "init", "Bound to beaglebone 6"); 
  std::cout << "Bound to beaglebone 6" << std::endl;

  //Now bind subscriber to receive data being pushed by beaglebones    
  //Subscribe to all incoming data 
  //Set to have no linger time
  //Set maximum time to wait to receive a message to 5 seconds

  subscriber.setsockopt(ZMQ_SUBSCRIBE, "", 0);
  subscriber.setsockopt(ZMQ_LINGER, 0);
  subscriber.setsockopt(ZMQ_RCVTIMEO, 120000);
  subscriber.bind("tcp://*:5550");
  cm_msg(MINFO,"init","Bound to surface coil subscribe socket");
  std::cout << "Bound to subscribe socket" << std::endl;

  //Send data to driver boards:                         
  //Define a zmq message and fill it with the contents of buffer
  //Send the message
  //Define a zmq reply and wait for a response. If no reply received, return an error
  
  std::string buffer = request.dump();              

  //Send messages
  //message 1 
  zmq::message_t message1 (buffer.size());                 
  std::copy(buffer.begin(), buffer.end(), (char *)message1.data());   
  requester1.send(message1);                                
                                                       
  //message 2
  zmq::message_t message2 (buffer.size());
  std::copy(buffer.begin(), buffer.end(), (char *)message2.data());
  requester2.send(message2);
  
  //message 3
  zmq::message_t message3 (buffer.size());                   
  std::copy(buffer.begin(), buffer.end(), (char *)message3.data());      
  requester3.send(message3);                  
  
  //message 4
  zmq::message_t message4 (buffer.size());                    
  std::copy(buffer.begin(), buffer.end(), (char *)message4.data());  
  requester4.send(message4);                 
                                                                      
  //message 5
  zmq::message_t message5 (buffer.size());                 
  std::copy(buffer.begin(), buffer.end(), (char *)message5.data());   
  requester5.send(message5);                       
                                                                 
  //message 6
  zmq::message_t message6 (buffer.size());  
  std::copy(buffer.begin(), buffer.end(), (char *)message6.data());  
  requester6.send(message6);   
  
  //Receive messages
  zmq::message_t reply1;
  if(!requester1.recv(&reply1)){
    cm_msg(MINFO, "frontend_init", "Crate 1 never responded");
    return FE_ERR_HW;
  }
  cm_msg(MINFO, "frontend_init", "set points were received by crate 1");

  zmq::message_t reply2;
  if(!requester2.recv(&reply2)){
    cm_msg(MINFO, "frontend_init", "Crate 2 never responded");
    return FE_ERR_HW;
  }
  cm_msg(MINFO, "frontend_init", "set points were received by crate 2");
  
  zmq::message_t reply3;
  if(!requester3.recv(&reply3)){
    cm_msg(MINFO, "frontend_init", "Crate 3 never responded");
    return FE_ERR_HW;
  }
  cm_msg(MINFO, "frontend_init", "set points were received by crate 3");

  zmq::message_t reply4;
  if(!requester4.recv(&reply4)){
    cm_msg(MINFO, "frontend_init", "Crate 4 never responded");
    return FE_ERR_HW;
  }
  cm_msg(MINFO, "frontend_init", "set points were received by crate 4");

  zmq::message_t reply5;
  if(!requester5.recv(&reply5)){
    cm_msg(MINFO, "frontend_init", "Crate 5 never responded");
    return FE_ERR_HW;
  }
  cm_msg(MINFO, "frontend_init", "set points were received by crate 5");

  zmq::message_t reply6;
  if(!requester6.recv(&reply6)){
    cm_msg(MINFO, "frontend_init", "Crate 6 never responded");
    return FE_ERR_HW;
  }
  cm_msg(MINFO, "frontend_init", "set points were received by crate 6");

  //cm_msg(MINFO, "begin_of_run", "Surface coil currents all set");
  
  //Set FrontendActive to True
  mlock.lock();
  FrontendActive = true;
  mlock.unlock();

  //Start the thread
  //Will now start receiving readback values from beaglebones and checking whether
  //set points have been updated in ODB
  read_thread = std::thread(ReadCurrents);
  cm_msg(MINFO, "init", "Surface coil thread started");

  globalLock.lock();
  run_in_progress = false;
  globalLock.unlock();

  cm_msg(MINFO, "init","Surface Coils initialization complete");
  
  return SUCCESS;
}

//--- Frontend Exit ---------------------------------------------------------//
INT frontend_exit()
{
  requester1.unbind("tcp://*:5551");
  requester2.unbind("tcp://*:5552");
  requester3.unbind("tcp://*:5553");
  requester4.unbind("tcp://*:5554");
  requester5.unbind("tcp://*:5555");
  requester6.unbind("tcp://*:5556");
  subscriber.unbind("tcp://*:5550");

  //Set FrontendActive to False. Will stop the thread
  mlock.lock();
  FrontendActive = false;
  mlock.unlock();

  //Join the threads
  read_thread.join();
  cm_msg(MINFO,"exit","Thread joined.");
  
  globalLock.lock();
  run_in_progress = false;
  globalLock.unlock();

  cm_msg(MINFO, "exit", "Surface Coils teardown complete");
   return SUCCESS;
}

//--- Begin of Run ----------------------------------------------------------//
/*
The main purpose of the begin of run function is to grab the run information from
the ODB. This includes the Runinfo, as well as information about whether to write
a root file, and if so, where to write it. If root data is being written, it also
sets up the root tree.
*/
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
  snprintf(str, sizeof(str), "/Equipment/Surface Coils/Settings/Root Directory");
  db_find_key(hDB, 0, str, &hkey);

  if (hkey) {
    size = sizeof(str);
    db_get_data(hDB, hkey, str, &size, TID_STRING);
    datadir = std::string(str);
    }

  // Set the filename       
  snprintf(str, sizeof(str), "surface_coil_run_%05d.root", runinfo.run_number);

  // Join the directory and filename using boost filesystem. 
  filename = (filesystem::path(datadir) / filesystem::path(str)).string();

  // Get the parameter for root output.  
  db_find_key(hDB, 0, "/Equipment/Surface Coils/Settings/Root Output", &hkey);

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
 
  globalLock.lock();
  event_number = 0;
  run_in_progress = true;
  globalLock.unlock();
  
  cm_msg(MLOG, "begin of run", "Completed successfully");
  
  return SUCCESS;
}

//--- End of Run -------------------------------------------------------------//
INT end_of_run(INT run_number, char *error)
{

  // Make sure we write the ROOT data.                                 
  if (run_in_progress && write_root) {                       
    mlock.lock();                             
    pt_norm->Write();                               
                                                                      
    pf->Write();                                         
    pf->Close();                                           
                                                                    
    delete pf;                        
    mlock.unlock();                                            
  }       

  globalLock.lock();
  run_in_progress = false;
  globalLock.unlock();

  cm_msg(MLOG, "end_of_run","Surface coil end of run completed successfully");
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
/*
Poll event function checks whether there is data to be written to file. If so,
returns "1" which prompts the frontend to enter the read_surface_coils function
which write the data to midas and, if told to do so, to root. 
*/
INT poll_event(INT source, INT count, BOOL test)
{
  //static bool triggered = false;

  //fake calibration
  if(test) {
    for(int i=0;i<count;i++){
      usleep(10);
    }
    return 0;
  }
  
  mlock.lock();
  BOOL check = dataBuffer.size()>1;
  mlock.unlock();
  if(check){
    //std::cout << "Data size, Triggered: " << dataBuffer.size() << std::endl;
    return 1;
  }
  else return 0;
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
/*
The read_surface_coils function writes the data. It reads the data from the first
entry in the dataBuffer and after writing this data, removes it from dataBuffer.
Writes to midas banks, and if told to do so, to the root tree.
*/
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

  //Get data ready for midas banks
  mlock.lock();
  for(int idx = 0; idx < nCoils; ++idx){
    data.bot_sys_clock[idx] = hw::systime_us();
    data.top_sys_clock[idx] = hw::systime_us();
    data.bot_coil_currents[idx] = dataBuffer[0].bot_currents[idx]; 
    data.top_coil_currents[idx] = dataBuffer[0].top_currents[idx];
    data.bot_coil_temps[idx] = dataBuffer[0].bot_temps[idx];
    data.top_coil_temps[idx] = dataBuffer[0].top_temps[idx];
   }

  //write root output
  if (write_root && run_in_progress) {
    //cm_msg(MINFO, "read_fixed_event", "Filling TTree");
    // Now that we have a copy of the latest event, fill the tree. 
    pt_norm->Fill();
    num_events++;

    if (num_events % 10 == 1) {
      //cm_msg(MINFO, frontend_name, "flushing TTree.");
      pt_norm->AutoSave("SaveSelf,FlushBaskets");
      pf->Flush();
    }

  }

  //Write midas output
  memcpy(pdata, &data, sizeof(data));
  pdata += sizeof(data) / sizeof(DWORD);

  bk_close(pevent, pdata);
  mlock.unlock();

  globalLock.lock();
  event_number++;
  globalLock.unlock();

  dataBuffer.erase(dataBuffer.begin());
  
  return bk_size(pevent); 
}

 
//Thread function
/*
First checks whether the frontend is active. If not, ends the thread.
Next gets the set points from the ODB and compares to the values it has currently
stored. If there is anything different, it copies these new values into memory
and produces a new json file of set points, which are then sent out to the beaglebones.
Listens for messages from beaglebones. When a message is received, parses the json
into a dataUnit and pushes the dataUnit into the dataBuffer, as well as updating the
values on the monitoring page of the ODB. Checks the currents and temperatures vs. set 
points/highest allowed temp. If anything is wrong, updates the health and bad channel 
strings in the ODB, which then prompt midas to throw an alarm.
*/
void ReadCurrents(){

  HNDLE hDB, hkey;
  char bk_name[10]; //bank name           
  DWORD *pdata; //place to store data                          

  //Grab the database handle                                  
  cm_get_experiment_database(&hDB, NULL);

  while(1){

    BOOL localFrontendActive;
    mlock.lock();
    localFrontendActive = FrontendActive;
    mlock.unlock();
    if(!localFrontendActive) break;

    mlock.lock();
    //TRY NEW METHOD                             
    const int SIZE = 512;                                                                                       
                                                                                                                         
    //Get bottom set currents                                                                   
    char arr_path_bot_comp[SIZE];                                                                              
    int bot_comp_size = sizeof(bot_comp_values);                                                             
    sprintf(arr_path_bot_comp,"/Equipment/Surface Coils/Settings/Set Points/Bottom Set Currents");          
    db_get_value(hDB,0,arr_path_bot_comp,bot_comp_values,&bot_comp_size,TID_DOUBLE,0);                                   
                                                                                                                         
    //Get top set currents                                                                                    
    char arr_path_top_comp[SIZE];                                                                                 
    int top_comp_size = sizeof(top_comp_values);                                                                    
    sprintf(arr_path_top_comp,"/Equipment/Surface Coils/Settings/Set Points/Top Set Currents");                    
    db_get_value(hDB,0,arr_path_top_comp,top_comp_values,&top_comp_size,TID_DOUBLE,0);           
                                                                                                                        
    //Get the allowable difference                                                                                      
    char path_comp_dif[SIZE];                                                 
    int dif_comp_size = sizeof(compSetPoint);                                                    
    sprintf(path_comp_dif,"/Equipment/Surface Coils/Settings/Set Points/Allowed Difference");        
    db_get_value(hDB,0,path_comp_dif,&compSetPoint,&dif_comp_size,TID_DOUBLE,0);                       
    mlock.unlock();

                                                                                                       
    //Compare these values to the previous set points. If any 1 current is different, update the actual set point arrays and send the values to the beaglebones. Because of timing, get set values again
    for(int i=0;i<nCoils;i++){
      if((fabs(bot_comp_values[i] - bot_set_values[i]) > 1e-6) || (fabs(top_comp_values[i] - top_set_values[i]) > 1e-6)){
	
	usleep(5000000);
	//Get bottom set currents                                                                                           
	char arr_path_bot_comp[SIZE];
	int bot_comp_size = sizeof(bot_comp_values);
	sprintf(arr_path_bot_comp,"/Equipment/Surface Coils/Settings/Set Points/Bottom Set Currents");
	db_get_value(hDB,0,arr_path_bot_comp,bot_comp_values,&bot_comp_size,TID_DOUBLE,0);

	//Get top set currents                                                                                                 
	char arr_path_top_comp[SIZE];
	int top_comp_size = sizeof(top_comp_values);
	sprintf(arr_path_top_comp,"/Equipment/Surface Coils/Settings/Set Points/Top Set Currents");
	db_get_value(hDB,0,arr_path_top_comp,top_comp_values,&top_comp_size,TID_DOUBLE,0);

	//Get the allowable difference                                                                        
	char path_comp_dif[SIZE];
	int dif_comp_size = sizeof(compSetPoint);
	sprintf(path_comp_dif,"/Equipment/Surface Coils/Settings/Set Points/Allowed Difference");
	db_get_value(hDB,0,path_comp_dif,&compSetPoint,&dif_comp_size,TID_DOUBLE,0);
	mlock.unlock();

	//Reset the value arrays
	for(int j=0;j<nCoils;j++){
	  bot_set_values[j] = bot_comp_values[j];
	  top_set_values[j] = top_comp_values[j];
	  setPoint = compSetPoint;
	}

	//Package message into json object
	json newRequest;
	newRequest["000"] = setPoint;

	for(int i=1;i<nCoils+1;i++){
	  string coilNum;
	  if(i >= 0 && i <= 9) coilNum = "00" + std::to_string(i);
	  if(i > 9 && i <= 99) coilNum = "0" + std::to_string(i);         
	  if(i==100) coilNum = std::to_string(i);

	  string topString = "T-"+coilNum;
	  string botString = "B-"+coilNum;

	  Double_t bot_val = bot_set_values[i-1];
	  Double_t top_val = top_set_values[i-1];

	  newRequest[coil_map[botString]] = bot_val;
	  newRequest[coil_map[topString]] = top_val;
	}

	//send data to driver boards                                            
	std::string buffer = newRequest.dump();

	//message 1
	zmq::message_t message1 (buffer.size());
	std::copy(buffer.begin(), buffer.end(), (char *)message1.data());
	requester1.send(message1);
	std::cout << "Sent the set points to crate 1" << std::endl;

	//message 2
	zmq::message_t message2 (buffer.size());
	  std::copy(buffer.begin(), buffer.end(), (char *)message2.data());
	  requester2.send(message2);
	std::cout << "Sent the set points to crate 2" << std::endl;
    
	//message 3
	zmq::message_t message3 (buffer.size());
	std::copy(buffer.begin(), buffer.end(), (char *)message3.data());
	requester3.send(message3);
	std::cout << "Sent the set points to crate 3" << std::endl;

	//message 4
	zmq::message_t message4 (buffer.size());
	std::copy(buffer.begin(), buffer.end(), (char *)message4.data());
	requester4.send(message4);
	std::cout << "Sent the set points to crate 4" << std::endl;

	//message 5
	zmq::message_t message5 (buffer.size());
	std::copy(buffer.begin(), buffer.end(), (char *)message5.data());
	requester5.send(message5);
	std::cout << "Sent the set points to crate 5" << std::endl;


	//message 6
	zmq::message_t message6 (buffer.size());
	std::copy(buffer.begin(), buffer.end(), (char *)message6.data());
	requester6.send(message6);
	std::cout << "Sent the set points to crate 6" << std::endl;

    	//Receive messages
	zmq::message_t reply1;
        if(!requester1.recv(&reply1)){
          cm_msg(MINFO, "ReadCurrents", "Crate 1 never responded");
          return FE_ERR_HW;
        }
        else std::cout << "set Points were received by crate 1" << std::endl; 

	zmq::message_t reply2;
        if(!requester2.recv(&reply2)){
          cm_msg(MINFO, "ReadCurrents", "Crate 2 never responded");
          return FE_ERR_HW;
        }
        else std::cout << "set Points were received by crate 2" << std::endl; 
	
        zmq::message_t reply3;
        if(!requester3.recv(&reply3)){
          cm_msg(MINFO, "ReadCurrents", "Crate 3 never responded");
          return FE_ERR_HW;
        }
        else std::cout << "set Points were received by crate 3" << std::endl; 

	zmq::message_t reply4;
        if(!requester4.recv(&reply4)){
          cm_msg(MINFO, "ReadCurrents", "Crate 4 never responded");
          return FE_ERR_HW;
        }
        else std::cout << "set Points were received by crate 4" << std::endl; 

	zmq::message_t reply5;
        if(!requester5.recv(&reply5)){
          cm_msg(MINFO, "ReadCurrents", "Crate 5 never responded");
          return FE_ERR_HW;
        }
        else std::cout << "set Points were received by crate 5" << std::endl; 

	zmq::message_t reply6;
	if(!requester6.recv(&reply6)){
	cm_msg(MINFO, "ReadCurrents", "Crate 6 never responded");
	return FE_ERR_HW;
	}
	else std::cout << "set Points were received by crate 6" << std::endl;

	break;
       }
    }

    //Receive values from beaglebones             
    bool st = false; //status of receiving data             
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
    mlock.unlock();

    string s(static_cast<char*>(bbVals.data()));
    s.resize(bbVals.size());
    //std::cout << s << std::endl;
    json reply_data = json::parse(s);

    //Process the data in the vector. Loops through all json objects.   
    //In the end, only most recent data is stored in array           
    for(json::iterator it = reply_data.begin(); it != reply_data.end(); ++it){
      //loop through all entries in json object     
      //First need to turn hw_id into sc_id        
      //Get element index (get ###-1 from A-### where A = T or B)   
      //Figure out if it is top or bottom                       
      string coil_id = rev_coil_map[it.key()];
      int coil_index = stoi(coil_id.substr(2,3).erase(0,coil_id.substr(2,3).find_first_not_of('0')))-1; //Get coil index without leading zeros   
      string tb = coil_id.substr(0,1); //Top or bottom?             

      if(tb == "B"){
	dataUnit.bot_currents[coil_index] = it.value()[0];
	dataUnit.bot_temps[coil_index] = it.value()[1];
      }
      else if(tb == "T"){
	dataUnit.top_currents[coil_index] = it.value()[0];
	dataUnit.top_temps[coil_index] = it.value()[1];
      }
      else std::cout << "PROBLEM! Neither a T nor a B!" << std::endl;
    }

    mlock.lock();
    dataBuffer.push_back(dataUnit);
    mlock.unlock();

    mlock.lock();
    //Update values in odb.
    db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Currents/Bottom Currents", &dataUnit.bot_currents, sizeof(dataUnit.bot_currents), 100, TID_DOUBLE);
    db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Temperatures/Bottom Temps", &dataUnit.bot_temps, sizeof(dataUnit.bot_temps), 100, TID_DOUBLE);
    db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Currents/Top Currents", &dataUnit.top_currents, sizeof(dataUnit.top_currents), 100, TID_DOUBLE);
    db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Temperatures/Top Temps", &dataUnit.top_temps, sizeof(dataUnit.top_temps), 100, TID_DOUBLE);
    mlock.unlock();
   

    //Check values vs. set points/allowed difference and temperature   
    //Will set alarm if something is bad
    string bad_curr_string = "";
    string bad_temp_string = "";

    mlock.lock(); 
    for(int i=0;i<nCoils;i++){
      //bottom currents            
      /*      if(std::abs(dataUnit.bot_currents[i]-bot_set_values[i]) >= setPoint && dataUnit.bot_currents[i]!=0.0){


	char str[256];
	current_health = 0;

	if(bad_curr_string == ""){ bad_curr_string = coil_string("bot", i);}
	else {bad_curr_string = bad_curr_string + ", " + coil_string("bot", i);}
	snprintf(str, 256, "%s", bad_curr_string.c_str());

	db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Current Health", &current_health, sizeof(current_health), 1, TID_BOOL);
	db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Problem Current Channel", str, sizeof(str), 1, TID_STRING);
	//cm_msg(MINFO, "read_surface_coils", "Bottom current out of spec");

	}*/


      //top currents                
      /*if(std::abs(dataUnit.top_currents[i]-top_set_values[i]) >= setPoint && dataUnit.top_currents[i]!=0.0){

	char str[256];
	current_health = 0;
	if(bad_curr_string == ""){ bad_curr_string = coil_string("top", i);}
	else {bad_curr_string = bad_curr_string + ", " + coil_string("top", i);}
	snprintf(str, 256, "%s", bad_curr_string.c_str());

	db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Current Health", &current_health, sizeof(current_health), 1, TID_BOOL);
	db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Problem Current Channel", str, sizeof(str), 1, TID_STRING);
	//cm_msg(MINFO, "read_surface_coils", "Top current out of spec");
	}*/

      //bottom temps       
      if(dataUnit.bot_temps[i] > high_temp){

	char str[256];
	temp_health = 0;
	if(bad_temp_string == ""){ bad_temp_string = coil_string("bot", i);}
	else {bad_temp_string = bad_temp_string + ", " + coil_string("bot", i);}
	snprintf(str, 256, "%s", bad_temp_string.c_str());

	db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Temp Health", &temp_health, sizeof(temp_health), 1, TID_BOOL);
	db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Problem Temp Channel", str, sizeof(str), 1, TID_STRING);
	cm_msg(MINFO, "read_surface_coils", "A bottom temp is too high!");
      }

      //top temps  
      if(dataUnit.top_temps[i] > high_temp){

	char str[256];
	temp_health = 0;
	if(bad_temp_string == ""){ bad_temp_string = coil_string("top", i);}
	else {bad_temp_string = bad_temp_string + ", " + coil_string("top", i);}
	snprintf(str, 256, "%s", bad_temp_string.c_str());

	db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Temp Health", &temp_health, sizeof(temp_health), 1, TID_BOOL);
	db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Problem Temp Channel", str, sizeof(str), 1, TID_STRING);
	cm_msg(MINFO, "read_surface_coils", "A top temp is too high!");
      }
    }
    mlock.unlock();


  }//end while(1) loop
}
