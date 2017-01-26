/********************************************************************\

Name:         gm2GalilFe.cxx
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
#include "field_constants.hh"
#include "field_structs.hh"
#include "TTree.h"
#include "TFile.h"

#define FRONTEND_NAME "Galil Motion Control" // Prefer capitalize with spaces

#define GALIL_EXAMPLE_OK G_NO_ERROR //return code for correct code execution
#define GALIL_EXAMPLE_ERROR -100
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

  INT read_galil_event(char *pevent, INT off);

  INT poll_event(INT source, INT count, BOOL test);
  INT interrupt_configure(INT cmd, INT source, POINTER_T adr);


  /*-- Equipment list ------------------------------------------------*/


  EQUIPMENT equipment[] = {


    {"Galil",                /* equipment name */
      {EVENTID_GALIL, 0,                   /* event ID, trigger mask */
	"SYSTEM",               /* event buffer */
	EQ_POLLED,            /* equipment type */
	0,                      /* event source */
	"MIDAS",                /* format */
	TRUE,                   /* enabled */
	RO_RUNNING|   /* read when running and on odb */
	RO_ODB,
	100,                  /* poll every 0.1 sec */
	0,                      /* stop run after this event limit */
	0,                      /* number of sub events */
	0,                      /* log history, logged once per minute */
	"", "", "",},
      read_galil_event,       /* readout routine */
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

vector<g2field::galil_data_t> GalilDataBuffer;
const char * const galil_bank_name = "GALI"; // 4 letters, try to make sensible
g2field::galil_data_t GalilDataCurrent;

thread read_thread;
mutex mlock;
mutex mlockdata;

void GetGalilMessage(const GCon &g);
bool RunActive;

GCon g = 0; //var used to refer to a unique connection. A valid connection is nonzero.

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
  }

//  GTimeout(g,2000);//adjust timeout
  //-------------end code to communicate with Galil------------------

  return SUCCESS;
}

/*-- Frontend Exit -------------------------------------------------*/

INT frontend_exit()
{
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
  db_get_value(hDB,0,"/Equipment/Galil/Settings/Root Output",&write_root,&write_root_size,TID_BOOL, 0);

  //Get Data dir
  string DataDir;
  char str[500];
  int str_size = sizeof(str);
  db_get_value(hDB,0,"/Equipment/Galil/Settings/Root Dir",&str,&str_size,TID_STRING, 0);
  DataDir=string(str);

  //Root File Name
  sprintf(str,"GalilOutput_%05d.root",RunNumber);
  string RootFileName = DataDir + string(str);

  if(write_root){
    cm_msg(MINFO,"begin_of_run","Writing to root file %s",RootFileName.c_str());
    pf = new TFile(RootFileName.c_str(), "recreate");
    pt_norm = new TTree("t_Galil", "Galil Data");
    pt_norm->SetAutoSave(5);
    pt_norm->SetAutoFlush(20);

    pt_norm->Branch(galil_bank_name, &GalilDataCurrent, g2field::galil_data_str);
  }
  
  //Load script
  GReturn b = G_NO_ERROR;
  char ScriptName[500];
  INT ScriptName_size = sizeof(ScriptName);
  db_get_value(hDB,0,"/Equipment/Galil/Settings/CmdScript",ScriptName,&ScriptName_size,TID_STRING,0);
  char DirectName[500];
  INT DirectName_size = sizeof(DirectName);
  db_get_value(hDB,0,"/Equipment/Galil/Settings/Script Directory",DirectName,&DirectName_size,TID_STRING,0);
  string FullScriptName = string(DirectName)+string(ScriptName)+string(".dmc");
  cm_msg(MINFO,"begin_of_run","Galil Script to load: %s",FullScriptName.c_str());

  GProgramDownload(g,"",0); //to erase prevoius programs
  //dump the buffer
  int rc = GALIL_EXAMPLE_OK; //return code
  char buffer[5000000];
  rc = GMessage(g, buffer, sizeof(buffer));
  b=GProgramDownloadFile(g,FullScriptName.c_str(),0);
  GCmd(g, "XQ #TH1,0");
  RunActive=true;
  //Start thread
  read_thread = thread(GetGalilMessage,g);
  return SUCCESS;
}

/*-- End of Run ----------------------------------------------------*/

INT end_of_run(INT run_number, char *error)
{
  mlock.lock();
  GCmd(g,"AB");
  mlock.unlock();
  cm_msg(MINFO,"end_of_run","Motion aborted.");

  mlock.lock();
  GCmd(g,"HX");
  cm_msg(MINFO,"end_of_run","Galil execution stopped.");
  mlock.unlock();

  mlock.lock();
  RunActive=false;
  mlock.unlock();
//  cm_msg(MINFO,"end_of_run","Trying to join threads.");
  read_thread.join();
  cm_msg(MINFO,"end_of_run","All threads joined.");
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
//  GCmd(g,"STA");
  return SUCCESS;
}

/*-- Resuem Run ----------------------------------------------------*/

INT resume_run(INT run_number, char *error)
{ 
//  GCmd(g,"SHA");
//  GCmd(g,"BGA"); 
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


INT read_galil_event(char *pevent, INT off){
  WORD *pdata;

  //Write to root files
  if (write_root) {
    for (int i=0;i<GALILREADGROUPSIZE;i++){
      mlockdata.lock();
      GalilDataCurrent = GalilDataBuffer[i];
      mlockdata.unlock();
      pt_norm->Fill();
    }
    pt_norm->AutoSave("SaveSelf,FlushBaskets");
    pf->Flush();
  }


  //Init bank
  bk_init32(pevent);

  //Write data to banks
  bk_create(pevent, galil_bank_name, TID_WORD, (void **)&pdata);
  
  mlockdata.lock();
  for (int i=0;i<GALILREADGROUPSIZE;i++){
    memcpy(pdata, &(GalilDataBuffer[i]), sizeof(g2field::galil_data_t));
    pdata += sizeof(g2field::galil_data_t)/sizeof(WORD);
  }
  GalilDataBuffer.erase(GalilDataBuffer.begin(),GalilDataBuffer.begin()+GALILREADGROUPSIZE);
  mlockdata.unlock();
  bk_close(pevent,pdata);

  return bk_size(pevent);
}

//GetGalilMessage
void GetGalilMessage(const GCon &g){
  int ReadThreadActive = 1;
  db_set_value(hDB,0,"/Equipment/Galil/Monitor/Read Thread Active",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
  char buffer[5000000];
  hkeyclient=0;
  string Header;
  int  rc = GALIL_EXAMPLE_OK; //return code
  /* residule string */
  string ResidualString = string("");

  //Readout loop
  int i=0;
  int jj=0;
  double Time,Time0;
  while (1){
    db_set_value(hDB,0,"/Equipment/Galil/Monitor/Read Loop Index",&i,sizeof(i), 1 ,TID_INT); 
    bool localRunActive;
    mlock.lock();
    localRunActive = RunActive;
    mlock.unlock();
    if (!localRunActive)break;
    //Read Message to buffer
    mlock.lock();
    rc = GMessage(g, buffer, sizeof(buffer));
    mlock.unlock();
    //if time out, sleep so that other threads are not blocked
    if (rc==-1100)sleep(1);
    //    ftime(&currenttime);
    //    double time = (currenttime.time-starttime.time)*1000 + (currenttime.millitm - starttime.millitm);
    //cout<<buffer<<endl;

    string BufString = string(buffer);
    //Add the residual from last read
    if (ResidualString.size()!=0)BufString = ResidualString+BufString;
    ResidualString.clear();

    size_t foundnewline = BufString.find("\n");
    //  static  bool flag = false;

    int iGalil = 0;
    g2field::galil_data_t GalilDataUnit;
    g2field::galil_data_d_t GalilDataUnitD;

    jj=0;
    while (foundnewline!=string::npos){
      stringstream iss (BufString.substr(0,foundnewline-1));
      // output returned by Galil is stored in the following variables

      iss >> Header;
      if(Header.compare("Galil")==0){
	//iss >> GalilDataUnit.TimeStamp;
	iss >> Time;
	if (i==0 && jj==0)Time0=Time;
	Time-=Time0;
	for (int j=0;j<2;j++){
	  iss >> GalilDataUnitD.Tensions[j];
	}
	for (int j=0;j<2;j++){
	  iss >> GalilDataUnitD.Positions[j];
	}
	for (int j=0;j<2;j++){
	  iss >> GalilDataUnitD.Velocities[j];
	}
	for (int j=0;j<2;j++){
	  iss >> GalilDataUnitD.OutputVs[j];
	}
	//Convert to INT
	GalilDataUnit.TimeStamp = ULong64_t(Time);
	for (int j=0;j<2;j++){
	  GalilDataUnit.Tensions[j] = Int_t(1000* GalilDataUnitD.Tensions[j]);
	}
	for (int j=0;j<2;j++){
	  GalilDataUnit.Positions[j] = Int_t(GalilDataUnitD.Positions[j]);
	}
	for (int j=0;j<2;j++){
	  GalilDataUnit.Velocities[j] = Int_t(GalilDataUnitD.Velocities[j]);
	}
	for (int j=0;j<2;j++){
	  GalilDataUnit.OutputVs[j] = Int_t(1000*GalilDataUnitD.OutputVs[j]);
	}

	mlockdata.lock();
	GalilDataBuffer.push_back(GalilDataUnit);
	mlockdata.unlock();

	iGalil++;
      }else if(Header.compare("Error")==0){
	mlock.lock();
	GCmd(g,"AB");
	mlock.unlock();
	cm_msg(MINFO,"Galil Message",BufString.substr(0,foundnewline-1).c_str());
	ReadThreadActive = 0;
	db_set_value(hDB,0,"/Equipment/Galil/Monitor/Read Thread Active",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL);
	return ;
      }else{
	cm_msg(MINFO,"Galil Message",BufString.substr(0,foundnewline-1).c_str());
      }


      BufString = BufString.substr(foundnewline+1,string::npos);
      foundnewline = BufString.find("\n");
      jj++;
    }
    //Monitors
    mlock.lock();
    db_set_value(hDB,0,"/Equipment/Galil/Monitor/Motor Positions",&GalilDataUnitD.Positions,sizeof(GalilDataUnitD.Positions), 3 ,TID_DOUBLE); 
    db_set_value(hDB,0,"/Equipment/Galil/Monitor/Motor Velocities",&GalilDataUnitD.Velocities,sizeof(GalilDataUnitD.Velocities), 3 ,TID_DOUBLE); 
    db_set_value(hDB,0,"/Equipment/Galil/Monitor/Tensions",&GalilDataUnitD.Tensions,sizeof(GalilDataUnitD.Tensions), 2 ,TID_DOUBLE); 
    //Monitor the buffer load
    INT BufferLoad = GalilDataBuffer.size();
    db_set_value(hDB,0,"/Equipment/Galil/Monitor/BufferLoad",&BufferLoad,sizeof(BufferLoad), 1 ,TID_INT); 
    mlock.unlock();
    if (BufString.size()!=0){
      ResidualString = BufString;
      //    cout <<ResidualString<<endl;
    }
    i++;
  }
  ReadThreadActive = 0;
  db_set_value(hDB,0,"/Equipment/Galil/Monitor/Read Thread Active",&ReadThreadActive,sizeof(ReadThreadActive), 1 ,TID_BOOL); 
}
