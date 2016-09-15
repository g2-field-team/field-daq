#ifndef MIDAS_FRONTENDS_INCLUDE_FRONTEND_UTILS_HH_
#define MIDAS_FRONTENDS_INCLUDE_FRONTEND_UTILS_HH_

/*===========================================================================*\

author: Matthias W. Smith
email:  mwsmith2@uw.edu
file:   frontend_utils.hh

about:  Contains helper functions for MIDAS frontends.

\*===========================================================================*/

//--- std includes ----------------------------------------------------------//
#include <cstdarg>
#include <iostream>
#include <sys/time.h>

//--- other includes --------------------------------------------------------//
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/filesystem.hpp>
#include "midas.h"

// Grab an odb key as JSON data.
inline int get_odb_key_json(char *keyname, boost::property_tree::ptree& conf)
{
  // Data part
  HNDLE hDB, hkey;
  int size = 0;
  int bytes_written = 0;
  int rc = 0;

  char *json_buf = new char[0x8000]; // This should be large enough
  boost::property_tree::ptree pt;
  std::stringstream ss;

  // Get the experiment database handle.
  cm_get_experiment_database(&hDB, NULL);

  db_find_key(hDB, 0, keyname, &hkey);

  if (hkey) {
    db_copy_json_values(hDB, hkey, &json_buf, &size, &bytes_written, 1, 1, 1);

  } else {

    cm_msg(MERROR, "failed to load \"%s\" from ODB", keyname);
    return FE_ERR_ODB;
  }

  // Store it in a property tree.
  ss << json_buf;
  boost::property_tree::read_json(ss, conf);

  return SUCCESS;
}

// Load the equipment settings for the frontend.
inline int load_settings(char *frontend, boost::property_tree::ptree& conf)
{
  // Set the keyname.
  char key[256];
  snprintf(key, 256, "/Equipment/%s/Settings", frontend);

  // Grab the odb data as json.
  int rc = get_odb_key_json(key, conf);
  
  // Return status.
  return rc;
}

// Load the experiment's global settings.
inline int load_global_settings(boost::property_tree::ptree& conf)
{
  // This is simple, since it won't change.
  int rc = get_odb_key_json((char *)"/Settings", conf);

  return rc;
}

inline long long systime_us() {
 static timeval t;
 gettimeofday(&t, nullptr);
 return (long long)(t.tv_sec)*1000000 + (long long)t.tv_usec;
}

inline long long steadyclock_us() {
 static timespec t;

 clock_gettime(CLOCK_MONOTONIC, &t);
 return (long long)(t.tv_sec)*1000000 + (long long)(t.tv_nsec * 0.001);
}

#endif
