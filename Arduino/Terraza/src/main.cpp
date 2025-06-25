
#include <Arduino.h>
#include <WiFiService.h>
#include <FirebaseService.h>
#include <TimeService.h>
#include <chrono>
#include <time.h>
#include <Oled.h>

int startTime = 0;

bool updateLog = false;
bool updateTime = false;

Timer timeNow;
Timer t_timeNow;
Timer timer1;
Timer timer2;
Timer manual;

auto timer1TimePoint = system_clock::now();
auto timer2TimePoint = system_clock::now();
auto intervalTimePoint = system_clock::now();

Oled oled;

int powerPin = 4;

int interval = 0;

WiFiService wifiService;
FirebaseService firebaseService;
TimeService timeService;

system_clock::time_point timeSnapPoint;

void setTimePoint(Timer &timer, Timer &timeNow) {
  timer.timePoint = timeNow.timePoint;

  time_t currentTime = system_clock::to_time_t(timer.timePoint);
  struct tm *timeInfo = localtime(&currentTime);
  timeInfo->tm_hour = timer.hour;
  timeInfo->tm_min = timer.minute;
  timeInfo->tm_sec = timer.second;
  timer.timePoint = system_clock::from_time_t(mktime(timeInfo));
}

void callback(String key, String value) {
  if (key == "time") {
    Serial.println(std::stoll(value.c_str()));
    timeSnapPoint = timeService.snapTimePoint(std::stoll(value.c_str()));
  }

  if (key == "timer1") {
    timer1.hour = std::stoi(value.c_str());
    timer1.minute = 0;
    timer1.second = 0;
    setTimePoint(timer1, timeNow);
    Serial.println("timer1");
  }

  if (key == "timer2") {
    timer2.hour = std::stoi(value.c_str());
    timer2.minute = 0;
    timer2.second = 0;
    setTimePoint(timer2, timeNow);
    Serial.println("timer2");
  }

  if (key == "timer1-enable") {
    if (std::stoi(value.c_str()) == 1) {
      timer1.enabled = true;
      setTimePoint(timer1, timeNow);
    }
    else {
      timer1.enabled = false;
    }
    Serial.println("timer1-enable");
  }

  if (key == "timer2-enable") {
    if (std::stoi(value.c_str()) == 1) {
      timer2.enabled = true;
      setTimePoint(timer2, timeNow);
    }
    else {
      timer2.enabled = false;
    }
    Serial.println("timer2-enable");
  }

  if (key == "interval") {
    interval = std::stoi(value.c_str());
    Serial.println(interval);
  }

  if (key == "manual") {
    if (std::stoi(value.c_str()) == 1) {
      manual.enabled = true;
    }
    else {
      manual.enabled = false;
    }
    manual.hour = timeNow.hour;
    manual.minute = timeNow.minute;
    manual.second = timeNow.second;
    setTimePoint(manual, timeNow);
    Serial.println("manual");
  }
}

void trigger(Timer timeNow, Timer &timer, int interval) {
    if (timeNow.timePoint >= timer.timePoint && timeNow.timePoint < timer.timePoint + minutes(interval) && timer.enabled){
      timer.active = true;
    }
    else {
      timer.active = false;
    }
}

void setup() {
  Serial.begin(9600);

  oled.init();
  oled.add(0, 0, "time", 2);
  oled.add(0, 25, "timer1", 1);
  oled.add(60, 25, "timer2", 1);
  oled.add(0, 40, "0", 2);
  oled.add(60, 40, "0", 2);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(powerPin, OUTPUT);

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
  timeNow.timePoint = timeSnapPoint + seconds(millis() / 1000 - startTime / 1000);
  timeNow = timeService.timerFromTimePoint(timeNow.timePoint);

  String formattedTime = String(timeNow.hour) + ":" + String(timeNow.minute) + ":" + String(timeNow.second);
  String formattedTimeAndDate = String(timeNow.day) + "/" + String(timeNow.month) + "/" + String(timeNow.year) + " " + formattedTime;



  if (timeNow.second != t_timeNow.second) {

    t_timeNow.second = timeNow.second;

    setTimePoint(timer1, timeNow);
    setTimePoint(timer2, timeNow);
    setTimePoint(manual, timeNow);

    trigger(timeNow, timer1, interval);
    trigger(timeNow, timer2, interval);
    trigger(timeNow, manual, interval);

    oled.stack[0].text = formattedTime;
    oled.stack[3].text = timer1.active ? "1" : "0";
    oled.stack[4].text = timer2.active ? "1" : "0";
    oled.print();

    if (timer1.active || timer2.active || manual.active) {
      digitalWrite(powerPin, LOW);
      digitalWrite(LED_BUILTIN, HIGH);
      updateLog = true;
    }
    else {
      digitalWrite(powerPin, HIGH);
      digitalWrite(LED_BUILTIN, LOW);

      if (manual.enabled) {
        manual.enabled = false;
        firebaseService.setPin("manual", 0);
      }

      if (updateLog) {
        updateLog = false;
        firebaseService.setPinString("log", std::string("Ostanie podlewanie: ") + formattedTimeAndDate.c_str());
      }
    }
  }
}