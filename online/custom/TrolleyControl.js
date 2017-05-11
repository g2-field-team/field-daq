var updatePeriod = 500; // in msec
var updateTimerId = 0;

function update()  {
  clearTimeout(updateTimerId);
  loadGalil();
  loadTrolley();
  if (updatePeriod > 0)
    updateTimerId = setTimeout('update()', updatePeriod);
}

function loadGalil()   {
  mjsonrpc_db_get_values(["/Equipment/GalilFermi/Monitors/Positions","/Equipment/GalilFermi/Monitors/Velocities","/Equipment/GalilFermi/Monitors/Control Voltages","/Equipment/GalilFermi/Monitors/Analogs","/Equipment/GalilFermi/Monitors/Limit Switches Forward","/Equipment/GalilFermi/Monitors/Limit Switches Reverse","/Equipment/GalilFermi/Monitors/Motor Status"]).then(function(rpc) {
      var Ps= String(rpc.result.data[0]);
      Ps = Ps.split(',');
      document.getElementById("PA").innerHTML = Ps[0];
      document.getElementById("PB").innerHTML = Ps[1];
      document.getElementById("PC").innerHTML = Ps[2];
      var Vs= String(rpc.result.data[1]);
      Vs = Vs.split(',');
      document.getElementById("VA").innerHTML = Vs[0];
      document.getElementById("VB").innerHTML = Vs[1];
      document.getElementById("VC").innerHTML = Vs[2];
      var Cs= String(rpc.result.data[2]);
      Cs = Cs.split(',');
      document.getElementById("CA").innerHTML = Cs[0]/1000.0;
      document.getElementById("CB").innerHTML = Cs[1]/1000.0;
      document.getElementById("CC").innerHTML = Cs[2]/1000.0;
      var As= String(rpc.result.data[3]);
      As = As.split(',');
      document.getElementById("TA").innerHTML = As[0];
      document.getElementById("TB").innerHTML = As[1];
      document.getElementById("TempA").innerHTML = As[2];
      document.getElementById("TempB").innerHTML = As[3];
      var Lf= String(rpc.result.data[4]);
      Lf = Lf.split(',');
      document.getElementById("LFA").innerHTML = Lf[0];
      document.getElementById("LFB").innerHTML = Lf[1];
      document.getElementById("LFC").innerHTML = Lf[2];
      var Lr= String(rpc.result.data[5]);
      Lr = Lr.split(',');
      document.getElementById("LRA").innerHTML = Lr[0];
      document.getElementById("LRB").innerHTML = Lr[1];
      document.getElementById("LRC").innerHTML = Lr[2];
      var MO= String(rpc.result.data[6]);
      MO = MO.split(',');
      if (MO[0]=="false")document.getElementById("MOA").innerHTML = "OFF";
      else document.getElementById("MOA").innerHTML = "ON";
      if (MO[1]=="false")document.getElementById("MOB").innerHTML = "OFF";
      else document.getElementById("MOB").innerHTML = "ON";
      if (MO[2]=="false")document.getElementById("MOC").innerHTML = "OFF";
      else document.getElementById("MOC").innerHTML = "ON";
      }).catch(function(error) {
	mjsonrpc_error_alert(error);
	});
} 

function loadTrolley()   {
  mjsonrpc_db_get_values(["/Equipment/TrolleyInterface/Monitors/Interface/Trolley Power Protection Trip","/Equipment/TrolleyInterface/Monitors/Interface/Trolley Power Status","/Equipment/TrolleyInterface/Monitors/Interface/Interface RF0","/Equipment/TrolleyInterface/Monitors/Interface/Interface RF1","/Equipment/TrolleyInterface/Monitors/Interface/ldo Temp Monitors Min","/Equipment/TrolleyInterface/Monitors/Interface/ldo Temp Monitors Max"]).then(function(rpc) {
      var PowerTrip= String(rpc.result.data[0]);
      if (PowerTrip=="false")document.getElementById("InterfacePWTrip").innerHTML = "N";
      else document.getElementById("InterfacePWTrip").innerHTML = "Y";
      var PowerStatus= String(rpc.result.data[1]);
      if (PowerStatus=="false")document.getElementById("InterfacePWStatus").innerHTML = "OFF";
      else document.getElementById("InterfacePWStatus").innerHTML = "ON";
      var RF0= Number(rpc.result.data[2]);
      document.getElementById("InterfaceRF0").innerHTML = RF0;
      var RF1= Number(rpc.result.data[3]);
      document.getElementById("InterfaceRF1").innerHTML = RF1;
      var TempMin= Number(rpc.result.data[4]);
      document.getElementById("InterfaceTempMin").innerHTML = TempMin.toFixed(4);
      var TempMax= Number(rpc.result.data[5]);
      document.getElementById("InterfaceTempMax").innerHTML = TempMax.toFixed(4);
      }).catch(function(error) {
	mjsonrpc_error_alert(error);
	});
  mjsonrpc_db_get_values(["/Equipment/TrolleyInterface/Monitors/Interface/V15neg Min","/Equipment/TrolleyInterface/Monitors/Interface/V15neg Max","/Equipment/TrolleyInterface/Monitors/Interface/V15pos Min","/Equipment/TrolleyInterface/Monitors/Interface/V15pos Max","/Equipment/TrolleyInterface/Monitors/Interface/v5 Min","/Equipment/TrolleyInterface/Monitors/Interface/v5 Max","/Equipment/TrolleyInterface/Monitors/Interface/v33 Min","/Equipment/TrolleyInterface/Monitors/Interface/v33 Max","/Equipment/TrolleyInterface/Monitors/Interface/V Monitors","/Equipment/TrolleyInterface/Monitors/Interface/I Monitors"]).then(function(rpc) {
      var V15negMin= Number(rpc.result.data[0]);
      var V15negMax= Number(rpc.result.data[1]);
      var avg15neg = (V15negMin+V15negMax)/2.0;
      document.getElementById("Interface15VNeg").innerHTML = avg15neg.toFixed(4);

      var V15posMin= Number(rpc.result.data[2]);
      var V15posMax= Number(rpc.result.data[3]);
      var avg15pos = (V15posMin+V15posMax)/2.0;
      document.getElementById("Interface15VPos").innerHTML = avg15pos.toFixed(4);

      var V5Min= Number(rpc.result.data[4]);
      var V5Max= Number(rpc.result.data[5]);
      var avgV5 = (V5Min+V5Max)/2.0;
      document.getElementById("Interface5V").innerHTML = avgV5.toFixed(4);

      var V33Min= Number(rpc.result.data[6]);
      var V33Max= Number(rpc.result.data[7]);
      var avgV33 = (V33Min+V33Max)/2.0;
      document.getElementById("Interface33V").innerHTML = avgV33.toFixed(4);

      var TLV= Number(rpc.result.data[8]);
      document.getElementById("InterfaceTLV").innerHTML = TLV.toFixed(4);

      var TLI= Number(rpc.result.data[9]);
      document.getElementById("InterfaceTLI").innerHTML = TLI.toFixed(4);

  }).catch(function(error) {
    mjsonrpc_error_alert(error);
    });
  mjsonrpc_db_get_values(["/Equipment/TrolleyInterface/Monitors/Trolley/Barcode Error","/Equipment/TrolleyInterface/Monitors/Trolley/Temperature Interrupt","/Equipment/TrolleyInterface/Monitors/Trolley/Power Supply Status","/Equipment/TrolleyInterface/Monitors/Trolley/NMR Check Sum","/Equipment/TrolleyInterface/Monitors/Trolley/Config Check Sum","/Equipment/TrolleyInterface/Monitors/Trolley/Frame Check Sum"]).then(function(rpc) {
      var BarcodeError= String(rpc.result.data[0]);
      if (BarcodeError=="false")document.getElementById("TrolleyBCError").innerHTML = "N";
      else document.getElementById("TrolleyBCError").innerHTML = "Y";

      var TempError= String(rpc.result.data[1]);
      if (TempError=="false")document.getElementById("TrolleyTempError").innerHTML = "N";
      else document.getElementById("TrolleyTempError").innerHTML = "Y";

      var PowerStatus= String(rpc.result.data[2]);
      PowerStatus = PowerStatus.split(',');
      if (PowerStatus[0]=="true" && PowerStatus[1]=="true" && PowerStatus[2]=="true")document.getElementById("TrolleyPWStatus").innerHTML = "Y";
      else document.getElementById("TrolleyPWStatus").innerHTML = "N";

      var CheckSum1= String(rpc.result.data[3]);
      var CheckSum2= String(rpc.result.data[4]);
      var CheckSum3= String(rpc.result.data[5]);
      if (CheckSum1=="true" && CheckSum2=="true" && CheckSum3=="true")document.getElementById("TrolleyCheckSum").innerHTML = "Y";
      else document.getElementById("TrolleyCheckSum").innerHTML = "N";
      }).catch(function(error) {
	mjsonrpc_error_alert(error);
	});
  mjsonrpc_db_get_values(["/Equipment/TrolleyInterface/Monitors/Trolley/Power Factor","/Equipment/TrolleyInterface/Monitors/Trolley/Pressure","/Equipment/TrolleyInterface/Monitors/Trolley/Temperature In","/Equipment/TrolleyInterface/Monitors/Trolley/Temperature Ext1","/Equipment/TrolleyInterface/Monitors/Trolley/Vmin 1","/Equipment/TrolleyInterface/Monitors/Trolley/Vmax 1","/Equipment/TrolleyInterface/Monitors/Trolley/Vmin 2","/Equipment/TrolleyInterface/Monitors/Trolley/Vmax 2","/Equipment/TrolleyInterface/Monitors/Trolley/LED Voltage"]).then(function(rpc) {
      var PowerFactor= Number(rpc.result.data[0]);
      document.getElementById("TrolleyPWFactor").innerHTML = PowerFactor.toFixed(4);

      var Pressure= Number(rpc.result.data[1]);
      document.getElementById("TrolleyPressure").innerHTML = Pressure.toFixed(4);

      var TIn = Number(rpc.result.data[2]);
      document.getElementById("TrolleyTempIn").innerHTML = TIn.toFixed(4);

      var TExt = Number(rpc.result.data[3]);
      document.getElementById("TrolleyTempExt").innerHTML = TExt.toFixed(4);

      var V6Min= Number(rpc.result.data[4]);
      var V6Max= Number(rpc.result.data[5]);
      var avgV6 = (V6Min+V6Max)/2.0;
      document.getElementById("TrolleyV1").innerHTML = avgV6.toFixed(4);

      var V4Min= Number(rpc.result.data[6]);
      var V4Max= Number(rpc.result.data[7]);
      var avgV4 = (V4Min+V4Max)/2.0;
      document.getElementById("TrolleyV2").innerHTML = avgV4.toFixed(4);

      var VLED= Number(rpc.result.data[8]);
      document.getElementById("TrolleyLEDV").innerHTML = VLED.toFixed(4);
  }).catch(function(error) {
    mjsonrpc_error_alert(error);
    });
  mjsonrpc_db_get_values(["/Equipment/TrolleyInterface/Monitors/Control Thread Active","/Equipment/TrolleyInterface/Monitors/Read Thread Active","/Equipment/TrolleyInterface/Monitors/Trolley Frame Size","/Equipment/TrolleyInterface/Monitors/Interface Frame Size","/Equipment/TrolleyInterface/Monitors/Buffer Load","/Equipment/TrolleyInterface/Monitors/Current Mode"]).then(function(rpc) {
      var ControlThread= String(rpc.result.data[0]);
      if (ControlThread=="false")document.getElementById("FrontendCtrlTh").innerHTML = "N";
      else document.getElementById("FrontendCtrlTh").innerHTML = "Y";

      var ReadThread= String(rpc.result.data[1]);
      if (ReadThread=="false")document.getElementById("FrontendMonTh").innerHTML = "N";
      else document.getElementById("FrontendMonTh").innerHTML = "Y";

      var FrameASize= String(rpc.result.data[2]);
      document.getElementById("FrontendASize").innerHTML = FrameASize;

      var FrameBSize= String(rpc.result.data[3]);
      document.getElementById("FrontendBSize").innerHTML = FrameBSize;

      var BufferLoad= String(rpc.result.data[4]);
      document.getElementById("FrontendBufferLoad").innerHTML = BufferLoad;

      var CurrentMode= String(rpc.result.data[5]);
      document.getElementById("FrontendCurrentMode").innerHTML = CurrentMode;

      }).catch(function(error) {
	mjsonrpc_error_alert(error);
	});
  mjsonrpc_db_get_values(["/Equipment/TrolleyInterface/Settings/Trolley Power/Voltage","/Equipment/TrolleyInterface/Monitors/Barcode/LED Voltage","/Equipment/TrolleyInterface/Settings/Run Config/Continuous/Baseline Cycles","/Equipment/TrolleyInterface/Settings/Run Config/Interactive/Repeat"]).then(function(rpc) {
      var TrolleyPowerSet = String(rpc.result.data[0]);
      document.getElementById("ValTrolleyPower").innerHTML = TrolleyPowerSet;

      var LEDVoltageSet = String(rpc.result.data[1]);
      document.getElementById("ValLEDVoltage").innerHTML = LEDVoltageSet;

      var NBaseLine = String(rpc.result.data[2]);
      document.getElementById("ValBaselineCycles").innerHTML = NBaseLine;

      var NRepeat = String(rpc.result.data[3]);
      document.getElementById("ValInteractiveRepeat").innerHTML = NRepeat;

      }).catch(function(error) {
	mjsonrpc_error_alert(error);
	});
} 

function AbortMotion(){
  var value=1;
  mjsonrpc_db_paste(["/Equipment/GalilFermi/Settings/Emergency/Abort"],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}

function TurnONTrolley(){
  var value=true;
  mjsonrpc_db_paste(["/Equipment/GalilFermi/Settings/Manual Control/Trolley/Trolley Switch"],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  var cmd = 4;
  mjsonrpc_db_paste(["/Equipment/GalilFermi/Settings/Manual Control/Cmd"],[cmd]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}

function TurnOFFTrolley(){
  var value=false;
  mjsonrpc_db_paste(["/Equipment/GalilFermi/Settings/Manual Control/Trolley/Trolley Switch"],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  var cmd = 4;
  mjsonrpc_db_paste(["/Equipment/GalilFermi/Settings/Manual Control/Cmd"],[cmd]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}

function TurnONGarage(){
  var value=true;
  mjsonrpc_db_paste(["/Equipment/GalilFermi/Settings/Manual Control/Trolley/Garage Switch"],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  var cmd = 4;
  mjsonrpc_db_paste(["/Equipment/GalilFermi/Settings/Manual Control/Cmd"],[cmd]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}

function TurnOFFGarage(){
  var value=false;
  mjsonrpc_db_paste(["/Equipment/GalilFermi/Settings/Manual Control/Trolley/Garage Switch"],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  var cmd = 4;
  mjsonrpc_db_paste(["/Equipment/GalilFermi/Settings/Manual Control/Cmd"],[cmd]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}

function ManualTrolleyControlRel(){
  var Delta = document.getElementById("ManRelTrolley").value;
  mjsonrpc_db_paste(["/Equipment/GalilFermi/Settings/Manual Control/Trolley/Trolley Rel Pos","/Equipment/GalilFermi/Settings/Manual Control/Cmd"],[Delta,2]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}

function ManualTrolleyIndependentControlRel(){
  var Delta1 = document.getElementById("ManRelTrolley1").value;
  var Delta2 = document.getElementById("ManRelTrolley2").value;
  mjsonrpc_db_paste(["/Equipment/GalilFermi/Settings/Manual Control/Trolley/Trolley1 Rel Pos","/Equipment/GalilFermi/Settings/Manual Control/Trolley/Trolley2 Rel Pos","/Equipment/GalilFermi/Settings/Manual Control/Cmd"],[Delta1,Delta2,1]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}

function ManualGarageControlRel(){
  var Delta = document.getElementById("ManRelGarage").value;
  mjsonrpc_db_paste(["/Equipment/GalilFermi/Settings/Manual Control/Trolley/Garage Rel Pos","/Equipment/GalilFermi/Settings/Manual Control/Cmd"],[Delta,5]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}

function ManualDefineOrigin(){
  var TrolleyPos1 = document.getElementById("DefTrolley1").value;
  var TrolleyPos2 = document.getElementById("DefTrolley2").value;
  var GaragePos = document.getElementById("DefGarage").value;
  mjsonrpc_db_paste(["/Equipment/GalilFermi/Settings/Manual Control/Trolley/Trolley Def Pos1","/Equipment/GalilFermi/Settings/Manual Control/Trolley/Trolley Def Pos2","/Equipment/GalilFermi/Settings/Manual Control/Trolley/Garage Def Pos"],[TrolleyPos1,TrolleyPos2,GaragePos]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  mjsonrpc_db_paste(["/Equipment/GalilFermi/Settings/Manual Control/Cmd"],[3]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}

function ManualGarageIn(){
  mjsonrpc_db_paste(["/Equipment/GalilFermi/Settings/Manual Control/Cmd"],[6]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}

function ManualGarageOut(){
  mjsonrpc_db_paste(["/Equipment/GalilFermi/Settings/Manual Control/Cmd"],[7]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}

function DropdownSelectGeneral(name,content) {
    var id = name;
      document.getElementById(id).innerHTML = content;
}


function SetAutoMotionControl(){
  var StepSize = document.getElementById("TrolleyStepSize").value;
  var StepN = document.getElementById("TrolleyStepNumber").value;
  var Mode = document.getElementById("AutoMotionMode").innerHTML;
  var ContinuousMotion = document.getElementById("ContinuousMotionSelect").innerHTML;
  mjsonrpc_db_paste(["/Equipment/GalilFermi/Settings/Auto Control/Trolley/Trolley Rel Pos","/Equipment/GalilFermi/Settings/Auto Control/Trolley/Trolley Step Number"],[StepSize,StepN]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  mjsonrpc_db_paste(["/Equipment/GalilFermi/Settings/Auto Control/Mode","/Equipment/GalilFermi/Settings/Auto Control/Continuous Motion"],[Mode,ContinuousMotion]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}

function SetTrolleyRegulation(){
  var TLow = document.getElementById("TensionLow").value;
  var THigh = document.getElementById("TensionHigh").value;
  var TrolleyV = document.getElementById("TrolleyVelocity").value;
  var GarageV = document.getElementById("GarageVelocity").value;
  mjsonrpc_db_paste(["/Equipment/GalilFermi/Settings/Manual Control/Trolley/Tension Range Low","/Equipment/GalilFermi/Settings/Manual Control/Trolley/Tension Range High"],[TLow,THigh]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  mjsonrpc_db_paste(["/Equipment/GalilFermi/Settings/Manual Control/Trolley/Trolley Velocity","/Equipment/GalilFermi/Settings/Manual Control/Trolley/Garage Velocity"],[TrolleyV,GarageV]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}

function TrolleySetCommand(){
  var TrolleyVoltage = document.getElementById("TrolleyPowerSet").value;
  var LEDVoltage = document.getElementById("LEDVoltageSet").value;
  var BaselineCycles = document.getElementById("BaselineCycles").value;
  var InteractiveRepeat = document.getElementById("InteractiveRepeat").value;

  var RunMode = document.getElementById("RunMode").innerHTML;
  var IdleMode = document.getElementById("IdleMode").innerHTML;
  var ReadBarcode_ = document.getElementById("IdleReadBarcode").innerHTML;
  var PowerDown_ = document.getElementById("SleepPowerDown").innerHTML;

  var ReadBarcode = false;
  var PowerDown = false;
  if (ReadBarcode_ == "Y")ReadBarcode = true;
  if (PowerDown_ == "Y")PowerDown = true;

  mjsonrpc_db_paste(["/Equipment/TrolleyInterface/Settings/Trolley Power/Voltage","/Equipment/TrolleyInterface/Settings/Barcode/LED Voltage","/Equipment/TrolleyInterface/Settings/Run Config/Continuous/Baseline Cycles","/Equipment/TrolleyInterface/Settings/Run Config/Interactive/Repeat","/Equipment/TrolleyInterface/Settings/Run Config/Mode","/Equipment/TrolleyInterface/Settings/Run Config/Idle Mode","/Equipment/TrolleyInterface/Settings/Run Config/Idle/Read Barcode","/Equipment/TrolleyInterface/Settings/Run Config/Sleep/Power Down"],[TrolleyVoltage,LEDVoltage,BaselineCycles,InteractiveRepeat,RunMode,IdleMode,ReadBarcode,PowerDown]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  var cmd = 1;
  mjsonrpc_db_paste(["/Equipment/TrolleyInterface/Settings/Cmd"],[cmd]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}

function TrolleyInteractiveTrigger(){
  var trigger = 1;
  mjsonrpc_db_paste(["/Equipment/TrolleyInterface/Settings/Run Config/Interactive/Trigger"],[trigger]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}

function UpdateTrolleyProbes(){
  var ProbeSelect = document.getElementById("ProbeSelect").innerHTML;
  var Source = document.getElementById("ProbeSourceSelect").innerHTML;

  var ProbeEnable= document.getElementById("ProbeEnable").value;
  var PreampDelay= document.getElementById("PreampDelay").value;
  var PreampPeriod= document.getElementById("PreampPeriod").value;
  var ADCGateDelay= document.getElementById("ADCGateDelay").value;
  var ADCGateOffset= document.getElementById("ADCGateOffset").value;
  var ADCGatePeriod= document.getElementById("ADCGatePeriod").value;
  var TXDelay= document.getElementById("TXDelay").value;
  var TXPeriod= document.getElementById("TXPeriod").value;
  var UserData= document.getElementById("UserData").value;

  var BaseAddress = "/Equipment/TrolleyInterface/Settings/Probe/Probe";

  if (ProbeSelect != "All"){
    var ProbeID = Number(ProbeSelect);
    var addr_ProbeID= BaseAddress + ProbeSelect + "/Probe ID";
    var addr_ProbeEnable= BaseAddress + ProbeSelect + "/Probe Enable";
    var addr_PreampDelay= BaseAddress + ProbeSelect + "/Preamp Delay";
    var addr_PreampPeriod= BaseAddress + ProbeSelect + "/Preamp Period";
    var addr_ADCGateDelay= BaseAddress + ProbeSelect + "/ADC Gate Delay";
    var addr_ADCGateOffset= BaseAddress + ProbeSelect + "/ADC Gate Offset";
    var addr_ADCGatePeriod= BaseAddress + ProbeSelect + "/ADC Gate Period";
    var addr_TXDelay= BaseAddress + ProbeSelect + "/TX Delay";
    var addr_TXPeriod= BaseAddress + ProbeSelect + "/TX Period";
    var addr_UserData= BaseAddress + ProbeSelect + "/User Data";

    mjsonrpc_db_paste([addr_ProbeID,addr_ProbeEnable,addr_PreampDelay,addr_PreampPeriod,addr_ADCGateDelay,addr_ADCGateOffset,addr_ADCGatePeriod,addr_TXDelay,addr_TXPeriod,addr_UserData],[ProbeID,ProbeEnable,PreampDelay,PreampPeriod,ADCGateDelay,ADCGateOffset,ADCGatePeriod,TXDelay,TXPeriod,UserData]).then(function(rpc){;}).catch(function(error) {
	mjsonrpc_error_alert(error);
	});
  }else{
    var ProbeID = 1;
    for (ProbeID=1;ProbeID<=17;ProbeID++){
      ProbeSelect = String(ProbeID);
      var addr_ProbeID= BaseAddress + ProbeSelect + "/Probe ID";
      var addr_ProbeEnable= BaseAddress + ProbeSelect + "/Probe Enable";
      var addr_PreampDelay= BaseAddress + ProbeSelect + "/Preamp Delay";
      var addr_PreampPeriod= BaseAddress + ProbeSelect + "/Preamp Period";
      var addr_ADCGateDelay= BaseAddress + ProbeSelect + "/ADC Gate Delay";
      var addr_ADCGateOffset= BaseAddress + ProbeSelect + "/ADC Gate Offset";
      var addr_ADCGatePeriod= BaseAddress + ProbeSelect + "/ADC Gate Period";
      var addr_TXDelay= BaseAddress + ProbeSelect + "/TX Delay";
      var addr_TXPeriod= BaseAddress + ProbeSelect + "/TX Period";
      var addr_UserData= BaseAddress + ProbeSelect + "/User Data";

      mjsonrpc_db_paste([addr_ProbeID,addr_ProbeEnable,addr_PreampDelay,addr_PreampPeriod,addr_ADCGateDelay,addr_ADCGateOffset,addr_ADCGatePeriod,addr_TXDelay,addr_TXPeriod,addr_UserData],[ProbeID,ProbeEnable,PreampDelay,PreampPeriod,ADCGateDelay,ADCGateOffset,ADCGatePeriod,TXDelay,TXPeriod,UserData]).then(function(rpc){;}).catch(function(error) {
	  mjsonrpc_error_alert(error);
	  });
    }
  }

  mjsonrpc_db_paste(["/Equipment/TrolleyInterface/Settings/Probe/Source"],[Source]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}
