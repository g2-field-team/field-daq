var updatePeriod = 500; // in msec
var updateTimerId = 0;

function update()  {
  clearTimeout(updateTimerId);
  load();
  if (updatePeriod > 0)
    updateTimerId = setTimeout('update()', updatePeriod);
}
function load()   {
  mjsonrpc_db_get_values(["/Equipment/Galil-Argonne/Monitors/Positions","/Equipment/Galil-Argonne/Monitors/Velocities","/Equipment/Galil-Argonne/Monitors/Control Voltages","/Equipment/Galil-Argonne/Monitors/Limit Switches Forward","/Equipment/Galil-Argonne/Monitors/Limit Switches Reverse","/Equipment/Galil-Argonne/Monitors/Motor Status"]).then(function(rpc) {
      var Ps= String(rpc.result.data[0]);
      Ps = Ps.split(',');
      document.getElementById("PX").innerHTML = -Ps[0];
      document.getElementById("PY").innerHTML = -Ps[1];
      document.getElementById("PZ").innerHTML = Ps[2];
      document.getElementById("PS").innerHTML = Ps[3];
      var Vs= String(rpc.result.data[1]);
      Vs = Vs.split(',');
      document.getElementById("VX").innerHTML = -Vs[0];
      document.getElementById("VY").innerHTML = -Vs[1];
      document.getElementById("VZ").innerHTML = Vs[2];
      document.getElementById("VS").innerHTML = Vs[3];
      var Cs= String(rpc.result.data[2]);
      Cs = Cs.split(',');
      document.getElementById("CX").innerHTML = -Cs[0]/1000.0;
      document.getElementById("CY").innerHTML = -Cs[1]/1000.0;
      document.getElementById("CZ").innerHTML = Cs[2]/1000.0;
      document.getElementById("CS").innerHTML = Cs[3]/1000.0;
      var Lf= String(rpc.result.data[3]);
      Lf = Lf.split(',');
      document.getElementById("FX").innerHTML = Lf[0];
      document.getElementById("FY").innerHTML = Lf[1];
      document.getElementById("FZ").innerHTML = Lf[2];
      document.getElementById("FS").innerHTML = Lf[3];
      var Lr= String(rpc.result.data[4]);
      Lr = Lr.split(',');
      document.getElementById("RX").innerHTML = Lr[0];
      document.getElementById("RY").innerHTML = Lr[1];
      document.getElementById("RZ").innerHTML = Lr[2];
      document.getElementById("RS").innerHTML = Lr[3];
      var MO= String(rpc.result.data[5]);
      MO = MO.split(',');
      if (MO[0]=="false")document.getElementById("MOX").innerHTML = "OFF";
      else document.getElementById("MOX").innerHTML = "ON";
      if (MO[1]=="false")document.getElementById("MOY").innerHTML = "OFF";
      else document.getElementById("MOY").innerHTML = "ON";
      if (MO[2]=="false")document.getElementById("MOZ").innerHTML = "OFF";
      else document.getElementById("MOZ").innerHTML = "ON";
      if (MO[3]=="false")document.getElementById("MOS").innerHTML = "OFF";
      else document.getElementById("MOS").innerHTML = "ON";
      }).catch(function(error) {
	mjsonrpc_error_alert(error);
	});
} 

function AbortMotion(){
  var value=1;
  mjsonrpc_db_paste(["/Equipment/Galil-Argonne/Settings/Emergency/Abort"],[value]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}

function MotorsON(){
  mjsonrpc_db_paste(["/Equipment/Galil-Argonne/Settings/Manual Control/Platform1/Platform1 Switch","/Equipment/Galil-Argonne/Settings/Manual Control/Cmd"],[true,4]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}

function MotorsOFF(){
  mjsonrpc_db_paste(["/Equipment/Galil-Argonne/Settings/Manual Control/Platform1/Platform1 Switch","/Equipment/Galil-Argonne/Settings/Manual Control/Cmd"],[false,4]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}

function SetAutoMotorControl(){
  var DeltaX = -document.getElementById("SetAutoCtrlX").value;
  var DeltaY = -document.getElementById("SetAutoCtrlY").value;
  var DeltaZ = document.getElementById("SetAutoCtrlZ").value;
  var DeltaS = document.getElementById("SetAutoCtrlS").value;
  document.getElementById("ValAutoCtrlX").innerHTML = document.getElementById("SetAutoCtrlX").value;
  document.getElementById("ValAutoCtrlY").innerHTML = document.getElementById("SetAutoCtrlY").value;
  document.getElementById("ValAutoCtrlZ").innerHTML = document.getElementById("SetAutoCtrlZ").value;
  document.getElementById("ValAutoCtrlS").innerHTML = document.getElementById("SetAutoCtrlS").value;
  mjsonrpc_db_paste(["/Equipment/Galil-Argonne/Settings/Auto Control/Platform1/Platform1 Rel Pos[0-3]"],[[DeltaX,DeltaY,DeltaZ,DeltaS]]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  var NX = document.getElementById("StepNumberX").value;
  var NY = document.getElementById("StepNumberY").value;
  var NZ = document.getElementById("StepNumberZ").value;
  var NS = document.getElementById("StepNumberS").value;
  document.getElementById("ValStepNumberX").innerHTML = document.getElementById("StepNumberX").value;
  document.getElementById("ValStepNumberY").innerHTML = document.getElementById("StepNumberY").value;
  document.getElementById("ValStepNumberZ").innerHTML = document.getElementById("StepNumberZ").value;
  document.getElementById("ValStepNumberS").innerHTML = document.getElementById("StepNumberS").value;
  mjsonrpc_db_paste(["/Equipment/Galil-Argonne/Settings/Auto Control/Platform1/Platform1 Step Number[0-3]"],[[NX,NY,NZ,NS]]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  //document.getElementById('SetConfirm').innerHTML = 'Confirmed';
}

function ManualMotorControlAbs(){
  var DeltaX = -document.getElementById("ManAbsX").value;
  var DeltaY = -document.getElementById("ManAbsY").value;
  var DeltaZ = document.getElementById("ManAbsZ").value;
  var DeltaS = document.getElementById("ManAbsS").value;
  mjsonrpc_db_paste(["/Equipment/Galil-Argonne/Settings/Manual Control/Platform1/Platform1 Abs Pos[0-3]","/Equipment/Galil-Argonne/Settings/Manual Control/Cmd"],[[DeltaX,DeltaY,DeltaZ,DeltaS],1]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}

function ManualDefinePosition(){
  var DefX = -document.getElementById("DefX").value;
  var DefY = -document.getElementById("DefY").value;
  var DefZ = document.getElementById("DefZ").value;
  var DefS = document.getElementById("DefS").value;
  mjsonrpc_db_paste(["/Equipment/Galil-Argonne/Settings/Manual Control/Platform1/Platform1 Def Pos[0-3]","/Equipment/Galil-Argonne/Settings/Manual Control/Cmd"],[[DefX,DefY,DefZ,DefS],3]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}

function ManualMotorControlRel(){
  var DeltaX = -document.getElementById("ManRelX").value;
  var DeltaY = -document.getElementById("ManRelY").value;
  var DeltaZ = document.getElementById("ManRelZ").value;
  var DeltaS = document.getElementById("ManRelS").value;
  mjsonrpc_db_paste(["/Equipment/Galil-Argonne/Settings/Manual Control/Platform1/Platform1 Rel Pos[0-3]","/Equipment/Galil-Argonne/Settings/Manual Control/Cmd"],[[DeltaX,DeltaY,DeltaZ,DeltaS],2]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}

