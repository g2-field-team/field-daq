var updatePeriod = 500; // in msec
var updateTimerId = 0;

function unCheck() {
  document.getElementById("OnOff").checked = false;
}

function update()  {
  clearTimeout(updateTimerId);
  load();
  if (updatePeriod > 0)
    updateTimerId = setTimeout('update()', updatePeriod);
}
function load()   {
  mjsonrpc_db_get_values(["/Equipment/CompressorChiller/Variables/sta1","/Equipment/CompressorChiller/Variables/Errs","/Equipment/CompressorChiller/Variables/sta3","/Equipment/CompressorChiller/Variables/Temp","/Equipment/CompressorChiller/Variables/Pres","/Equipment/CompressorChiller/Variables/Flow","/Equipment/HeLevel/Variables/HeLe","/Equipment/CompressorChiller/Variables/Hour","/Equipment/HeLevel/Variables/ShTe","/Equipment/MagnetCoils/Variables/CoiX","/Equipment/MagnetCoils/Variables/CoiY","/Equipment/MagnetCoils/Variables/CoiZ","/Equipment/CompressorChiller/Common/Period","/Equipment/HeLevel/Common/Period","/Equipment/MagnetCoils/Common/Period"]).then(function(rpc) {
      var CoS = String(rpc.result.data[0]);
      if(CoS == "0x0001") {var sta = "On";}
      else if(CoS == "0x0000"){var sta = "Off";}
      else {var sta = "Err";}
      document.getElementById("CoS").innerHTML = sta;
      document.getElementById("CoE").innerHTML = writeErrString(rpc.result.data[1]);
      var ChS = String(rpc.result.data[2]);
      if(ChS == "0x0001") {var sta = "On";}
      else if(ChS == "0x0000"){var sta = "Off";}
      else {var sta = "Err";}
      document.getElementById("ChS").innerHTML = sta;
      var ChT = String(Math.round(rpc.result.data[3]*10)/10);
      document.getElementById("ChT").innerHTML = ChT;
      var ChP = String(Math.round(rpc.result.data[4]*10)/10);
      document.getElementById("ChP").innerHTML = ChP;
      var ChF = String(Math.round(rpc.result.data[5]*10)/10);
      document.getElementById("ChF").innerHTML = ChF;
      var HeL = String(Math.round(rpc.result.data[6]*10)/10);
      document.getElementById("HeL").innerHTML = HeL;
      var CoH = String(rpc.result.data[7]);
      document.getElementById("CoH").innerHTML = CoH;
      var ShT = String(rpc.result.data[8]);
      document.getElementById("ShT").innerHTML = ShT;
      var CoX = String(Math.round(rpc.result.data[9]*1000)/1000);
      document.getElementById("CoX").innerHTML = CoX;
      var CoY = String(Math.round(rpc.result.data[10]*1000)/1000);
      document.getElementById("CoY").innerHTML = CoY;
      var CoZ = String(Math.round(rpc.result.data[11]*1000)/1000);
      document.getElementById("CoZ").innerHTML = CoZ;
      var HeM = String(rpc.result.data[13] / 60000);
      document.getElementById("HeM").innerHTML = HeM.concat(" minutes");
      var CCM = String(rpc.result.data[12] / 60000);
      document.getElementById("CCM").innerHTML = CCM.concat(" minutes");
      var MCM = String(rpc.result.data[14] / 1000);
      document.getElementById("MCM").innerHTML = MCM.concat(" seconds");
      }).catch(function(error) {
        mjsonrpc_error_alert(error);
	});
} 

function writeErrString(errArray){
  CompressorErrorMessage = "Errors: ";

  if (errArray[0] == 1) {
    CompressorErrorMessage += "SYSTEM ERROR, ";
  }
  if (errArray[1] == 1) {
    CompressorErrorMessage += "Compressor fail, ";
  }
  if (errArray[2] == 1) {
    CompressorErrorMessage += "Locked rotor, ";
  }
  if (errArray[3] == 1) {
    CompressorErrorMessage += "OVERLOAD, ";
  }
  if (errArray[4] == 1) {
    CompressorErrorMessage += "Phase/fuse ERROR, ";
  }
  if (errArray[5] == 1) {
    CompressorErrorMessage += "Pressure alarm, ";
  }
  if (errArray[6] == 1) {
    CompressorErrorMessage += "Helium temp. fail, ";
  }
  if (errArray[7] == 1) {
    CompressorErrorMessage += "Oil circuit fail, ";
  }
  if (errArray[8] == 1) {
    CompressorErrorMessage += "RAM ERROR, ";
  }
  if (errArray[9] == 1) {
    CompressorErrorMessage += "ROM ERROR, ";
  }
  if (errArray[10] == 1) {
    CompressorErrorMessage += "EEPROM ERROR, ";
  }
  if (errArray[11] == 1) {
    CompressorErrorMessage += "DC Voltage error, ";
  }
  if (errArray[12] == 1) {
    CompressorErrorMessage += "MAINS LEVEL !!!!, ";
  }
 
  if (CompressorErrorMessage == "Errors: "){
    CompressorErrorMessage += "None"
  } else {
    CompressorErrorMessage = CompressorErrorMessage.substr(0,CompressorErrorMessage.length-2);
  }

  return CompressorErrorMessage;
}

function SetUpdatePeriod(he, cc, mc){
  he = 60000 * he;
  cc = 60000 * cc;
  mc = 1000 * mc;

  mjsonrpc_db_paste(["/Equipment/HeLevel/Common/Period"],[he]).then(function(rpc){;}).catch(function(error) {
    mjsonrpc_error_alert(error);
  });
  mjsonrpc_db_paste(["/Equipment/CompressorChiller/Common/Period"],[cc]).then(function(rpc){;}).catch(function(error) {
    mjsonrpc_error_alert(error);
  });
  mjsonrpc_db_paste(["/Equipment/MagnetCoils/Common/Period"],[mc]).then(function(rpc){;}).catch(function(error) {
    mjsonrpc_error_alert(error);
  });

  ccalarm = cc / 1000;
  if (ccalarm < 300){
    ccalarm = 300;
  }
  mjsonrpc_db_paste(["/Alarms/Alarms/FE Monitor/Check interval"],[ccalarm]).then(function(rpc){;}).catch(function(error) {
    mjsonrpc_error_alert(error);
  });
  mjsonrpc_db_paste(["/Alarms/Alarms/FE Monitor/Triggered"],[0]).then(function(rpc){;}).catch(function(error) {
    mjsonrpc_error_alert(error);
  });
}

function SetCurrents(x,y,z){
  x = 1000*x;
  y = 1000*y;
  z = 1000*z;
  updateval = 1;
 
  mjsonrpc_db_get_values(["/Equipment/MagnetCoils/Settings/update"]).then(function(rpc) {
    updateval = rpc.result.data[0]; 
 
    if (x > 10000 || x < -10000 || y > 10000 || y < -10000 || z > 10000 || z < -10000){
      alert("Please enter valid current values (-10A to 10A)");
    } else if(updateval == 1){
      alert("Please wait until last command completes");
    } else {
      mjsonrpc_db_paste(["/Equipment/MagnetCoils/Settings/update"],[1]).then(function(rpc){;}).catch(function(error) {
        mjsonrpc_error_alert(error);
      });
      mjsonrpc_db_paste(["/Equipment/MagnetCoils/Settings/setX"],[x]).then(function(rpc){;}).catch(function(error) {
        mjsonrpc_error_alert(error);
      });
      mjsonrpc_db_paste(["/Equipment/MagnetCoils/Settings/setY"],[y]).then(function(rpc){;}).catch(function(error) {
        mjsonrpc_error_alert(error);
      });
      mjsonrpc_db_paste(["/Equipment/MagnetCoils/Settings/setZ"],[z]).then(function(rpc){;}).catch(function(error) {
        mjsonrpc_error_alert(error);
      });
    }
  }).catch(function(error) {
    mjsonrpc_error_alert(error);
  });

}

function CompressorOn(){
  if (document.getElementById("OnOff").checked) {
    if (confirm("Are You Sure?")) {
      SetOnOff(1,1);
      strOnOff = "on";
    } 
  }
}

function CompressorOff(){
  if (document.getElementById("OnOff").checked) {
    if (confirm("Are You Sure?")) {
      SetOnOff(1,0);
      strOnOff = "off";
    } 
  } 
}

function ChillerOn(){
  if (document.getElementById("OnOff").checked) {
    if (confirm("Are You Sure?")) {
      SetOnOff(3,1);
      strOnOff = "on";
    } 
  } 
}

function ChillerOff(){
  if (document.getElementById("OnOff").checked) {
    if (confirm("Are You Sure?")) {
      SetOnOff(3,0);
      strOnOff = "off";
    } 
  } 
}

function SetOnOff(machine, chSta){
  var curSta;
  var proceed;
  if (machine == 1) {
    mjsonrpc_db_get_values(["/Equipment/CompressorChiller/Settings/comp_ctrl"]).then(function(rpc) {
      proceed = rpc.result.data[0];
      if (proceed == 2){
        curSta = document.getElementById("CoS").innerHTML;
        if (curSta != chSta) {
          alert("Compressor is already " + strOnOff);
        } else {
          mjsonrpc_db_paste(["/Equipment/CompressorChiller/Settings/comp_ctrl"],[chSta]).then(function(rpc){;}).catch(function(error) {
          mjsonrpc_error_alert(error);
          });
        }
      } else {
        alert("Please Wait on Last Command");
      }
    }).catch(function(error) {
	mjsonrpc_error_alert(error);
    });    
  } else if (machine == 3) {
    mjsonrpc_db_get_values(["/Equipment/CompressorChiller/Settings/chil_ctrl"]).then(function(rpc) {
      proceed = rpc.result.data[0];
      if (proceed == 2){
        curSta = document.getElementById("ChS").innerHTML;
        if (curSta != chSta) {
          alert("Chiller is already " + strOnOff);
        } else {
          mjsonrpc_db_paste(["/Equipment/CompressorChiller/Settings/chil_ctrl"],[chSta]).then(function(rpc){;}).catch(function(error) {
          mjsonrpc_error_alert(error);
          });
        }
      } else {
        alert("Please Wait on Last Command");
      }
    }).catch(function(error) {
	mjsonrpc_error_alert(error);
    });    
  }
}

