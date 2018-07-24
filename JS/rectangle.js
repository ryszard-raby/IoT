class Rectangle{
	constructor(height, width){
		this.height = height;
		this.width = width;
	}
	
	get area(){
		return this.calcArea();
	}
	
	calcArea(){
		return this.height * this.width;
	}
}

const square = new Rectangle(10,10);

console.log(square.area);

class Point{
	constructor(x, y){
		this.x = x;
		this.y = y;
	}
	
	static distance(a, b){
		const dx = a.x - b.x;
		const dy = a.y - b.y;
	
		return Math.hypot(dx, dy);
	}
}

const p1 = new Point(5,5);
const p2 = new Point(10,5);

console.log(Point.distance(p1,p2));



var timeEnd = function(){
	return 5;
}

var end = function(param){
	console.log('end ' + param);
}

var socket = 2;

class Timer{
	constructor(timeEnd, socket){
		this.s = socket;
		this.e = timeEnd;
		this.interval = setInterval(this.countdown.bind(this), 1000);
	}
	
	countdown(){
		console.log('countdown ' + this.s + ' - ' + this.e);
		if (this.e == 0){
			this.end();
		}
		this.e--;
	}
	
//	get stop(){
//		return this.end();
//	}
	
	end(){
			end(this.s);
			clearInterval(this.interval);
	}
}

var t = [];

for (i = 0; i<=5; i++){
	t[i]= new Timer(i, i);
}

for (i = 0; i<=5; i++){
	//t[i].end();
	t[i] = null;
}

//for (i = 0; i<=5; i++){
//	t[i]= new Timer(i, i);
//}

//const t2 = new Timer(5, 2);


