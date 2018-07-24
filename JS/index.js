
require("./motor");

var exec = require('child_process').exec;
var Blynk = require('blynk-library');
var AUTH = 'xxx';
var blynk = new Blynk.Blynk(AUTH);

var Gpio = require('onoff').Gpio;
var led_g = new Gpio(26, 'out');

var v0 = new blynk.VirtualPin(0);
var v1 = new blynk.VirtualPin(1);
var v2 = new blynk.VirtualPin(2);
var v3 = new blynk.VirtualPin(3);
var v4 = new blynk.VirtualPin(4);

var v10 = new blynk.VirtualPin(10);
var v11 = new blynk.VirtualPin(11);

var v6 = new blynk.VirtualPin(6);

var led = [];

led[1] = new blynk.WidgetLED(1);
led[2] = new blynk.WidgetLED(2);
led[3] = new blynk.WidgetLED(3);
led[4] = new blynk.WidgetLED(4);
led[5] = new blynk.WidgetLED(5);

var socket = 1;
var cmd;
var click = false; 

var timeEnd = 0;

v0.on('write',function(param){
	socket = param[0];
})

v10.on('write', function(param){
	if (param[0]=='1') turn(socket, 'on');
});

v11.on('write', function(param){
	if (param[0]=='1') turn(socket, 'off');
});

v4.on('write', function(param){
	timeEnd = param[0]/60;
	console.log('timer: ' + timeEnd);
})


class Timer{
	constructor(timeEnd, socket){
		this.s = socket;
		this.e = timeEnd;
		this.interval = setInterval(this.countdown.bind(this), 1000);
	}
	
	countdown(){
		console.log('countdown ' + this.s + ' - ' + this.e);
		if (this.e == 0){
			this.off();
			this.clr();
		}
		this.e--;
	}
	
	off(){
			turn(this.s,'off');
	}
	
	clr(){
			clearInterval(this.interval);
	}
	
}

var t = [];

var turn = function(soc,state){
	console.log('soc: ' + soc + ' state: ' + state);
	var code;
	if (soc == 1 && state == 'on') {code = '4474193'};
	if (soc == 1 && state == 'off'){code = '4474196'};
	if (soc == 2 && state == 'on') {code = '4477265'};
	if (soc == 2 && state == 'off'){code = '4477268'};
	if (soc == 3 && state == 'on') {code = '4478033'};
	if (soc == 3 && state == 'off'){code = '4478036'};
	if (soc == 4 && state == 'on') {code = '4478225'};
	if (soc == 4 && state == 'off'){code = '4478228'};	
	
	if (state == 'on'){
		t[soc] = new Timer(timeEnd, soc);
	}
	
	if (state == 'off'){
		if (t[soc] != undefined){
			t[soc].clr()
			blynk.notify('Socket ' + soc + ' turn off');
			};
		t[soc] = null;

	}
	
	if (click == false){
		send(code);
		if (state == 'on') led[soc].setValue(255);
		if (state == 'off') led[soc].setValue(0);
		led[5].setValue(255);
		led_g.writeSync(1);
		console.log('SOCKET ' + soc + ' ' + state);
	}
	click = true;
}

var send = function(code){
		var counter = 0;
		var interval = setInterval(function(){
		cmd = '/home/pi/Desktop/libaries/433Utils/RPi_utils/codesend ' + code;
		exec(cmd, function(error, stdout, stderr) {
		  // command output is in stdout
		});
		
		counter++;
		if(counter === 2) {
			clearInterval(interval);
			led[5].setValue(0);
			led_g.writeSync(0);
			click = false;
		}
	}, 500);
}

blynk.on('connect', function() {
  console.log("Blynk ready.");
  blynk.syncAll();
});