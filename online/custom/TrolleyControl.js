var updatePeriod = 500; // in msec
var updateTimerId = 0;

function update()  {
  clearTimeout(updateTimerId);
  load();
  if (updatePeriod > 0)
    updateTimerId = setTimeout('update()', updatePeriod);
}
function load()   {
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
      document.getElementById("TA").innerHTML = As[0]/1000.0;
      document.getElementById("TB").innerHTML = As[1]/1000.0;
      document.getElementById("TempA").innerHTML = As[2]/1000.0;
      document.getElementById("TempB").innerHTML = As[3]/1000.0;
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
  var TLow = document.getElementById("TensionLow").value*1000;
  var THigh = document.getElementById("TensionHigh").value*1000;
  var TrolleyV = document.getElementById("TrolleyVelocity").value;
  var GarageV = document.getElementById("GarageVelocity").value;
  mjsonrpc_db_paste(["/Equipment/GalilFermi/Settings/Manual Control/Trolley/Tension Range Low","/Equipment/GalilFermi/Settings/Manual Control/Trolley/Tension Range High"],[TLow,THigh]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
  mjsonrpc_db_paste(["/Equipment/GalilFermi/Settings/Manual Control/Trolley/Trolley Velocity","/Equipment/GalilFermi/Settings/Manual Control/Trolley/Garage Velocity"],[TrolleyV,GarageV]).then(function(rpc){;}).catch(function(error) {
      mjsonrpc_error_alert(error);
      });
}


