/********************************************************************\

Name:         abs-probe.cxx
Created by:   Matteo Bartolini
Modified by:  Ran Hong

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

#define FRONTEND_NAME "Absolute Probe" // Prefer capitalize with spaces

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

  INT read_event(char *pevent, INT off);

  INT poll_event(INT source, INT count, BOOL test);
  INT interrupt_configure(INT cmd, INT source, POINTER_T adr);


  /*-- Equipment list ------------------------------------------------*/


  EQUIPMENT equipment[] = {

    {"AbsoluteProbe",                /* equipment name */
      {1, 0,                   /* event ID, trigger mask */
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
const char * ODB_Setting_Base = "/Equipment/AbsoluteProbe/Settings/";

//Flags
bool ReadyToMove = false;
bool ReadyToRead = false;

//Globals
string EOFstr = "end_of_file";
string NMRProbeProgramDir;
bool IsDebug = false;

//Print out functions

int PrintToFileGlobal(string fn);
int PrintToFileFPGA(string fn);
int PrintToFileFG(string fn);
int PrintToFileADC(string fn);
int PrintToFileUtil(string fn);
int PrintToFileCom(string fn);
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
  INT Size = sizeof(RunNumber);
  cm_get_experiment_database(&hDB, NULL);
  db_get_value(hDB,0,"/Runinfo/Run number",&RunNumber,&Size,TID_INT, FALSE);

  char key[512];
  char temp_str[512];

  snprintf(key,512,"%s%s",ODB_Setting_Base,"NMRProbe Program Dir");
  Size = sizeof(temp_str);
  db_get_value(hDB,0,key,&temp_str,&Size,TID_STRING,FALSE);
  NMRProbeProgramDir = string(temp_str);

  //file lables
  string global_fn     = "global_on_off";
  string delay_time_fn = "delay-time";
  string fpga_fn       = "pulse-data";
  string fg_fn         = "sg382";
  string adc_fn        = "struck_adc";
  string util_fn       = "utilities";
  string com_fn        = "comments";

  snprintf(key,512,"%s%s",ODB_Setting_Base,"Utilities/Config Label");
  Size = sizeof(temp_str);
  db_get_value(hDB,0,key,&temp_str,&Size,TID_STRING,FALSE);
  
  string config_tag = string(temp_str);
  string conf_path  = NMRProbeProgramDir + string("input/configs/") + config_tag + string(".cnf");
  string prefix     = NMRProbeProgramDir + string("input/configs/files/");
  string common_path= NMRProbeProgramDir + string("input/");

  //file paths
  string global_path= prefix + global_fn + "_" + config_tag + ".dat";
  string fpga_path  = prefix + fpga_fn   + "_" + config_tag + ".dat";
  string fg_path    = prefix + fg_fn     + "_" + config_tag + ".dat";
  string adc_path   = prefix + adc_fn    + "_" + config_tag + ".dat";
  string util_path  = prefix + util_fn   + "_" + config_tag + ".dat";
  string com_path   = prefix + com_fn    + "_" + config_tag + ".txt";

  //Print to setup files
  PrintToFileGlobal(global_path);
  PrintToFileFPGA(fpga_path);
  PrintToFileFG(fg_path);
  PrintToFileADC(adc_path);
  PrintToFileUtil(util_path);
  PrintToFileCom(com_path);

  //create symbolic links
  string common_global_path= common_path + global_fn  + ".dat";
  string common_fpga_path  = common_path + fpga_fn    + ".dat";
  string common_fg_path    = common_path + fg_fn      + ".dat";
  string common_adc_path   = common_path + adc_fn     + ".dat";
  string common_util_path  = common_path + util_fn    + ".dat";
  string common_com_path   = common_path + com_fn     + ".txt";

  string rm_cmd;
  string link_cmd;

  rm_cmd = string("rm ") + common_global_path;
  link_cmd = string("ln -s ") + global_path + string(" ") + common_global_path;
  system(rm_cmd.c_str());
  system(link_cmd.c_str());

  rm_cmd = string("rm ") + common_fpga_path;
  link_cmd = string("ln -s ") + fpga_path + string(" ") + common_fpga_path;
  system(rm_cmd.c_str());
  system(link_cmd.c_str());
  
  rm_cmd = string("rm ") + common_fg_path;
  link_cmd = string("ln -s ") + fg_path + string(" ") + common_fg_path;
  system(rm_cmd.c_str());
  system(link_cmd.c_str());
  
  rm_cmd = string("rm ") + common_adc_path;
  link_cmd = string("ln -s ") + adc_path + string(" ") + common_adc_path;
  system(rm_cmd.c_str());
  system(link_cmd.c_str());
  
  rm_cmd = string("rm ") + common_util_path;
  link_cmd = string("ln -s ") + util_path + string(" ") + common_util_path;
  system(rm_cmd.c_str());
  system(link_cmd.c_str());
  
  rm_cmd = string("rm ") + common_com_path;
  link_cmd = string("ln -s ") + com_path + string(" ") + common_com_path;
  system(rm_cmd.c_str());
  system(link_cmd.c_str());

  //Change directory to NMRDAQ
  chdir(NMRProbeProgramDir.c_str());

  ReadyToRead = true;

  return SUCCESS;
}

/*-- End of Run ----------------------------------------------------*/

INT end_of_run(INT run_number, char *error)

{
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

  if (ReadyToRead)return 1;
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
  ReadyToRead = false;
  //Execute the DAQ command
  string cmd = "./muon_g2_nmr /dev/sis1100_00remote 0x5500ffff";
//  system(cmd.c_str());
  system("ls");
  sleep(2);
  ReadyToMove = true;
  ReadyToRead = true;
  return bk_size(pevent);
}

int PrintToFileGlobal(string fn)
{
  char key[512];
  BOOL temp_bool;
  INT Size = sizeof(temp_bool);
  snprintf(key,512,"%s%s",ODB_Setting_Base,"Global On");
  db_get_value(hDB,0,key,&temp_bool,&Size,TID_BOOL,FALSE);

  string global_tag   = "global_on_off";
  string header       = "#    ID    value";
  BOOL global_state = temp_bool;
  string global_str = global_tag + "\t";
  if (global_state){
    global_str += "1";
  }else{
    global_str += "0";
  }
  string eof_str = EOFstr+"\t99";
  if (!IsDebug){
    ofstream globalFile;
    globalFile.open(fn.c_str(),ios::out);
    globalFile<<header<<endl;
    globalFile<<global_str<<endl;
    globalFile<<eof_str<<endl;
    globalFile.close();
  }else if (IsDebug){
    cout << fn<<endl;
    cout << header<<endl;
    cout << global_str<<endl;
    cout << eof_str<<endl;
  }
  return 0;
}

int PrintToFileFPGA(string fn){
  char key[512];
  char temp_str[512];
  double temp_double;
  BOOL temp_bool;
  INT Size = sizeof(temp_str);
  string cnfCh[4];
  double msw_off[4];  
  double msw_dur[4]; 
  string msw_unit[4];
  double rft_off[4]; 
  double rft_dur[4]; 
  string rft_unit[4];
  double tom_off[4]; 
  double tom_dur[4]; 
  int tom_enbl[4]; 
  string tom_unit[4];
  double rfr_off[4];
  double rfr_dur[4];
  string rfr_unit[4];

  for (int i=0;i<4;i++){
    //Config
    snprintf(key,512,"%s%s%1d%s",ODB_Setting_Base,"FPGA Timing/S",i+1,"/Configuration");
    Size = sizeof(temp_str);
    db_get_value(hDB,0,key,&temp_str,&Size,TID_STRING,FALSE);
    cnfCh[i] = string(temp_str);

    //Units
    snprintf(key,512,"%s%s%1d%s",ODB_Setting_Base,"FPGA Timing/S",i+1,"/Mech Switch Unit");
    db_get_value(hDB,0,key,&temp_str,&Size,TID_STRING,FALSE);
    msw_unit[i] = string(temp_str);
    snprintf(key,512,"%s%s%1d%s",ODB_Setting_Base,"FPGA Timing/S",i+1,"/RF Trans Switch Unit");
    db_get_value(hDB,0,key,&temp_str,&Size,TID_STRING,FALSE);
    rft_unit[i] = string(temp_str);
    snprintf(key,512,"%s%s%1d%s",ODB_Setting_Base,"FPGA Timing/S",i+1,"/RF Rec Switch Unit");
    db_get_value(hDB,0,key,&temp_str,&Size,TID_STRING,FALSE);
    rfr_unit[i] = string(temp_str);
    snprintf(key,512,"%s%s%1d%s",ODB_Setting_Base,"FPGA Timing/S",i+1,"/Amplifier Unit");
    db_get_value(hDB,0,key,&temp_str,&Size,TID_STRING,FALSE);
    tom_unit[i] = string(temp_str);

    //Mech. switch
    Size = sizeof(double);
    snprintf(key,512,"%s%s%1d%s",ODB_Setting_Base,"FPGA Timing/S",i+1,"/Mechanical Offset");
    db_get_value(hDB,0,key,&temp_double,&Size,TID_DOUBLE,FALSE);
    msw_off[i] = temp_double;
    snprintf(key,512,"%s%s%1d%s",ODB_Setting_Base,"FPGA Timing/S",i+1,"/Mech Switch Duration");
    db_get_value(hDB,0,key,&temp_double,&Size,TID_DOUBLE,FALSE);
    msw_dur[i] = temp_double;

    //RF
    snprintf(key,512,"%s%s%1d%s",ODB_Setting_Base,"FPGA Timing/S",i+1,"/RF Transmit Offset");
    db_get_value(hDB,0,key,&temp_double,&Size,TID_DOUBLE,FALSE);
    rft_off[i] = temp_double;

    snprintf(key,512,"%s%s%1d%s",ODB_Setting_Base,"FPGA Timing/S",i+1,"/RF Trans Switch Duration");
    db_get_value(hDB,0,key,&temp_double,&Size,TID_DOUBLE,FALSE);
    rft_dur[i] = temp_double;

    snprintf(key,512,"%s%s%1d%s",ODB_Setting_Base,"FPGA Timing/S",i+1,"/RF Receive Offset");
    db_get_value(hDB,0,key,&temp_double,&Size,TID_DOUBLE,FALSE);
    rfr_off[i] = temp_double;

    snprintf(key,512,"%s%s%1d%s",ODB_Setting_Base,"FPGA Timing/S",i+1,"/RF Rec Switch Duration");
    db_get_value(hDB,0,key,&temp_double,&Size,TID_DOUBLE,FALSE);
    rfr_dur[i] = temp_double;

    //Tomco
    snprintf(key,512,"%s%s%1d%s",ODB_Setting_Base,"FPGA Timing/S",i+1,"/Tomco Offset");
    db_get_value(hDB,0,key,&temp_double,&Size,TID_DOUBLE,FALSE);
    tom_off[i] = temp_double;

    snprintf(key,512,"%s%s%1d%s",ODB_Setting_Base,"FPGA Timing/S",i+1,"/Amplifier Duration");
    db_get_value(hDB,0,key,&temp_double,&Size,TID_DOUBLE,FALSE);
    tom_dur[i] = temp_double;

    Size=sizeof(temp_bool);
    snprintf(key,512,"%s%s%1d%s",ODB_Setting_Base,"FPGA Timing/S",i+1,"/Tomco Enable");
    db_get_value(hDB,0,key,&temp_bool,&Size,TID_BOOL,FALSE);
    if(temp_bool)tom_enbl[i]=1;
    else tom_enbl[i]=0;
  }

  // init vars 
  string fpga_header_1  = "# ID     on/off";
  string fpga_header_2  = " mech-sw offset     mech-sw duration     units";
  string fpga_header_3  = " rf-trans offset    rf-trans duration    units";
  string fpga_header_4  = " tomco offset       tomco duration       units    tomco enable";
  string fpga_header_5  = " rf-rec offset      rf-rec duration      units";
  string fpga_header    = fpga_header_1 + fpga_header_2 + fpga_header_3 + fpga_header_4 + fpga_header_5;
  char eof1[512];
  char eof2[512];
  char eof3[512];
  snprintf(eof1,512,"%-5s %-5s ","99","--");
  snprintf(eof2,512,"%-5s %-5s %-5s","0","0","ND");
  snprintf(eof3,512,"%-5s %-5s %-5s %-5s","0","0","ND","0");
  string eof_fpga_str   = string(eof1) + string(eof2) +string(eof2) + string(eof3) + string(eof2);
  // write to file 
  if (!IsDebug){
    ofstream fpgaFile;
    fpgaFile.open(fn.c_str(),ios::out);
    fpgaFile<<fpga_header<<endl;
    for (int i=0;i<4;i++){
      if (cnfCh[i].compare("OFF")!=0){
	fpgaFile<<i+1<<" "<<"on ";
	fpgaFile<<msw_off[i]<<" "<<msw_dur[i]<<" "<<msw_unit[i]<<" ";
	fpgaFile<<rft_off[i]<<" "<<rft_dur[i]<<" "<<rft_unit[i]<<" ";
	fpgaFile<<tom_off[i]<<" "<<tom_dur[i]<<" "<<tom_unit[i]<<" "<<tom_enbl[i]<<" ";
	fpgaFile<<rfr_off[i]<<" "<<rfr_dur[i]<<" "<<rfr_unit[i]<<" "<<endl;
      }
    }
    fpgaFile<<eof_fpga_str<<endl;
    fpgaFile.close();
  }else if (IsDebug){
    cout << fn<<endl;
    cout<<fpga_header<<endl;
    for (int i=0;i<4;i++){
      if (cnfCh[i].compare("OFF")!=0){
	cout<<i+1<<" "<<"on ";
	cout<<msw_off[i]<<" "<<msw_dur[i]<<" "<<"s ";
	cout<<rft_off[i]<<" "<<rft_dur[i]<<" "<<"s ";
	cout<<tom_off[i]<<" "<<tom_dur[i]<<" "<<"s "<<tom_enbl[i]<<" ";
	cout<<rfr_off[i]<<" "<<rfr_dur[i]<<" "<<"s "<<endl;
      }
    }
    cout<<eof_fpga_str<<endl;
  }
  return 0;
}

int PrintToFileFG(string fn){
  char key[512];
  char temp_str[512];
  BOOL temp_bool;
  double temp_double;
  INT Size;

  string fg_header  = "# type   state      value           units";
  snprintf(temp_str,512,"%-20s %-20s %-20s %-20s",EOFstr.c_str(),"--","0","ND");
  string eof_fg_str = string(temp_str);

  string bnc_state       = "off";
  string ntype_state     = "off";
  double bnc_value;
  double ntype_value;
  string bnc_VChoice       = "Vpp";
  string ntype_VChoice     = "Vpp";
  double freq;
  string freq_unit;

  Size = sizeof(temp_bool);
  snprintf(key,512,"%s%s",ODB_Setting_Base,"LO/BNC Switch");
  db_get_value(hDB,0,key,&temp_bool,&Size,TID_BOOL,FALSE);
  if (temp_bool) bnc_state = string("on");

  snprintf(key,512,"%s%s",ODB_Setting_Base,"LO/N-Type Switch");
  db_get_value(hDB,0,key,&temp_bool,&Size,TID_BOOL,FALSE);
  if (temp_bool) ntype_state = string("on");

  Size = sizeof(temp_double);
  snprintf(key,512,"%s%s",ODB_Setting_Base,"LO/BNC Voltage");
  db_get_value(hDB,0,key,&temp_double,&Size,TID_DOUBLE,FALSE);
  bnc_value = temp_double;

  snprintf(key,512,"%s%s",ODB_Setting_Base,"LO/N-Type Voltage");
  db_get_value(hDB,0,key,&temp_double,&Size,TID_DOUBLE,FALSE);
  ntype_value = temp_double;

  snprintf(key,512,"%s%s",ODB_Setting_Base,"LO/Frequency");
  db_get_value(hDB,0,key,&temp_double,&Size,TID_DOUBLE,FALSE);
  freq = temp_double;

  Size = sizeof(temp_str);
  snprintf(key,512,"%s%s",ODB_Setting_Base,"LO/BNC V-choice");
  db_get_value(hDB,0,key,&temp_str,&Size,TID_STRING,FALSE);
  bnc_VChoice = string(temp_str);

  snprintf(key,512,"%s%s",ODB_Setting_Base,"LO/N-Type V-choice");
  db_get_value(hDB,0,key,&temp_str,&Size,TID_STRING,FALSE);
  ntype_VChoice = string(temp_str);

  snprintf(key,512,"%s%s",ODB_Setting_Base,"LO/Frequency Unit");
  db_get_value(hDB,0,key,&temp_str,&Size,TID_STRING,FALSE);
  freq_unit = string(temp_str);

  
  snprintf(temp_str,512,"%-20s %-20s %-20f %-20s","frequency","--",freq,freq_unit.c_str());
  string freq_str = string(temp_str);

  snprintf(temp_str,512,"%-20s %-20s %-20f %-20s","bnc",bnc_state.c_str(),bnc_value,bnc_VChoice.c_str());
  string bnc_str = string(temp_str);

  snprintf(temp_str,512,"%-20s %-20s %-20f %-20s","ntype",ntype_state.c_str(),ntype_value,ntype_VChoice.c_str());
  string ntype_str = string(temp_str);

  // write to file 
  if (!IsDebug){
    ofstream fgFile;
    fgFile.open(fn.c_str(),ios::out);
    fgFile<<fg_header<<endl; 
    fgFile<<freq_str <<endl; 
    fgFile<<bnc_str <<endl;  
    fgFile<<ntype_str <<endl;
    fgFile<<eof_fg_str<<endl;
    fgFile.close();
  }else if (IsDebug){
    cout<<fg_header<<endl; 
    cout<<freq_str <<endl; 
    cout<<bnc_str <<endl;  
    cout<<ntype_str <<endl;
    cout<<eof_fg_str<<endl;
  }
  return 0;
}

int PrintToFileADC(string fn){
  char key[512];
  char temp_str[512];
  BOOL temp_bool;
  double temp_double;
  int temp_int;
  INT Size;

  string adc_header  = "# ID     value      units";
  snprintf(temp_str,512,"%-20s %-20s %-20s",EOFstr.c_str(),"0","ND");
  string eof_adc_str = string(temp_str);

  int adc_id;
  int ch_num;
  int pulse_num;
  double freq;
  int extern_clk;
  string freq_unit;

  string adc_id_str;      
  string ch_num_str;      
  string pulse_num_str;   
  string sample_freq_str; 
  string extern_clk_str; 

  Size = sizeof(temp_str);
  snprintf(key,512,"%s%s",ODB_Setting_Base,"Digitizer/Frequency Unit");
  db_get_value(hDB,0,key,&temp_str,&Size,TID_STRING,FALSE);
  freq_unit = string(temp_str);

  Size = sizeof(temp_bool);
  snprintf(key,512,"%s%s",ODB_Setting_Base,"Digitizer/External");
  db_get_value(hDB,0,key,&temp_bool,&Size,TID_BOOL,FALSE);
  if (temp_bool) extern_clk = 1;
  else extern_clk=-1;

  Size = sizeof(temp_double);
  snprintf(key,512,"%s%s",ODB_Setting_Base,"Digitizer/Sampling Frequency");
  db_get_value(hDB,0,key,&temp_double,&Size,TID_DOUBLE,FALSE);
  freq = temp_double;

  Size = sizeof(temp_int);
  snprintf(key,512,"%s%s",ODB_Setting_Base,"Digitizer/Struck ID");
  db_get_value(hDB,0,key,&temp_int,&Size,TID_INT,FALSE);
  adc_id = temp_int;

  snprintf(key,512,"%s%s",ODB_Setting_Base,"Digitizer/Channel Number");
  db_get_value(hDB,0,key,&temp_int,&Size,TID_INT,FALSE);
  ch_num = temp_int;

  snprintf(key,512,"%s%s",ODB_Setting_Base,"Digitizer/Number of Pulses");
  db_get_value(hDB,0,key,&temp_int,&Size,TID_INT,FALSE);
  pulse_num = temp_int;

  snprintf(temp_str,512,"%-20s %-20d %-20s","adc_id",adc_id,"ND");
  adc_id_str = string(temp_str);
  snprintf(temp_str,512,"%-20s %-20d %-20s","channel_number",ch_num,"ND");
  ch_num_str = string(temp_str);
  snprintf(temp_str,512,"%-20s %-20d %-20s","number_of_events",pulse_num,"ND");
  pulse_num_str = string(temp_str);
  snprintf(temp_str,512,"%-20s %-20f %-20s","frequency",freq,freq_unit.c_str());
  sample_freq_str = string(temp_str);
  snprintf(temp_str,512,"%-20s %-20f %-20s","external_clock",freq*extern_clk,freq_unit.c_str());
  extern_clk_str = string(temp_str);

  // write to file 
  if (!IsDebug){
    ofstream adcFile;
    adcFile.open(fn.c_str(),ios::out);
    adcFile<<adc_header<<endl; 
    adcFile<<adc_id_str <<endl;
    adcFile<<ch_num_str <<endl;
    adcFile<<pulse_num_str <<endl;
    adcFile<<sample_freq_str <<endl;
    adcFile<<extern_clk_str <<endl;
    adcFile<<eof_adc_str<<endl;
    adcFile.close();
  }else if (IsDebug){
    cout<<adc_header<<endl; 
    cout<<adc_id_str <<endl;
    cout<<ch_num_str <<endl;
    cout<<pulse_num_str <<endl;
    cout<<sample_freq_str <<endl;
    cout<<extern_clk_str <<endl;
    cout<<eof_adc_str<<endl;
  }
  return 0;
}

int PrintToFileUtil(string fn){
  char key[512];
  char temp_str[512];
  BOOL temp_bool;
  double temp_double;
  int temp_int;
  INT Size;

  string util_header  = "# ID     value";
  snprintf(temp_str,512,"%-20s %-20s",EOFstr.c_str(),"99");
  string eof_util_str = string(temp_str);

  int debug;
  int verb;
  int testmode;
  double freq;
  double dt;
  string dt_unit;
  string freq_unit;

  string debug_str;
  string verb_str;
  string test_str; 
  string rf_str;
  string dt_str;

  Size = sizeof(temp_str);
  snprintf(key,512,"%s%s",ODB_Setting_Base,"Utilities/Debug Mode");
  db_get_value(hDB,0,key,&temp_str,&Size,TID_STRING,FALSE);
  if (string(temp_str).compare("OFF")==0) debug = 0;
  else if (string(temp_str).compare("ON")==0) debug = 1;
  else debug = 0;

  snprintf(key,512,"%s%s",ODB_Setting_Base,"Utilities/PTS160 Frequency Unit");
  db_get_value(hDB,0,key,&temp_str,&Size,TID_STRING,FALSE);
  freq_unit = string(temp_str);

  snprintf(key,512,"%s%s",ODB_Setting_Base,"Utilities/Delay Time Unit");
  db_get_value(hDB,0,key,&temp_str,&Size,TID_STRING,FALSE);
  dt_unit = string(temp_str);

  Size = sizeof(temp_int);
  snprintf(key,512,"%s%s",ODB_Setting_Base,"Utilities/Verbosity");
  db_get_value(hDB,0,key,&temp_int,&Size,TID_INT,FALSE);
  verb = temp_int;

  snprintf(key,512,"%s%s",ODB_Setting_Base,"Utilities/Test Mode");
  db_get_value(hDB,0,key,&temp_int,&Size,TID_INT,FALSE);
  testmode = temp_int;

  Size = sizeof(temp_double);
  snprintf(key,512,"%s%s",ODB_Setting_Base,"Utilities/PTS160 Frequency");
  db_get_value(hDB,0,key,&temp_double,&Size,TID_DOUBLE,FALSE);
  freq = temp_double;

  snprintf(key,512,"%s%s",ODB_Setting_Base,"Utilities/Delay Time");
  db_get_value(hDB,0,key,&temp_double,&Size,TID_DOUBLE,FALSE);
  dt = temp_double;

  //Applying units
  if (dt_unit.compare("s")==0) dt *= 1;
  else if (dt_unit.compare("ms")==0) dt *= 1e-3;
  else if (dt_unit.compare("us")==0) dt *= 1e-6;

  if (freq_unit.compare("kHz")==0) freq *= 1e3;
  else if (freq_unit.compare("MHz")==0) freq *= 1e6;
  else if (freq_unit.compare("GHz")==0) freq *= 1e9;

  snprintf(temp_str,512,"%-20s %-20d","debug_mode",debug);
  debug_str = string(temp_str);

  snprintf(temp_str,512,"%-20s %-20d","verbosity",verb);
  verb_str = string(temp_str);

  snprintf(temp_str,512,"%-20s %-20d","test_mode",testmode);
  test_str = string(temp_str);

  snprintf(temp_str,512,"%-20s %-20f","delay_time",dt);
  dt_str = string(temp_str);

  snprintf(temp_str,512,"%-20s %-20f","rf_frequency",freq);
  rf_str = string(temp_str);

  // write to file 
  if (!IsDebug){
    ofstream utilFile;
    utilFile.open(fn.c_str(),ios::out);
    utilFile<<util_header<<endl; 
    utilFile<<debug_str <<endl;
    utilFile<<verb_str <<endl;
    utilFile<<test_str <<endl;
    utilFile<<dt_str <<endl;
    utilFile<<rf_str <<endl;
    utilFile<<eof_util_str<<endl;
    utilFile.close();
  }else if (IsDebug){
    cout<<util_header<<endl; 
    cout<<debug_str <<endl;
    cout<<verb_str <<endl;
    cout<<test_str <<endl;
    cout<<dt_str <<endl;
    cout<<rf_str <<endl;
    cout<<eof_util_str<<endl;
  }
  return 0;
}

int PrintToFileCom(string fn){
  char key[512];
  char temp_str[512];
  INT Size = sizeof(temp_str);
  snprintf(key,512,"%s%s",ODB_Setting_Base,"Utilities/Comments");
  db_get_value(hDB,0,key,&temp_str,&Size,TID_STRING,FALSE);

  string com_str = string(temp_str);

  // write to file 
  if (!IsDebug){
    ofstream commentFile;
    commentFile.open(fn.c_str(),ios::out);
    commentFile<<com_str<<endl;
    commentFile.close();
  }else if (IsDebug){
    cout << fn<<endl;
    cout<<com_str<<endl;
  }
  return 0;
}
