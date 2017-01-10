#ifndef FRONTENDS_INCLUDE_FRONTEND_UTILS_HH_
#define FRONTENDS_INCLUDE_FRONTEND_UTILS_HH_

/*===========================================================================*\

author: Matthias W. Smith
email:  mwsmith2@uw.edu
file:   frontend_utils.hh

about:  Contains helper functions for MIDAS frontends.

\*===========================================================================*/

//--- std includes ----------------------------------------------------------//
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <memory>
#include <vector>
#include <array>
#include <mutex>
#include <sys/time.h>

//--- other includes --------------------------------------------------------//
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/filesystem.hpp>
#include "midas.h"

// Load the config from the ODB as json format.
inline int load_settings(char *frontend, boost::property_tree::ptree& conf)
{
  // Data part
  HNDLE hDB, hkey;
  char str[256], logdir[256];
  int size = 0;
  int bytes_written = 0;
  int rc = 0;

  char *json_buf = new char[0x8000];
  boost::property_tree::ptree pt;
  std::stringstream ss;

  // Get the experiment database handle.
  cm_get_experiment_database(&hDB, NULL);

  snprintf(str, 256, "/Equipment/%s/Settings", frontend);
  db_find_key(hDB, 0, str, &hkey);

  if (hkey) {
    db_copy_json_values(hDB, hkey, &json_buf, &size, &bytes_written, 1, 1, 1);

  } else {

    cm_msg(MERROR, frontend, "failed to load \"%s\" from ODB", str);
    return FE_ERR_ODB;
  }

  // Store it in a property tree.
  ss << json_buf;
  boost::property_tree::read_json(ss, conf);

  // Check if we need to alter the logfile path.
  std::string logfile = conf.get<std::string>("logfile", "");

  // If none or absolute path do nothing
  if ((logfile.length() != 0) && (logfile.find("/") != 0)) {

    snprintf(str, 256, "/Logger/Log Dir");
    db_find_key(hDB, 0, str, &hkey);

    if (hkey) {
      size = sizeof(logdir);
      db_get_data(hDB, hkey, logdir, &size, TID_STRING);

    } else {
      cm_msg(MERROR, "log directory not defined, odb:%s", str);
      return FE_ERR_ODB;
    }

    boost::filesystem::path path_logdir(logdir);
    boost::filesystem::path path_logfile(logfile);

    std::string new_logfile((path_logdir / path_logfile).string());
    conf.put<std::string>("logfile", new_logfile);
  }

  return SUCCESS;
}

// Call a shell command and return stdout.
inline std::string exec(const char *cmd) 
{
  char buffer[128];
  std::string result("");
  std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
  
  if (!pipe) throw std::runtime_error("popen() failed!");

  while (!feof(pipe.get())) {
    if (fgets(buffer, 128, pipe.get()) != NULL) {
      result += buffer;
    }
  }
  
  return result;
}

// Parse the output of mbgfasttstamp into UTC nanoseconds.
inline uint64_t parse_mbg_string_ns(std::string ts="") 
{
  std::stringstream ss;
  std::string timestamp;
  std::string str;
  
  bool valid_ts = false;
  uint64_t time_ns = 0;

  // If necessary, grab a timestamp, and push result into stringstream.
  if (ts == "") {
    ss.str(exec("mgbfasttstamp"));
  } else {
    ss.str(ts);
  }

  while (ss.good()) {

    std::getline(ss, str, '\n');

    if (str.find("HR time raw:") == 0) {
      valid_ts = true;
      timestamp = str;
      break;
    }
  }

  if (valid_ts) {
    
    ss.str(timestamp);
    uint64_t t;

    std::getline(ss, str, ':');

    std::getline(ss, str, ',');
    ss.str(str);

    std::string sec_string;
    std::string nsec_string;
    
    std::getline(ss, sec_string, '.');
    std::getline(ss, nsec_string, '.');
    
    t = std::stoul(sec_string, nullptr, 16);
    time_ns += t * 1.0e9;

    t = std::stoul(nsec_string, nullptr, 16);
    time_ns += 1.0e9 * (t / 4294967296.0); // 1 = fraction of int max.

  } else {

    std::cout << "No valid timestamp found." << std::endl;
  }

  return time_ns;
}

#endif
