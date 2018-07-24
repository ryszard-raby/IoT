var Blynk = require('blynk-library');
var AUTH = 'xxx';
var blynk = new Blynk.Blynk(AUTH);

var v0 = new blynk.VirtualPin(0);
var v1 = new blynk.VirtualPin(1);
var v2 = new blynk.VirtualPin(2);
var v3 = new blynk.VirtualPin(3);
var v4 = new blynk.VirtualPin(4);
var v5 = new blynk.VirtualPin(5);
var v6 = new blynk.VirtualPin(6);
var v7 = new blynk.VirtualPin(7);
var v8 = new blynk.VirtualPin(8);

v4.write('interval:');
var i=0;
setInterval(function(){
	var val = Math.cos(i/10)*1000;
	v0.write(val);

	var x = v7.read;
	
	i++;
}, 1000)

v7.on('write',(function(param){
		console.log(param);
		v3.write(param);
}))



var j=0;
setInterval(function(){
	var val = Math.cos(j/10)*1000;
	v1.write(val);
	
	v2.write(val);
	v6.write(val);
	j++;
}, 60000)

blynk.on('connect', function() {
  console.log("Blynk ready.");
});
/*
v0.on('read',function(){
	var val=new Date().getSeconds()*17;
	v0.write(val); 
	v2.write(val);
})

v3.on('read',function(){
	var val = new Date().getSeconds();
	v3.write(val);
})
 
var i=0;
v0.on('read',function(){
	i++;
	val = Math.cos(i)*1000;
	v1.write(val);
	v0.write(val);
})
*/

