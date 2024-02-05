
#include <Arduino.h>
#include <Ultrasonic.h>
#include <WiFiService.h>
#include <FirebaseService.h>
#include <TimeService.h>
#include <chrono>
#include <time.h>
#include <Oled.h>

int startTime = 0;

Timer timeNow;
Timer t_timeNow;
Timer timer1;
Timer timer2;

Oled oled;

int power1 = 0;
int power2 = 0;

int powerNew = 0;
int powerGlobal = 0;

int led_pin = 16;

Ultrasonic ultrasonic(0, 4); // (Trig PIN, Echo PIN)

WiFiService wifiService;
FirebaseService firebaseService;
TimeService timeService;

system_clock::time_point timeSnapPoint;

void callback(String key, String value) {
  if (key == "time") {
    Serial.println(std::stoll(value.c_str()));
    timeSnapPoint = timeService.setTimePoint(std::stoll(value.c_str()));
  }
  if (key == "timer1") {
    timer1.hour = timeNow.hour;
    timer1.minute = timeNow.minute;
    timer1.second = std::stoi(value.c_str());
    timer1.active = true;
  }
  if (key == "timer2") {
    timer2.hour = timeNow.hour;
    timer2.minute = timeNow.minute;
    timer2.second = std::stoi(value.c_str());
    timer2.active = true;
  }
  if (key == "power1") {
    power1 = std::stoi(value.c_str());
    timer1.active = true;
    oled.stack[1].text = String(value.c_str());
    oled.print();
  }
  if (key == "power2") {
    power2 = std::stoi(value.c_str());
    timer2.active = true;
    oled.stack[2].text = String(value.c_str());
    oled.print();
  }
}

void trigger(Timer timeNow, Timer &timer, int power) {
  if (timeNow.hour >= timer.hour && timeNow.minute >= timer.minute && timeNow.second >= timer.second && timer.active == true) {
    powerGlobal = power;
    timer.active = false;
  }
}

void reset(Timer timeNow, Timer &timer) {
  // if (timeNow.hour == 0 && timeNow.minute == 0 && timeNow.second == 0) {
  if (timeNow.second == 0) {
    timer.active = true;
  }
}

void increase(int &powerNew) {

  if (powerNew < powerGlobal) {
    powerNew++;
  }
  if (powerNew > powerGlobal) {
    powerNew--;
  }
  
  oled.stack[3].text = String(powerNew);
  if (powerNew != powerGlobal) {
    oled.print();
  }
}

void distance() {
  int distance = ultrasonic.read(40000UL);
  Serial.println(distance);
}

void setup() {
  Serial.begin(9600);

  oled.init();
  oled.add(0, 0, "time", 2);
  oled.add(0, 25, "power1", 1);
  oled.add(30, 25, "power2", 1);
  oled.add(0, 40, "increase", 2);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(led_pin, OUTPUT);

  wifiService.connect();
  
  firebaseService.connectFirebase();
  firebaseService.setCallback([](String key, String value) {
    callback(key, value);
  });
  firebaseService.setTimestamp();

  startTime = millis();
}

void loop() {
  firebaseService.firebaseStream();
  timeNow = timeService.getTimer(millis() - startTime);

  if (timeNow.second != t_timeNow.second) {
    // Serial.println(timeNow.second);

    String formattedTime = String(timeNow.hour) + ":" + String(timeNow.minute) + ":" + String(timeNow.second);

    oled.stack[0].text = formattedTime;
    oled.print();
    
    trigger(timeNow, timer1, power1);
    trigger(timeNow, timer2, power2);
    reset(timeNow, timer1);
    reset(timeNow, timer2);
    
    t_timeNow = timeNow;

    // distance();
  }

  increase(powerNew);
  analogWrite(led_pin, powerNew);
  
}