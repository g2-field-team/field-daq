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

void ReadCurrents(); //Function for thread

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
  zmq::socket_t requester1(context, ZMQ_REQ);
  //zmq::socket_t requester2(context, ZMQ_REQ);
  zmq::socket_t requester3(context, ZMQ_REQ);
  /*zmq::socket_t requester4(context, ZMQ_REQ);
  zmq::socket_t requester5(context, ZMQ_REQ);
  zmq::socket_t requester6(context, ZMQ_REQ);*/
  zmq::socket_t subscriber(context, ZMQ_SUB); //subscribe to data being sent back from beaglebones

  std::thread read_thread;
  std::mutex mlock;
  std::mutex globalLock;
  BOOL FrontendActive;

  TFile *pf;
  TTree * pt_norm;

  std::atomic<bool> run_in_progress;

  boost::property_tree::ptree conf;
  
  g2field::surface_coil_t data; //Surface coil data type defined in field_structs.hh

  const int nCoils = g2field::kNumSCoils; //Defined in field_constants.hh
  Double_t setPoint; //If difference between value and set point is larger than this, need to change the current. Value stored in odb
  Double_t compSetPoint;

  Double_t bot_set_values[nCoils];
  Double_t top_set_values[nCoils];
  Double_t bot_comp_values[nCoils]; //for comparison with set points
  Double_t top_comp_values[nCoils]; //for comparison with set points

  struct unpacked_data{
    Double_t bot_currents[nCoils];
    Double_t top_currents[nCoils];
    Double_t bot_temps[nCoils];
    Double_t top_temps[nCoils];
  };

  std::vector<unpacked_data> dataBuffer;

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

  //Start with all healths being good      
  mlock.lock();

  current_health = 1;
  temp_health = 1;
  std::string blank = "";

  db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Current Health", &current_health, sizeof(current_health), 1, TID_BOOL);
  db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Temp Health", &temp_health, sizeof(temp_health), 1, TID_BOOL);
  db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Problem Current Channel", blank.c_str(), sizeof(blank.c_str()), 1, TID_STRING);
  db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Problem Temp Channel", blank.c_str(), sizeof(blank.c_str()), 1, TID_STRING);

  mlock.unlock();

  mlock.lock();
  //Get the map from ODB
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

  //Get bottom set currents
  mlock.lock();
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
 
  mlock.unlock();

  //Now that we have the current set points, package them into json object  
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

  //bind to servers
  requester1.setsockopt(ZMQ_LINGER, 0);     
  requester1.setsockopt(ZMQ_RCVTIMEO, 2000);           
  requester1.bind("tcp://*:5551");               
  cm_msg(MINFO, "init", "Bound to beaglebone 1"); 
  std::cout << "Bound to beaglebone 1" << std::endl;

  //requester2.setsockopt(ZMQ_LINGER, 0);
  //requester2.setsockopt(ZMQ_RCVTIMEO, 2000);
  //requester2.bind("tcp://*:5552");
  //cm_msg(MINFO, "init", "Bound to beaglebone 2");

  requester3.setsockopt(ZMQ_LINGER, 0);
  requester3.setsockopt(ZMQ_RCVTIMEO, 2000);
  requester3.bind("tcp://*:5553");
  cm_msg(MINFO, "init", "Bound to beaglebone 3");
  std::cout << "Bound to beaglebone 3" << std::endl;

  //requester4.setsockopt(ZMQ_LINGER, 0);  
  //requester4.setsockopt(ZMQ_RCVTIMEO, 2000);         
  //requester4.bind("tcp://*:5554");                
  //cm_msg(MINFO, "init", "Bound to beaglebone 4");

  //requester5.setsockopt(ZMQ_LINGER, 0); 
  //requester5.setsockopt(ZMQ_RCVTIMEO, 2000);      
  //requester5.bind("tcp://*:5552");             
  //cm_msg(MINFO, "init", "Bound to beaglebone 5");

  //requester6.setsockopt(ZMQ_LINGER, 0);      
  //requester6.setsockopt(ZMQ_RCVTIMEO, 2000);     
  //requester6.bind("tcp://*:5556");               
  //cm_msg(MINFO, "init", "Bound to beaglebone 6"); 

  //Now bind subscriber to receive data being pushed by beaglebones    
  //Subscribe to all incoming data                                             
  subscriber.setsockopt(ZMQ_SUBSCRIBE, "", 0);
  subscriber.setsockopt(ZMQ_LINGER, 0);
  subscriber.setsockopt(ZMQ_RCVTIMEO, 5000);
  subscriber.bind("tcp://*:5550");
  std::cout << "Bound to subscribe socket" << std::endl;

  //send data to driver boards                         
  std::string buffer = request.dump();              
           
  //message 1                                  
  zmq::message_t message1 (buffer.size());                 
  std::copy(buffer.begin(), buffer.end(), (char *)message1.data());   
  requester1.send(message1);                                
                                                       
  zmq::message_t reply1;                     
  if(!requester1.recv(&reply1)){                       
    cm_msg(MINFO, "frontend_init", "Crate 1 never responded");       
    return FE_ERR_HW;                                 
  }                                                        
  cm_msg(MINFO, "frontend_init", "set points were received by crate 1");

  //message 2
  /*zmq::message_t message2 (buffer.size());
  std::copy(buffer.begin(), buffer.end(), (char *)message2.data());
  requester2.send(message2);
  
  zmq::message_t reply2;
  if(!requester2.recv(&reply2)){
    cm_msg(MINFO, "frontend_init", "Crate 2 never responded");
    return FE_ERR_HW;
  }
  cm_msg(MINFO, "frontend_init", "set points were received by crate 2");*/
                                                 
  //message 3
  zmq::message_t message3 (buffer.size());                   
  std::copy(buffer.begin(), buffer.end(), (char *)message3.data());      
  requester3.send(message3);                  
  
  zmq::message_t reply3;                   
  if(!requester3.recv(&reply3)){
    cm_msg(MINFO, "frontend_init", "Crate 3 never responded");
    return FE_ERR_HW;
   }                                                  
  cm_msg(MINFO, "frontend_init", "set points were received by crate 3");

  //message 4
  /*zmq::message_t message4 (buffer.size());                    
  std::copy(buffer.begin(), buffer.end(), (char *)message4.data());  
  requester4.send(message4);                 
                                                                      
  zmq::message_t reply4;                                       
  if(!requester4.recv(&reply4){                  
    cm_msg(MINFO, "frontend_init", "Crate 4 never responded");     
    return FE_ERR_HW;                          
  }                                                         
  cm_msg(MINFO, "frontend_init", "set points were received by crate 4");*/

  //message 5
  /*zmq::message_t message5 (buffer.size());                 
  std::copy(buffer.begin(), buffer.end(), (char *)message5.data());   
  requester5.send(message5);                       
                                                                 
  zmq::message_t reply5;           
  if(!requester5.recv(&reply5)){                 
    cm_msg(MINFO, "frontend_init", "Crate 5 never responded"); 
    return FE_ERR_HW;                              
  }                                            
  cm_msg(MINFO, "frontend_init", "set points were received by crate 5");*/

  //message 6
  /*zmq::message_t message6 (buffer.size());  
  std::copy(buffer.begin(), buffer.end(), (char *)message6.data());  
  requester6.send(message6);            
  
  zmq::message_t reply6;   
  if(!requester6.recv(&reply6)){                  
    cm_msg(MINFO, "frontend_init", "Crate 6 never responded");
    return FE_ERR_HW;               
  }                                                      
  cm_msg(MINFO, "frontend_init", "set points were received by crate 6");*/

  cm_msg(MINFO, "begin_of_run", "Surface coil currents all set");
  
  //Set FrontendActive to True
  mlock.lock();
  FrontendActive = true;
  mlock.unlock();

  //Start the read thread
  read_thread = std::thread(ReadCurrents);

  globalLock.lock();
  run_in_progress = false;
  globalLock.unlock();

  cm_msg(MINFO, "init","Surface Coils initialization complete");
  
  return SUCCESS;
}

//--- Frontend Exit ---------------------------------------------------------//
INT frontend_exit()
{
  //Set FrontendActive to False. Will stop the thread
  mlock.lock();
  FrontendActive = false;
  mlock.unlock();

  //Join the thread
  read_thread.join();
  cm_msg(MINFO,"exit","Thread joined.");
  
  globalLock.lock();
  run_in_progress = false;
  globalLock.unlock();

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
    //std::cout << "Trigger" << std::endl;
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

 
//Thread function!
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
  //Get the current odb values
  //Get bottom set currents
  db_find_key(hDB, 0, "/Equipment/Surface Coils/Settings/Set Points/Bottom Set Currents", &hkey);
  if(hkey == NULL){
    cm_msg(MERROR, "ReadCurrents", "unable to find Bottom Set Currents key");
  }

  for(int i=0;i<nCoils;i++){
    bot_comp_values[i] = 0;
    top_comp_values[i] = 0;
  }

  int bot_comp_size = sizeof(bot_comp_values);
  db_get_data(hDB, hkey, &bot_comp_values, &bot_comp_size, TID_DOUBLE);

  //Get top set currents            
  db_find_key(hDB, 0, "/Equipment/Surface Coils/Settings/Set Points/Top Set Currents", &hkey);
  if(hkey == NULL){
    cm_msg(MERROR, "ReadCurrents", "unable to find Top Set Currents key");
  }

  int top_comp_size = sizeof(top_comp_values);
  db_get_data(hDB, hkey, &top_comp_values, &top_comp_size, TID_DOUBLE);

  //Get allowable difference
  db_find_key(hDB, 0, "/Equipment/Surface Coils/Settings/Set Points/Allowed Difference", &hkey);                           
  compSetPoint = 0;               
  int compsetpt_size = sizeof(compSetPoint);
  db_get_data(hDB, hkey, &compSetPoint, &compsetpt_size, TID_DOUBLE);
 

  //Compare these values to the previous set points. If any 1 current is different, update the actual set point arrays and send the values to the beaglebones
  for(int i=0;i<nCoils;i++){
    if(bot_comp_values[i] != bot_set_values[i] || top_comp_values[i] != top_set_values[i]){
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
      //std::cout << "Sent the set points to crate 1" << std::endl;

      zmq::message_t reply1;
      if(!requester1.recv(&reply1)){
        cm_msg(MINFO, "ReadCurrents", "Crate 1 never responded");
	return FE_ERR_HW;
      }
      //else std::cout << "set Points were received by crate 1" << std::endl;

      //message 2
      /*zmq::message_t message2 (buffer.size());
      std::copy(buffer.begin(), buffer.end(), (char *)message2.data());
      requester2.send(message2);
      //std::cout << "Sent the set points to crate 2" << std::endl;

      zmq::message_t reply2;
      if(!requester2.recv(&reply2)){
        cm_msg(MINFO, "ReadCurrents", "Crate 2 never responded");
	return FE_ERR_HW;
      }
      //else std::cout << "set Points were received by crate 2" << std::endl;*/

      //message 3
      zmq::message_t message3 (buffer.size());
      std::copy(buffer.begin(), buffer.end(), (char *)message3.data());
      requester3.send(message3);
      //std::cout << "Sent the set points to crate 3" << std::endl;

      zmq::message_t reply3;
      if(!requester3.recv(&reply3)){
	cm_msg(MINFO, "ReadCurrents", "Crate 3 never responded");
	return FE_ERR_HW;
      }
      //else std::cout << "set Points were received by crate 3" << std::endl;
      
      //message 4
      /*zmq::message_t message4 (buffer.size());
      std::copy(buffer.begin(), buffer.end(), (char *)message4.data());
      requester4.send(message4);
      //std::cout << "Sent the set points to crate 4" << std::endl;

      zmq::message_t reply4;
      if(!requester4.recv(&reply4)){
        cm_msg(MINFO, "ReadCurrents", "Crate 4 never responded");
	return FE_ERR_HW;
      }
      //else std::cout << "set Points were received by crate 4" << std::endl;*/

      //message 5
      /*zmq::message_t message5 (buffer.size());
      std::copy(buffer.begin(), buffer.end(), (char *)message5.data());
      requester3.send(message5);
      //std::cout << "Sent the set points to crate 5" << std::endl;

      zmq::message_t reply5;
      if(!requester5.recv(&reply5)){
        cm_msg(MINFO, "ReadCurrents", "Crate 5 never responded");
	return FE_ERR_HW;
      }
      //else std::cout << "set Points were received by crate 5" << std::endl;*/

      //message 6
      /*zmq::message_t message6 (buffer.size());
      std::copy(buffer.begin(), buffer.end(), (char *)message6.data());
      requester6.send(message6);
      //std::cout << "Sent the set points to crate 6" << std::endl;

      zmq::message_t reply6;
      if(!requester6.recv(&reply6)){
        cm_msg(MINFO, "ReadCurrents", "Crate 6 never responded");
	return FE_ERR_HW;
      }
      //else std::cout << "set Points were received by crate 6" << std::endl;*/

      break;
    }
  }
 
  //make an instance of the struct
  unpacked_data dataUnit;

  //reset arrays to hold data sent from beaglebones                
  for(int i=0;i<nCoils;i++){
    dataUnit.bot_currents[i] = 0.0;
    dataUnit.top_currents[i] = 0.0;
    dataUnit.bot_temps[i] = 0.0;
    dataUnit.top_temps[i] = 0.0;
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
  //Update values in odb   
  db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Currents/Bottom Currents", &dataUnit.bot_currents, sizeof(dataUnit.bot_currents), 100, TID_DOUBLE);
  db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Currents/Top Currents", &dataUnit.top_currents, sizeof(dataUnit.top_currents), 100, TID_DOUBLE);
  db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Temperatures/Bottom Temps", &dataUnit.bot_temps, sizeof(dataUnit.bot_temps), 100, TID_DOUBLE);
  db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Temperatures/Top Temps", &dataUnit.top_temps, sizeof(dataUnit.top_temps), 100, TID_DOUBLE);
  mlock.unlock();
  
  //Check values vs. set points/allowed difference and temperature   
  //Will set alarm if something is bad

  string bad_curr_string = "";
  string bad_temp_string = "";
 
  mlock.lock(); 
  for(int i=0;i<nCoils;i++){
    //bottom currents            
    if(std::abs(dataUnit.bot_currents[i]-bot_set_values[i]) >= setPoint){
 
      
      char str[256];
      current_health = 0;

      if(bad_curr_string == ""){ bad_curr_string = coil_string("bot", i);}
      else {bad_curr_string = bad_curr_string + ", " + coil_string("bot", i);}
      snprintf(str, 256, "%s", bad_curr_string.c_str());

      db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Current Health", &current_health, sizeof(current_health), 1, TID_BOOL);
      db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Problem Current Channel", str, sizeof(str), 1, TID_STRING);
      cm_msg(MINFO, "read_surface_coils", "Bottom current out of spec");

    }


    //top currents                
    if(std::abs(dataUnit.top_currents[i]-top_set_values[i]) >= setPoint){

      char str[256];
      current_health = 0;
      if(bad_curr_string == ""){ bad_curr_string = coil_string("top", i);}
      else {bad_curr_string = bad_curr_string + ", " + coil_string("top", i);}
      snprintf(str, 256, "%s", bad_curr_string.c_str());
      
      db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Current Health", &current_health, sizeof(current_health), 1, TID_BOOL);
      db_set_value(hDB, 0, "/Equipment/Surface Coils/Settings/Monitoring/Problem Current Channel", str, sizeof(str), 1, TID_STRING);
      cm_msg(MINFO, "read_surface_coils", "Top current out of spec");
    }

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
