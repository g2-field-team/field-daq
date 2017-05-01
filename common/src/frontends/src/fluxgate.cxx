/*****************************************************************************\
Name:   fluxgateFe.cxx
Author: Alec Tewsley-Booth
Email:  aetb@umich.edu
About:  Addresses NI PCIe daq card, reads multiple channels of data, performs simple manipulations on the data, saves a header and .mid file.        
\*****************************************************************************/
#include <unistd.h>
#include <stdio.h>
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

//--- globals ---------------------------------------------------------------//

#define FRONTEND_NAME "Fluxgate" // Prefer capitalize with spaces
const char * const bank_name = "FLUX"; // 4 letters, try to make sensible

// Any structs need to be defined. NEED TO ALTER FOR FLUXGATE
//
//
//

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
        60000,           // read every 60 seconds 
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
// NEED TO LEARN MORE ABOUT THESE
//
//ODB
//
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
	int32 error = 0; //DAQ tasks
	int errorreturn = 0; //error check/message returns
	TaskHandle taskHandle; //NI DAQ task identifier
	int32 read;
	char errBuff[2048]={'\0'};
	int32 numDCChannels = 12; //these should go into ODB
	int32 numACChannels = 12; //these should go into ODB
	int32 numChannels = 24; //these should go into ODB
	//--- Channel Parameters ----------------------------------------------------//
	//--- Timing Parameters -----------------------------------------------------//
	float64 rate = 8000; //8000 samples per second
	float64 aqTime = 60; //time to acquire in seconds
	uInt64 sampsPerChanToAcquire = static_cast<uInt64>(rate*aqTime);
	size_t dataSize = static_cast<size_t>(sampsPerChanToAcquire*numChannels);
	std::vector<float64> data(dataSize); //vector for writing DAQ buffer into
	//--- DC Coupled Channels ---------------------------------------------------//
	const char physicalChannelDC[] = "Dev1/ai0:11"; //creates DC AI channels 0-15
	float64 minVolDC = -10.0;
	float64 maxVolDC = 10.0;
	//--- AC Coupled Channels -----------------------------------------------//
	const char physicalChannelAC[] = "Dev1/ai12:23"; //creates AC AI channels 16-31
	float64 minVolAC = -1.0;
	float64 maxVolAC = 1.0;
	//
	//
	//
} //end anonymous namespace
//
//
//

//--- NI Error Check/Message ------------------------------------------------//
INT ni_error_msg(INT error, const char *func, const char *successmsg){
	if( DAQmxFailed(error) ) {
			DAQmxBaseGetExtendedErrorInfo(errBuff,2048);
			cm_msg(MERROR, func, errBuff);
			return 1;
	}
	else {
			cm_msg(MINFO, func, successmsg);
			return 0;
	}
}

//--- Frontend Init ---------------------------------------------------------//
INT frontend_init() {
	error = DAQmxBaseCreateTask("",&taskHandle);
	errorreturn = ni_error_msg(error, "frontend_init", "NI DAQ task created");
	/*if( DAQmxFailed(error) ) {
		DAQmxGetExtendedErrorInfo(errBuff,2048);
		cm_msg(MERROR,"frontend_init",errBuff);
	}
	else {
		cm_msg(MINFO,"frontend_init","fluxgate DAQ task created");
	}*/
	return SUCCESS;
}

//--- Frontend Exit ---------------------------------------------------------//
INT frontend_exit(){
	error = DAQmxBaseClearTask(taskHandle);
	errorreturn = ni_error_msg(error, "frontend_exit", "NI DAQ task cleared");
	/*if( DAQmxFailed(error) ) {
		DAQmxGetExtendedErrorInfo(errBuff,2048);
		cm_msg(MERROR,"frontend_init",errBuff);
	} else {
		cm_msg(MINFO,"frontend_exit","fluxgate DAQ task cleared");
	}*/
	return SUCCESS;
}

//--- Begin of Run ----------------------------------------------------------//
INT begin_of_run(INT run_number, char *err){

	int rc = load_settings(frontend_name, conf);
	if (rc != 0) {
		cm_msg(MERROR, "begin_of_run", "could not load equipment settings");
		return rc;
	}

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
	error = DAQmxBaseCreateAIVoltageChan(taskHandle,physicalChannelDC,"Voltage",DAQmx_Val_Cfg_Default,minVolDC,maxVolDC,DAQmx_Val_Volts,NULL);
	errorreturn = ni_error_msg(error, "begin_of_run", "NI DAQ DC channels configured");
	
	//create AC channels
	error = DAQmxBaseCreateAIVoltageChan(taskHandle,physicalChannelAC,"Voltage",DAQmx_Val_Cfg_Default,minVolAC,maxVolAC,DAQmx_Val_Volts,NULL);
	errorreturn = ni_error_msg(error, "begin_of_run", "NI DAQ AC channels configured");
	
	//setup timing
	error = DAQmxBaseCfgSampClkTiming(taskHandle,"",rate,DAQmx_Val_Rising,DAQmx_Val_ContSamps,sampsPerChanToAcquire);
	errorreturn = ni_error_msg(error, "begin_of_run", "NI DAQ timing configured");
	
	//start task
	error = DAQmxBaseStartTask(taskHandle);
	errorreturn = ni_error_msg(error, "begin_of_run", "NI DAQ task started");
	
	return SUCCESS;
}

//--- End of Run ------------------------------------------------------------//
INT end_of_run(INT run_number, char *err){
	//stop task
	error = DAQmxBaseStopTask(taskHandle);
	errorreturn = ni_error_msg(error, "end_of_run", "NI DAQ task stopped");
	
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
	bk_create(pevent, bank_name, TID_BYTE, (void**)&pdata);

	// Grab a timestamp.
	//last_event = steadyclock_us();

	// And MIDAS output.
	bk_init32(pevent);
	// Copy the fluxgate data.
	memcpy(pfluxdata, &data, sizeof(data));
	pdata += sizeof(data) / sizeof(WORD);
	bk_close(pevent, pdata);
  
	return bk_size(pevent);
}

