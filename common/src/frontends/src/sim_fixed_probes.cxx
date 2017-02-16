/*****************************************************************************\

Name:   sim_fixed_probes.cxx
Author: Matthias W. Smith
Email:  mwsmith2@uw.edu

About:  Implements a MIDAS frontend that is aware of the
        multiplexers.  It configures them, takes data in a
        sequence, and builds an event from all the data.

\*****************************************************************************/

//--- std includes ----------------------------------------------------------//
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <ctime>
using std::string;

//--- other includes --------------------------------------------------------//
#include "midas.h"
#include "TFile.h"
#include "TTree.h"
#include "fid.h"

//--- project includes ------------------------------------------------------//
#include "g2field/common.hh"
#include "g2field/core/field_structs.hh"
#include "fixed_probe_sequencer.hh"
#include "frontend_utils.hh"

//--- globals ---------------------------------------------------------------//
#define FRONTEND_NAME "Fixed Probes"

extern "C" {

  // The frontend name (client name) as seen by other MIDAS clients
  char *frontend_name = (char *)FRONTEND_NAME;

  // The frontend file name, don't change it.
  char *frontend_file_name = (char *)__FILE__;

  // frontend_loop is called periodically if this variable is TRUE
  BOOL frontend_call_loop = FALSE;

  // A frontend status page is displayed with this frequency in ms.
  INT display_period = 1000;

  // maximum event size produced by this frontend
  INT max_event_size = 0x800000; // 8 MB

  // maximum event size for fragmented events (EQ_FRAGMENTED)
  INT max_event_size_frag = 0x100000;

  // buffer size to hold events
  INT event_buffer_size = 0x1000000; // 64 MB

  // Function declarations
  INT frontend_init();
  INT frontend_exit();
  INT begin_of_run(INT run_number, char *error);
  INT end_of_run(INT run_number, char *error);
  INT pause_run(INT run_number, char *error);
  INT resume_run(INT run_number, char *error);

  INT frontend_loop();
  INT read_fixed_probe_event(char *pevent, INT off);
  INT poll_event(INT source, INT count, BOOL test);
  INT interrupt_configure(INT cmd, INT source, POINTER_T adr);

  // Equipment list

  EQUIPMENT equipment[] =
    {
      {FRONTEND_NAME,  // equipment name
       { EVENTID_FIXED_PROBES, 0x01,        // event ID, trigger mask
         "SYSTEM",      // event buffer (use to be SYSTEM)
         EQ_PERIODIC,   // equipment type
         0,             // not used
         "MIDAS",       // format
         TRUE,          // enabled
         RO_RUNNING,    // read only when running
         10,            // poll for 10ms
         0,             // stop run after this event limit
         0,             // number of sub events
         0,             // don't log history
         "", "", "",
       },
       read_fixed_probe_event,      // readout routine
      },

      {""}
    };

} //extern C

RUNINFO runinfo;

// Anonymous namespace for my "globals"
namespace {
double step_size = 0.4;  //  23.375 mm per 1 step size // So pretty close to 1 inch per 1 step_size (note: this calibration occurred using a chord of about 3 meters, so the string was not moving along the azimuth. So there is a missing cos(theta) -- where theta is small -- that needs to be included

double event_rate_limit = 10.0;
int num_steps = 50; // num_steps was 50
int num_shots = 1;
int trigger_count = 0;
int event_number = 0;
bool write_root = false;
bool write_midas = true;
bool use_stepper = true;
bool ino_stepper_type = false;

TFile *pf;
TTree *pt_norm;

std::atomic<bool> run_in_progress;
std::mutex data_mutex;

boost::property_tree::ptree conf;

g2field::fixed_t data;

const int nprobes = g2field::kNmrNumFixedProbes;
const char *const mbank_name = (char *)"FXPR";
}

void trigger_loop();

int load_device_class();

//--- Frontend Init ---------------------------------------------------------//
INT frontend_init()
{
  run_in_progress = false;

  cm_msg(MINFO, "init", "Sim Fixed Probes initialization complete");
  return SUCCESS;
}

//--- Frontend Exit ---------------------------------------------------------//
INT frontend_exit()
{
  run_in_progress = false;

  cm_msg(MINFO, "exit", "Sim Fixed Probes teardown complete");
  return SUCCESS;
}

//--- Begin of Run --------------------------------------------------//
INT begin_of_run(INT run_number, char *error)
{
  using namespace boost;

   // ODB parameters
  HNDLE hDB, hkey;
  char str[256];
  int size;
  BOOL flag;

  // Set up the data.
  std::string datadir;
  std::string filename;

  // Grab the database handle.
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
    pt_norm = new TTree("t_fxpr", "Sim Fixed Probes Probe Data");
    pt_norm->SetAutoSave(5);
    pt_norm->SetAutoFlush(20);

    std::string br_name("fixed_probes");

    pt_norm->Branch(br_name.c_str(), &data.sys_clock[0], g2field::fixed_str);
  }

  event_number = 0;
  run_in_progress = true;

  cm_msg(MLOG, "begin_of_run", "Completed successfully");

  return SUCCESS;
}

//--- End of Run ----------------------------------------------------*/
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

  cm_msg(MLOG, "end_of_run", "Completed successfully");
  return SUCCESS;
}

//--- Pause Run -----------------------------------------------------*/
INT pause_run(INT run_number, char *error)
{
  return SUCCESS;
}

//--- Resuem Run ----------------------------------------------------*/
INT resume_run(INT run_number, char *error)
{
  return SUCCESS;
}

//--- Frontend Loop -------------------------------------------------*/

INT frontend_loop()
{
  // If frontend_call_loop is true, this routine gets called when
  // the frontend is idle or once between every event
  return SUCCESS;
}

//-------------------------------------------------------------------*/

/********************************************************************\

  Readout routines for different events

\********************************************************************/

//--- Trigger event routines ----------------------------------------*/

INT poll_event(INT source, INT count, BOOL test) 
{
  static bool triggered = false;

  // fake calibration
  if (test) {
    for (int i = 0; i < count; i++) {
      usleep(1000);
    }
    return 0;
  }

  return 0;
}

//--- Interrupt configuration ---------------------------------------*/

INT interrupt_configure(INT cmd, INT source, PTYPE adr)
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

//--- Event readout -------------------------------------------------*/

INT read_fixed_probe_event(char *pevent, INT off)
{
  static bool triggered = false;
  static unsigned long long num_events;
  static unsigned long long events_written;

  // Allocate vectors for the FIDs
  static std::vector<double> tm;
  static std::vector<double> wf;
  static fid::FidFactory ff;

  int count = 0;
  char bk_name[10];
  DWORD *pdata;

  cm_msg(MINFO, "read_fixed_event", "simulating data");

  // Set the time vector.
  if (tm.size() == 0) {

    tm.resize(g2field::kNmrFidLengthRecord);
    wf.resize(g2field::kNmrFidLengthRecord);

    for (int n = 0; n < g2field::kNmrFidLengthRecord; ++n) {
      tm[n] = 0.001 * n - 0.3;
    }
  }

  data_mutex.lock();

  for (int idx = 0; idx < nprobes; ++idx) {

    ff.IdealFid(wf, tm);
    fid::FastFid myfid(wf, tm);

    std::copy(wf.begin(), wf.end(), &data.trace[idx][0]);

    // Make sure we got an FID signal
    if (myfid.isgood()) {

      data.freq[idx] = ff.freq();
      data.ferr[idx] = 0.0;

      data.freq_zc[idx] = myfid.GetFreq();
      data.ferr_zc[idx] = myfid.freq_err();

      data.snr[idx] = myfid.snr();
      data.len[idx] = myfid.fid_time();

    } else {

      myfid.DiagnosticInfo();
      data.freq[idx] = -1.0;
      data.ferr[idx] = -1.0;

      data.freq_zc[idx] = -1.0;
      data.ferr_zc[idx] = -1.0;

      data.snr[idx] = -1.0;
      data.len[idx] = -1.0;
    }

    data.sys_clock[idx] = hw::systime_us();
    data.gps_clock[idx] = 0;
    data.dev_clock[idx] = 0;
  }

  data_mutex.unlock();

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

  // And MIDAS output.
  bk_init32(pevent);

  if (write_midas) {

    // Copy the shimming trolley data.
    bk_create(pevent, mbank_name, TID_DWORD, &pdata);

    memcpy(pdata, &data, sizeof(data));
    pdata += sizeof(data) / sizeof(DWORD);

    bk_close(pevent, pdata);
  }

  // Pop the event now that we are done copying it.
  cm_msg(MINFO, "read_fixed_event", "Finished generating event");

  // Let the front-end know we are ready for another trigger.
  triggered = false;

  return bk_size(pevent);
}
