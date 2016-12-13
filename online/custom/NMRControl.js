
function NMRDropdownSelect(name,ch,content) {
  var id = name+ch;
  document.getElementById(id).innerHTML = content;
}

function NMRDropdownSelectGeneral(name,content) {
  var id = name;
  document.getElementById(id).innerHTML = content;
}

function LoadToOdb(){
  //FPGA Timing
  for (i=1;i<=4;i++){
    var odb_base = "/Equipment/NMRProbes/Settings/FPGA Timing/";
    var odb_dir = odb_base + "S" + i +"/";
    //Configuration
    var key = "Configuration";
    var id = "FPGAConfig" + i;
    var value = document.getElementById(id).innerHTML;
    mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
	mjsonrpc_error_alert(error);
	});
    // Mechanical Offset
    key = "Mechanical Offset";
    id = "MechOffset" + i;
    value = document.getElementById(id).value;
    mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
	mjsonrpc_error_alert(error);
	});
    // Mechanical Switch Duration
    key = "Mech Switch Duration";
    id = "MechSwitchDuration" + i;
    value = document.getElementById(id).value;
//    var unit = getTimeUnit(document.getElementById(unit_id).innerHTML);
//    value = value * unit;
    mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
	mjsonrpc_error_alert(error);
	});
    // Mechanical Switch Unit
    key = "Mech Switch Unit";
    var unit_id = "MechSwitchDurationUnit" + i;
    var unit = document.getElementById(unit_id).innerHTML;
    mjsonrpc_db_paste([odb_dir+key],[unit]).then(function(rpc){;}).catch(function(error) {
	mjsonrpc_error_alert(error);
	});
    // RF Transmit Offset
    key = "RF Transmit Offset";
    id = "RFTransOffset" + i;
    value = document.getElementById(id).value;
    mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
	mjsonrpc_error_alert(error);
	});
    // RF Transmit Switch Duration
    key = "RF Trans Switch Duration";
    id = "RFTransSwitchDuration" + i;
    value = document.getElementById(id).value;
//    var unit = getTimeUnit(document.getElementById(unit_id).innerHTML);
//    value = value * unit;
    mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
	mjsonrpc_error_alert(error);
	});
    // RF Trasmit Unit
    key = "RF Trans Switch Unit";
    var unit_id = "RFTransSwitchDurationUnit" + i;
    var unit = document.getElementById(unit_id).innerHTML;
    mjsonrpc_db_paste([odb_dir+key],[unit]).then(function(rpc){;}).catch(function(error) {
	mjsonrpc_error_alert(error);
	});
    // Tomco Offset
    key = "Tomco Offset";
    id = "TomcoOffset" + i;
    value = document.getElementById(id).value;
    mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
	mjsonrpc_error_alert(error);
	});
    // Amplifier Duration
    key = "Amplifier Duration";
    id = "TomcoAmpDuration" + i;
    var unit_id = "TomcoAmpDurationUnit" + i;
    value = document.getElementById(id).value;
 //   var unit = getTimeUnit(document.getElementById(unit_id).innerHTML);
 //   value = value * unit;
    mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
	mjsonrpc_error_alert(error);
	});
    // Amplifier Unit
    key = "Amplifier Unit";
    var unit_id = "TomcoAmpDurationUnit" + i;
    var unit = document.getElementById(unit_id).innerHTML;
    mjsonrpc_db_paste([odb_dir+key],[unit]).then(function(rpc){;}).catch(function(error) {
	mjsonrpc_error_alert(error);
	});
    // Tomco Enable
    key = "Tomco Enable";
    id = "TomcoEnable" + i;
    value = document.getElementById(id).checked;
    mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
	mjsonrpc_error_alert(error);
	});
    // RF Receive Offset
    key = "RF Receive Offset";
    id = "RFRecvOffset" + i;
    value = document.getElementById(id).value;
    mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
	mjsonrpc_error_alert(error);
	});
    // RF Receive Switch Duration
    key = "RF Rec Switch Duration";
    id = "RFRecvSwitchDuration" + i;
    var unit_id = "RFRecvSwitchDurationUnit" + i;
    value = document.getElementById(id).value;
//    var unit = getTimeUnit(document.getElementById(unit_id).innerHTML);
//    value = value * unit;
    mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
	mjsonrpc_error_alert(error);
	});
    // RF Receive Unit
    key = "RF Rec Switch Unit";
    var unit_id = "RFRecvSwitchDurationUnit" + i;
    var unit = document.getElementById(unit_id).innerHTML;
    mjsonrpc_db_paste([odb_dir+key],[unit]).then(function(rpc){;}).catch(function(error) {
	mjsonrpc_error_alert(error);
	});
  }

  //LO Settings
  var odb_dir = "/Equipment/NMRProbes/Settings/LO/";
  //Frequency
  var key = "Frequency";
  var id = "LOFreq";
  var value = document.getElementById(id).value;
  var unit_id = "LOSettingFreqUnit";
  value = document.getElementById(id).value;
//  var unit = getFreqUnit(document.getElementById(unit_id).innerHTML);
//  value = value * unit;
  mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  //Frequency Unit
  var key = "Frequency Unit";
  var unit_id = "LOSettingFreqUnit";
  var unit = document.getElementById(unit_id).innerHTML;
  mjsonrpc_db_paste([odb_dir+key],[unit]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  //BNC
  key = "BNC Voltage";
  id = "BNCVoltage";
  value = document.getElementById(id).value;
  mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  key = "BNC V-choice";
  id = "LOSettingBNCUnit";
  value = document.getElementById(id).innerHTML;
  mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  key = "BNC Switch";
  id = "BNCOn";
  value = document.getElementById(id).checked;
  mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  //N-type
  key = "N-Type Voltage";
  id = "NTypeVoltage";
  value = document.getElementById(id).value;
  mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  key = "N-Type V-choice";
  id = "LOSettingNTypeUnit";
  value = document.getElementById(id).innerHTML;
  mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  key = "N-Type Switch";
  id = "NTypeOn";
  value = document.getElementById(id).checked;
  mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  //Digitizer Settings
  odb_dir = "/Equipment/NMRProbes/Settings/Digitizer/";
  //Struck ID
  key = "Struck ID";
  id = "StruckID";
  value = document.getElementById(id).value;
  mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  //Channel Number
  key = "Channel Number";
  id = "DigitizerChNumber";
  value = document.getElementById(id).value;
  mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  //Channel Number
  key = "Number of Pulses";
  id = "NOfPulses";
  value = document.getElementById(id).value;
  mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  //Sampling Frequency
  key = "Sampling Frequency";
  id = "SamplingFreq";
  unit_id = "SamplingFreqUnit";
  value = document.getElementById(id).value;
//  unit = getFreqUnit(document.getElementById(unit_id).innerHTML);
//  value = value * unit;
  mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  //Sampling Frequency Unit
  key = "Frequency Unit";
  unit_id = "SamplingFreqUnit";
  unit = document.getElementById(unit_id).innerHTML;
  mjsonrpc_db_paste([odb_dir+key],[unit]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  //External
  key = "External";
  id = "DigiExternal";
  value = document.getElementById(id).checked;
  mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  //Utilities
  odb_dir = "/Equipment/NMRProbes/Settings/Utilities/";
  //Debug Mode
  key = "Debug Mode";
  id = "DebugMode";
  value = document.getElementById(id).innerHTML;
  mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  //Verbosity
  key = "Verbosity";
  id = "Verbosity";
  value = document.getElementById(id).innerHTML;
  mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  //Test Mode
  key = "Test Mode";
  id = "TestMode";
  value = document.getElementById(id).innerHTML;
  mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  // Delay Time
  key = "Delay Time";
  id = "DelayTime";
  unit_id = "DelayTimeUnit";
  value = document.getElementById(id).value;
//  unit = getTimeUnit(document.getElementById(unit_id).innerHTML);
//  value = value * unit;
  mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  // Delay Time Unit
  key = "Delay Time Unit";
  unit_id = "DelayTimeUnit";
  unit = document.getElementById(unit_id).innerHTML;
  mjsonrpc_db_paste([odb_dir+key],[unit]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  //PTS160 Frequency
  key = "PTS160 Frequency";
  id = "PTS160Freq";
  unit_id = "PTS160FreqUnit";
  value = document.getElementById(id).value;
//  unit = getFreqUnit(document.getElementById(unit_id).innerHTML);
//  value = value * unit;
  mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  //PTS160 Frequency Unit
  key = "PTS160 Frequency Unit";
  unit_id = "PTS160FreqUnit";
  unit = document.getElementById(unit_id).innerHTML;
  mjsonrpc_db_paste([odb_dir+key],[unit]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  //Config Label
  key = "Config Label";
  id = "ConfigLabel";
  value = document.getElementById(id).value;
  mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  //Comments
  key = "Comments";
  id = "RunComments";
  value = document.getElementById(id).value;
  mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  //Global On/Off
  key = "Global On";
  id = "GlobalON";
  value = document.getElementById(id).checked;
  mjsonrpc_db_paste([odb_dir+key],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}

function getTimeUnit(input){
  if (input == "s"){return 1;}
  else if (input == "ms"){return 1e-3;}
  else if (input == "us"){return 1e-6;}
  else {return 1;}//default
}

function getFreqUnit(input){
  if (input == "kHz"){return 1e3;}
  else if (input == "MHz"){return 1e6;}
  else {return 1e3;}//default
}
