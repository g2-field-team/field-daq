#ifndef FIELD_CORE_INCLUDE_FIELD_STRUCTS_HH_
#define FIELD_CORE_INCLUDE_FIELD_STRUCTS_HH_

/*===========================================================================*\

author: Matthias W. Smith
email:  mwsmith2@uw.edu
file:   shim_structs.hh

about:  Contains the data structures for all field DAQ devices in one place.

\*===========================================================================*/

//--- other includes --------------------------------------------------------//
#include "TFile.h"

//--- project includes ------------------------------------------------------//

namespace g2 {

// Basic example data type.
struct point_t {
  ULong_t timestamp;
  Double_t x;
  Double_t y;
  Double_t z;
};

const char * const point_str_root = "timestamp/U:x/D:y/D:z/D";

const char * const point_str_midas[] = {
  "[.]",\
  "timestamp = DWORD : 0",\
  "x = DOUBLE : 0",\
  "y = DOUBLE : 0",\
  "z = DOUBLE : 0",\
  "",
  NULL };

} // ::g2

#endif
