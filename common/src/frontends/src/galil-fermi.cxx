/********************************************************************\

Name:         galil-fermi.cxx
Created by:   Matteo Bartolini
Modified by:  Joe Grange, Ran Hong

Contents:     readout code to talk to Galil motion control

$Id$

\********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "midas.h"
#include "mcstd.h"
#include "gclib.h"
#include "gclibo.h"
#include <iostream>
#include <string>
#include <iomanip>
#include <vector>
#include <unistd.h>
#include <sys/timeb.h>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>
#include <memory>
#include <pqxx/pqxx>
#include "g2field/core/field_constants.hh"
#include "g2field/core/field_structs.hh"
#include "TTree.h"
#include "TFile.h"

#define GALIL_EXAMPLE_OK G_NO_ERROR //return code for correct code execution
#define GALIL_EXAMPLE_ERROR -100

#define FRONTEND_NAME "Galil Motion Control" // Prefer capitalize with spaces
using namespace std;

/* make frontend functions callable from the C framework */
#ifdef __cplusplus
extern "C" {
#endif

  // i am defining some Galil libraries variables

  //----------------------------------------------------------
  /*-- Globals -------------------------------------------------------*/

  /* The frontend name (client name) as seen by other MIDAS clients   */
  const char *frontend_name = FRONTEND_NAME	;
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

  INT read_event(char *pevent, INT off);

  INT poll_event(INT source, INT count, BOOL test);
  INT interrupt_configure(INT cmd, INT source, POINTER_T adr);


  /*-- Equipment list ------------------------------------------------*/


  EQUIPMENT equipment[] = {

   {"GalilFermi",                /* equipment name */
      {EVENTID_GALIL_FERMI, 0,                   /* event ID, trigger mask */
	"SYSTEM",               /* event buffer */
	EQ_POLLED,            /* equipment type */
	0,                      /* event source */
	"MIDAS",                /* format */
	TRUE,                   /* enabled */
	RO_RUNNING   /* read when running*/
	//|RO_ODB
	,
	500,                  /* poll every 0.5 sec */
	0,                      /* stop run after this event limit */
	0,                      /* number of sub events */
	0,                      /* log history, logged once per minute */
	"", "", "",},
      read_event,       /* readout routine */
    },

    {""}
  };


#ifdef __cplusplus
}
#endif

HNDLE hDB, hkeyclient;

BOOL write_root = false;
TFile *pf;
TTree *pt_norm;

typedef struct GalilDataStruct{
  INT TimeStamp;
  INT PositionArray[6];
  INT VelocityArray[6];
  INT OutputVArray[6];
  INT LimFArray[6];
  INT LimRArray[6];
  INT AnalogArray[6];
  BOOL StatusArray[6];
}GalilDataStruct;

typedef struct GalilDataStructD{
  double PositionArray[6];
  double VelocityArray[6];
  double OutputVArray[6];
  double LimFArray[6];
  double LimRArray[6];
  double AnalogArray[6];
  double StatusArray[6];
}GalilDataStructD;

INT PlungingProbeStepSize[3];
INT PlungingProbeStepNumber[3];
int IX,IY,IZ;

INT TrolleyStepSize;
INT ITrolley;

vector<GalilDataStruct> GalilDataBuffer;
const char * const galil_trolley_bank_name = "GLTL"; // 4 letters, try to make sensible
const char * const galil_plunging_probe_bank_name = "GLPP"; // 4 letters, try to make sensible
g2field::galil_trolley_t GalilTrolleyDataCurrent;
g2field::galil_plunging_probe_t GalilPlungingProbeDataCurrent;

thread monitor_thread;
thread control_thread;

//Flags
bool ReadyToMove = false;
bool ReadyToRead = false;
bool PreventManualCtrl = false;
BOOL TestMode = FALSE;

mutex mlock;
mutex mlockdata;

void GalilMonitor(const GCon &g);
void GalilControl(const GCon &g);

bool MonitorActive;
bool ControlActive;
bool RunActive;
int ReadGroupSize = 50;

GCon g = 0; //var used to refer to a unique connection. A valid connection is nonzero.

std::unique_ptr<pqxx::connection> psql_con; //PSQL connection handle

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

/*-- Frontend Init -------------------------------------------------*/

INT frontend_init()
{ 
  
  char buf[1023]; //traffic buffer
  GReturn b = G_NO_ERROR;
	//Change the status color
	char odbColour[32];
	int size = sizeof(odbColour);
	db_get_value(hDB,0,"/Equipment/GalilFermi/Common/Status Color",&odbColour,&size,TID_STRING,FALSE);
	strcpy(odbColour, "#8A2BE2");
	db_set_value(hDB,0,"/Equipment/GalilFermi/Common/Status Color",&odbColour,size,1,TID_STRING);

  b=GOpen("192.168.2.13 -s ALL -t 1000 -d",&g);
  GInfo(g, buf, sizeof(buf)); //grab connection string
//  cout << "connection string is" << " "<<  buf << "\n";
  cm_msg(MINFO,"init","connection string is %s",buf);
  if (b==G_NO_ERROR){
    //cout << "connection successful\n";
    cm_msg(MINFO,"init","Galil connection successful");
  }
  else {
 //   cout << "connection failed \n";
    cm_msg(MERROR,"init","Galil connection failed");
    return -1;
  }
  GTimeout(g,5000);//adjust timeout

  //Initialize data base connection
  const char * host = "g2db-priv";
  const char * dbname = "test";
  const char * user = "daq";
  const char * password = "";
  const char * port = "5432";
  std::string psql_con_str = std::string("host=") + std::string(host) + std::string(" dbname=")\
			     + std::string(dbname) + std::string(" user=") + std::string(user) \
			     + std::string(" password=") + std::string(password) + std::string(" port=")\
			     + std::string(port);
//  psql_con = std::make_unique<pqxx::connection>(psql_con_str);

  //Set the motor switch odb values to off as default
  BOOL MOTOR_OFF = FALSE;
  db_set_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Trolley/Trolley Switch",&MOTOR_OFF,sizeof(MOTOR_OFF), 1 ,TID_BOOL); 
  db_set_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Trolley/Garage Switch",&MOTOR_OFF,sizeof(MOTOR_OFF), 1 ,TID_BOOL); 
  //Load script
  char ScriptName[500];
  INT ScriptName_size = sizeof(ScriptName);
  db_get_value(hDB,0,"/Equipment/GalilFermi/Settings/Cmd Script",ScriptName,&ScriptName_size,TID_STRING,0);
  char DirectName[500];
  INT DirectName_size = sizeof(DirectName);
  db_get_value(hDB,0,"/Equipment/GalilFermi/Settings/Script Directory",DirectName,&DirectName_size,TID_STRING,0);
  string FullScriptName = string(DirectName)+string(ScriptName)+string(".dmc");
  cm_msg(MINFO,"init","Galil Script to load: %s",FullScriptName.c_str());
  //Clear command
  INT command=0;
  db_set_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Cmd",&command,sizeof(command), 1 ,TID_INT); 

  //Flip the AllStop bit and then motions are allowed
  GCmd(g, "CB 2");
  sleep(1);
  GCmd(g, "SB 2");

//  b=GProgramDownload(g,"",0); //to erase prevoius programs
//  b=GProgramDownloadFile(g,FullScriptName.c_str(),0);
//  cm_msg(MINFO,"init","Galil Program Download return code: %d",b);
  b=GCmd(g, "XQ #MNLP,0");
  cm_msg(MINFO,"init","Galil XQ return code: %d",b);
  MonitorActive=true;
  ControlActive=true;
  RunActive = false;
  //Start threads
  monitor_thread = thread(GalilMonitor,g);
  control_thread = thread(GalilControl,g);
  //-------------end code to communicate with Galil------------------
  PreventManualCtrl = false;

	db_get_value(hDB,0,"/Equipment/GalilFermi/Common/Status Color",&odbColour,&size,TID_STRING,FALSE);
	strcpy(odbColour, "greenLight");
	db_set_value(hDB,0,"/Equipment/GalilFermi/Common/Status Color",&odbColour,size,1,TID_STRING);
  return SUCCESS;
}

/*-- Frontend Exit -------------------------------------------------*/

INT frontend_exit()
{
  mlock.lock();
  GCmd(g,"AB 1");
  //Set the motor switch odb values to off
  BOOL MOTOR_OFF = FALSE;
  db_set_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Trolley/Trolley Switch",&MOTOR_OFF,sizeof(MOTOR_OFF), 1 ,TID_BOOL); 
  db_set_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Trolley/Garage Switch",&MOTOR_OFF,sizeof(MOTOR_OFF), 1 ,TID_BOOL); 
  //Clear command
  INT command=0;
  db_set_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Cmd",&command,sizeof(command), 1 ,TID_INT); 
  mlock.unlock();
  cm_msg(MINFO,"end_of_run","Motion aborted.");

  mlock.lock();
  GCmd(g,"HX");
  cm_msg(MINFO,"end_of_run","Galil execution stopped.");
  mlock.unlock();

  mlock.lock();
  MonitorActive=false;
  ControlActive=false;
  RunActive = false;
  mlock.unlock();
//  cm_msg(MINFO,"end_of_run","Trying to join threads.");
  monitor_thread.join();
  control_thread.join();
  cm_msg(MINFO,"end_of_run","All threads joined.");
  GClose(g);
  return SUCCESS;
}

/*-- Begin of Run --------------------------------------------------*/

INT begin_of_run(INT run_number, char *error)
{
  //Get run number
  INT RunNumber;
  INT RunNumber_size = sizeof(RunNumber);
  cm_get_experiment_database(&hDB, NULL);
  db_get_value(hDB,0,"/Runinfo/Run number",&RunNumber,&RunNumber_size,TID_INT, 0);

  //Get Root output switch
  int write_root_size = sizeof(write_root);
  db_get_value(hDB,0,"/Equipment/GalilFermi/Settings/Root Output",&write_root,&write_root_size,TID_BOOL, 0);
  
  //Get Data dir
  string DataDir;
  char str[500];
  int str_size = sizeof(str);
  db_get_value(hDB,0,"/Equipment/GalilFermi/Settings/Root Dir",&str,&str_size,TID_STRING, 0);
  DataDir=string(str);
  
  //Root File Name
  sprintf(str,"GalilOutput_%05d.root",RunNumber);
  string RootFileName = DataDir + string(str);

  if(write_root){
    cm_msg(MINFO,"begin_of_run","Writing to root file %s",RootFileName.c_str());
    pf = new TFile(RootFileName.c_str(), "recreate");
    pt_norm = new TTree("t_GalilFermi", "Galil Data");
    pt_norm->SetAutoSave(5);
    pt_norm->SetAutoFlush(20);

    pt_norm->Branch(galil_trolley_bank_name, &GalilTrolleyDataCurrent, g2field::galil_trolley_str);
    pt_norm->Branch(galil_plunging_probe_bank_name, &GalilPlungingProbeDataCurrent, g2field::galil_plunging_probe_str);
  }

  //Clear history data in buffer
  GalilDataBuffer.clear();
  //Turn the Run Active Flag to true
  RunActive = true;

  /*
  INT StepSize_size = sizeof(StepSize);
  INT StepNumber_size = sizeof(StepNumber);
  db_get_value(hDB,0,"/Equipment/GalilFermi/AutoControl/RelPos",StepSize,&StepSize_size,TID_INT,0);
  db_get_value(hDB,0,"/Equipment/GalilFermi/AutoControl/StepNumber",StepNumber,&StepNumber_size,TID_INT,0);

  cm_msg(MINFO,"read_event","Step Sizes: %d %d %d %d",StepSize[0],StepSize[1],StepSize[2],StepSize[3]);
  cm_msg(MINFO,"read_event","Step Numbers: %d %d %d %d",StepNumber[0],StepNumber[1],StepNumber[2],StepNumber[3]);
 
  IX=IY=IZ=IS=0;

  //Check if it is in test mode. if so, do not interact with the abs-probe frontend
  INT TestMode_size = sizeof(TestMode);
  db_get_value(hDB,0,"/Equipment/GalilFermi/Settings/TestMode",&TestMode,&TestMode_size,TID_BOOL,0);

  if(TestMode){
    ReadyToMove = true;
  }else{
    ReadyToMove = false;
  }
  BOOL temp_bool = BOOL(ReadyToMove);
  db_set_value(hDB,0,"/Equipment/GalilFermi/Monitors/ReadyToMove",&temp_bool,sizeof(temp_bool), 1 ,TID_BOOL);
*/
  //PreventManualCtrl = true;
/*
  //Init alarm-watched odb value
  INT Finished=0;
  db_set_value(hDB,0,"/Equipment/GalilFermi/Monitors/Finished",&Finished,sizeof(Finished), 1 ,TID_INT);
  //Clear triggered alarms
  INT AlarmTriggered=0;
  db_set_value(hDB,0,"/Alarms/Alarms/Galil Platform Stop/Triggered",&AlarmTriggered,sizeof(AlarmTriggered), 1 ,TID_INT);
*/
  return SUCCESS;
}

/*-- End of Run ----------------------------------------------------*/

INT end_of_run(INT run_number, char *error)
{
 // INT Finished=1;
 // db_set_value(hDB,0,"/Equipment/GalilFermi/Monitors/Finished",&Finished,sizeof(Finished), 1 ,TID_INT);
  PreventManualCtrl = false;
  RunActive = false;

  GalilDataBuffer.clear();
  cm_msg(MINFO,"end_of_run","Data buffer is emptied.");

  if(write_root){
    pt_norm->Write();

    pf->Write();
    pf->Close();
  }

  return SUCCESS;
}

/*-- Pause Run -----------------------------------------------------*/

INT pause_run(INT run_number, char *error)
{
  return SUCCESS;
}

/*-- Resuem Run ----------------------------------------------------*/

INT resume_run(INT run_number, char *error)
{ 
  return SUCCESS;
}

/*-- Frontend Loop -------------------------------------------------*/

INT frontend_loop()
{
  /* if frontend_call_loop is true, this routine gets called when
     the frontend is idle or once between every event */
  return SUCCESS;
}

/*------------------------------------------------------------------*/

/********************************************************************\

  Readout routines for different events

  \********************************************************************/

/*-- Trigger event routines ----------------------------------------*/

INT poll_event(INT source, INT count, BOOL test)
  /* Polling routine for events. Returns TRUE if event
     is available. If test equals TRUE, don't return. The test
     flag is used to time the polling */
{
  static unsigned int i;
  if (test) {
    for (i = 0; i < count; i++) {
      usleep(10);
    }
    return 0;
  }

  mlockdata.lock();
  bool check = (GalilDataBuffer.size()>GALILREADGROUPSIZE);
  //if (check)cout <<"poll "<<GalilDataBuffer.size()<<" "<<int(check)<<endl;
  mlockdata.unlock();
  if (check)return 1;
  else return 0;
}

/*-- Interrupt configuration ---------------------------------------*/

INT interrupt_configure(INT cmd, INT source, POINTER_T adr)
{
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

/*-- Event readout -------------------------------------------------*/

INT read_event(char *pevent, INT off){
  WORD *pdataTrolley;
  WORD *pdataPP;

  //Init bank
  bk_init32(pevent);
  
  //Write data to banks
  bk_create(pevent, galil_trolley_bank_name, TID_WORD, (void **)&pdataTrolley);

  for (int i=0;i<GALILREADGROUPSIZE;i++){
    mlockdata.lock();
    GalilTrolleyDataCurrent.TimeStamp = GalilDataBuffer[i].TimeStamp;
    for (int j=0;j<2;j++){
      GalilTrolleyDataCurrent.Tensions[j] = GalilDataBuffer[i].AnalogArray[j];
    }
    for (int j=0;j<2;j++){
      GalilTrolleyDataCurrent.Temperatures[j] = GalilDataBuffer[i].AnalogArray[j+2];
    }
    for (int j=0;j<3;j++){
      GalilTrolleyDataCurrent.Positions[j] = GalilDataBuffer[i].PositionArray[j];
      GalilTrolleyDataCurrent.Velocities[j] = GalilDataBuffer[i].VelocityArray[j];
      GalilTrolleyDataCurrent.OutputVs[j] = GalilDataBuffer[i].OutputVArray[j];
    }

    GalilPlungingProbeDataCurrent.TimeStamp = GalilDataBuffer[i].TimeStamp;
    for (int j=0;j<3;j++){
      GalilPlungingProbeDataCurrent.Positions[j] = GalilDataBuffer[i].PositionArray[j];
      GalilPlungingProbeDataCurrent.Velocities[j] = GalilDataBuffer[i].VelocityArray[j];
      GalilPlungingProbeDataCurrent.OutputVs[j] = GalilDataBuffer[i].OutputVArray[j];
    }
    memcpy(pdataTrolley, &GalilTrolleyDataCurrent, sizeof(g2field::galil_trolley_t));
    pdataTrolley += sizeof(g2field::galil_trolley_t)/sizeof(WORD);

    //Write to ROOT file
    if (write_root) {
      pt_norm->Fill();
    }
    mlockdata.unlock();
  } 
  bk_close(pevent,pdataTrolley);

  bk_create(pevent, galil_plunging_probe_bank_name, TID_WORD, (void **)&pdataPP);
  for (int i=0;i<GALILREADGROUPSIZE;i++){
    mlockdata.lock();
    GalilPlungingProbeDataCurrent.TimeStamp = GalilDataBuffer[i].TimeStamp;
    for (int j=0;j<3;j++){
      GalilPlungingProbeDataCurrent.Positions[j] = GalilDataBuffer[i].PositionArray[j];
      GalilPlungingProbeDataCurrent.Velocities[j] = GalilDataBuffer[i].VelocityArray[j];
      GalilPlungingProbeDataCurrent.OutputVs[j] = GalilDataBuffer[i].OutputVArray[j];
    }
    memcpy(pdataPP, &GalilPlungingProbeDataCurrent, sizeof(g2field::galil_plunging_probe_t));
    pdataPP += sizeof(g2field::galil_plunging_probe_t)/sizeof(WORD);
    mlockdata.unlock();
  } 
  mlockdata.lock();
  GalilDataBuffer.erase(GalilDataBuffer.begin(),GalilDataBuffer.begin()+GALILREADGROUPSIZE);
  mlockdata.unlock();
  bk_close(pevent,pdataPP);

  //Write to ROOT file
  if (write_root) {
    pt_norm->AutoSave("SaveSelf,FlushBaskets");
    pf->Flush();
  }
  /*
     ReadyToMove = false;
     BOOL temp_bool = BOOL(ReadyToMove);
  db_set_value(hDB,0,"/Equipment/GalilFermi/Monitors/ReadyToMove",&temp_bool,sizeof(temp_bool), 1 ,TID_BOOL);

  char CmdBuffer[500];
  //Regeister current position from odb
  INT CurrentPositions[4];
  INT CurrentPositions_size = sizeof(CurrentPositions);
  db_get_value(hDB,0,"/Equipment/GalilFermi/Monitors/Positions",CurrentPositions,&CurrentPositions_size,TID_INT,0);
  INT CurrentVelocities[4];
  INT CurrentVelocities_size = sizeof(CurrentPositions);
  db_get_value(hDB,0,"/Equipment/GalilFermi/Monitors/Velocities",CurrentVelocities,&CurrentVelocities_size,TID_INT,0);
  //Take a measurement
 // cm_msg(MINFO,"read_event","Step Indices: %d %d %d %d",IX,IY,IZ,IS);


  //Move platform
  //Must enclose the GCmd commands with mutex locks!!!!!
  
  if (IX>=StepNumber[0]){
    IX=0;
    //move back to X0
    sprintf(CmdBuffer,"PRA=%d",-StepNumber[0]*StepSize[0]);
    mlock.lock();
    GCmd(g,CmdBuffer);
    GCmd(g,"BGA");
    mlock.unlock();
    if (IZ>=StepNumber[2]){
      IZ=0;
      //move back to Z0
      sprintf(CmdBuffer,"PRC=%d",-StepNumber[2]*StepSize[2]);
      mlock.lock();
      GCmd(g,CmdBuffer);
      GCmd(g,"BGC");
      mlock.unlock();
      if (IY>=StepNumber[1]){
	IY=0;
	//move back to Y0
	sprintf(CmdBuffer,"PRB=%d",-StepNumber[1]*StepSize[1]);
	mlock.lock();
	GCmd(g,CmdBuffer);
	GCmd(g,"BGB");
	mlock.unlock();
	if (IS>=StepNumber[3]){
	  IS=0;
	  //move back to S0
	  sprintf(CmdBuffer,"PRD=%d",-StepNumber[3]*StepSize[3]);
          mlock.lock();
	  GCmd(g,CmdBuffer);
	  GCmd(g,"BGD");
          mlock.unlock();
	  //Reach the destination
	  //Stop the run
	  cm_msg(MINFO,"read_event","Destination is reached. Stop the run.");
	  INT Finished=2;
	  db_set_value(hDB,0,"/Equipment/GalilFermi/Monitors/Finished",&Finished,sizeof(Finished), 1 ,TID_INT);
	  return bk_size(pevent);
	}else{
	  //move forward in S
	  sprintf(CmdBuffer,"PRD=%d",StepSize[3]);
          mlock.lock();
	  GCmd(g,CmdBuffer);
	  GCmd(g,"BGD");
          mlock.unlock();
	  IS++;
	}
      }else{
	//move forward in Y
	sprintf(CmdBuffer,"PRB=%d",StepSize[1]);
	mlock.lock();
	GCmd(g,CmdBuffer);
	GCmd(g,"BGB");
	mlock.unlock();
	IY++;
      }
    }else{
      //move forward in Z
      sprintf(CmdBuffer,"PRC=%d",StepSize[2]);
      mlock.lock();
      GCmd(g,CmdBuffer);
      GCmd(g,"BGC");
      mlock.unlock();
      IZ++;
    }
  }else{
    //move forward in X
    sprintf(CmdBuffer,"PRA=%d",StepSize[0]);
    mlock.lock();
    GCmd(g,CmdBuffer);
    GCmd(g,"BGA");
    mlock.unlock();
    IX++;
  }

  sleep(1);
  //Check if motion is stopped
  while(1){
    db_get_value(hDB,0,"/Equipment/GalilFermi/Monitors/Velocities",CurrentVelocities,&CurrentVelocities_size,TID_INT,0);
    if (CurrentVelocities[0]==0 && CurrentVelocities[1]==0 && CurrentVelocities[2]==0 && CurrentVelocities[3]==0)break;
  //  cm_msg(MINFO,"read_event","V = %d",CurrentVelocities[0]);
    sleep(1);
  }

  //If in test mode, start to move. If not, tell abs-probe to read
  if(TestMode){
    ReadyToMove = true;
    temp_bool = BOOL(ReadyToMove);
    db_set_value(hDB,0,"/Equipment/GalilFermi/Monitors/ReadyToMove",&temp_bool,sizeof(temp_bool), 1 ,TID_BOOL);
  }else{
    ReadyToRead = true;
    temp_bool = BOOL(ReadyToRead);
    db_set_value(hDB,0,"/Equipment/AbsoluteProbe/Monitor/ReadyToRead",&temp_bool,sizeof(temp_bool), 1 ,TID_BOOL);
  }
  */
  return bk_size(pevent);
}

//GalilMonitor
void GalilMonitor(const GCon &g){
  int MonitorThreadActive = 1;
  db_set_value(hDB,0,"/Equipment/GalilFermi/Monitors/Monitor Thread Active",&MonitorThreadActive,sizeof(MonitorThreadActive), 1 ,TID_BOOL); 
  char buffer[50000];
  hkeyclient=0;
  string Header;
  GReturn rc = G_NO_ERROR;
  /* residule string */
  string ResidualString = string("");
  INT command = 0;
  INT emergency = 0;

  int position =0;
//  timeb starttime,currenttime;
//  ftime(&starttime);
 
  //Readout loop
  int i=0;
  int jj=0;
  double Time;
  //Data trigger mask
  //bit0:position
  //bit1:velocity
  //bit2:contolve voltage
  //bit3 analog
  //When all bits are 1, push to data buffer
  short DataTriggerMask = 0;

  while (1){
    bool localMonitorActive;
    mlock.lock();
    localMonitorActive = MonitorActive;
    mlock.unlock();
    if (!localMonitorActive)break;
    //Read Message to buffer
    mlock.lock();
    rc = GMessage(g, buffer, sizeof(buffer));
    mlock.unlock();
    if (rc!=G_NO_ERROR){
      cm_msg(MINFO,"GalilMonitor","Galil Message return code: %d",rc);
      continue;
    }
//    ftime(&currenttime);
//    double time = (currenttime.time-starttime.time)*1000 + (currenttime.millitm - starttime.millitm);
    //cout<<buffer<<endl;
    //cout <<rc<<endl;

    string BufString = string(buffer);
    //Add the residual from last read
    if (ResidualString.size()!=0)BufString = ResidualString+BufString;
    ResidualString.clear();

    size_t foundnewline = BufString.find("\n");
    //  static  bool flag = false;

    GalilDataStruct GalilDataUnit;
    GalilDataStructD GalilDataUnitD;

    jj=0;

    int iGalil = 0;

    while (foundnewline!=string::npos){
      stringstream iss (BufString.substr(0,foundnewline-1));
      // output returned by Galil is stored in the following variables
      //cout << BufString.substr(0,foundnewline-1)<<endl;

      iss >> Header;
      if(Header.compare("POSITION")==0){
	//iss >> GalilDataUnit.TimeStamp;
	iss >> Time;
	for (int j=0;j<6;j++){
	  iss >> GalilDataUnitD.PositionArray[j];
	}
	//Convert to INT
	GalilDataUnit.TimeStamp = INT(Time);
	for (int j=0;j<6;j++){
	  GalilDataUnit.PositionArray[j] = INT(GalilDataUnitD.PositionArray[j]);
	}
	DataTriggerMask |= 1;
      }else if(Header.compare("VELOCITY")==0){
	//iss >> GalilDataUnit.TimeStamp;
	iss >> Time;
	for (int j=0;j<6;j++){
	  iss >> GalilDataUnitD.VelocityArray[j];
	}
	//Convert to INT
	for (int j=0;j<6;j++){
	  GalilDataUnit.VelocityArray[j] = INT(GalilDataUnitD.VelocityArray[j]);
	}
	DataTriggerMask |= 1<<1;
      }else if(Header.compare("CONTROLVOLTAGE")==0){
	//iss >> GalilDataUnit.TimeStamp;
	iss >> Time;
	for (int j=0;j<6;j++){
	  iss >> GalilDataUnitD.OutputVArray[j];
	}
	//Convert to INT
	for (int j=0;j<6;j++){
	  GalilDataUnit.OutputVArray[j] = INT(GalilDataUnitD.OutputVArray[j] * 1000);
	}
	DataTriggerMask |= 1<<2;
      }else if(Header.compare("ANALOG")==0){
	//iss >> GalilDataUnit.TimeStamp;
	iss >> Time;
	for (int j=0;j<6;j++){
	  iss >> GalilDataUnitD.AnalogArray[j];
	}
	//Convert to INT
	for (int j=0;j<6;j++){
	  GalilDataUnit.AnalogArray[j] = INT(GalilDataUnitD.AnalogArray[j] * 1000);
	}
	DataTriggerMask |= 1<<3;
      }else if(Header.compare("LIMITSWITCHF")==0){
        iss >> Time;
        for (int j=0; j<6; j++){
          iss >> GalilDataUnitD.LimFArray[j];
        }
        for (int j=0;j<6;j++){
          GalilDataUnit.LimFArray[j] = INT(GalilDataUnitD.LimFArray[j]);
        }
      }else if(Header.compare("LIMITSWITCHR")==0){
        iss >> Time;
        for (int j=0; j<6; j++){
          iss >> GalilDataUnitD.LimRArray[j];
        }
        for (int j=0;j<6;j++){
          GalilDataUnit.LimRArray[j] = INT(GalilDataUnitD.LimRArray[j]);
	}
      }else if(Header.compare("MOTORSTATUS")==0){
        iss >> Time;
        for (int j=0; j<6; j++){
          iss >> GalilDataUnitD.StatusArray[j];
        }
        for (int j=0;j<6;j++){
	  if (GalilDataUnitD.StatusArray[j]>0.0){
	    GalilDataUnit.StatusArray[j] = FALSE;
	  }else{
	    GalilDataUnit.StatusArray[j] = TRUE;
	  }
	}
      }else if(Header.compare("ERROR")==0){
	mlock.lock();
	GCmd(g,"AB 1");
	mlock.unlock();
	cm_msg(MINFO,"Galil Message",BufString.substr(0,foundnewline-1).c_str());
      }else{
	cm_msg(MINFO,"Galil Message",BufString.substr(0,foundnewline-1).c_str());
      }

      //Check DataTriggerMask, pack data
      if (DataTriggerMask == 0xF){
	bool localRunActive;
	mlock.lock();
	localRunActive = RunActive;
	mlock.unlock();
	mlockdata.lock();
	if (localRunActive){
	  GalilDataBuffer.push_back(GalilDataUnit);
	}
	mlockdata.unlock();
	DataTriggerMask = 0;
	
//	cm_msg(MINFO,"Galil Message","Time = %d",GalilDataUnit.TimeStamp);
	iGalil++;
      }

      BufString = BufString.substr(foundnewline+1,string::npos);
      foundnewline = BufString.find("\n");
      jj++;
    }
    if (BufString.size()!=0){
      ResidualString = BufString;
    }

    INT BufferLoad;
    mlockdata.lock();
    BufferLoad = GalilDataBuffer.size();
    mlockdata.unlock();

    //Update odb for monitoring
    mlock.lock();
    db_set_value(hDB,0,"/Equipment/GalilFermi/Monitors/Time Stamp",&GalilDataUnit.TimeStamp,sizeof(GalilDataUnit.TimeStamp), 1 ,TID_INT); 
    db_set_value(hDB,0,"/Equipment/GalilFermi/Monitors/Positions",&GalilDataUnit.PositionArray,sizeof(GalilDataUnit.PositionArray), 6 ,TID_INT); 
    db_set_value(hDB,0,"/Equipment/GalilFermi/Monitors/Velocities",&GalilDataUnit.VelocityArray,sizeof(GalilDataUnit.VelocityArray), 6 ,TID_INT); 
    db_set_value(hDB,0,"/Equipment/GalilFermi/Monitors/Control Voltages",&GalilDataUnit.OutputVArray,sizeof(GalilDataUnit.OutputVArray), 6 ,TID_INT); 
    db_set_value(hDB,0,"/Equipment/GalilFermi/Monitors/Analogs",&GalilDataUnit.AnalogArray,sizeof(GalilDataUnit.AnalogArray), 6 ,TID_INT);
    db_set_value(hDB,0,"/Equipment/GalilFermi/Monitors/Limit Switches Forward",&GalilDataUnit.LimFArray,sizeof(GalilDataUnit.LimFArray), 6 ,TID_INT);
    db_set_value(hDB,0,"/Equipment/GalilFermi/Monitors/Limit Switches Reverse",&GalilDataUnit.LimRArray,sizeof(GalilDataUnit.LimRArray), 6 ,TID_INT);
    db_set_value(hDB,0,"/Equipment/GalilFermi/Monitors/Motor Status",&GalilDataUnit.StatusArray,sizeof(GalilDataUnit.StatusArray), 6 ,TID_BOOL);
    db_set_value(hDB,0,"/Equipment/GalilFermi/Monitors/Buffer Load",&BufferLoad,sizeof(BufferLoad), 1 ,TID_INT);

    //Add Time, Position and Velocity to TrolleyInterface subtree
    char SourceName[256];
    INT SourceName_size = sizeof(SourceName);
    sprintf(SourceName,"Galil-Fermi");
    db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitors/Extra/Source",SourceName,SourceName_size,1,TID_STRING);
    db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitors/Extra/Time Stamp",&GalilDataUnit.TimeStamp,sizeof(GalilDataUnit.TimeStamp), 1 ,TID_INT); 
    db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitors/Extra/Positions",&GalilDataUnit.PositionArray,sizeof(GalilDataUnit.PositionArray), 6 ,TID_INT); 
    db_set_value(hDB,0,"/Equipment/TrolleyInterface/Monitors/Extra/Velocities",&GalilDataUnit.VelocityArray,sizeof(GalilDataUnit.VelocityArray), 6 ,TID_INT); 
    mlock.unlock();

    //Load to Data base
/*    mlock.lock();
    std::unique_ptr<pqxx::work> txnp;
    try {
      txnp = std::make_unique<pqxx::work>(*psql_con);
    } catch (const pqxx::broken_connection& bc) {
      txnp.reset();
      cm_msg(MERROR, __FILE__, "broken connection exception %s!", bc.what());
      // for now I will throw an exception.
      // In future, handle this through alarm system
      cm_yield(0);
      throw;
    }
    if (txnp) {
      // form query
      std::string query_str =
	"INSERT into g2field-monitor (name, value, time) VALUES ('";
      query_str += "Trolley Motor Temperatures";
      query_str += "', '";
      query_str += "{20.8,35.9}";
      query_str += "', now())";
      txnp->exec(query_str);
      txnp->commit();
    }
    mlock.unlock();
*/
    //Check emergencies
    //This thread is not blocking
    INT emergency_size = sizeof(emergency);
    //Abort
    db_get_value(hDB,0,"/Equipment/GalilFermi/Settings/Emergency/Abort",&emergency,&emergency_size,TID_INT,0);
    if (emergency == 1){
      mlock.lock();
      GCmd(g,"AB 1");
      //Update finish labels
      GCmd(g,"tlfinish=1");
      GCmd(g,"grfinish=1");
      //Terminate thread
      GCmd(g,"HX 1");
      //Set the motor switch odb values to off
      BOOL MOTOR_OFF = FALSE;
      db_set_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Trolley/Trolley Switch",&MOTOR_OFF,sizeof(MOTOR_OFF), 1 ,TID_BOOL); 
      db_set_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Trolley/Garage Switch",&MOTOR_OFF,sizeof(MOTOR_OFF), 1 ,TID_BOOL); 
      
//      INT Finished=2;
//      db_set_value(hDB,0,"/Equipment/GalilFermi/Monitors/Finished",&Finished,sizeof(Finished), 1 ,TID_INT);
      mlock.unlock();
      cm_msg(MINFO,"Emergency","Motion Aborted.");
    }
    emergency=0;
    db_set_value(hDB,0,"/Equipment/GalilFermi/Settings/Emergency/Abort",&emergency,sizeof(emergency), 1 ,TID_INT); 

    i++;
  }
  MonitorThreadActive = 0;
  mlock.lock();
  db_set_value(hDB,0,"/Equipment/GalilFermi/Monitors/Monitor Thread Active",&MonitorThreadActive,sizeof(MonitorThreadActive), 1 ,TID_BOOL); 
  mlock.unlock();
}

//GalilControl
void GalilControl(const GCon &g){
  int ControlThreadActive = 1;
  mlock.lock();
  db_set_value(hDB,0,"/Equipment/GalilFermi/Monitors/Control Thread Active",&ControlThreadActive,sizeof(ControlThreadActive), 1 ,TID_BOOL); 
  mlock.unlock();
  char buffer[50000];

  hkeyclient=0;
  int  rc = GALIL_EXAMPLE_OK; //return code
  INT command = 0;
  INT emergency = 0;

  int position =0;
//  timeb starttime,currenttime;
//  ftime(&starttime);

  //Variables passed to Galil system
  int destiny = 0;
  int velocity = 100;
  double tlow = 0.4;
  double thigh = 0.8;
  double ofst1 = 0.0;
  double ofst2 = 0.0;
  int garage_velocity = 1000;
 
  //Control loop
  int i=0;
  while (1){
    bool localControlActive;
    mlock.lock();
    localControlActive = ControlActive;
    mlock.unlock();
    if (!localControlActive)break;
    //Check commandand execute;
    INT command_size = sizeof(command);
    mlock.lock();
    db_get_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Cmd",&command,&command_size,TID_INT,0);
    mlock.unlock();
    char CmdBuffer[500];

    //For trolley commands, load galil variables first
    if (command >= 1 && command <=7){
      INT Vel;
      INT TLow;
      INT THigh;
      INT TOffset1;
      INT TOffset2;
      INT GarageVel;
      INT Size_Int = sizeof(Vel);
      mlock.lock();
      db_get_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Trolley/Trolley Velocity",&Vel,&Size_Int,TID_INT,0);
      db_get_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Trolley/Tension Range Low",&TLow,&Size_Int,TID_INT,0);
      db_get_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Trolley/Tension Range High",&THigh,&Size_Int,TID_INT,0);
      db_get_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Trolley/Tension Offset 1",&TOffset1,&Size_Int,TID_INT,0);
      db_get_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Trolley/Tension Offset 2",&TOffset2,&Size_Int,TID_INT,0);
      db_get_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Trolley/Garage Velocity",&GarageVel,&Size_Int,TID_INT,0);
      mlock.unlock();
      velocity = Vel;
      garage_velocity = GarageVel;
      tlow = static_cast<double>(TLow)/1000.0;
      thigh = static_cast<double>(THigh)/1000.0;
      ofst1 = static_cast<double>(TOffset1)/1000.0;
      ofst2 = static_cast<double>(TOffset2)/1000.0;
      if (!PreventManualCtrl){
	mlock.lock();
	sprintf(CmdBuffer,"velocity=%d",velocity);
	GCmd(g,CmdBuffer);
	sprintf(CmdBuffer,"tlow=%f",tlow);
	GCmd(g,CmdBuffer);
	sprintf(CmdBuffer,"thigh=%f",thigh);
	GCmd(g,CmdBuffer);
	sprintf(CmdBuffer,"ofst1=%f",ofst1);
	GCmd(g,CmdBuffer);
	sprintf(CmdBuffer,"ofst2=%f",ofst2);
	GCmd(g,CmdBuffer);
	sprintf(CmdBuffer,"gvel=%d",garage_velocity);
	GCmd(g,CmdBuffer);
	mlock.unlock();
      }else{
	cm_msg(MINFO,"ManualCtrl","Manual control is prevented during an run.");
      }
      //Do not execute command if corresponding motors are not ON
      BOOL MotorStatus[6];
      INT size_MotorStatus = sizeof(MotorStatus);
      mlock.lock();
      db_get_value(hDB,0,"/Equipment/GalilFermi/Monitors/Motor Status",&MotorStatus,&size_MotorStatus,TID_BOOL,0);
      mlock.unlock();
      if (MotorStatus[0] == FALSE || MotorStatus[1] == FALSE){
	if (command == 1 || command == 2 || command == 6 || command == 7){
	  command = 0;
	  mlock.lock();
	  cm_msg(MINFO,"ManualCtrl","This command requires the trolley motors being ON.");
	  db_set_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Cmd",&command,sizeof(command), 1 ,TID_INT); 
	  mlock.unlock();
	}
      }
      if (MotorStatus[2] == FALSE){
	if (command == 5 || command == 6 || command == 7){
	  command = 0;
	  mlock.lock();
	  cm_msg(MINFO,"ManualCtrl","This command requires the garage motor being ON.");
	  db_set_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Cmd",&command,sizeof(command), 1 ,TID_INT); 
	  mlock.unlock();
	}
      }
    }
    if (command == 1){
      INT RelPos1;
      INT RelPos2;
      INT Size_Int = sizeof(RelPos1);
      mlock.lock();
      db_get_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Trolley/Trolley1 Rel Pos",&RelPos1,&Size_Int,TID_INT,0);
      db_get_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Trolley/Trolley2 Rel Pos",&RelPos2,&Size_Int,TID_INT,0);
      mlock.unlock();
      sprintf(CmdBuffer,"PR %d,%d",RelPos1,RelPos2);
      if (!PreventManualCtrl){
	mlock.lock();
	GCmd(g,"KPA=100");
	GCmd(g,"KPB=100");
	GCmd(g,CmdBuffer);
	GCmd(g,"BGAB");
	mlock.unlock();
      }else{
	cm_msg(MINFO,"ManualCtrl","Manual control is prevented during an run.");
      }

      //Block before the motion is done
      INT CurrentVelocities[6];
      INT CurrentVelocities_size = sizeof(CurrentVelocities);

      while(1){
	sleep(1);
	db_get_value(hDB,0,"/Equipment/GalilFermi/Monitors/Velocities",CurrentVelocities,&CurrentVelocities_size,TID_INT,0);
	if (CurrentVelocities[0]==0 && CurrentVelocities[1]==0)break;
      }

      command=0;
      mlock.lock();
      GCmd(g,"KPA=0");
      GCmd(g,"KPB=0");
      db_set_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Cmd",&command,sizeof(command), 1 ,TID_INT); 
      mlock.unlock();
    }else if (command == 2){
      INT RelPos;
      INT Size_Int = sizeof(RelPos);
      INT CurrentPos[6];
      INT Size_Current_Pos = sizeof(CurrentPos);
      mlock.lock();
      db_get_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Trolley/Trolley Rel Pos",&RelPos,&Size_Int,TID_INT,0);
      db_get_value(hDB,0,"/Equipment/GalilFermi/Monitors/Positions",CurrentPos,&Size_Current_Pos,TID_INT,0);
      mlock.unlock();

      if (!PreventManualCtrl){
	mlock.lock();
	//GCmd(g,"SHA");
	//GCmd(g,"SHB");
	if (RelPos >= 0){
	  destiny = CurrentPos[0]+RelPos;
	  sprintf(CmdBuffer,"destiny=%d",destiny);
	  GCmd(g,CmdBuffer);
	  GCmd(g,"tlfinish=0");
	  GCmd(g,"XQ #FDMTN,1");
	}else{
	  destiny = CurrentPos[1]-RelPos;
	  sprintf(CmdBuffer,"destiny=%d",destiny);
	  GCmd(g,CmdBuffer);
	  GCmd(g,"tlfinish=0");
	  GCmd(g,"XQ #BKMTN,1");
	}
	mlock.unlock();
      }else{
	cm_msg(MINFO,"ManualCtrl","Manual control is prevented during an run.");
      }
      //Block before the motion is done
      while(1){
	int trolley_complete = 0;
	mlock.lock();
	GCmdI(g,"tlfinish=?",&trolley_complete);
	mlock.unlock();
	if (trolley_complete==1){
	  break;
	}
	sleep(1);
      }

      command=0;
      mlock.lock();
      db_set_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Cmd",&command,sizeof(command), 1 ,TID_INT);
      mlock.unlock();
    }else if (command == 3){
      INT DefTrolleyPos1;
      INT DefTrolleyPos2;
      INT DefGaragePos;
      INT Size_Int = sizeof(DefTrolleyPos1);

      mlock.lock();
      db_get_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Trolley/Trolley Def Pos1",&DefTrolleyPos1,&Size_Int,TID_INT,0);
      db_get_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Trolley/Trolley Def Pos2",&DefTrolleyPos2,&Size_Int,TID_INT,0);
      db_get_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Trolley/Garage Def Pos",&DefGaragePos,&Size_Int,TID_INT,0);
      mlock.unlock();
      sprintf(CmdBuffer,"DP %d,%d,%d",DefTrolleyPos1,DefTrolleyPos2,DefGaragePos);

      if (!PreventManualCtrl){
	mlock.lock();
	GCmd(g,CmdBuffer);
	mlock.unlock();
      }else{
	cm_msg(MINFO,"ManualCtrl","Manual control is prevented during an run.");
      }
      command=0;
      mlock.lock();
      db_set_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Cmd",&command,sizeof(command), 1 ,TID_INT); 
      mlock.unlock();
    }else if (command == 4){
      BOOL TrolleySwitch;
      BOOL GarageSwitch;
      INT Size_BOOL = sizeof(TrolleySwitch);
      mlock.lock();
      db_get_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Trolley/Trolley Switch",&TrolleySwitch,&Size_BOOL,TID_BOOL,0);
      db_get_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Trolley/Garage Switch",&GarageSwitch,&Size_BOOL,TID_BOOL,0);
      if (TrolleySwitch){
	GCmd(g, "SHA");
	GCmd(g, "SHB");
      }else{
	GCmd(g, "MOA");
	GCmd(g, "MOB");
      }
      if (GarageSwitch){
	GCmd(g, "SHC");
      }else{
	GCmd(g, "MOC");
      }
      mlock.unlock();
      command=0;
      mlock.lock();
      db_set_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Cmd",&command,sizeof(command), 1 ,TID_INT); 
      mlock.unlock();
    }else if (command == 5){
      INT RelPos;
      INT Size_Int = sizeof(RelPos);
      mlock.lock();
      db_get_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Trolley/Garage Rel Pos",&RelPos,&Size_Int,TID_INT,0);
      mlock.unlock();
      sprintf(CmdBuffer,"PRC=%d",RelPos);
      if (!PreventManualCtrl){
	mlock.lock();
	GCmd(g,"KPC=100");
	GCmd(g,CmdBuffer);
	GCmd(g,"BGC");
	mlock.unlock();
      }else{
	cm_msg(MINFO,"ManualCtrl","Manual control is prevented during an run.");
      }

      //Block before the motion is done
      INT CurrentVelocities[6];
      INT CurrentVelocities_size = sizeof(CurrentVelocities);

      while(1){
	sleep(1);
	db_get_value(hDB,0,"/Equipment/GalilFermi/Monitors/Velocities",CurrentVelocities,&CurrentVelocities_size,TID_INT,0);
	if (CurrentVelocities[2]==0)break;
      }

      command=0;
      mlock.lock();
      GCmd(g,"KPC=0");
      db_set_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Cmd",&command,sizeof(command), 1 ,TID_INT); 
      mlock.unlock();
    }else if (command == 6){
      if (!PreventManualCtrl){
	mlock.lock();
	sprintf(CmdBuffer,"gipos=%d",100);
	GCmd(g,CmdBuffer);
	GCmd(g,"grfinish=0");
	GCmd(g,"XQ #GRIN,1");
	mlock.unlock();
      }else{
	cm_msg(MINFO,"ManualCtrl","Manual control is prevented during an run.");
      }
      //Block before the motion is done
      while(1){
	int garage_complete = 0;
	mlock.lock();
	GCmdI(g,"grfinish=?",&garage_complete);
	mlock.unlock();
	if (garage_complete==1){
	  break;
	}
	sleep(1);
      }

      command=0;
      mlock.lock();
      db_set_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Cmd",&command,sizeof(command), 1 ,TID_INT);
      mlock.unlock();
    }else if (command == 7){
      if (!PreventManualCtrl){
	mlock.lock();
	sprintf(CmdBuffer,"gopos=%d",1010000);
	GCmd(g,CmdBuffer);
	GCmd(g,"grfinish=0");
	GCmd(g,"XQ #GROUT,1");
	mlock.unlock();
      }else{
	cm_msg(MINFO,"ManualCtrl","Manual control is prevented during an run.");
      }
      //Block before the motion is done
      while(1){
	int garage_complete = 0;
	mlock.lock();
	GCmdI(g,"grfinish=?",&garage_complete);
	mlock.unlock();
	if (garage_complete==1){
	  break;
	}
	sleep(1);
      }

      command=0;
      mlock.lock();
      db_set_value(hDB,0,"/Equipment/GalilFermi/Settings/Manual Control/Cmd",&command,sizeof(command), 1 ,TID_INT);
      mlock.unlock();
    }
    i++;
    usleep(10000);
  }
  ControlThreadActive = 0;
  mlock.lock();
  db_set_value(hDB,0,"/Equipment/GalilFermi/Monitors/Control Thread Active",&ControlThreadActive,sizeof(ControlThreadActive), 1 ,TID_BOOL); 
  mlock.unlock();
}
