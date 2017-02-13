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
  mjsonrpc_db_get_values(["/Equipment/CompressorChiller/Variables/sta1","/Equipment/CompressorChiller/Variables/CompressorErrors","/Equipment/CompressorChiller/Variables/sta3","/Equipment/CompressorChiller/Variables/Temp","/Equipment/CompressorChiller/Variables/Pres","/Equipment/CompressorChiller/Variables/Flow","/Equipment/HeLevel/Variables/HeLe"]).then(function(rpc) {
      var CoS = String(rpc.result.data[0]);
      document.getElementById("CoS").innerHTML = CoS;
      var CoE = String(rpc.result.data[1]);
      document.getElementById("CoE").innerHTML = CoE;
      var ChS = String(rpc.result.data[2]);
      document.getElementById("ChS").innerHTML = ChS;
      var ChT = String(rpc.result.data[3]);
      document.getElementById("ChT").innerHTML = ChT;
      var ChP = String(rpc.result.data[4]);
      document.getElementById("ChP").innerHTML = ChP;
      var ChF = String(rpc.result.data[5]);
      document.getElementById("ChF").innerHTML = ChF;
      var HeL = String(rpc.result.data[6]);
      document.getElementById("HeL").innerHTML = HeL;
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

