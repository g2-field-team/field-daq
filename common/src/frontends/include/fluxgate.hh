#ifndef FRONTENDS_INCLUDE_FLUXGATE_HH_
#define FRONTENDS_INCLUDE_FLUXGATE_HH_

/*=================================================================================*\

author: Alec Tewsley-Booth
email:  aetb@umich.edu
file:   fluxgate.hh
about:  Contains parameters, structures, and helper functions for fluxgate frontend.

\*=================================================================================*/

//--- std includes ----------------------------------------------------------//
//--- other includes --------------------------------------------------------//
#include <string>
#include <ctime>

#include <NIDAQmxBase.h>
#include "TROOT.h"
#include "g2field/core/field_constants.hh"
#include "g2field/core/field_structs.hh"

/*======================"Hidden" parameters --- timing/array sizes, mostly ===================================*/
/*----------------------------------- acquisition parameters -------------------------------------------------*/
//SET parameters
#define AQ_RATE 2000 // acquisition rate in samples per second per channel (MAKE AN INT!!!)
#define AQ_TIME 10 // acquisiton time in seconds (MAKE AN INT!!!)
#define AQ_TOTALCHAN 32 // total channels, including AC and DC channels
//DERIVED paramters
#define AQ_TIMEMS AQ_TIME*1000 // acquisition time in msec
#define AQ_SAMPSPERCHAN AQ_RATE*AQ_TIME // number of samples to aquire per channel, equals AQ_RATE x AQ_TIME
#define AQ_TOTALSAMPS AQ_SAMPSPERCHAN*AQ_TOTALCHAN // total number of samples, equals AQ_SAMPSPERCHAN x AQ_TOTALCHAN
/*------------------------------------------------------------------------------------------------------------*/
/*----------------------------------- filter parameters ------------------------------------------------------*/
#define FILT_ROLLOFF 1000 // digital lowpass rolloff in Hz
#define FILT_BINSIZE 1 // bin size for binning and averaging routine, note rate/binsize should be >= 2x filter rolloff
/*------------------------------------------------------------------------------------------------------------*/
/*============================================================================================================*/

namespace g2field {
//fluxgate struct
  struct fluxgate_t{
    Int_t sys_time;
    Int_t gps_time;
    Float_t fg_r[8];
    Float_t fg_theta[8];
    Float_t fg_z[8];
    Double_t eff_rate;
    Double_t data[AQ_TOTALSAMPS];
  };
  #define MAKE_FG_STRING(name) FG_HELPER(name)
  #define FG_HELPER(name) const char * const name = "sys_clock/D:gps_clock/D:fg_r[8]/F:fg_theta[8]/F:fg_z[8]/F:data[AQ_TOTALSAMPS]/D"
  MAKE_FG_STRING(fg_str);
  
  constexpr Double_t kFluxRate = AQ_RATE;
  constexpr Double_t kFluxEffRate = AQ_RATE/FILT_BINSIZE;
  constexpr Double_t kFluxTime = AQ_TIME;
  constexpr Int_t kFluxNumChannels = AQ_TOTALCHAN;
  constexpr Int_t kFluxBinSize = FILT_BINSIZE;
  constexpr Double_t kFluxDigitalRolloff = FILT_ROLLOFF;
  constexpr Int_t kFluxSampsPerChanToAcquire = AQ_SAMPSPERCHAN;
  constexpr Int_t kFluxArraySizeInSamps = AQ_TOTALSAMPS;
  
} // ::g2field
//helper functions
// ---------- debug file name function
inline std::string debugFileName(){
  time_t rawTime;
  time(&rawTime);
  struct tm * timeinfo;
  timeinfo = localtime(&rawTime);
  char buffer[80];
  strftime(buffer,sizeof(buffer),"/home/newg2/aetb/flux_%d-%m-%Y_%H-%M-%S.csv",timeinfo);
  std::string fileName(buffer);
  return fileName;
}
//----- lowpass filter (antialiasing before bin + average)
void lpFilt(Double_t *input){
  }

//----- bin and average (equivalent to moving average + decimate)
void binAvg(Double_t *input, Double_t *output){
  int inputSize = AQ_TOTALSAMPS;
  int binSize = FILT_BINSIZE;
  int outputSize = AQ_TOTALSAMPS / FILT_BINSIZE;
  for(int ii = 0; ii < outputSize; ii++){
    for(int jj = 0; jj < binSize; jj++){
      output[ii] += input[ii*binSize + jj];
    }
    output[ii] = output[ii]/binSize;
  }
}


#endif
