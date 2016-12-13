var updatePeriod = 500; // in msec
var updateTimerId = 0;

function update()  {
  clearTimeout(updateTimerId);
  load();
  if (updatePeriod > 0)
    updateTimerId = setTimeout('update()', updatePeriod);
}
function load()   {
  mjsonrpc_db_get_values(["/Equipment/Galil/Monitors/Positions","/Equipment/Galil/Monitors/Velocities","/Equipment/Galil/Monitors/ControlVoltages"]).then(function(rpc) {
      var Ps= String(rpc.result.data[0]);
      Ps = Ps.split(',');
      document.getElementById("PX").innerHTML = Ps[0];
      document.getElementById("PY").innerHTML = Ps[1];
      document.getElementById("PZ").innerHTML = Ps[2];
      document.getElementById("PS").innerHTML = Ps[3];
      var Vs= String(rpc.result.data[1]);
      Vs = Vs.split(',');
      document.getElementById("VX").innerHTML = Vs[0];
      document.getElementById("VY").innerHTML = Vs[1];
      document.getElementById("VZ").innerHTML = Vs[2];
      document.getElementById("VS").innerHTML = Vs[3];
      var Cs= String(rpc.result.data[2]);
      Cs = Cs.split(',');
      document.getElementById("CX").innerHTML = Cs[0];
      document.getElementById("CY").innerHTML = Cs[1];
      document.getElementById("CZ").innerHTML = Cs[2];
      document.getElementById("CS").innerHTML = Cs[3];
      }).catch(function(error) {
	mjsonrpc_error_alert(error);
	});
} 

function SetAutoMotorControl(){
  var DeltaX = document.getElementById("SetAutoCtrlX").value;
  var DeltaY = document.getElementById("SetAutoCtrlY").value;
  var DeltaZ = document.getElementById("SetAutoCtrlZ").value;
  var DeltaS = document.getElementById("SetAutoCtrlS").value;
  document.getElementById("ValAutoCtrlX").innerHTML = document.getElementById("SetAutoCtrlX").value;
  document.getElementById("ValAutoCtrlY").innerHTML = document.getElementById("SetAutoCtrlY").value;
  document.getElementById("ValAutoCtrlZ").innerHTML = document.getElementById("SetAutoCtrlZ").value;
  document.getElementById("ValAutoCtrlS").innerHTML = document.getElementById("SetAutoCtrlS").value;
  mjsonrpc_db_paste(["/Equipment/Galil/AutoControl/RelPos[0-3]"],[[DeltaX,DeltaY,DeltaZ,DeltaS]]).then(function(rpc){;}).catch(function(error) {
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
  mjsonrpc_db_paste(["/Equipment/Galil/AutoControl/StepNumber[0-3]"],[[NX,NY,NZ,NS]]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  document.getElementById('SetConfirm').innerHTML = 'Confirmed';
}

function ManualMotorControlAbs(){
  var DeltaX = document.getElementById("ManAbsX").value;
  var DeltaY = document.getElementById("ManAbsY").value;
  var DeltaZ = document.getElementById("ManAbsZ").value;
  var DeltaS = document.getElementById("ManAbsS").value;
  mjsonrpc_db_paste(["/Equipment/Galil/ManualControl/AbsPos[0-3]","/Equipment/Galil/ManualControl/Cmd"],[[DeltaX,DeltaY,DeltaZ,DeltaS],1]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}

function ManualDefineOrigin(){
  document.getElementById("ManAbsX").value = 0;
  document.getElementById("ManAbsY").value = 0;
  document.getElementById("ManAbsZ").value = 0;
  document.getElementById("ManAbsS").value = 0;
  var DeltaX = document.getElementById("ManAbsX").value;
  var DeltaY = document.getElementById("ManAbsY").value;
  var DeltaZ = document.getElementById("ManAbsZ").value;
  var DeltaS = document.getElementById("ManAbsS").value;
  mjsonrpc_db_paste(["/Equipment/Galil/ManualControl/AbsPos[0-3]","/Equipment/Galil/ManualControl/Cmd"],[[DeltaX,DeltaY,DeltaZ,DeltaS],3]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}

function ManualMotorControlRel(){
  var DeltaX = document.getElementById("ManRelX").value;
  var DeltaY = document.getElementById("ManRelY").value;
  var DeltaZ = document.getElementById("ManRelZ").value;
  var DeltaS = document.getElementById("ManRelS").value;
  mjsonrpc_db_paste(["/Equipment/Galil/ManualControl/RelPos[0-3]","/Equipment/Galil/ManualControl/Cmd"],[[DeltaX,DeltaY,DeltaZ,DeltaS],2]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}

