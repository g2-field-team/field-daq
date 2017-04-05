var cfg = {}
var keys = []
keys.push("/Equipment/Fixed Probes/Settings/output");
keys.push("/Equipment/Fixed Probes/Settings/config");
keys.push("/Equipment/Fixed Probes/Settings/devices");

function getSettings(callback) {
    var req = new Object();
    req.paths = keys;
    req.omit_names = true;
    req.omit_last_written = true;
    mjsonrpc_call("db_get_values", req).then(function(rpc) {
	cfg['output'] = rpc.result.data[0];
	cfg['config'] = rpc.result.data[1];
	cfg['devices'] = rpc.result.data[2];
	callback();
   })
}


function displaySettings() {
    console.log(JSON.stringify(cfg, undefined, 2));
    document.getElementById('settings').innerHTML = JSON.stringify(cfg, undefined, 2);
}


function putSettings()   {
    var req = new Object();
    req.paths = keys;
    req.omit_names = true;
    req.omit_last_written = true;
    mjsonrpc_call("db_get_values", req).then(function(rpc) {
	cfg['output'] = rpc.result.data[0];
	cfg['config'] = rpc.result.data[1];
	cfg['devices'] = rpc.result.data[2];
	console.log(JSON.stringify(cfg, undefined, 2));
	document.getElementById('settings').innerHTML = JSON.stringify(cfg, undefined, 2);
   })
}


window.onload = function() {
    getSettings(displaySettings);
}