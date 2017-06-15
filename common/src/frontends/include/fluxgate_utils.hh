#ifndef FRONTENDS_INCLUDE_FLUXGATE_UTILS_HH_
#define FRONTENDS_INCLUDE_FLUXGATE_UTILS_HH_

/*=================================================================================*\

author: Alec Tewsley-Booth
email:  aetb@umich.edu
file:   fluxgate_utils.hh
about:  Contains parameters, structures, and helper functions for fluxgate frontend.

\*=================================================================================*/

//--- std includes ----------------------------------------------------------//
//--- other includes --------------------------------------------------------//
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
	
	constexpr float64 kFluxRate = AQ_RATE;
	constexpr float64 kFluxEffRate = AQ_RATE/FILT_BINSIZE;
	constexpr float64 kFluxTime = AQ_TIME;
	constexpr int32 kFluxNumChannels = AQ_TOTALCHAN;
	constexpr int32 kFluxBinSize = FILT_BINSIZE;
	constexpr float64 kFluxDigitalRolloff = FILT_ROLLOFF;
	constexpr uInt64 kFluxSampsPerChanToAcquire = AQ_SAMPSPERCHAN;
	constexpr uInt32 kFluxArraySizeInSamps = AQ_TOTALSAMPS;
	
} // ::g2field








#endif
