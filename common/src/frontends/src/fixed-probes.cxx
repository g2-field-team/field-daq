/*****************************************************************************\

Name:   fixed-probes.cxx
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
#include <random>
using std::string;

//--- other includes --------------------------------------------------------//
#include "midas.h"
#include "TFile.h"
#include "TTree.h"

//--- project includes ------------------------------------------------------//
#include "g2field/core/field_structs.hh"
#include "g2field/core/field_constants.hh"
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

// Configuration variables
bool write_midas = true;
bool write_root = false;
bool write_full_waveform = false;
int full_waveform_subsampling = 1;
bool simulation_mode = false;
bool recrunch_in_fe = false;

boost::property_tree::ptree conf;
std::string nmr_sequence_conf_file;
std::atomic<bool> run_in_progress;
std::mutex data_mutex;

// Data variables
TFile *pf;
TTree *pt;
TTree *pt_full;

g2field::fixed_t data;
g2field::online_fixed_t full_data;
g2field::FixedProbeSequencer *event_manager;

const int nprobes = g2field::kNmrNumFixedProbes;
const char *const mbank_name = (char *)"FXPR";
}

void trigger_loop();
void set_json_tmpfiles();
int load_device_classes();
int simulate_fixed_probe_event();
void update_feedback_params();
void systems_check();

void set_json_tmpfiles()
{
  // Allocations
  char tmp_file[256];
  std::string conf_file;
  boost::property_tree::ptree pt;

  // Copy the wfd config file.
  snprintf(tmp_file, 128, "/tmp/g2-nmr-config_XXXXXX.json");
  mkstemps(tmp_file, 5);
  conf_file = std::string(tmp_file);

  // Copy the json, and set to the temp file.
  pt = conf.get_child("devices.sis_3316");
  boost::property_tree::write_json(conf_file, pt.get_child("sis_3316_0"));
  pt.clear();
  pt.put("sis_3316_0", conf_file);

  conf.get_child("devices.sis_3316").erase("sis_3316_0");
  conf.put_child("devices.sis_3316", pt);

  // Create a temp file for the sis_3302.
  snprintf(tmp_file, 128, "/tmp/g2-nmr-config_XXXXXX.json");
  mkstemps(tmp_file, 5);
  conf_file = std::string(tmp_file);

  // Copy the json, and set to the temp file.
  pt = conf.get_child("devices.sis_3302");
  boost::property_tree::write_json(conf_file, pt.get_child("sis_3302_0"));
  pt.clear();
  pt.put("sis_3302_0", conf_file);

  conf.get_child("devices.sis_3302").erase("sis_3302_0");
  conf.put_child("devices.sis_3302", pt);

  // Now handle the config subtree.
  pt = conf.get_child("config");

  // Copy the trigger sequence file.
  snprintf(tmp_file, 128, "/tmp/g2-nmr-config_XXXXXX.json");
  mkstemps(tmp_file, 5);
  conf_file = std::string(tmp_file);

  // Copy the json, and set to the temp file.
  boost::property_tree::write_json(conf_file, pt.get_child("mux_sequence"));
  pt.erase("mux_sequence");
  pt.put<std::string>("mux_sequence", conf_file);

  // Now the mux configuration
  snprintf(tmp_file, 128, "/tmp/g2-nmr-config_XXXXXX.json");
  mkstemps(tmp_file, 5);
  conf_file = std::string(tmp_file);

  // Copy the json, and set to the temp file.
  boost::property_tree::write_json(conf_file, pt.get_child("mux_connections"));
  pt.erase("mux_connections");
  pt.put<std::string>("mux_connections",  conf_file);

  // And the fid params.
  snprintf(tmp_file, 128, "/tmp/g2-nmr-config_XXXXXX.json");
  mkstemps(tmp_file, 5);
  conf_file = std::string(tmp_file);

  // Copy the json, and set to the temp file.
  boost::property_tree::write_json(conf_file, pt.get_child("fid_analysis"));
  pt.erase("fid_analysis");
  pt.put<std::string>("fid_analysis", conf_file);

  conf.erase("config");
  conf.put_child("config", pt);

  // Now save the config to a temp file and feed it to the Event Manager.
  snprintf(tmp_file, 128, "/tmp/g2-nmr-config_XXXXXX.json");
  mkstemps(tmp_file, 5);
  nmr_sequence_conf_file = std::string(tmp_file);
  boost::property_tree::write_json(nmr_sequence_conf_file, conf);
}

INT load_device_classes()
{
  // Set up the event mananger.
  if (event_manager == nullptr) {
    event_manager = new g2field::FixedProbeSequencer(nmr_sequence_conf_file, 
						     nprobes);
  } else {

    event_manager->EndOfRun();
    delete event_manager;
    event_manager = new g2field::FixedProbeSequencer(nmr_sequence_conf_file, 
						     nprobes);
  }

  event_manager->BeginOfRun();

  return SUCCESS;
}

//--- Frontend Init ----------------------------------------------------------//
INT frontend_init()
{
  // Perform the initial hardware check.
  systems_check();

  // Load settings and save to temp files.
  INT rc = load_settings(frontend_name, conf);

  if (rc != SUCCESS) {
    std::string al_msg("Fixed Probe System: failed to load settings from ODB");
    al_trigger_class("Error", al_msg.c_str(), false);
    return rc;
  }

  set_json_tmpfiles();

  rc = load_device_classes();
  if (rc != SUCCESS) {
    std::string al_msg("Fixed Probe System: failed to load device classes.");
    al_trigger_class("Error", al_msg.c_str(), false);
    return rc;
  }

  run_in_progress = false;

  cm_msg(MINFO, "init", "Fixed Probe initialization complete");
  return SUCCESS;
}

//--- Frontend Exit ---------------------------------------------------------//
INT frontend_exit()
{
  run_in_progress = false;

  event_manager->EndOfRun();
  delete event_manager;

  cm_msg(MINFO, "exit", "Fixed Probe teardown complete");
  return SUCCESS;
}

//--- Begin of Run --------------------------------------------------//
INT begin_of_run(INT run_number, char *error)
{
  cm_msg(MINFO, "fixed-probes", "begin of run");
   // ODB parameters
  HNDLE hDB, hkey;
  char str[256];
  INT size, rc;
  BOOL flag;

  // Set up the data.
  std::string datadir;
  std::string filename;
  
  // Load settings and save to temp files.
  rc = load_settings(frontend_name, conf);

  if (rc != SUCCESS) {
    std::string al_msg("Fixed Probe System: failed to load settings from ODB");
    al_trigger_class("Error", al_msg.c_str(), false);
    return rc;
  }

  set_json_tmpfiles();

  rc = load_device_classes();
  if (rc != SUCCESS) {
    std::string al_msg("Fixed Probe System: failed to load device classes.");
    al_trigger_class("Error", al_msg.c_str(), false);
    return rc;
  }

  // Grab the database handle.
  cm_get_experiment_database(&hDB, NULL);

  cm_msg(MINFO, "fixed-probes", "loading config");
  // Get the run info out of the ODB.
  db_find_key(hDB, 0, "/Runinfo", &hkey);
  if (db_open_record(hDB, hkey, &runinfo, 
		     sizeof(runinfo), MODE_READ,
		     NULL, NULL) != DB_SUCCESS) {
    cm_msg(MERROR, "begin_of_run", "Can't open \"/Runinfo\" in ODB");
    return CM_DB_ERROR;
  }

  // Get the filename for ROOT output.
  snprintf(str, sizeof(str), 
	   conf.get<std::string>("output.root_file").c_str(), 
	   runinfo.run_number);

  // Join the directory and filename using boost filesystem.
  {
    std::string dir = conf.get<std::string>("output.root_path");
    auto p = boost::filesystem::path(dir) / boost::filesystem::path(str);
    filename = p.string();
  }

  // Get the parameters for root output.
  write_root = conf.get<bool>("output.write_root", false);
  write_full_waveform = conf.get<bool>("output.write_full_waveform");
  full_waveform_subsampling = conf.get<int>("output.full_waveform_subsampling");

  cm_msg(MINFO, "fixed-probes", "loading root file");
  // Set up the ROOT data output.
  if (write_root) {
  
    pf = new TFile(filename.c_str(), "recreate");
    pt = new TTree("t_fxpr", "Fixed Probe Data");
    pt->SetAutoSave(5);
    pt->SetAutoFlush(20);

    std::string br_name("fixed");

    pt->Branch(br_name.c_str(), &data.clock_sys_ns[0], g2field::fixed_str);

    if (write_full_waveform) {
      std::string br_name("full_fixed");

      pt_full->SetAutoSave(5);
      pt_full->SetAutoFlush(20);
      pt_full->Branch(br_name.c_str(),
                      &full_data.clock_sys_ns[0],
                      g2field::online_fixed_str);
    }
  }

  // HW part
  simulation_mode = conf.get<bool>("simulation_mode", simulation_mode);
  recrunch_in_fe = conf.get<bool>("recrunch_in_fe", recrunch_in_fe);

  run_in_progress = true;

  cm_msg(MLOG, "begin_of_run", "Completed successfully");

  return SUCCESS;
}

//--- End of Run ----------------------------------------------------*/
INT end_of_run(INT run_number, char *error)
{
  // Make sure we write the ROOT data.
  if (run_in_progress && write_root) {

    pt->Write();

    if (write_full_waveform) {
      pt_full->Write();
    }

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
  event_manager->PauseRun();
  return SUCCESS;
}

//--- Resuem Run ----------------------------------------------------*/
INT resume_run(INT run_number, char *error)
{
  event_manager->ResumeRun();
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
  // fake calibration
  if (test) {
    for (int i = 0; i < count; i++) {
      usleep(10);
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
  // Allocations
  static bool triggered = false;
  static unsigned long long num_events;
  static std::vector<double> tm;
  static std::vector<double> wf;

  char bk_name[10];
  DWORD *pdata;

  // Trigger the digitizers if not triggered.
  if (!triggered) {
    event_manager->IssueTrigger();
    cm_msg(MDEBUG, "read_fixed_probe_event", "issued trigger");
    triggered = true;
  }

  // Read data or fill a simulated event.
  if (triggered && simulation_mode) {

    simulate_fixed_probe_event();
    triggered = false;

  } else if (triggered && !event_manager->HasEvent()) {
    // No event yet.
    cm_msg(MDEBUG, "read_fixed_probe_event", "no data yet");
    return 0;

  } else {

    cm_msg(MDEBUG, "read_fixed_probe_event", "got real data event");

    auto fp_data = event_manager->GetCurrentEvent();

    if ((fp_data.clock_sys_ns[0] == 0) && 
	(fp_data.clock_sys_ns[nprobes-1] == 0)) {

      event_manager->PopCurrentEvent();
      triggered = false;
      return 0;
    }

    // Set the time vector.
    if (tm.size() == 0) {
      tm.resize(g2field::kNmrFidLengthRecord);
      wf.resize(g2field::kNmrFidLengthRecord);
    }

    data_mutex.lock();

    cm_msg(MINFO, frontend_name, "copying the data from event");
    std::copy(fp_data.clock_sys_ns.begin(),
	      fp_data.clock_sys_ns.begin() + nprobes,
	      &data.clock_sys_ns[0]);
    
    std::copy(fp_data.clock_gps_ns.begin(),
	      fp_data.clock_gps_ns.begin() + nprobes,
	      &data.clock_gps_ns[0]);
    
    std::copy(fp_data.device_clock.begin(),
	      fp_data.device_clock.begin() + nprobes,
	      &data.device_clock[0]);

    std::copy(fp_data.device_rate_mhz.begin(),
	      fp_data.device_rate_mhz.begin() + nprobes,
	      &data.device_rate_mhz[0]);

    std::copy(fp_data.device_gain_vpp.begin(),
	      fp_data.device_gain_vpp.begin() + nprobes,
	      &data.device_gain_vpp[0]);
    
    std::copy(fp_data.fid_snr.begin(),
	      fp_data.fid_snr.begin() + nprobes,
	      &data.fid_snr[0]);
    
    std::copy(fp_data.fid_len.begin(),
	      fp_data.fid_len.begin() + nprobes,
	      &data.fid_len[0]);

    for (int idx = 0; idx < nprobes; ++idx) {
      
      for (int n = 0; n < g2field::kNmrFidLengthRecord; ++n) {
	wf[n] = fp_data.trace[idx][n*10 + 1]; // Offset avoids wfd spikes
	tm[n] = 0.1 * n / fp_data.device_rate_mhz[idx];
	data.trace[idx][n] = wf[n];
      }

      if (recrunch_in_fe) {
	
	fid::Fid myfid(wf, tm);
      
	// Make sure we got an FID signal
	if (myfid.isgood()) {
	  
	  data.freq[idx] = myfid.CalcPhaseFreq();
	  data.ferr[idx] = myfid.freq_err();
	  data.fid_amp[idx] = myfid.amp();
	  data.fid_snr[idx] = myfid.snr();
	  data.fid_len[idx] = myfid.fid_time();
	  
	} else {
	  
	  myfid.DiagnosticInfo();
	  data.freq[idx] = -1.0;
	  data.ferr[idx] = -1.0;
	  data.fid_amp[idx] = -1.0;
	  data.fid_snr[idx] = -1.0;
	  data.fid_len[idx] = -1.0;
	}
      }
    }

    data_mutex.unlock();
  }

  if (write_root && run_in_progress) {
    cm_msg(MINFO, "read_fixed_event", "Filling TTree");
    // Now that we have a copy of the latest event, fill the tree.
    pt->Fill();

    if (write_full_waveform) {
      if (num_events % full_waveform_subsampling == 0) pt_full->Fill();
    }

    num_events++;

    if (num_events % 10 == 1) {

      cm_msg(MINFO, frontend_name, "flushing TTree.");
      pt->AutoSave("SaveSelf,FlushBaskets");

      if (write_full_waveform) {
        pt_full->AutoSave("SaveSelf,FlushBaskets");
      }

      pf->Flush();
    }
  }

  // And MIDAS output.
  bk_init32(pevent);

  if (write_midas) {

    // Copy the fixed probe data.
    bk_create(pevent, mbank_name, TID_DWORD, &pdata);

    memcpy(pdata, &data, sizeof(data));
    pdata += sizeof(data) / sizeof(DWORD);

    bk_close(pevent, pdata);
  }

  // Pop the event now that we are done copying it.
  cm_msg(MINFO, "read_fixed_event", "Updating PS Feedback variables");
  update_feedback_params();

  // Let the front-end know we are ready for another trigger.
  triggered = false;

  return bk_size(pevent);
}

INT simulate_fixed_probe_event()
{
  // Allocate vectors for the FIDs
  static std::vector<double> tm;
  static std::vector<double> wf;
  static fid::FidFactory ff;
  static std::default_random_engine gen(hw::systime_us()); 
  static std::normal_distribution<double> norm(0.0, 0.1);
  
  cm_msg(MINFO, "read_fixed_event", "simulating data");

  // Set the time vector.
  if (tm.size() == 0) {

    tm.resize(g2field::kNmrFidLengthRecord);
    wf.resize(g2field::kNmrFidLengthRecord);

    for (int n = 0; n < g2field::kNmrFidLengthRecord; ++n) {
      tm[n] = 0.001 * n - 0.3;
    }
  }

  for (int idx = 0; idx < nprobes; ++idx) {

    ff.SetFidFreq(47.0 + norm(gen));
    ff.IdealFid(wf, tm);
    fid::FastFid myfid(wf, tm);

    std::copy(wf.begin(), wf.end(), &data.trace[idx][0]);

    // Make sure we got an FID signal
    if (myfid.isgood()) {

      data.freq[idx] = ff.freq();
      data.ferr[idx] = 0.0;

      data.freq_zc[idx] = myfid.GetFreq();
      data.ferr_zc[idx] = myfid.freq_err();

      data.fid_snr[idx] = myfid.snr();
      data.fid_len[idx] = myfid.fid_time();

    } else {

      myfid.DiagnosticInfo();
      data.freq[idx] = -1.0;
      data.ferr[idx] = -1.0;

      data.freq_zc[idx] = -1.0;
      data.ferr_zc[idx] = -1.0;

      data.fid_snr[idx] = -1.0;
      data.fid_len[idx] = -1.0;
    }

    data.clock_sys_ns[idx] = hw::systime_us() * 1000;
    data.clock_gps_ns[idx] = 0;
    data.device_clock[idx] = 0;
  }

  // Pop the event now that we are done copying it.
  cm_msg(MINFO, "read_fixed_event", "Finished simulating event");
}

void update_feedback_params()
{
  // Necessary ODB parameters
  HNDLE hDB, hkey;
  char str[256], stub[256];
  int size;
  BOOL flag;
  double freq[nprobes] = {0};
  double ferr[nprobes] = {0};
  double uniform_mean_freq = 0.0;
  double weighted_mean_freq = 0.0;
  
  cm_get_experiment_database(&hDB, NULL);

  // Check to see if the feedback subtree exists.
  snprintf(stub, sizeof(stub), "/Shared/Variables/PS Feedback");
  db_find_key(hDB, 0, stub, &hkey);

  // Create all the keys if not.
  if (!hkey) {
    
    snprintf(str, sizeof(str), "%s/uniform_mean_nmr_freq", stub);
    db_create_key(hDB, 0, str, TID_DOUBLE);

    snprintf(str, sizeof(str), "%s/weighted_mean_nmr_freq", stub);
    db_create_key(hDB, 0, str, TID_DOUBLE);

    snprintf(str, sizeof(str), "%s/using_freq_zc", stub);
    db_create_key(hDB, 0, str, TID_BOOL);

    snprintf(str, sizeof(str), "%s/nmr_freq_array", stub);
    db_create_key(hDB, 0, str, TID_DOUBLE);

    db_set_value(hDB, 0, str, &freq, nprobes, 
		 sizeof(freq), TID_DOUBLE);

    snprintf(str, sizeof(str), "%s/nmr_ferr_array", stub);
    db_create_key(hDB, 0, str, TID_DOUBLE);

    db_set_value(hDB, 0, str, &ferr, nprobes,
		 sizeof(ferr), TID_DOUBLE);
  }

  // See if we need to use the zero count freqs.
  BOOL use_zc = false;
  if (freq[0] > 0.0) {
    use_zc = true;
  }

  snprintf(str, sizeof(str), "%s/using_freq_zc", stub);
  db_set_value(hDB, 0, str, &use_zc, sizeof(use_zc), 1, TID_BOOL);
  
  double w_sum = 0.0;
  for (int i = 0; i < nprobes; ++i) {
    
    if (use_zc) {
      freq[i] = data.freq_zc[i];
      ferr[i] = data.ferr_zc[i];

    } else {

      freq[i] = data.freq[i];
      ferr[i] = data.ferr[i];
    }

    uniform_mean_freq += freq[i];
    weighted_mean_freq += freq[i] / (ferr[i] + 0.001);

    w_sum += 1.0 / (ferr[i] + 0.001);
  }

  uniform_mean_freq /= nprobes;
  weighted_mean_freq /= w_sum;

  snprintf(str, sizeof(str), "%s/weighted_mean_nmr_freq", stub);
  db_set_value(hDB, 0, str, 
	       &weighted_mean_freq, 
	       sizeof(weighted_mean_freq), 
	       1, TID_DOUBLE);

  snprintf(str, sizeof(str), "%s/uniform_mean_nmr_freq", stub);
  db_set_value(hDB, 0, str, 
	       &uniform_mean_freq, 
	       sizeof(uniform_mean_freq), 
	       1, TID_DOUBLE);

  snprintf(str, sizeof(str), "%s/nmr_freq_array", stub);
  db_set_value(hDB, 0, str, &freq, sizeof(freq), 
	       nprobes, TID_DOUBLE);

  snprintf(str, sizeof(str), "%s/nmr_ferr_array", stub);
  db_set_value(hDB, 0, str, &ferr, sizeof(ferr), 
	       nprobes, TID_DOUBLE);
}


void systems_check() 
{
  std::string al_msg;
  int vme_dev, rc; 
  ushort state;
  
  // Make sure the VME device file exists.
  vme_dev = open(hw::vme_path.c_str(), O_RDWR | O_NONBLOCK, 0);
  
  if (vme_dev < 0) {
    al_msg.assign("Fixed Probe System: VME driver not found");
    al_trigger_class("Failure", al_msg.c_str(), false);
    return;
  }

  // Make sure we can talk to the VME crate.
  rc = vme_A16D16_read(vme_dev, 0x0, &state);
  close(vme_dev);

  if (rc) {
    al_msg.assign("Fixed Probe System: communication with VME failed");
    al_trigger_class("Failure", al_msg.c_str(), false);
    return;
  }    

  // Verify that Meinberg software is installed.
  if (system("which mbgfasttstamp")) {
    al_msg.assign("Fixed Probe System: Meinberg software not found");
    al_trigger_class("Failure", al_msg.c_str(), false);
  }
}
