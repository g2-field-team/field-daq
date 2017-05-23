/*****************************************************************************\
Name:   fluxgateFe.cxx
Author: Alec Tewsley-Booth
Email:  aetb@umich.edu
About:  Addresses NI PCIe daq card, reads multiple channels of data, performs simple manipulations on the data, saves a header and .mid file.        
\*****************************************************************************/
#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string>
#include <random>
#include <sys/time.h>
#include "midas.h"
#include <NIDAQmxBase.h> //NI drivers for daq card
#include "frontend_utils.hh"
#include "g2field/core/field_constants.hh"
#include "g2field/core/field_structs.hh"
using std::string;
using namespace std;

//---Run parameters that I don't want editable through the ODB --------------//
// acquisition parameters
#define AQ_RATE 2000 // acquisition rate in samples per second per channel
#define AQ_TIME 10 // acquisiton time in seconds
#define AQ_TIMEMS 10000 // acquisition time in msec
#define AQ_SAMPSPERCHAN 120000 // number of samples to aquire per channel, equals AQ_RATE x AQ_TIME
#define AQ_TOTALCHAN 24 // total channels, including AC and DC channels
#define AQ_TOTALSAMPS 2880000 // total number of samples, equals AQ_SAMPSPERCHAN x AQ_TOTALCHAN
// filter parameters
#define FILT_ROLLOFF 1000 // digital lowpass rolloff in Hz
#define FILT_BINSIZE 1 // bin size for binning and averaging routine, note rate/binsize should be >= 2x filter rolloff

//--- globals ---------------------------------------------------------------//

#define FRONTEND_NAME "Fluxgate" // Prefer capitalize with spaces
const char * const bank_name = "FLUX"; // 4 letters, try to make sensible

// Any structs need to be defined.

extern "C" {
	// The frontend name (client name) as seen by other MIDAS clients
	char *frontend_name = (char*)FRONTEND_NAME;
	// The frontend file name, don't change it.
	char *frontend_file_name = (char*)__FILE__;
	// frontend_loop is called periodically if this variable is TRUE
	BOOL frontend_call_loop = FALSE;
	// A frontend status page is displayed with this frequency in ms.
	INT display_period = 1000;
	// maximum event size produced by this frontend
	INT max_event_size = 0x8000; // 32 kB @EXAMPLE - adjust if neeeded
	// maximum event size for fragmented events (EQ_FRAGMENTED)
	INT max_event_size_frag = 0x800000; // DEPRECATED
	// buffer size to hold events
	INT event_buffer_size = 0x800000;
	// Function declarations
	INT frontend_init();
	INT frontend_exit();
	INT begin_of_run(INT run_number, char *err);
	INT end_of_run(INT run_number, char *err);
	INT pause_run(INT run_number, char *err);
	INT resume_run(INT run_number, char *err);
	INT frontend_loop();
	INT read_fluxgate_event(char *pevent, INT off);
	INT poll_event(INT source, INT count, BOOL test); //needed?
	INT interrupt_configure(INT cmd, INT source, PTYPE adr);
	INT ni_error_msg(INT error, const char *func, const char *successmsg); //checks for errors and displays msgs as required

	//Equipment list for fluxgate, changed from POLLED to PERIODIC

	EQUIPMENT equipment[] = 
    {
		{FRONTEND_NAME,   // equipment name 
		{10, 0,         // event ID, trigger mask (need to figure these out)
        "SYSTEM",      // event buffer (used to be SYSTEM)
        EQ_PERIODIC,     // equipment type 
        0,             // event source, not used 
        "MIDAS",       // format 
        TRUE,          // enabled 
        RO_RUNNING,    // read only when running 
        10000,           // read every 10 seconds 
        0,             // stop run after this event limit 
        0,             // number of sub events 
        1,             // don't log history 
        "", "", "",
		},
		read_fluxgate_event,      // readout routine 
		NULL,
		NULL,
		NULL, 
		},
		{""}
    };
} //end extern "C"

// Put necessary globals in an anonymous namespace here.
namespace {
	boost::property_tree::ptree conf;
	boost::property_tree::ptree global_conf;
	double point_min = 0.0;
	double point_max = 1.0;
	long long last_event;
	long long next_event;
	//
	//
	//THESE SHOULD BE SWAPPED TO ODB PARAMETERS WHERE POSSIBLE
	//REMOVE OUTDATED MACROES, SWITCH TO ODB --- MINIMIZE RECOMPILING
	//--- NI DAQ globals --------------------------------------------------------//
	int32 DAQerr = 0; //DAQ tasks
	int errorreturn = 0; //error check/message returns
	TaskHandle taskHandle; //NI DAQ task identifier
	int32 read;
	char errBuff[2048]={'\0'};
	int32 numDCChannels = 12; //these should go into ODB
	int32 numACChannels = 12; //these should go into ODB
	int32 numChannels = 24; //these should go into ODB
	//--- Channel Parameters ----------------------------------------------------//
	//--- Timing Parameters -----------------------------------------------------//
	float64 rate = AQ_RATE;
	float64 aqTime = AQ_TIME;
	uInt64 sampsPerChanToAcquire = AQ_SAMPSPERCHAN;
	float64 data[AQ_TOTALSAMPS]; //vector for writing DAQ buffer into, fixing runtime allocation for testing
	//--- DC Coupled Channels ---------------------------------------------------//
	const char *physicalChannelDC = "Dev1/ai0:11"; //creates DC AI channels 0-15
	float64 minVolDC = -10.0;
	float64 maxVolDC = 10.0;
	//--- AC Coupled Channels -----------------------------------------------//
	const char *physicalChannelAC = "Dev1/ai12:23"; //creates AC AI channels 16-31
	float64 minVolAC = -1.0;
	float64 maxVolAC = 1.0;
	//--- Readout Parameters ---------------------------------------//
	uInt32 arraySizeInSamps = sampsPerChanToAcquire*numChannels;
	int32 sampsRead = 0;
	//--- ODB Parameters --------------------------------------------//
	HNDLE hDB, hkey;
	//
	//
} //end anonymous namespace
//
//
//

//--- Frontend Init ---------------------------------------------------------//
INT frontend_init() {
	DAQerr = DAQmxBaseCreateTask("",&taskHandle);
	if( DAQmxFailed(DAQerr) ) {
		DAQmxBaseGetExtendedErrorInfo(errBuff,2048);
		cm_msg(MERROR,"frontend_init",errBuff);
	}
	else {
		cm_msg(MINFO,"frontend_init","fluxgate DAQ task created");
	}
	return SUCCESS;
}

//--- Frontend Exit ---------------------------------------------------------//
INT frontend_exit(){
	DAQerr = DAQmxBaseClearTask(taskHandle);
	if( DAQmxFailed(DAQerr) ) {
		DAQmxBaseGetExtendedErrorInfo(errBuff,2048);
		cm_msg(MERROR,"frontend_init",errBuff);
	} else {
		cm_msg(MINFO,"frontend_exit","fluxgate DAQ task cleared");
	}
	return SUCCESS;
}

//--- Begin of Run ----------------------------------------------------------//
INT begin_of_run(INT run_number, char *err){
/*
	int rc = load_settings(frontend_name, conf);
	if (rc != 0) {
		cm_msg(MERROR, "begin_of_run", "could not load equipment settings");
		return rc;
	}
*/
	// ODB parameters
  	HNDLE hDB, hkey;
  	char str[256];
  	int dbsize;
  	BOOL flag;
	
	//get run parameters from ODB
	cm_get_experiment_database(&hDB, NULL);
	//
	//
	//physical channels are strings of the form "Dev1/ai0:15", THESE MIGHT BE IMPOSSIBLE TO ODB FOR REASONS
	//dbsize = sizeof(physicalChannelDC);
	//db_get_value(hDB,0,"/Equipment/Fluxgate/Settings/Physical Channels DC",&physicalChannelDC,&dbsize,TID_STRING, 0);
	//dbsize = sizeof(physicalChannelAC);
	//db_get_value(hDB,0,"/Equipment/Fluxgate/Settings/Physical Channels AC",&physicalChannelAC,&dbsize,TID_STRING, 0);
	//
	//
	//DC voltage parameters (nominally -10 to 10 V)
	dbsize = sizeof(minVolDC);
	db_get_value(hDB,0,"/Equipment/Fluxgate/Settings/Minimum Voltage DC",&minVolDC,&dbsize,TID_FLOAT, 0);
	dbsize = sizeof(maxVolDC);
	db_get_value(hDB,0,"/Equipment/Fluxgate/Settings/Maximum Voltage DC",&maxVolDC,&dbsize,TID_FLOAT, 0);
	//AC voltage parameters (depends on needed gain)
	dbsize = sizeof(minVolAC);
	db_get_value(hDB,0,"/Equipment/Fluxgate/Settings/Minimum Voltage AC",&minVolAC,&dbsize,TID_FLOAT, 0);
	dbsize = sizeof(maxVolAC);
	db_get_value(hDB,0,"/Equipment/Fluxgate/Settings/Maximum Voltage AC",&maxVolAC,&dbsize,TID_FLOAT, 0);
	//timing parameters
	dbsize = sizeof(rate);
	db_get_value(hDB,0,"/Equipment/Fluxgate/Settings/Sample Rate",&rate,&dbsize,TID_FLOAT, 0);
	dbsize = sizeof(aqTime);
	db_get_value(hDB,0,"/Equipment/Fluxgate/Settings/Acquisition Time",&aqTime,&dbsize,TID_FLOAT, 0);
	sampsPerChanToAcquire = static_cast<uInt64>(rate*aqTime);
	  
	//setup channel and timing parameters
	//create DC channels
	DAQerr = DAQmxBaseCreateAIVoltageChan(taskHandle,physicalChannelDC,"Voltage",DAQmx_Val_Cfg_Default,-10,10,DAQmx_Val_Volts,NULL);
	if( DAQmxFailed(DAQerr) ) {
		DAQmxBaseGetExtendedErrorInfo(errBuff,2048);
		cm_msg(MERROR,"begin_of_run","error creating fluxgate DC channels");
		cm_msg(MERROR,"begin_of_run",errBuff);
	}
	else {
		cm_msg(MINFO,"begin_of_run","fluxgate DC channels created");
	}
	
	//create AC channels
	DAQerr = DAQmxBaseCreateAIVoltageChan(taskHandle,physicalChannelAC,"Voltage",DAQmx_Val_Cfg_Default,minVolAC,maxVolAC,DAQmx_Val_Volts,NULL);
	if( DAQmxFailed(DAQerr) ) {
		DAQmxBaseGetExtendedErrorInfo(errBuff,2048);
		cm_msg(MERROR,"begin_of_run","error creating fluxgate AC channels");
		cm_msg(MERROR,"begin_of_run",errBuff);
	}
	else {
		cm_msg(MINFO,"begin_of_run","fluxgate AC channels created");
	}
	
	//setup timing
	DAQerr = DAQmxBaseCfgSampClkTiming(taskHandle,"",rate,DAQmx_Val_Rising,DAQmx_Val_FiniteSamps,sampsPerChanToAcquire);
	if( DAQmxFailed(DAQerr) ) {
		DAQmxBaseGetExtendedErrorInfo(errBuff,2048);
		cm_msg(MERROR,"begin_of_run","error configuring fluxgate sample clock");
		cm_msg(MERROR,"begin_of_run",errBuff);
	}
	else {
		cm_msg(MINFO,"begin_of_run","fluxgate sample clock configured");
	}
	
	//start task
	DAQerr = DAQmxBaseStartTask(taskHandle);
	if( DAQmxFailed(DAQerr) ) {
		DAQmxBaseGetExtendedErrorInfo(errBuff,2048);

		cm_msg(MERROR,"begin_of_run","error starting fluxgate task");
		cm_msg(MERROR,"begin_of_run",errBuff);
	}
	else {
		cm_msg(MINFO,"begin_of_run","fluxgate task started");
	}
	
	return SUCCESS;
}

//--- End of Run ------------------------------------------------------------//
INT end_of_run(INT run_number, char *err){
	//stop task
	DAQerr = DAQmxBaseStopTask(taskHandle);
	if( DAQmxFailed(DAQerr) ) {
		DAQmxBaseGetExtendedErrorInfo(errBuff,2048);

		cm_msg(MERROR,"end_of_run","error stopping fluxgate task");
		cm_msg(MERROR,"end_of_run",errBuff);
	}
	else {
		cm_msg(MINFO,"end_of_run","fluxgate task stopped");
	}
	
	return SUCCESS;
}

//--- Pause Run -------------------------------------------------------------//
INT pause_run(INT run_number, char *err){
	return SUCCESS;
}

//--- Resume Run ------------------------------------------------------------//
INT resume_run(INT run_number, char *err){
	return SUCCESS;
}

//--- Frontend Loop ---------------------------------------------------------//
INT frontend_loop(){
	// If frontend_call_loop is true, this routine gets called when
	// the frontend is idle or once between every event
	return SUCCESS;
}

/****************************************************************************\
	Readout routines for different events
\*****************************************************************************/
INT poll_event(INT source, INT count, BOOL test) { //do I even need this, since it is periodic?
	static unsigned int i;

	// fake calibration
	if (test) {
		for (i = 0; i < count; i++) {
			usleep(10);
		}

		return 0;
	}
}

//--- Interrupt configuration ---------------------------------------*/
INT interrupt_configure(INT cmd, INT source, PTYPE adr){
	switch (cmd) {
	case CMD_INTERRUPT_ENABLE:
		break;
	case CMD_INTERRUPT_DISABLE:
		break;
	case CMD_INTERRUPT_ATTACH:
		break;
	case CMD_INTERRUPT_DETACH:
		break;
	}
	return SUCCESS;
}

//--- Event readout -------------------------------------------------*/
INT read_fluxgate_event(char *pevent, INT off){
	BYTE *pdata;
	WORD *pfluxdata;
	// Let MIDAS allocate the struct.


	//setup nidaq acquisition
	//DAQerr = DAQmxBaseReadAnalogF64(taskHandle,numSampsPerChan,timeout,fillMode,data,arraySizeInSamps,&sampsRead,NULL);
	DAQerr = DAQmxBaseReadAnalogF64(taskHandle,AQ_SAMPSPERCHAN,AQ_TIME + 5,DAQmx_Val_GroupByChannel,data,AQ_TOTALSAMPS,&sampsRead,NULL);
	if( DAQmxFailed(DAQerr) ) {
		DAQmxBaseGetExtendedErrorInfo(errBuff,2048);
		cm_msg(MERROR,"read_fluxgate_event","error reading fluxgate event");
		cm_msg(MERROR,"read_fluxgate_event",errBuff);
	} else {
		cm_msg(MINFO,"read_fluxgate_event","fluxgate DAQ event read");
	}
	//restart DAQ task
	DAQerr = DAQmxBaseStopTask(taskHandle);
	if( DAQmxFailed(DAQerr) ) {
		DAQmxBaseGetExtendedErrorInfo(errBuff,2048);

		cm_msg(MERROR,"read_fluxgate_event","error restarting (stop) fluxgate task");
		cm_msg(MERROR,"read_fluxgate_event",errBuff);
	}
	DAQerr = DAQmxBaseStartTask(taskHandle);
	if( DAQmxFailed(DAQerr) ) {
		DAQmxBaseGetExtendedErrorInfo(errBuff,2048);

		cm_msg(MERROR,"read_fluxgate_event","error restarting (start) fluxgate task");
		cm_msg(MERROR,"read_fluxgate_event",errBuff);
	}


	// And MIDAS output.
/*
	bk_init32(pevent);
	bk_create(pevent, bank_name, TID_BYTE, (void**)&pdata);
	// Copy the fluxgate data.
	// THIS IS VERY SIMPLE AND ONLY STORES THE READ DATA. NEEDS TO FILL DATA STRUCT INSTEAD
	//
	//memcpy(pfluxdata, &data, sizeof(data));
	//
	//
	pdata += sizeof(data) / sizeof(WORD);
	bk_close(pevent, pdata);
*/ 
	return 0;//bk_size(pevent);

}

