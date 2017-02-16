#include "fixed_probe_sequencer.hh"

namespace g2field {

FixedProbeSequencer::FixedProbeSequencer(int num_probes) : EventManagerBase()
{
  conf_file_ = std::string("config/fe_vme_shimming.json");
  num_probes_ = num_probes;

  Init();
}

FixedProbeSequencer::FixedProbeSequencer(std::string conf_file, int num_probes) :
  EventManagerBase()
{
  conf_file_ = conf_file;
  num_probes_ = num_probes;

  Init();
}

FixedProbeSequencer::~FixedProbeSequencer()
{
  for (auto &val : mux_boards_) {
    delete val;
  }

  if (nmr_pulser_trg_ != nullptr) {
    delete nmr_pulser_trg_;
  }
}

int FixedProbeSequencer::Init()
{
  go_time_ = false;
  thread_live_ = false;

  sequence_in_progress_ = false;
  builder_has_finished_ = true;
  mux_round_configured_ = false;
  analyze_fids_online_ = false;
  use_fast_fids_class_ = false;

  nmr_pulser_trg_ = nullptr;

  // Change the logfile if there is one in the config.
  boost::property_tree::ptree conf;
  boost::property_tree::read_json(conf_file_, conf);
  SetLogfile(conf.get<std::string>("logfile", logfile_));
}

int FixedProbeSequencer::BeginOfRun()
{
  boost::property_tree::ptree conf;
  boost::property_tree::read_json(conf_file_, conf);

  LogMessage("BeginOfRun executed - configuring with %s", conf_file_.c_str());

  // First set the config-dir if there is one.
  hw::conf_dir = conf.get<std::string>("config_dir", hw::conf_dir);
  fid_conf_file_ = conf.get<std::string>("fid_conf_file", "");
  analyze_fids_online_ = conf.get<bool>("analyze_fids_online", false);
  use_fast_fids_class_ = conf.get<bool>("use_fast_fids_class", false);

  if (fid_conf_file_ != std::string("")) {

    if ((fid_conf_file_[0] != '/') && (fid_conf_file_[0] != '\\')) {
      fid::load_params(hw::conf_dir + fid_conf_file_);

    } else {

      fid::load_params(fid_conf_file_);
    }
  }

  int sis_idx = 0;
  for (auto &v : conf.get_child("devices.sis_3302")) {

    std::string name(v.first);
    std::string dev_conf_file = std::string(v.second.data());

    if (dev_conf_file[0] != '/') {
      dev_conf_file = hw::conf_dir + std::string(v.second.data());
    }

    sis_idx_map_[name] = sis_idx++;

    LogDebug("loading hw: %s, %s", name.c_str(), dev_conf_file.c_str());
    workers_.PushBack(new hw::Sis3302(name, 
				      dev_conf_file, 
				      NMR_FID_LENGTH_ONLINE));
  }

  for (auto &v : conf.get_child("devices.sis_3316")) {

    std::string name(v.first);
    std::string dev_conf_file = std::string(v.second.data());

    if (dev_conf_file[0] != '/') {
      dev_conf_file = hw::conf_dir + std::string(v.second.data());
    }

    sis_idx_map_[name] = sis_idx++;

    LogDebug("loading hw: %s, %s", name.c_str(), dev_conf_file.c_str());
    workers_.PushBack(new hw::Sis3316(name, 
                                      dev_conf_file, 
                                      NMR_FID_LENGTH_ONLINE));
  }
  
  sis_idx = 0;

  // for (auto &v : conf.get_child("devices.sis_3350")) {

  //   std::string name(v.first);
  //   std::string dev_conf_file = std::string(v.second.data());

  //   if (dev_conf_file[0] != '/') {
  //     dev_conf_file = hw::conf_dir + std::string(v.second.data());
  //   }

  //   sis_idx_map_[name] = sis_idx++;

  //   LogDebug("loading hw: %s, %s", name.c_str(), dev_conf_file.c_str());
  //   workers_.PushBack(new hw::Sis3350(name, dev_conf_file));
  // }

  // Set up the NMR pulser trigger.
  char bid = conf.get<char>("devices.nmr_pulser.dio_board_id");
  int port = conf.get<int>("devices.nmr_pulser.dio_port_num");
  nmr_trg_mask_ = conf.get<int>("devices.nmr_pulser.dio_trg_mask");

  switch (bid) {
    case 'a':
      LogDebug("setting NMR pulser trigger on dio board A, port %i", port);
      nmr_pulser_trg_ = new hw::DioTriggerBoard(0x0, hw::BOARD_A, port);
      break;

    case 'b':
      LogDebug("setting NMR pulser trigger on dio board B, port %i", port);
      nmr_pulser_trg_ = new hw::DioTriggerBoard(0x0, hw::BOARD_B, port);
      break;

    case 'c':
      LogDebug("setting NMR pulser trigger on dio board C, port %i", port);
      nmr_pulser_trg_ = new hw::DioTriggerBoard(0x0, hw::BOARD_C, port);
      break;

    default:
      LogDebug("setting NMR pulser trigger on dio board D, port %i", port);
      nmr_pulser_trg_ = new hw::DioTriggerBoard(0x0, hw::BOARD_D, port);
      break;
  }

  max_event_time_ = conf.get<int>("max_event_time", 1000);
  mux_switch_time_ = conf.get<int>("mux_switch_time", 10000);

  trg_seq_file_ = conf.get<std::string>("trg_seq_file");

  if (trg_seq_file_[0] != '/') {
    trg_seq_file_ = hw::conf_dir + conf.get<std::string>("trg_seq_file");
  }

  mux_conf_file_ = conf.get<std::string>("mux_conf_file");

  if (mux_conf_file_[0] != '/') {
    mux_conf_file_ = hw::conf_dir + conf.get<std::string>("mux_conf_file");
  }

  // Load trigger sequence
  LogMessage("loading trigger sequence from %s", trg_seq_file_.c_str());
  boost::property_tree::read_json(trg_seq_file_, conf);

  for (auto &mux : conf) {

    int count = 0;
    for (auto &chan : mux.second) {

      // If we are adding a new mux, resize.
      if (trg_seq_.size() <= count) {
        trg_seq_.resize(count + 1);
      }

      // Map the multiplexer configuration.
      std::pair<std::string, int> trg(mux.first, std::stoi(chan.first));
      trg_seq_[count++].push_back(trg);

      // Map the data type and array index.
      for (auto &data : chan.second) {
	int idx = std::stoi(data.second.data());
	std::pair<std::string, int> loc(data.first, idx);
	data_out_[trg] = loc;
      }
    }
  }

  mux_boards_.resize(0);
  mux_boards_.push_back(new hw::DioMuxController(0x0, hw::BOARD_A, true));
  mux_boards_.push_back(new hw::DioMuxController(0x0, hw::BOARD_B, true));
  mux_boards_.push_back(new hw::DioMuxController(0x0, hw::BOARD_C, true));
  mux_boards_.push_back(new hw::DioMuxController(0x0, hw::BOARD_D, true));

  std::map<char, int> bid_map;
  bid_map['a'] = 0;
  bid_map['b'] = 1;
  bid_map['c'] = 2;
  bid_map['d'] = 3;

  // Load the data channel/mux maps
  boost::property_tree::read_json(mux_conf_file_, conf);
  for (auto &mux : conf) {
    char bid = mux.second.get<char>("dio_board_id");
    std::string mux_name(mux.first);
    int port = mux.second.get<int>("dio_port_num");

    mux_idx_map_[mux_name] = bid_map[bid];
    mux_boards_[mux_idx_map_[mux_name]]->AddMux(mux_name, port, false);

    std::string wfd_name(mux.second.get<std::string>("wfd_name"));
    int wfd_chan(mux.second.get<int>("wfd_chan"));
    std::pair<std::string, int> data_map(wfd_name, wfd_chan);

    data_in_[mux.first] = data_map;
  }

  // Start threads
  thread_live_ = true;
  run_thread_ = std::thread(&FixedProbeSequencer::RunLoop, this);
  trigger_thread_ = std::thread(&FixedProbeSequencer::TriggerLoop, this);
  builder_thread_ = std::thread(&FixedProbeSequencer::BuilderLoop, this);
  starter_thread_ = std::thread(&FixedProbeSequencer::StarterLoop, this);

  go_time_ = true;
  LogMessage("Starting workers");
  workers_.StartRun();

  // Pop stale events
  while (workers_.AnyWorkersHaveEvent()) {
    workers_.FlushEventData();
  }

  LogDebug("configuration loaded");
}

int FixedProbeSequencer::EndOfRun()
{
  int count = 0;
  do {
    usleep(1000);
  } while (sequence_in_progress_ && (count++ < 200));

  go_time_ = false;
  thread_live_ = false;

  workers_.StopRun();

  LogDebug("EndOfRun: joining threads");
  if (run_thread_.joinable()) {
    run_thread_.join();
  }

  if (trigger_thread_.joinable()) {
    trigger_thread_.join();
  }

  if (builder_thread_.joinable()) {
    builder_thread_.join();
  }

  if (starter_thread_.joinable()) {
    starter_thread_.join();
  }

  workers_.FreeList();

  mux_idx_map_.clear();
  sis_idx_map_.clear();
  trg_seq_.resize(0);

  queue_mutex_.lock();
  data_out_.clear();

  while (!data_queue_.empty()) {
    data_queue_.pop();
  }

  while (!run_queue_.empty()) {
    run_queue_.pop();
  }
  queue_mutex_.unlock();

  return 0;
}

int FixedProbeSequencer::ResizeEventData(hw::event_data_t &data)
{
  return 0;
}

void FixedProbeSequencer::RunLoop()
{
  while (thread_live_) {

    workers_.FlushEventData();

    while (go_time_) {

      if (workers_.AnyWorkersHaveEvent()) {

        LogDebug("RunLoop: got potential event");

        // Wait to be sure the others have events too.
        usleep(max_event_time_);

        if (!workers_.AllWorkersHaveEvent()) {

          LogWarning("event was not synchronized among all workers, dropping");
          workers_.FlushEventData();
          continue;

        } else if (workers_.AnyWorkersHaveMultiEvent()) {

          LogWarning("two events detected among some workers, dropping");
          workers_.FlushEventData();
          continue;
        }

        hw::event_data_t bundle;
        workers_.GetEventData(bundle);

        queue_mutex_.lock();
        if (data_queue_.size() <= kMaxQueueSize) {
          data_queue_.push(bundle);

          LogDebug("RunLoop: Got data. Data queue now: %i",
                   data_queue_.size());
        }

        queue_mutex_.unlock();
        workers_.FlushEventData();
      }

      std::this_thread::yield();
      usleep(hw::short_sleep);
    }

    std::this_thread::yield();
    usleep(hw::long_sleep);
  }
}

void FixedProbeSequencer::TriggerLoop()
{
  while (thread_live_) {

    while (go_time_) {

      if (got_start_trg_) {

	sequence_in_progress_ = true;
	builder_has_finished_ = false;
	mux_round_configured_ = false;

	LogMessage("received trigger, sequencing multiplexers");

	for (auto &round : trg_seq_) { // {mux_conf_0...mux_conf_n}
	  if (!go_time_) break;

	  got_round_data_ = false;

	  for (auto &conf : round) { // {mux_name, set_channel}
	    if (!go_time_) break;

	    LogDebug(std::string("TriggerLoop: setting ") +
		     conf.first + std::string(" to ") +
		     std::to_string(conf.second));

	    auto mux_board = mux_boards_[mux_idx_map_[conf.first]];
	    mux_board->SetMux(conf.first, conf.second);
	  }

      	  LogDebug("TriggerLoop: muxes are configured for this round");
          usleep(mux_switch_time_);
      	  nmr_pulser_trg_->FireTriggers(nmr_trg_mask_);

    	   LogDebug("TriggerLoop: muxes configure, triggers fired");
	       mux_round_configured_ = true;

         while (!got_round_data_ && go_time_) {
	    std::this_thread::yield();
	    usleep(hw::short_sleep);
	  }

	} // on to the next round

	sequence_in_progress_ = false;
	got_start_trg_ = false;

  LogDebug("TriggerLoop: waiting for builder to finish");
	while (!builder_has_finished_ && go_time_) {
	  std::this_thread::yield();
	  usleep(hw::short_sleep);
	};

  LogDebug("TriggerLoop: builder finished packing event");

      } // done with trigger sequence

      std::this_thread::yield();
      usleep(hw::short_sleep);
    }

    std::this_thread::yield();
    usleep(hw::long_sleep);
  }
}

void FixedProbeSequencer::BuilderLoop()
{
  using namespace std::chrono;

  LogDebug("BuilderLoop: launched");

  std::vector<double> tm(NMR_FID_LENGTH_ONLINE, 0.0);
  std::vector<double> wf(NMR_FID_LENGTH_ONLINE, 0.0);
  std::vector<int> indices;

  for (int i = 0; i < NMR_FID_LENGTH_ONLINE; ++i) {
    tm[i] = i * NMR_SAMPLE_PERIOD;
  }

  while (thread_live_) {

    while (go_time_) {

      static nmr_vector bundle;
      if (bundle.sys_clock.size() < num_probes_) bundle.Resize(num_probes_);

      static hw::event_data_t data;
      int seq_index = 0;

      if (sequence_in_progress_) {
        LogMessage("assembling a new event");
      }

      while (sequence_in_progress_ && go_time_) {

        if (mux_round_configured_) {

          queue_mutex_.lock();

          if (data_queue_.empty()) {

            queue_mutex_.unlock();

          } else {

            // Get the time as close to readout as we can.
            auto gps_clock = 0.0;

            // Grab the data itself.
            data = data_queue_.front();
            data_queue_.pop();
            queue_mutex_.unlock();

            LogDebug("BuilderLoop: copying data");

            for (auto &pair : trg_seq_[seq_index]) {

              // Get the right data out of the input.
              auto sis_name = data_in_[pair.first].first;
              int sis_idx = sis_idx_map_[sis_name];
              int trace_idx = data_in_[pair.first].second;

              LogDebug("BuilderLoop: copying %s, ch %i",
                       sis_name.c_str(), trace_idx);

              int idx = 0;
              ULong64_t clock = 0;

              // Store index and clock.
              clock = data[sis_idx].dev_clock[trace_idx];
              idx = data_out_[pair].second;
              bundle.dev_clock[idx] = clock;
              bundle.gps_clock[idx] = gps_clock;
              
              // Get FID data.
              auto arr_ptr = &bundle.trace[idx][0];
              auto trace = data[sis_idx].trace[trace_idx];
              auto size = data[sis_idx].trace[trace_idx].size();
              std::copy(&trace[0], &trace[0] + size, arr_ptr);
              
              // Save the index for analysis after copying
              indices.push_back(idx);
            }

            // Let the data collection begin for next round.
            seq_index++;
            got_round_data_ = true;
            mux_round_configured_ = false;

            // Analyze the FIDs from this round.
            for (auto &idx : indices) {

              // Get the timestamp
              bundle.sys_clock[idx] = hw::systime_us();

              if (analyze_fids_online_ || (idx == 0)) {

                LogDebug("BuilderLoop: analyzing FID %i", idx);

                std::copy(&bundle.trace[idx][0],
                          &bundle.trace[idx][NMR_FID_LENGTH_ONLINE],
                          wf.begin());

                // Extract the FID frequency and some diagnostic params.
                if (use_fast_fids_class_) {

                  fid::FastFid myfid(wf, tm);

                  // Make sure we got an FID signal
                  if (myfid.isgood()) {

                    bundle.snr[idx] = myfid.snr();
                    bundle.len[idx] = myfid.fid_time();
                    bundle.freq[idx] = myfid.CalcFreq();
                    bundle.ferr[idx] = myfid.freq_err();
                    bundle.method[idx] = (ushort)fid::Method::ZC;
                    bundle.health[idx] = myfid.isgood();
                    bundle.freq_zc[idx] = myfid.CalcFreq();
                    bundle.ferr_zc[idx] = myfid.freq_err();

                  } else {

                    myfid.DiagnosticInfo();
                    bundle.snr[idx] = 0.0;
                    bundle.len[idx] = 0.0;
                    bundle.freq[idx] = 0.0;
                    bundle.ferr[idx] = 0.0;
                    bundle.method[idx] = (ushort)fid::Method::ZC;
                    bundle.health[idx] = myfid.isgood();
                    bundle.freq_zc[idx] = 0.0;
                    bundle.ferr_zc[idx] = 0.0;
                  }

                } else {

                  fid::Fid myfid(wf, tm);

                  // Make sure we got an FID signal
                  if (myfid.isgood()) {

                    bundle.snr[idx] = myfid.snr();
                    bundle.len[idx] = myfid.fid_time();
                    bundle.freq[idx] = myfid.CalcPhaseFreq();
                    bundle.ferr[idx] = myfid.freq_err();
                    bundle.method[idx] = (ushort)fid::Method::PH;
                    bundle.health[idx] = myfid.isgood();
                    bundle.freq_zc[idx] = myfid.CalcZeroCountFreq();
                    bundle.ferr_zc[idx] = myfid.freq_err();

                  } else {

                    myfid.DiagnosticInfo();
                    bundle.snr[idx] = 0.0;
                    bundle.len[idx] = 0.0;
                    bundle.freq[idx] = 0.0;
                    bundle.ferr[idx] = 0.0;
                    bundle.method[idx] = (ushort)fid::Method::PH;
                    bundle.health[idx] = myfid.isgood();
                    bundle.freq_zc[idx] = 0.0;
                    bundle.ferr_zc[idx] = 0.0;
                  }
                }
              } else {

                LogDebug("BuilderLoop: skipping analysis");

                bundle.snr[idx] = 0.0;
                bundle.len[idx] = 0.0;
                bundle.freq[idx] = 0.0;
                bundle.ferr[idx] = 0.0;
                bundle.method[idx] = (ushort)fid::Method::PH;
                bundle.health[idx] = 0;
                bundle.freq_zc[idx] = 0.0;
                bundle.ferr_zc[idx] = 0.0;
              }
            } // next pair

            indices.resize(0);
	  }
        } // next round

        std::this_thread::yield();
        usleep(hw::short_sleep);
      }

      // Sequence finished.
      if (!sequence_in_progress_ && !builder_has_finished_) {

        // Get the system time.
        LogMessage("new event assembled, pushing to run_queue_");

        queue_mutex_.lock();
        run_queue_.push(bundle);

        LogDebug("BuilderLoop: Size of run_queue_ = ", run_queue_.size());

        has_event_ = true;
        seq_index = 0;
        builder_has_finished_ = true;
        queue_mutex_.unlock();
      }

      std::this_thread::yield();
      usleep(hw::long_sleep);
    } // go_time_

    std::this_thread::yield();
    usleep(hw::long_sleep);
  } // thread_live_
}

void FixedProbeSequencer::StarterLoop()
{
  bool rc = false;

  boost::property_tree::ptree conf;
  boost::property_tree::read_json(conf_file_, conf);

  while (thread_live_) {

    while (go_time_ && thread_live_) {

      if (got_software_trg_){
	got_start_trg_ = true;
	got_software_trg_ = false;
	LogDebug("StarterLoop: Got software trigger");
      }

      std::this_thread::yield();
      usleep(hw::long_sleep);
    }

    std::this_thread::yield();
    usleep(hw::long_sleep);
  }
}

} // ::gm2


