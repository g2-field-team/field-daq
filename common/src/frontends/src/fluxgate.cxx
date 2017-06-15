/*****************************************************************************\
Name:   fluxgateFe.cxx
Author: Alec Tewsley-Booth
Email:  aetb@umich.edu
About:  Addresses NI PCIe daq card, reads multiple channels of data, performs simple manipulations on the data, saves a .mid file.        
\*****************************************************************************/
#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <string>
#include <sys/time.h>
#include <ctime>
#include "midas.h"
#include <NIDAQmxBase.h> //NI drivers for daq card
#include "frontend_utils.hh"
#include "fluxgate_utils.hh"
#include "g2field/core/field_constants.hh"
#include "g2field/core/field_structs.hh"
using std::string;

// Run parameters that I don't want editable through the ODB are in frontends/include/fluxgate_utils.hh


//====================== BEGIN GLOBALS =========================================================//
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
	EQUIPMENT equipment[] = {
		{FRONTEND_NAME,   // equipment name 
			{10, 0,         // event ID, trigger mask (need to figure these out)
        		"SYSTEM",      // event buffer (used to be SYSTEM)
        		EQ_PERIODIC,     // equipment type 
        		0,             // event source, not used 
        		"MIDAS",       // format 
        		TRUE,          // enabled 
        		RO_RUNNING,    // read only when running 
        		AQ_TIMEMS,           // read every 10 seconds 
       			0,             // stop run after this event limit 
        		0,             // number of sub events 
        		1,             // don't log history 
        		"", "", "",},
		read_fluxgate_event,      // readout routine 
		NULL,
		NULL,
		NULL,},
		{""}
	}; //end equipment
} //end extern "C"
// Put necessary globals in an anonymous namespace here.
namespace {
	//--- NI DAQ globals --------------------------------------------------------//
	int32 DAQerr; //DAQ tasks
	TaskHandle taskHandle; //NI DAQ task identifier
	char errBuff[2048]={'\0'};
	//--- Channel Parameters ----------------------------------------------------//
	int32 numChannels = AQ_TOTALCHAN; //defined in fluxgate_utils.hh
	const char *physicalChannelDC = "Dev1/ai0:31"; //creates DC AI channels 16-27
	float64 minVolDC = -10.0;
	float64 maxVolDC = 10.0;
	const char *physicalChannelAC = "Dev1/ai12:15"; //creates AC AI channels 28-31
	float64 minVolAC = -1.0;
	float64 maxVolAC = 1.0;
	//--- Timing Parameters -----------------------------------------------------//
	float64 aqRate = AQ_RATE;
	float64 aqTime = AQ_TIME;
	uInt64 sampsPerChanToAcquire = AQ_SAMPSPERCHAN;
	//--- Readout Parameters ---------------------------------------//
	uInt32 arraySizeInSamps = AQ_TOTALSAMPS;
	float64 readOut[AQ_TOTALSAMPS]; //vector for writing DAQ buffer into, fixing runtime allocation for testing
	int32 sampsRead = 0;
	g2field::fluxgate_t data; //fluxgate data type defined in fluxgate_utils.hh
	//--- Filter Parameters -----------------------------------------//
	float64 effRate = AQ_RATE/FILT_BINSIZE;
	uInt32 sampsPerChanEff = AQ_SAMPSPERCHAN/FILT_BINSIZE;
	uInt32 effArraySize = AQ_TOTALSAMPS/FILT_BINSIZE;
	//--- ODB Parameters --------------------------------------------//
	HNDLE hDB, hkey;
} //end anonymous namespace
//====================== END GLOBALS ===========================================================//

//====================== MACROS ================================================================//
#define NIERRORCHECK_SUCCESSMSG(func,errormsg,successmsg) \
	if( DAQmxFailed(DAQerr) ) {\
		DAQmxBaseGetExtendedErrorInfo(errBuff,2048);\
		cm_msg(MERROR,func,errormsg);\
		cm_msg(MERROR,func,errBuff);}\
	else {cm_msg(MINFO,func,successmsg);}
#define NIERRORCHECK_NOSUCCESSMSG(func,errormsg) \
	if( DAQmxFailed(DAQerr) ) {\
		DAQmxBaseGetExtendedErrorInfo(errBuff,2048);\
		cm_msg(MERROR,func,errormsg);\
		cm_msg(MERROR,func,errBuff);}

//--- Frontend Init ---------------------------------------------------------//
INT frontend_init() {
	DAQerr = DAQmxBaseCreateTask("",&taskHandle);
	NIERRORCHECK_SUCCESSMSG("frontend_init","error creating fluxgate task","fluxgate task created");
	return SUCCESS;
}

//--- Frontend Exit ---------------------------------------------------------//
INT frontend_exit(){
	DAQerr = DAQmxBaseClearTask(taskHandle);
	NIERRORCHECK_SUCCESSMSG("frontend_exit","error clearing fluxgate task","fluxgate task cleared");
/*
	if( DAQmxFailed(DAQerr) ) {
		DAQmxBaseGetExtendedErrorInfo(errBuff,2048);
		cm_msg(MERROR,"frontend_init",errBuff);
	} else {
		cm_msg(MINFO,"frontend_exit","fluxgate DAQ task cleared");
	}
*/
	return SUCCESS;
}

//--- Begin of Run ----------------------------------------------------------//
INT begin_of_run(INT run_number, char *err){

	// ODB parameters
  	HNDLE hDB, hkey;
  	char str[256];
  	int dbsize;
  	BOOL flag;
	
	//get run parameters from ODB
	cm_get_experiment_database(&hDB, NULL);
		  
	//setup channel and timing parameters
	//create DC channels
	//db_get_value(hDB,0,"/Equipment/Fluxgate/Settings/minVolDC",&minVolDC,sizeof(minVolDC),TID_FLOAT, 0);
	//db_get_value(hDB,0,"/Equipment/Fluxgate/Settings/maxVolDC",&maxVolDC,sizeof(maxVolDC),TID_FLOAT, 0);
	db_set_value(hDB,0,"/Equipment/Fluxgate/Settings/physicalChannelDC",&physicalChannelDC,sizeof(physicalChannelDC),1,TID_STRING);
	DAQerr = DAQmxBaseCreateAIVoltageChan(taskHandle, physicalChannelDC, "Voltage", DAQmx_Val_Cfg_Default, minVolDC, maxVolDC, DAQmx_Val_Volts, NULL);
	NIERRORCHECK_SUCCESSMSG("begin_of_run","error creating fluxgate DC channels","fluxgate DC channels created");
/*
	if( DAQmxFailed(DAQerr) ) {
		DAQmxBaseGetExtendedErrorInfo(errBuff,2048);
		cm_msg(MERROR,"begin_of_run","error creating fluxgate DC channels");
		cm_msg(MERROR,"begin_of_run",errBuff);
	}
	else {
		cm_msg(MINFO,"begin_of_run","fluxgate DC channels created");
	}
*/	
	//create AC channels
	//db_get_value(hDB,0,"/Equipment/Fluxgate/Settings/minVolAC",&minVolAC,sizeof(minVolAC),TID_FLOAT, 0);
	//db_get_value(hDB,0,"/Equipment/Fluxgate/Settings/maxVolAC",&maxVolAC,sizeof(maxVolAC),TID_FLOAT, 0);
	db_set_value(hDB,0,"/Equipment/Fluxgate/Settings/physicalChannelAC",&physicalChannelAC,sizeof(physicalChannelAC),1,TID_STRING);
//	DAQerr = DAQmxBaseCreateAIVoltageChan(taskHandle, physicalChannelAC, "Voltage", DAQmx_Val_Cfg_Default, minVolAC, maxVolAC, DAQmx_Val_Volts, NULL);
//	NIERRORCHECK_SUCCESSMSG("begin_of_run","error creating fluxgate AC channels","fluxgate AC channels created");
/*
	if( DAQmxFailed(DAQerr) ) {
		DAQmxBaseGetExtendedErrorInfo(errBuff,2048);
		cm_msg(MERROR,"begin_of_run","error creating fluxgate AC channels");
		cm_msg(MERROR,"begin_of_run",errBuff);
	}
	else {
		cm_msg(MINFO,"begin_of_run","fluxgate AC channels created");
	}
*/	
	//setup timing

	db_set_value(hDB,0,"/Equipment/Fluxgate/Settings/aqRate",&aqRate,sizeof(aqRate),1,TID_FLOAT);
	db_set_value(hDB,0,"/Equipment/Fluxgate/Settings/aqTime",&aqTime,sizeof(aqTime),1,TID_FLOAT);
	db_set_value(hDB, 0, "/Equipment/Fluxgate/Settings/sampsPerChanToAcquire", &sampsPerChanToAcquire, sizeof(sampsPerChanToAcquire), 1, TID_INT);
	DAQerr = DAQmxBaseCfgSampClkTiming(taskHandle, "", aqRate, DAQmx_Val_Rising, DAQmx_Val_FiniteSamps, sampsPerChanToAcquire);
	NIERRORCHECK_SUCCESSMSG("begin_of_run","error configuring fluxgate sample clock","fluxgate sample clock configured");
/*
	if( DAQmxFailed(DAQerr) ) {
		DAQmxBaseGetExtendedErrorInfo(errBuff,2048);
		cm_msg(MERROR,"begin_of_run","error configuring fluxgate sample clock");
		cm_msg(MERROR,"begin_of_run",errBuff);
	}
	else {
		cm_msg(MINFO,"begin_of_run","fluxgate sample clock configured");
	}
*/
	//start task
	DAQerr = DAQmxBaseStartTask(taskHandle);
	NIERRORCHECK_NOSUCCESSMSG("begin_of_run","error starting fluxgate task");
/*
	if( DAQmxFailed(DAQerr) ) {
		DAQmxBaseGetExtendedErrorInfo(errBuff,2048);

		cm_msg(MERROR,"begin_of_run","error starting fluxgate task");
		cm_msg(MERROR,"begin_of_run",errBuff);
	}
	else {
		cm_msg(MINFO,"begin_of_run","fluxgate task started");
	}
*/
	return SUCCESS;
}

//--- End of Run ------------------------------------------------------------//
INT end_of_run(INT run_number, char *err){
	//stop task
	DAQerr = DAQmxBaseStopTask(taskHandle);
	NIERRORCHECK_NOSUCCESSMSG("end_of_run","error stopping fluxgate task");
/*
	if( DAQmxFailed(DAQerr) ) {
		DAQmxBaseGetExtendedErrorInfo(errBuff,2048);

		cm_msg(MERROR,"end_of_run","error stopping fluxgate task");
		cm_msg(MERROR,"end_of_run",errBuff);
	}
	else {
		cm_msg(MINFO,"end_of_run","fluxgate task stopped");
	}
*/
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

	DWORD *pdata;

	//setup nidaq acquisition
	//DAQerr = DAQmxBaseReadAnalogF64(taskHandle,numSampsPerChan,timeout,fillMode,data,arraySizeInSamps,&sampsRead,NULL);
	DAQerr = DAQmxBaseReadAnalogF64(taskHandle, sampsPerChanToAcquire, aqTime + 5, DAQmx_Val_GroupByChannel, readOut, arraySizeInSamps, &sampsRead, NULL);	
	NIERRORCHECK_NOSUCCESSMSG("read_fluxgate_event","error reading fluxgate event");

	//restart DAQ task
	DAQerr = DAQmxBaseStopTask(taskHandle);
	NIERRORCHECK_NOSUCCESSMSG("read_fluxgate_event","error restarting (stop) fluxgate task");
	DAQerr = DAQmxBaseStartTask(taskHandle);
	NIERRORCHECK_NOSUCCESSMSG("read_fluxgate_event","error restarting (start) fluxgate task");

	// MIDAS output.

	// fill fluxgate data structure
	data.sys_time = 0;
	data.gps_time = 0; //CHANGE THIS TO GPS TIME
	for(int ifg = 0; ifg < 8; ++ifg){ //fluxgate positions, needs to be read from odb
		data.fg_r[ifg] = 0;
		data.fg_theta[ifg] = 0;
		data.fg_z[ifg] = 0;}
	data.eff_rate = effRate;

	//memcpy(data.data,&readOut, sizeof(readOut));

	for (int ii = 0; ii < arraySizeInSamps; ii++){
		data.data[ii] = readOut[ii];
	}

//------Copy the fluxgate data.

/*------Write to debug file
	std::fstream debugFile;	
	debugFile.open("test.aetb",std::fstream::out);
	write a csv style file
	for(int row = 0; row < sampsPerChanToAcquire; row++){
		for(int col = 0; col < 32; col++){
			debugFile << data.data[col*sampsPerChanToAcquire + row];
			if(col!=15){debugFile << ",";}
		}
		debugFile << std::endl;
	}
	debugFile.close();
*/
//	MIDAS output
	bk_init32(pevent);
	bk_create(pevent, bank_name, TID_WORD, (void**)&pdata);
//	memcpy(pdata, &data, sizeof(data));
	pdata += sizeof(data) / sizeof(WORD);
	bk_close(pevent, pdata);
	return bk_size(pevent);
//	return 0;

}
