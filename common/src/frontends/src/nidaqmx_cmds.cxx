#include <stdio.h>
#include <NIDAQmx.h>
#define DAQmxErrChk(functionCall) if( DAQmxFailed(error=(functionCall)) ) goto Error; else

const char taskName[] = "";
TaskHandle *taskHandle = 0;
int32 createTask = DAQmxCreateTask (taskName[], &taskHandle);

//create voltage channels
int32 numChannels = 12;
const char physicalChannel[] = "Dev1/ai0:11"; //creates AI channels 0-11
const char nameToAssignToChannel[] = "";
int32 terminalConfig = DAQmx_Val_RSE;
float64 minVal = -10.0;
float64 maxVal = 10.0;
int32 units = DAQmx_Val_Volts;
const char customScaleName[] = NULL;
int32 voltageChan = DAQmxCreateAIVoltageChan (taskHandle, physicalChannel[], nameToAssignToChannel[], terminalConfig, minVal, maxVal, units, customScaleName[]);

//create timing parameters
//continuous samples so me dont have to restart the task over and over
//8000 samples per second per channel
//480000 samples per channel = 8000 samples/sec * 60 sec
float64 rate = 8000;
int32 activeEdge = DAQmx_Val_Rising;
int32 sampleMode = DAQmx_Val_ContSamps;
uInt64 sampsPerChanToAcquire = 480000;
const char source[] = NULL;
int32 clkTiming = DAQmxCfgSampClkTiming(taskHandle, source[], rate, activeEdge, sampleMode, sampsPerChanToAcquire);

//start the acquisition
DAQmxStartTask(taskHandle)

//read out the buffer
int32 numSampsPerChan = -1; //reads all available samples
float64 timeout = 0; //tries to read samples once, otherwise returns a timeout error
bool32 filllMode = DAQmx_Val_GroupByChannel; //no interleaving
uInt32 arraySizeInSamps = sampsPerChanToAcquire*numChannels;
float64 readArray[arraySizeInSamps];
int32 sampsRead;
int32 readOutput = DAQmxReadAnalogF64(taskhandle,numSampsPerChan,fillmode,readArray[],arraySizeInSamps,&sampsRead,NULL);

Error:
	if( DAQmxFailed(error) )
		DAQmxGetExtendedErrorInfo(errBuff,2048);
	if( taskHandle!=0 )  {
		/*********************************************/
		// DAQmx Stop Code
		/*********************************************/
		DAQmxStopTask(taskHandle);
		DAQmxClearTask(taskHandle);
	}
	if( DAQmxFailed(error) )
		printf("DAQmx Error: %s\n",errBuff);
	printf("End of program, press Enter key to quit\n");
	getchar();
	return 0;
