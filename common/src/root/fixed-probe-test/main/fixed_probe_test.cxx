/*****************************************************************************\

Name:   fixed_probe_test.cxx
Author: Matthias W. Smith
Email:  mwsmith2@uw.edu

About:  Implements a test front-end that sequences fixed probes and
        writes a ROOT file.

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
#include <atomic>
#include <array>
#include <cmath>
#include <ctime>
using std::string;

//--- other includes --------------------------------------------------------//
#include "TFile.h"
#include "TTree.h"

//--- project includes ------------------------------------------------------//
#include "fixed_probe_sequencer.hh"
#include "g2field/core/field_structs.hh"

//--- globals ---------------------------------------------------------------//
namespace {

// Run defaults
int event_total = 120;
std::atomic<int> event_count(0);
double event_rate_limit = 1.0;
bool save_full_waveforms = false;

// ROOT vars
TFile *pf;
TTree *pt;

g2field::fixed_t data;
g2field::online_fixed_t full_data;
g2field::FixedProbeSequencer *event_manager;

// Constants
const int nprobes = g2field::kNmrNumFixedProbes;
}

int m_init(int argc, char *argv[]);
int m_exit();
int read_event();

int main(int argc, char *argv[])
{
  m_init(argc, argv);

  while (event_count < event_total) {
    read_event();
    usleep(1e6 / event_rate_limit);
  }

  m_exit();
  
  return 0;
}


//---------------------------------------------------------------------------//
//---------------------------------------------------------------------------//
int m_init(int argc, char *argv[])
{
  // Allocations
  int rc = 0;
  std::string conf_file;
  std::string filename;

  // Parse args
  if (argc >= 2) {

    conf_file = std::string(argv[1]);
    filename = std::string(argv[2]);

  } else {

    std::cout << "usage:" << std::endl;
    std::cout << "fixed_probe_test <config-file> <output-file>" << std::endl;
    exit(1);
  }

  // Optional args
  save_full_waveforms = false;
  event_rate_limit = 1.0; 

  // Set up the ROOT output.
  pf = new TFile(filename.c_str(), "recreate");
  pt = new TTree("t", "Fixed Probe Test Data");
  pt->SetAutoSave(5);
  pt->SetAutoFlush(20);

  pt->Branch("fixed", &data.sys_clock[0], g2field::fixed_str);

  if (save_full_waveforms) {
    pt->Branch("full_fixed",
               &full_data.sys_clock[0],
               g2field::online_fixed_str);
  }

  // Set up the event mananger.
  event_manager = new g2field::FixedProbeSequencer(conf_file, nprobes);

  event_manager->BeginOfRun();

  return 0;
}


//---------------------------------------------------------------------------//
//---------------------------------------------------------------------------//
int m_exit()
{
  // Make sure we write the ROOT data.
  pt->Write();
  pf->Write();
  pf->Close();

  delete pf;

  // Clean up the event manager.
  event_manager->EndOfRun();
  delete event_manager;

  return 0;
}

//--- read_event ------------------------------------------------------------//
int read_event()
{
  static bool triggered = false;
  static unsigned long long num_events;
  static unsigned long long events_written;

  // Allocate vectors for the FIDs
  static std::vector<double> tm;
  static std::vector<double> wf;

  if (!triggered) {
    event_manager->IssueTrigger();
    triggered = true;
  }

  if (triggered && !event_manager->HasEvent()) {

    std::cout << "Event Manager has no data, exiting readout" << std::endl;
    return 0;

  } else {

    std::cout << "Event Manager has data, beginning readout" << std::endl;
    
  }

  auto event_data = event_manager->GetCurrentEvent();

  std::cout << "event_readout: copied data" << std::endl;

  if ((event_data.sys_clock[0] == 0) && (event_data.sys_clock[nprobes-1] == 0)) {
    event_manager->PopCurrentEvent();
    triggered = false;
    std::cout << "event_readout: empty event, aborting" << std::endl;
    return 0;
  }

  // Set the time vector.
  if (tm.size() == 0) {

    tm.resize(g2field::kNmrFidLengthRecord);
    wf.resize(g2field::kNmrFidLengthRecord);

    for (int n = 0; n < g2field::kNmrFidLengthRecord; ++n) {
      tm[n] = 0.001 * n;
    }
  }

  for (int idx = 0; idx < nprobes; ++idx) {
    
    std::cout << "read_event: re-analyzing event " << idx << std::endl;

    for (int n = 0; n < g2field::kNmrFidLengthRecord; ++n) {
      wf[n] = event_data.trace[idx][n*10 + 1]; // Offset avoid spikes in sis3302
      data.trace[idx][n] = wf[n];
    }

    std::cout << "read_event: copied waveform " << idx << std::endl;

    fid::FastFid myfid(wf, tm);

    std::cout << "read_event: Fid initialized " << idx << std::endl;

    // Make sure we got an FID signal
    if (myfid.isgood()) {

      //      data.freq[idx] = myfid.CalcPhaseFreq();
      data.freq[idx] = myfid.freq();
      data.ferr[idx] = myfid.freq_err();
      data.snr[idx] = myfid.snr();
      data.len[idx] = myfid.fid_time();

    } else {

      myfid.DiagnosticInfo();
      data.freq[idx] = -1.0;
      data.ferr[idx] = -1.0;
      data.snr[idx] = -1.0;
      data.len[idx] = -1.0;
    }
  }

  std::copy(event_data.sys_clock.begin(),
            event_data.sys_clock.begin() + nprobes,
            &data.sys_clock[0]);

  std::copy(event_data.gps_clock.begin(),
            event_data.gps_clock.begin() + nprobes,
            &data.gps_clock[0]);

  std::copy(event_data.dev_clock.begin(),
            event_data.dev_clock.begin() + nprobes,
            &data.dev_clock[0]);

  std::copy(event_data.snr.begin(),
            event_data.snr.begin() + nprobes,
            &data.snr[0]);

  std::copy(event_data.len.begin(),
            event_data.len.begin() + nprobes,
            &data.len[0]);

  std::copy(event_data.freq.begin(),
            event_data.freq.begin() + nprobes,
            &data.freq[0]);

  std::copy(event_data.ferr.begin(),
            event_data.ferr.begin() + nprobes,
            &data.ferr[0]);

  std::copy(event_data.freq_zc.begin(),
            event_data.freq_zc.begin() + nprobes,
            &data.freq_zc[0]);

  std::copy(event_data.ferr_zc.begin(),
            event_data.ferr_zc.begin() + nprobes,
            &data.ferr_zc[0]);

  std::copy(event_data.method.begin(),
            event_data.method.begin() + nprobes,
            &data.method[0]);

  std::copy(event_data.health.begin(),
            event_data.health.begin() + nprobes,
            &data.health[0]);

  // Now that we have a copy of the latest event, fill the tree.
  pt->Fill();
  
  num_events++;

  if (num_events % 10 == 1) {
    
    pt->AutoSave("SaveSelf,FlushBaskets");
    pf->Flush();
  }

  // Pop the event now that we are done copying it.
  event_manager->PopCurrentEvent();

  // Let the front-end know we are ready for another trigger.
  triggered = false;
  event_count++;

  return 0;
}
