var exec = require('child_process').exec;
var Blynk = require('blynk-library');
var AUTH = 'xxx';
var blynk = new Blynk.Blynk(AUTH);

var s=[];
	s[0] = 12;
	s[1] = 18;

var Gpio = require('pigpio').Gpio,
	s1 = new Gpio(s[0], {mode: Gpio.PWM, pullUpDown: Gpio.PUD_DOWN}),
	s2 = new Gpio(s[1], {mode: Gpio.PWM, pullUpDown: Gpio.PUD_DOWN,});

	//Gpio(12, Gpio.PUD_DOWN);
	//Gpio(18, Gpio.PUD_DOWN);
	
var v0 = new blynk.VirtualPin(0);
var v1 = new blynk.VirtualPin(1);
var v2 = new blynk.VirtualPin(2);

var width = 0;
var height = 0;

var pulseWidth = 1000;
var pulseWidth_ = 0;
var pulseHeight = 1000;
var pulseHeight_ = 0;

var state = 0;
var i = 0;

var off = function(pin){
	s1.digitalWrite(0);
	s2.digitalWrite(0);
}

var i = 0;
var intervaly = 0;
var intervalx = 0;

var start = setInterval(function () {
  s1.servoWrite(pulseHeight);
}, 20);

v1.on('write',function(param){
	width = parseInt(param[0]-10);
	
	if (width == 0 && pulseWidth != 0 ) {pulseWidth_= pulseWidth; pulseWidth = 0;}
    else if(width != 0 && pulseWidth == 0){pulseWidth = pulseWidth_}
	
	pulseWidth -= width;
	
	if (pulseWidth > 2500) pulseWidth = 2500;
    if (pulseWidth < 500 && width != 0) pulseWidth = 500;
})

v2.on('write',function(param){
	height = parseInt(param[0]-10);
	
	if (height == 0 && pulseHeight != 0 ) {pulseHeight_= pulseHeight; pulseHeight = 0;}
    else if(height != 0 && pulseHeight == 0){pulseHeight = pulseHeight_} 
	
	pulseHeight-= height;  
	
	if (pulseHeight > 2500) pulseHeight = 2500;
    if (pulseHeight < 500 && height != 0) pulseHeight = 500;
})

blynk.on('connect', function() {
  console.log("Blynk ready.");
  blynk.syncAll();
});