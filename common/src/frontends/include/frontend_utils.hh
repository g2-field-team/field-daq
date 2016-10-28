#ifndef FRONTENDS_INCLUDE_FRONTEND_UTILS_HH_
#define FRONTENDS_INCLUDE_FRONTEND_UTILS_HH_

/*===========================================================================*\

author: Matthias W. Smith
email:  mwsmith2@uw.edu
file:   frontend_utils.hh

about:  Contains helper functions for MIDAS frontends.

\*===========================================================================*/

//--- std includes ----------------------------------------------------------//
#include <vector>
#include <array>
#include <mutex>
#include <cstdarg>
#include <iostream>
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

#endif
