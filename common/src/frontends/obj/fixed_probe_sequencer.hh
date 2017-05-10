#ifndef FIELD_DAQ_FRONTENDS_OBJ_FIXED_PROBE_SEQUENCER_HH_
#define FIELD_DAQ_FRONTENDS_OBJ_FIXED_PROBE_SEQUENCER_HH_

/*===========================================================================*\

  author: Matthias W. Smith
  email:  mwsmith2@uw.edu
  file:   fixed_probe_sequencer.hh

  about:  Implements an event builder that sequences each channel of
          a set of multiplexers according to a json config file
	  which defines the trigger sequence.

\*===========================================================================*/


//--- std includes ----------------------------------------------------------//
#include <string>
#include <map>
#include <vector>
#include <cassert>

//--- other includes --------------------------------------------------------//
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "fid.h"

//--- project includes -------------------------------------------------------/
#include "g2field/event_manager_base.hh"
#include "g2field/dio_mux_controller.hh"
#include "g2field/dio_trigger_board.hh"
#include "g2field/sis3302.hh"
#include "g2field/sis3316.hh"
#include "g2field/core/field_constants.hh"
#include "g2field/core/field_structs.hh"
#include "frontend_utils.hh"


namespace g2field {

class FixedProbeSequencer: public hw::EventManagerBase {

public:

  //ctor
  FixedProbeSequencer(std::string conf_file, int num_probes);

  //dtor
  ~FixedProbeSequencer();

  // Simple initialization.
  int Init();

  // Launches any threads needed and start collecting data.
  int BeginOfRun();

  // Rejoins threads and stops data collection.
  int EndOfRun();

  int ResizeEventData(hw::event_data_t &data);

  // Issue a software trigger to take another sequence.
  inline int IssueTrigger() {
    got_software_trg_ = true;
  }

  // Returns the oldest stored event.
  inline const nmr_vector GetCurrentEvent() {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    if (!run_queue_.empty()) {

      return run_queue_.front();

    } else {

      nmr_vector tmp;
      tmp.Resize(num_probes_);
      return tmp;
    }
  };

  // Removes the oldest event from the front of the queue.
  inline void PopCurrentEvent() {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    if (!run_queue_.empty()) {
      run_queue_.pop();
    }
    if (run_queue_.empty()) {
      has_event_ = false;
    }
  };

private:

  const std::string name_ = "FixedProbeSequencer";

  int min_event_time_;
  int max_event_time_;
  int num_probes_;
  std::atomic<bool> got_software_trg_;
  std::atomic<bool> got_start_trg_;
  std::atomic<bool> got_round_data_;
  std::atomic<bool> generate_software_triggers_;
  std::atomic<bool> builder_has_finished_;
  std::atomic<bool> sequence_in_progress_;
  std::atomic<bool> mux_round_configured_;
  std::atomic<bool> analyze_fids_online_;
  std::atomic<bool> use_fast_fids_class_;
  std::string mux_sequence_;
  std::string mux_connections_;
  std::string fid_analysis_;

  int nmr_trg_mask_;
  int mux_switch_time_;
  std::vector<hw::DioTriggerBoard *> dio_triggers_;
  std::vector<hw::DioMuxController *> mux_boards_;
  std::map<std::string, int> mux_idx_map_;
  std::map<std::string, int> sis_idx_map_;
  std::map<std::string, std::pair<std::string, int>> data_in_;
  std::map<std::pair<std::string, int>, std::pair<std::string, int>> data_out_;
  std::vector<std::vector<std::pair<std::string, int>>> trg_seq_;

  std::queue<nmr_vector> run_queue_;
  std::mutex queue_mutex_;
  std::thread trigger_thread_;
  std::thread builder_thread_;
  std::thread starter_thread_;

  // Collects from data workers, i.e., direct from the waveform digitizers.
  void RunLoop();

  // Listens for sequence start signals.
  void StarterLoop();

  // Runs through the trigger sequence by configuring multiplexers for
  // several rounds and interacting with the BuilderLoop to ensure data
  // capture.
  void TriggerLoop();

  // Builds the event by selecting the proper data from each round.
  void BuilderLoop();

  // Thread sleep functions.
  inline void ThreadSleepLong() {
    auto dt = std::chrono::microseconds(hw::long_sleep);
    std::this_thread::sleep_for(dt);
  }

  inline void ThreadSleepShort() {
    auto dt = std::chrono::microseconds(hw::short_sleep);
    std::this_thread::sleep_for(dt);
  }
};

} // ::daq

#endif

