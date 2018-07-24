var exec = require('child_process').exec;
var Blynk = require('blynk-library');
var AUTH = '982fd15abcf74858b17b27d99d6e98c4';
var blynk = new Blynk.Blynk(AUTH);

var Gpio = require('onoff').Gpio;

var sm1 = new Gpio(26, 'out');
var dm1 = new Gpio(19, 'out');

var ms1_1 = new Gpio(16, 'out');
var ms2_1 = new Gpio(20, 'out');
var ms3_1 = new Gpio(21, 'out');

var v0 = new blynk.VirtualPin(0);
var v1 = new blynk.VirtualPin(1);
var v2 = new blynk.VirtualPin(2);

var interval;

var run;

var turn = function(){
	var state = 0;
	
	interval = setInterval(function(){
		if(state == 0){state = 1; sm1.writeSync(1);}
		else if(state == 1){state = 0; sm1.writeSync(0);}
		
	},2);
	
}

v0.on('write',function(param){
	if (param == 1 && run == 0){turn(); run == 1;}
	if (param == 0){clearInterval(interval);run = 0;}
});

v1.on('write',function(param){
	if(param == 0)dm1.writeSync(0);
	if(param == 1)dm1.writeSync(1);
});
	
v2.on('write',function(param){
	if(param == 3){ms1_1.writeSync(0);ms2_1.writeSync(0);ms3_1.writeSync(0);}
	if(param == 2){ms1_1.writeSync(1);ms2_1.writeSync(0);ms3_1.writeSync(0);}
	if(param == 1){ms1_1.writeSync(0);ms2_1.writeSync(1);ms3_1.writeSync(0);}
	if(param == 0){ms1_1.writeSync(1);ms2_1.writeSync(1);ms3_1.writeSync(1);}
});

blynk.on('connect', function() {
  console.log("Blynk ready.");
  blynk.syncAll();
});