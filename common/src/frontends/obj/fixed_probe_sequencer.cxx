#include "fixed_probe_sequencer.hh"

namespace g2field {

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

  for (auto &val : dio_triggers_) {
     delete val;
  }
}

int FixedProbeSequencer::Init()
{
  go_time_ = false;
  thread_live_ = false;

  sequence_in_progress_ = false;
  builder_has_finished_ = false;
  mux_round_configured_ = false;
  analyze_fids_online_ = false;
  use_fast_fids_class_ = false;
  got_software_trg_ = false;
  got_start_trg_ = false;

  // Change the logfile if there is one in the config.
  boost::property_tree::ptree conf;
  boost::property_tree::read_json(conf_file_, conf);
  SetLogfile(conf.get<std::string>("output.logfile", logfile_));
  SetVerbosity(conf.get<int>("output.verbosity", logging_verbosity_));
}

int FixedProbeSequencer::BeginOfRun()
{
  // Synchronize with g2field-be.
  DWORD time;
  cm_synchronize(&time);

  // Open the config property tree.
  boost::property_tree::ptree conf;
  boost::property_tree::read_json(conf_file_, conf);

  LogMessage("BeginOfRun executed - configuring with %s", conf_file_.c_str());

  // First set the config-dir if there is one.
  hw::conf_dir = conf.get<std::string>("config_dir", hw::conf_dir);
  fid_analysis_ = conf.get<std::string>("config.fid_analysis", "");
  analyze_fids_online_ = conf.get<bool>("analyze_fids_online", false);
  use_fast_fids_class_ = conf.get<bool>("use_fast_fids_class", false);

  generate_software_triggers_ = 
    conf.get<bool>("generate_software_triggers", false);

  if (fid_analysis_ != std::string("")) {

    if ((fid_analysis_[0] != '/') && (fid_analysis_[0] != '\\')) {
      fid::load_params(hw::conf_dir + fid_analysis_);

    } else {

      fid::load_params(fid_analysis_);
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

  // Set up the NMR pulser triggers
  dio_triggers_.resize(0);

  for (auto &v : conf.get_child("devices.dio_triggers")) {
    
    char bid = v.second.get<char>("dio_board_id");
    int port = v.second.get<int>("dio_port_num");
    int trg_mask = v.second.get<int>("dio_trg_mask");

    switch (bid) {
      case 'a':
	LogDebug("setting NMR pulser trigger on dio board A, port %i", port);
	dio_triggers_.push_back(new hw::DioTriggerBoard(0x0, hw::BOARD_A, port, false));
	break;
	
      case 'b':
	LogDebug("setting NMR pulser trigger on dio board B, port %i", port);
	dio_triggers_.push_back(new hw::DioTriggerBoard(0x0, hw::BOARD_B, port, false));
	break;
      
      case 'c':
	LogDebug("setting NMR pulser trigger on dio board C, port %i", port);
	dio_triggers_.push_back(new hw::DioTriggerBoard(0x0, hw::BOARD_C, port, false));
	break;

      default:
	LogDebug("setting NMR pulser trigger on dio board D, port %i", port);
	dio_triggers_.push_back(new hw::DioTriggerBoard(0x0, hw::BOARD_D, port, false));
	break;
    }

    // Set the trigger mask.
    (*(dio_triggers_.end() - 1))->SetTriggerMask(trg_mask);
  }

  min_event_time_ = conf.get<int>("min_event_time", 1000);
  max_event_time_ = conf.get<int>("max_event_time", 10000);
  mux_switch_time_ = conf.get<int>("mux_switch_time", 15000);

  mux_sequence_ = conf.get<std::string>("config.mux_sequence");

  if (mux_sequence_[0] != '/') {
    mux_sequence_ = hw::conf_dir + 
      conf.get<std::string>("config.mux_sequence");
  }

  mux_connections_ = conf.get<std::string>("config.mux_connections");

  if (mux_connections_[0] != '/') {
    mux_connections_ = hw::conf_dir + 
      conf.get<std::string>("config.mux_connections");
  }

  // Load trigger sequence
  LogMessage("loading trigger sequence from %s", mux_sequence_.c_str());
  boost::property_tree::read_json(mux_sequence_, conf);

  for (auto &mux : conf) {

    int count = 0;
    // std::cout << "load mux: " << mux.first << ", " << std::endl;
    // boost::property_tree::write_json(std::cout, mux.second);

    for (auto &chan : mux.second) {

      // If we are adding a new mux, resize.
      if (trg_seq_.size() <= count) {
	trg_seq_.resize(count + 1);
      }

      // Map the multiplexer configuration.
      int ch = std::stoi(chan.first.substr(3)); // skip 'ch_'
      std::pair<std::string, int> trg(mux.first, ch);
      trg_seq_[count++].push_back(trg);

      // Map the data type and array index.
      int idx = std::stoi(chan.second.data());
      std::pair<std::string, int> loc(chan.first, idx);
      data_out_[trg] = loc;
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
  boost::property_tree::read_json(mux_connections_, conf);
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

  usleep(5000);

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

    while (go_time_ && !generate_software_triggers_) {

      if (workers_.AnyWorkersHaveEvent()) {

        LogDebug("RunLoop: got potential event");

        // Wait to be sure the others have events too.
	int count = 0;

	while (count++ * 500 < min_event_time_) {
	  usleep(500);
	}
	
	LogDebug("RunLoop: slept minimum");

	while (!workers_.AllWorkersHaveEvent() &&
	       (count++ * 500 < max_event_time_)) {
	  usleep(500);
	  LogDump("RunLoop: sleep check");
	}
	
	LogDebug("RunLoop: slept max");

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

	LogDebug("RunLoop: copying data bundle");

        queue_mutex_.lock();
        if (data_queue_.size() <= kMaxQueueSize) {
          data_queue_.push(bundle);

          LogDebug("RunLoop: Got data. Data queue now: %i",
                   data_queue_.size());
        }

	

        queue_mutex_.unlock();
      }
      
      ThreadSleepLong();
    }

    ThreadSleepLong();
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

	LogMessage("TriggerLoop: received trigger, sequencing multiplexers");

	for (auto &round : trg_seq_) { // {mux_conf_0...mux_conf_n}
	  if (!go_time_) break;

	  got_round_data_ = false;
	  mux_round_configured_ = false;

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

	  if (generate_software_triggers_) {

	    // Pause workers.
	    workers_.StopWorkers();
	    hw::wait_ns(hw::short_sleep * 2);
	    
	    // Fire triggers and get waveforms on 3316.
	    int idx = sis_idx_map_["sis_3316_0"];
	    auto wfd_3316 = workers_[idx];
	    
	    idx = sis_idx_map_["sis_3302_0"];
	    auto wfd_3302 = workers_[idx];

	    // Trigger the 3316 and relevant pulser modules.
	    while (wfd_3316->GenerateTrigger() != 0) usleep(10000);
	    dio_triggers_[1]->FireTriggers(0xff);
	    dio_triggers_[2]->FireTriggers(0xff);

	    // Trigger the 3302 and pulse the relevant NMR pulsers.
	    while (wfd_3302->GenerateTrigger() != 0) usleep(10000);
	    hw::wait_ns(0.5e6);
	    dio_triggers_[3]->FireTriggers(0x0f);

	    workers_.StartWorkers();
	    hw::wait_ns(100e6);
	  
	    while (!workers_.AllWorkersHaveEvent()) usleep(500);

	    hw::event_data_t bundle;
	    workers_.GetEventData(bundle);
	    hw::wait_ns(hw::short_sleep * 2);
	    
	    queue_mutex_.lock();
	    if (data_queue_.size() <= kMaxQueueSize) {
	      data_queue_.push(bundle);
	      
	      LogDebug("RunLoop: Got data. Data queue now: %i",
		       data_queue_.size());
	    }
	    queue_mutex_.unlock();
	    
	  } else {
	    
	    LogDebug("Generated DIO triggers");
	    int trg_count = 0;
	    
	    for (auto &trg : dio_triggers_) {

	      int rc = trg->FireTriggers();

	      LogDebug("Trigger %i fired", trg_count);

	      while (rc > 0) {
		LogError("Trigger %i failed with rc = %i", trg_count, rc);
	       	rc = trg->FireTriggers();
	       	LogMessage("Trigger %i re-fired", trg_count);
	      }
	      ++trg_count;
	    }
	  }
	  
	  LogDebug("TriggerLoop: muxes configured, triggers fired");
	  mux_round_configured_ = true;
	  
	  uint count = 0;

	  while (!got_round_data_ && go_time_) {
	    
	    // If one second passes, flush and re-trigger.
	    if (count++ * hw::short_sleep > 1e6) {

	      workers_.FlushEventData();

	      for (auto &trg : dio_triggers_) {
		trg->FireTriggers();
	      }

	      count = 0;
	    }
	    
	    ThreadSleepShort();
	  };
	  
	} // on to the next round

	sequence_in_progress_ = false;
	got_start_trg_ = false;

	LogDebug("TriggerLoop: waiting for builder to finish");
	
	while (!builder_has_finished_ && go_time_) {
	  ThreadSleepShort();
	};
	
	LogDebug("TriggerLoop: builder finished packing event");
	ThreadSleepShort();
	
      } // done with trigger sequence
    }

    ThreadSleepLong();
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
      if (bundle.clock_sys_ns.size() < num_probes_) bundle.Resize(num_probes_);

      static hw::event_data_t data;
      int seq_index = 0;

      if (sequence_in_progress_) {
        LogMessage("BuilderLoop: beginning a new event");
      }

      while (sequence_in_progress_ && go_time_) {

        if (mux_round_configured_) {

          queue_mutex_.lock();

          if (data_queue_.empty()) {

            queue_mutex_.unlock();

          } else {

            // Get the time as close to readout as we can.
            auto gps_clock = parse_mbg_string_ns();

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

              int idx = 0;
              ULong64_t clock = 0;

              // Store index and clock.
              clock = data[sis_idx].dev_clock[trace_idx];
              idx = data_out_[pair].second;
              bundle.clock_gps_ns[idx] = gps_clock;
              bundle.device_clock[idx] = clock;

	      if (sis_idx == 0) {

		bundle.device_rate_mhz[idx] = 10.0;
		bundle.device_gain_vpp[idx] = 20.0;

	      } else if (sis_idx == 1) {

		bundle.device_rate_mhz[idx] = 10.0;
		bundle.device_gain_vpp[idx] = 10.0;
	      }

              // Get FID data.
              auto arr_ptr = &bundle.trace[idx][0];
              auto trace = data[sis_idx].trace[trace_idx];
              auto size = data[sis_idx].trace[trace_idx].size();

              LogDebug("BuilderLoop: round %i, copying %s, ch %i -> %i, %i samples",
                       seq_index, sis_name.c_str(), trace_idx, idx, size);

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
              bundle.clock_sys_ns[idx] = hw::systime_us() * 1000;

              if (analyze_fids_online_) {

                LogDebug("BuilderLoop: analyzing FID %i", idx);

                std::copy(&bundle.trace[idx][0],
                          &bundle.trace[idx][NMR_FID_LENGTH_ONLINE],
                          wf.begin());

                // Extract the FID frequency and some diagnostic params.
                if (use_fast_fids_class_) {

                  fid::FastFid myfid(wf, tm);

                  // Make sure we got an FID signal
                  if (myfid.isgood() && std::isfinite(myfid.CalcFreq())) {
		    
		    // if (idx == 0) {
		    //   std::cout << "probe000: freq = " << myfid.freq() << std::endl;
		    // }

                    bundle.fid_amp[idx] = myfid.amp();
                    bundle.fid_snr[idx] = myfid.snr();
                    bundle.fid_len[idx] = myfid.fid_time();
                    bundle.freq[idx] = myfid.CalcFreq();
                    bundle.ferr[idx] = myfid.freq_err();
                    bundle.freq_zc[idx] = myfid.CalcFreq();
                    bundle.ferr_zc[idx] = myfid.freq_err();
                    bundle.method[idx] = (ushort)fid::Method::ZC;
                    bundle.health[idx] = myfid.health();

                  } else {

                    // myfid.DiagnosticInfo();
                    bundle.fid_snr[idx] = 0.0;
                    bundle.fid_len[idx] = 0.0;
                    bundle.freq[idx] = 0.0;
                    bundle.ferr[idx] = 0.0;
                    bundle.freq_zc[idx] = 0.0;
                    bundle.ferr_zc[idx] = 0.0;
                    bundle.method[idx] = (ushort)fid::Method::ZC;
                    bundle.health[idx] = myfid.health();
                  }

                } else {

                  fid::Fid myfid(wf, tm);

                  // Make sure we got an FID signal
                  if (myfid.isgood()) {

                    bundle.fid_amp[idx] = myfid.amp();
                    bundle.fid_snr[idx] = myfid.snr();
                    bundle.fid_len[idx] = myfid.fid_time();
                    bundle.freq[idx] = myfid.CalcPhaseFreq();
                    bundle.ferr[idx] = myfid.freq_err();
                    bundle.freq_zc[idx] = myfid.CalcZeroCountFreq();
                    bundle.ferr_zc[idx] = myfid.freq_err();
                    bundle.method[idx] = (ushort)fid::Method::PH;
                    bundle.health[idx] = myfid.health();

                  } else {

                    myfid.DiagnosticInfo();
                    bundle.fid_amp[idx] = 0.0;
                    bundle.fid_snr[idx] = 0.0;
                    bundle.fid_len[idx] = 0.0;
                    bundle.freq[idx] = 0.0;
                    bundle.ferr[idx] = 0.0;
                    bundle.freq_zc[idx] = 0.0;
                    bundle.ferr_zc[idx] = 0.0;
                    bundle.method[idx] = (ushort)fid::Method::PH;
                    bundle.health[idx] = myfid.health();
                  }
                }
              } else {

                LogDebug("BuilderLoop: skipping analysis");

                bundle.fid_amp[idx] = 0.0;
                bundle.fid_snr[idx] = 0.0;
                bundle.fid_len[idx] = 0.0;
                bundle.freq[idx] = 0.0;
                bundle.ferr[idx] = 0.0;
                bundle.freq_zc[idx] = 0.0;
                bundle.ferr_zc[idx] = 0.0;
                bundle.method[idx] = (ushort)fid::Method::PH;
                bundle.health[idx] = 0;
              }
            } // next pair

            indices.resize(0);
	  }
        } // next round

	ThreadSleepShort();
      }

      // Sequence finished.
      if (!sequence_in_progress_ && !builder_has_finished_) {

        // Get the system time.
        LogMessage("new event assembled, pushing to run_queue_");

        queue_mutex_.lock();
        run_queue_.push(bundle);

        LogDebug("BuilderLoop: Size of run_queue_ = %i", run_queue_.size());

        has_event_ = true;
        seq_index = 0;
        builder_has_finished_ = true;
        queue_mutex_.unlock();
      }

      ThreadSleepLong();
    } // go_time_

    ThreadSleepLong();
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

      ThreadSleepLong();
    }

    ThreadSleepLong();
  }
}

} // ::gm2


