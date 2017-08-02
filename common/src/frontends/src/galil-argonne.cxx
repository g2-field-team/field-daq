/********************************************************************\

Name:         galil-argonne.cxx
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
//#include <pqxx/pqxx>
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

   {"Galil-Argonne",                /* equipment name */
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
  unsigned int TimeStamp; //Galil time is unsigned int
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

//double TempCalCoefficients[6] = {24.9724 , 11.4814 , -0.64452 , 1.35996 , -0.528525 , 0.107673};

INT StepSize[6];
INT StepNumber[6];
int IX,IY,IZ;

vector<GalilDataStruct> GalilDataBuffer;
const char * const galil_platform_bank_name = "GLPF"; // 4 letters, try to make sensible
g2field::galil_platform_t GalilPlatformDataCurrent;

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

//std::unique_ptr<pqxx::connection> psql_con; //PSQL connection handle

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
  db_get_value(hDB,0,"/Equipment/Galil-Argonne/Common/Status Color",&odbColour,&size,TID_STRING,FALSE);
  strcpy(odbColour, "#8A2BE2");
  db_set_value(hDB,0,"/Equipment/Galil-Argonne/Common/Status Color",&odbColour,size,1,TID_STRING);

  b=GOpen("192.168.1.13 -s ALL -t 1000 -d",&g);
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
/*  const char * host = "g2db-priv";
  const char * dbname = "gm2_online_prod";
  const char * user = "gm2_writer";
  const char * password = "";
  const char * port = "5433";
  std::string psql_con_str = std::string("host=") + std::string(host) + std::string(" dbname=")\
			     + std::string(dbname) + std::string(" user=") + std::string(user) \
			  //   + std::string(" password=") + std::string(password) 
			     + std::string(" port=")\
			     + std::string(port);
  cout << psql_con_str<<endl;
  psql_con = std::make_unique<pqxx::connection>(psql_con_str);
*/
  //Set the motor switch odb values to off as default
  BOOL MOTOR_OFF = FALSE;
  db_set_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Platform1/Platform1 Switch",&MOTOR_OFF,sizeof(MOTOR_OFF), 1 ,TID_BOOL); 
  db_set_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Platform2/Platform2  Switch",&MOTOR_OFF,sizeof(MOTOR_OFF), 1 ,TID_BOOL); 
  //Load script
  char ScriptName[500];
  INT ScriptName_size = sizeof(ScriptName);
  db_get_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Cmd Script",ScriptName,&ScriptName_size,TID_STRING,0);
  char DirectName[500];
  INT DirectName_size = sizeof(DirectName);
  db_get_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Script Directory",DirectName,&DirectName_size,TID_STRING,0);
  string FullScriptName = string(DirectName)+string(ScriptName)+string(".dmc");
  cm_msg(MINFO,"init","Galil Script to load: %s",FullScriptName.c_str());
  //Clear command
  INT command=0;
  db_set_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Cmd",&command,sizeof(command), 1 ,TID_INT); 

  //Flip the AllStop bit and then motions are allowed
  GCmd(g, "CB 2");
  sleep(1);
  GCmd(g, "SB 2");

  b=GProgramDownload(g,"",0); //to erase prevoius programs
  b=GProgramDownloadFile(g,FullScriptName.c_str(),0);
  cm_msg(MINFO,"init","Galil Program Download return code: %d",b);
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

	db_get_value(hDB,0,"/Equipment/Galil-Argonne/Common/Status Color",&odbColour,&size,TID_STRING,FALSE);
	strcpy(odbColour, "greenLight");
	db_set_value(hDB,0,"/Equipment/Galil-Argonne/Common/Status Color",&odbColour,size,1,TID_STRING);
  return SUCCESS;
}

/*-- Frontend Exit -------------------------------------------------*/

INT frontend_exit()
{
  mlock.lock();
  GCmd(g,"AB 1");
  //Set the motor switch odb values to off
  BOOL MOTOR_OFF = FALSE;
  db_set_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Platform1/Platform1 Switch",&MOTOR_OFF,sizeof(MOTOR_OFF), 1 ,TID_BOOL); 
  db_set_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Platform2/Platform2  Switch",&MOTOR_OFF,sizeof(MOTOR_OFF), 1 ,TID_BOOL); 
  //Clear command
  INT command=0;
  db_set_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Cmd",&command,sizeof(command), 1 ,TID_INT); 
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
  db_get_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Root Output",&write_root,&write_root_size,TID_BOOL, 0);
  
  //Get Data dir
  string DataDir;
  char str[500];
  int str_size = sizeof(str);
  db_get_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Root Dir",&str,&str_size,TID_STRING, 0);
  DataDir=string(str);
  
  //Root File Name
  sprintf(str,"GalilOutput_%05d.root",RunNumber);
  string RootFileName = DataDir + string(str);

  if(write_root){
    cm_msg(MINFO,"begin_of_run","Writing to root file %s",RootFileName.c_str());
    pf = new TFile(RootFileName.c_str(), "recreate");
    pt_norm = new TTree("t_Galil-Argonne", "Galil Data");
    pt_norm->SetAutoSave(5);
    pt_norm->SetAutoFlush(20);

    pt_norm->Branch(galil_platform_bank_name, &GalilPlatformDataCurrent, g2field::galil_platform_str);
  }

  //Clear history data in buffer
  GalilDataBuffer.clear();
  //Turn the Run Active Flag to true
  RunActive = true;

  /*
  INT StepSize_size = sizeof(StepSize);
  INT StepNumber_size = sizeof(StepNumber);
  db_get_value(hDB,0,"/Equipment/Galil-Argonne/AutoControl/RelPos",StepSize,&StepSize_size,TID_INT,0);
  db_get_value(hDB,0,"/Equipment/Galil-Argonne/AutoControl/StepNumber",StepNumber,&StepNumber_size,TID_INT,0);

  cm_msg(MINFO,"read_event","Step Sizes: %d %d %d %d",StepSize[0],StepSize[1],StepSize[2],StepSize[3]);
  cm_msg(MINFO,"read_event","Step Numbers: %d %d %d %d",StepNumber[0],StepNumber[1],StepNumber[2],StepNumber[3]);
 
  IX=IY=IZ=IS=0;

  //Check if it is in test mode. if so, do not interact with the abs-probe frontend
  INT TestMode_size = sizeof(TestMode);
  db_get_value(hDB,0,"/Equipment/Galil-Argonne/Settings/TestMode",&TestMode,&TestMode_size,TID_BOOL,0);

  if(TestMode){
    ReadyToMove = true;
  }else{
    ReadyToMove = false;
  }
  BOOL temp_bool = BOOL(ReadyToMove);
  db_set_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/ReadyToMove",&temp_bool,sizeof(temp_bool), 1 ,TID_BOOL);
*/
  //PreventManualCtrl = true;
/*
  //Init alarm-watched odb value
  INT Finished=0;
  db_set_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Finished",&Finished,sizeof(Finished), 1 ,TID_INT);
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
 // db_set_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Finished",&Finished,sizeof(Finished), 1 ,TID_INT);
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
  WORD *pdataPlatform;

  //Init bank
  bk_init32(pevent);
  
  //Write data to banks
  bk_create(pevent, galil_platform_bank_name, TID_WORD, (void **)&pdataPlatform);

  for (int i=0;i<GALILREADGROUPSIZE;i++){
    mlockdata.lock();
    GalilPlatformDataCurrent.TimeStamp = ULong64_t(GalilDataBuffer[i].TimeStamp);
    for (int j=0;j<3;j++){
      GalilPlatformDataCurrent.Positions[j] = GalilDataBuffer[i].PositionArray[j];
      GalilPlatformDataCurrent.Velocities[j] = GalilDataBuffer[i].VelocityArray[j];
      GalilPlatformDataCurrent.OutputVs[j] = GalilDataBuffer[i].OutputVArray[j];
    }

    //Write to ROOT file
    if (write_root) {
      pt_norm->Fill();
    }
    mlockdata.unlock();
  } 
  bk_close(pevent,pdataPlatform);

  mlockdata.lock();
  GalilDataBuffer.erase(GalilDataBuffer.begin(),GalilDataBuffer.begin()+GALILREADGROUPSIZE);
  mlockdata.unlock();

  //Write to ROOT file
  if (write_root) {
    pt_norm->AutoSave("SaveSelf,FlushBaskets");
    pf->Flush();
  }
  /*
     ReadyToMove = false;
     BOOL temp_bool = BOOL(ReadyToMove);
  db_set_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/ReadyToMove",&temp_bool,sizeof(temp_bool), 1 ,TID_BOOL);

  char CmdBuffer[500];
  //Regeister current position from odb
  INT CurrentPositions[4];
  INT CurrentPositions_size = sizeof(CurrentPositions);
  db_get_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Positions",CurrentPositions,&CurrentPositions_size,TID_INT,0);
  INT CurrentVelocities[4];
  INT CurrentVelocities_size = sizeof(CurrentPositions);
  db_get_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Velocities",CurrentVelocities,&CurrentVelocities_size,TID_INT,0);
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
	  db_set_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Finished",&Finished,sizeof(Finished), 1 ,TID_INT);
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
    db_get_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Velocities",CurrentVelocities,&CurrentVelocities_size,TID_INT,0);
    if (CurrentVelocities[0]==0 && CurrentVelocities[1]==0 && CurrentVelocities[2]==0 && CurrentVelocities[3]==0)break;
  //  cm_msg(MINFO,"read_event","V = %d",CurrentVelocities[0]);
    sleep(1);
  }

  //If in test mode, start to move. If not, tell abs-probe to read
  if(TestMode){
    ReadyToMove = true;
    temp_bool = BOOL(ReadyToMove);
    db_set_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/ReadyToMove",&temp_bool,sizeof(temp_bool), 1 ,TID_BOOL);
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
  db_set_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Monitor Thread Active",&MonitorThreadActive,sizeof(MonitorThreadActive), 1 ,TID_BOOL); 
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
  double Time0;
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
	//Convert to unsigned int
	GalilDataUnit.TimeStamp = (unsigned int)(Time);
	Time0 = double (GalilDataUnit.TimeStamp);
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
      }else if(Header[0]=='?' || Header.compare("ERROR")==0){
	BOOL Tripped = TRUE;
	mlock.lock();
	GCmd(g,"AB 1");
	//Set the motor switch odb values to off
	BOOL MOTOR_OFF = FALSE;
	db_set_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Platform1/Platform1 Switch",&MOTOR_OFF,sizeof(MOTOR_OFF), 1 ,TID_BOOL); 
	db_set_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Platform2/Platform2  Switch",&MOTOR_OFF,sizeof(MOTOR_OFF), 1 ,TID_BOOL); 
	db_set_value(hDB,0,"/Equipment/Galil-Argonne/Alarms/Motion Trip",&Tripped,sizeof(Tripped), 1 ,TID_BOOL); 
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
/*    float T1 = GalilDataUnitD.AnalogArray[2];
    float T2 = GalilDataUnitD.AnalogArray[3];
    T1 = TempCalCoefficients[0] + TempCalCoefficients[1]*T1 + TempCalCoefficients[2]*pow(T1,2) + TempCalCoefficients[3]*pow(T1,3) + TempCalCoefficients[4]*pow(T1,4) + TempCalCoefficients[5]*pow(T1,5);
    T2 = TempCalCoefficients[0] + TempCalCoefficients[1]*T2 + TempCalCoefficients[2]*pow(T2,2) + TempCalCoefficients[3]*pow(T2,3) + TempCalCoefficients[4]*pow(T2,4) + TempCalCoefficients[5]*pow(T2,5);
    float Tension1 = GalilDataUnitD.AnalogArray[0]*4.0;
    float Tension2 = (GalilDataUnitD.AnalogArray[1]-0.18)*4.0;
*/

    mlock.lock();
    db_set_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Time Stamp",&Time0,sizeof(Time0), 1 ,TID_DOUBLE); 
    db_set_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Positions",&GalilDataUnit.PositionArray,sizeof(GalilDataUnit.PositionArray), 6 ,TID_INT); 
    db_set_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Velocities",&GalilDataUnit.VelocityArray,sizeof(GalilDataUnit.VelocityArray), 6 ,TID_INT); 
    db_set_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Control Voltages",&GalilDataUnit.OutputVArray,sizeof(GalilDataUnit.OutputVArray), 6 ,TID_INT); 
    db_set_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Analogs",&GalilDataUnit.AnalogArray,sizeof(GalilDataUnit.AnalogArray), 6 ,TID_INT);
    db_set_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Limit Switches Forward",&GalilDataUnit.LimFArray,sizeof(GalilDataUnit.LimFArray), 6 ,TID_INT);
    db_set_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Limit Switches Reverse",&GalilDataUnit.LimRArray,sizeof(GalilDataUnit.LimRArray), 6 ,TID_INT);
    db_set_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Motor Status",&GalilDataUnit.StatusArray,sizeof(GalilDataUnit.StatusArray), 6 ,TID_BOOL);
    db_set_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Buffer Load",&BufferLoad,sizeof(BufferLoad), 1 ,TID_INT);
    
    mlock.unlock();

    //Load to Data base
/*    if (i%5000==0){
      mlock.lock();
      char message_buffer[512];
      sprintf(message_buffer,"{%f,%f}",T1,T2);

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
	  "INSERT into gm2field_monitor (name, value, time) VALUES ('";
	query_str += "Trolley Motor Temperatures";
	query_str += "', '";
	query_str += string(message_buffer);
	query_str += "', now())";
	txnp->exec(query_str);
	txnp->commit();
      }
      mlock.unlock();
    }
*/
    //Check emergencies
    //This thread is not blocking
    INT emergency_size = sizeof(emergency);
    //Abort
    db_get_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Emergency/Abort",&emergency,&emergency_size,TID_INT,0);
    if (emergency == 1){
      mlock.lock();
      GCmd(g,"AB 1");
      //Set the motor switch odb values to off
      BOOL MOTOR_OFF = FALSE;
      db_set_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Platform1/Platform1 Switch",&MOTOR_OFF,sizeof(MOTOR_OFF), 1 ,TID_BOOL); 
      db_set_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Platform2/Platform2 Switch",&MOTOR_OFF,sizeof(MOTOR_OFF), 1 ,TID_BOOL); 
      
//      INT Finished=2;
//      db_set_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Finished",&Finished,sizeof(Finished), 1 ,TID_INT);
      mlock.unlock();
      cm_msg(MINFO,"Emergency","Motion Aborted.");
    }
    emergency=0;
    db_set_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Emergency/Abort",&emergency,sizeof(emergency), 1 ,TID_INT); 

    i++;
  }
  MonitorThreadActive = 0;
  mlock.lock();
  db_set_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Monitor Thread Active",&MonitorThreadActive,sizeof(MonitorThreadActive), 1 ,TID_BOOL); 
  mlock.unlock();
}

//GalilControl
void GalilControl(const GCon &g){
  int ControlThreadActive = 1;
  mlock.lock();
  db_set_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Control Thread Active",&ControlThreadActive,sizeof(ControlThreadActive), 1 ,TID_BOOL); 
  mlock.unlock();
  char buffer[50000];

  hkeyclient=0;
  int  rc = GALIL_EXAMPLE_OK; //return code
  INT command = 0;
  INT emergency = 0;

  int position =0;
//  timeb starttime,currenttime;
//  ftime(&starttime);

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
    db_get_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Cmd",&command,&command_size,TID_INT,0);
    mlock.unlock();
    char CmdBuffer[500];

    //Determine whether trolley or garage motions are allowed
    BOOL MotionAllowed = FALSE;
    BOOL ExpertApprove = FALSE;
    INT sizeBOOL = sizeof(MotionAllowed);

    mlock.lock();
    db_get_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Interlock/Expert Approve",&ExpertApprove,&sizeBOOL,TID_BOOL,0);
    mlock.unlock();

    if (ExpertApprove==TRUE){
      MotionAllowed = TRUE;
    }

    mlock.lock();
    db_set_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Motion Allowed",&MotionAllowed,sizeof(MotionAllowed), 1 ,TID_BOOL); 
    mlock.unlock();

    //For trolley commands, load galil variables first
    if (command >= 1 && command <=7){
      if (!PreventManualCtrl){
	;
      }else{
	cm_msg(MINFO,"ManualCtrl","Manual control is prevented during an run.");
      }
      //Safety checks
      //Do not execute command if motions are not allowed
      if (MotionAllowed == FALSE){
	if (command == 1 || command == 2){
	  BOOL BadAttempt = TRUE;
	  command = 0;
	  mlock.lock();
	  cm_msg(MINFO,"ManualCtrl","Motion is not allowed. Check prerequisits.");
	  db_set_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Cmd",&command,sizeof(command), 1 ,TID_INT); 
	  db_set_value(hDB,0,"/Equipment/Galil-Argonne/Alarms/Bad Motion Attempt",&BadAttempt,sizeof(BadAttempt), 1 ,TID_BOOL); 
	  mlock.unlock();
	}
      }
      //Do not execute command if corresponding motors are not ON, or limit switches triggered
      BOOL MotorStatus[6];
      INT FLimitSwitch[6];
      INT RLimitSwitch[6];
      INT size_MotorStatus = sizeof(MotorStatus);
      INT size_LimitSwitches = sizeof(FLimitSwitch);
      mlock.lock();
      db_get_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Motor Status",&MotorStatus,&size_MotorStatus,TID_BOOL,0);
      db_get_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Limit Switches Forward",&FLimitSwitch,&size_LimitSwitches,TID_INT,0);
      db_get_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Limit Switches Reverse",&RLimitSwitch,&size_LimitSwitches,TID_INT,0);
      mlock.unlock();
      if (MotorStatus[0] == FALSE || MotorStatus[1] == FALSE || MotorStatus[2] == FALSE || MotorStatus[3] == FALSE){
	if (command == 1 || command == 2 ){
	  command = 0;
	  mlock.lock();
	  cm_msg(MINFO,"ManualCtrl","This command requires the Platform1 motors being ON.");
	  db_set_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Cmd",&command,sizeof(command), 1 ,TID_INT); 
	  mlock.unlock();
	}
      }
      if (MotorStatus[4] == FALSE){
	if (command == 5 || command == 6){
	  command = 0;
	  mlock.lock();
	  cm_msg(MINFO,"ManualCtrl","This command requires the Platform2 motor being ON.");
	  db_set_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Cmd",&command,sizeof(command), 1 ,TID_INT); 
	  mlock.unlock();
	}
      }
    }
    if (command == 1){
      INT AbsPos[4];
      INT P_size = sizeof(AbsPos);
      db_get_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Platform1/Platform1 Abs Pos",AbsPos,&P_size,TID_INT,0);
      sprintf(CmdBuffer,"PA %d,%d,%d,%d",AbsPos[0],AbsPos[1],AbsPos[2],AbsPos[3]);
      if (!PreventManualCtrl){
	mlock.lock();
	GCmd(g,CmdBuffer);
	GCmd(g,"BGABCD");
	mlock.unlock();
      }else{
	cm_msg(MINFO,"ManualCtrl","Manual control is prevented during an run.");
      }

      //Block before the motion is done
      INT CurrentVelocities[6];
      INT CurrentVelocities_size = sizeof(CurrentVelocities);

      while(1){
	sleep(1);
	mlock.lock();
	db_get_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Velocities",CurrentVelocities,&CurrentVelocities_size,TID_INT,0);
	mlock.unlock();
	if (CurrentVelocities[0]==0 && CurrentVelocities[1]==0 && CurrentVelocities[2]==0 && CurrentVelocities[3]==0)break;
      }

      command=0;
      mlock.lock();
      db_set_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Cmd",&command,sizeof(command), 1 ,TID_INT);
      mlock.unlock();
    }else if (command == 2){
      INT RelPos[4];
      INT P_size = sizeof(RelPos);
      db_get_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Platform1/Platform1 Rel Pos",RelPos,&P_size,TID_INT,0);
      sprintf(CmdBuffer,"PR %d,%d,%d,%d",RelPos[0],RelPos[1],RelPos[2],RelPos[3]);
      if (!PreventManualCtrl){
	mlock.lock();
	GCmd(g,CmdBuffer);
	GCmd(g,"BGABCD");
	mlock.unlock();
      }else{
	cm_msg(MINFO,"ManualCtrl","Manual control is prevented during an run.");
      }

      //Block before the motion is done
      INT CurrentVelocities[6];
      INT CurrentVelocities_size = sizeof(CurrentVelocities);

      while(1){
	sleep(1);
	mlock.lock();
	db_get_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Velocities",CurrentVelocities,&CurrentVelocities_size,TID_INT,0);
	mlock.unlock();
	if (CurrentVelocities[0]==0 && CurrentVelocities[1]==0 && CurrentVelocities[2]==0 && CurrentVelocities[3]==0)break;
      }

      command=0;
      mlock.lock();
      db_set_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Cmd",&command,sizeof(command), 1 ,TID_INT);
      mlock.unlock();
    }else if (command == 3){
      INT DefPos[4];
      INT P_size = sizeof(DefPos);
      db_get_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Platform1/Platform1 Def Pos",DefPos,&P_size,TID_INT,0);
      sprintf(CmdBuffer,"DP %d,%d,%d,%d",DefPos[0],DefPos[1],DefPos[2],DefPos[3]);
      if (!PreventManualCtrl){
	mlock.lock();
	GCmd(g,CmdBuffer);
	mlock.unlock();
      }else{
	cm_msg(MINFO,"ManualCtrl","Manual control is prevented during an run.");
      }

      command=0;
      mlock.lock();
      db_set_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Cmd",&command,sizeof(command), 1 ,TID_INT);
      mlock.unlock();
    }else if (command == 4){
      BOOL Switch;
      INT Size_BOOL = sizeof(Switch);
      mlock.lock();
      db_get_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Platform1/Platform1 Switch",&Switch,&Size_BOOL,TID_BOOL,0);
      if (Switch){
	GCmd(g, "SHA");
	GCmd(g, "SHB");
	GCmd(g, "SHC");
	GCmd(g, "SHD");
      }else{
	GCmd(g, "MOA");
	GCmd(g, "MOB");
	GCmd(g, "MOC");
	GCmd(g, "MOD");
      }
      mlock.unlock();
      command=0;
      mlock.lock();
      db_set_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Cmd",&command,sizeof(command), 1 ,TID_INT);
      mlock.unlock();
    }else if (command == 5){
      INT AbsPos;
      INT P_size = sizeof(AbsPos);
      db_get_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Platform2/Platform2 Abs Pos",&AbsPos,&P_size,TID_INT,0);
      sprintf(CmdBuffer,"PAE=%d",AbsPos);
      if (!PreventManualCtrl){
	mlock.lock();
	GCmd(g,CmdBuffer);
	GCmd(g,"BGE");
	mlock.unlock();
      }else{
	cm_msg(MINFO,"ManualCtrl","Manual control is prevented during an run.");
      }

      //Block before the motion is done
      INT CurrentVelocities[6];
      INT CurrentVelocities_size = sizeof(CurrentVelocities);

      while(1){
	sleep(1);
	mlock.lock();
	db_get_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Velocities",CurrentVelocities,&CurrentVelocities_size,TID_INT,0);
	mlock.unlock();
	if (CurrentVelocities[4]==0)break;
      }

      command=0;
      mlock.lock();
      db_set_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Cmd",&command,sizeof(command), 1 ,TID_INT);
      mlock.unlock();
    }else if (command == 6){
      INT RelPos;
      INT P_size = sizeof(RelPos);
      db_get_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Platform2/Platform2 Rel Pos",&RelPos,&P_size,TID_INT,0);
      sprintf(CmdBuffer,"PRE=%d",RelPos);
      if (!PreventManualCtrl){
	mlock.lock();
	GCmd(g,CmdBuffer);
	GCmd(g,"BGE");
	mlock.unlock();
      }else{
	cm_msg(MINFO,"ManualCtrl","Manual control is prevented during an run.");
      }

      //Block before the motion is done
      INT CurrentVelocities[6];
      INT CurrentVelocities_size = sizeof(CurrentVelocities);

      while(1){
	sleep(1);
	mlock.lock();
	db_get_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Velocities",CurrentVelocities,&CurrentVelocities_size,TID_INT,0);
	mlock.unlock();
	if (CurrentVelocities[4]==0)break;
      }

      command=0;
      mlock.lock();
      db_set_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Cmd",&command,sizeof(command), 1 ,TID_INT);
      mlock.unlock();
    }else if (command == 7){
      INT DefPos;
      INT P_size = sizeof(DefPos);
      db_get_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Platform2/Platform2 Def Pos",&DefPos,&P_size,TID_INT,0);
      sprintf(CmdBuffer,"DP ,,,,%d",DefPos);
      if (!PreventManualCtrl){
	mlock.lock();
	GCmd(g,CmdBuffer);
	mlock.unlock();
      }else{
	cm_msg(MINFO,"ManualCtrl","Manual control is prevented during an run.");
      }

      command=0;
      mlock.lock();
      db_set_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Cmd",&command,sizeof(command), 1 ,TID_INT);
      mlock.unlock();
    }else if (command == 8){
      BOOL Switch;
      INT Size_BOOL = sizeof(Switch);
      mlock.lock();
      db_get_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Platform2/Platform2 Switch",&Switch,&Size_BOOL,TID_BOOL,0);
      if (Switch){
	GCmd(g, "SHE");
      }else{
	GCmd(g, "MOE");
      }
      mlock.unlock();
      command=0;
      mlock.lock();
      db_set_value(hDB,0,"/Equipment/Galil-Argonne/Settings/Manual Control/Cmd",&command,sizeof(command), 1 ,TID_INT);
      mlock.unlock();
    }
    i++;
    usleep(10000);
  }
  ControlThreadActive = 0;
  mlock.lock();
  db_set_value(hDB,0,"/Equipment/Galil-Argonne/Monitors/Control Thread Active",&ControlThreadActive,sizeof(ControlThreadActive), 1 ,TID_BOOL); 
  mlock.unlock();
}
