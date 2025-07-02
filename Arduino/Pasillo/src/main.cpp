#include <Arduino.h>
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

int powerPin = 4;

int manual = 0;
int brightness = 0;

FirebaseService firebaseService;
WiFiService wifiService;
TimeService timeService;
Oled oled;

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
    Serial.println("timer1: " + String(timer1.hour));
  }

  if (key == "timer2") {
    timer2.hour = std::stoi(value.c_str());
    timer2.minute = 0;
    timer2.second = 0;
    setTimePoint(timer2, timeNow);
    Serial.println("timer2: " + String(timer2.hour));
  }

  if (key == "manual") {
    manual = std::stoi(value.c_str());
  }

  if (key == "brightness") {
    brightness = std::stoi(value.c_str());
    if (brightness < 0) brightness = 0;
    if (brightness > 255) brightness = 255;
    Serial.println("brightness: " + String(brightness));
  }
}

bool trigger(Timer timeNow, Timer &timer1, Timer &timer2) {
  if (timer1.hour <= timer2.hour) {
    // Przedział nie przechodzi przez północ
    return (timeNow.hour >= timer1.hour && timeNow.hour < timer2.hour);
  } else {
    // Przedział przechodzi przez północ
    return (timeNow.hour >= timer1.hour || timeNow.hour < timer2.hour);
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

  wifiService.connect();
  
  firebaseService.connectFirebase();
  firebaseService.setCallback([](String key, String value) {
    callback(key, value);
  });
  firebaseService.setTimestamp();

  startTime = millis();

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(powerPin, OUTPUT);
}

void loop() {
  firebaseService.firebaseStream();
  
  unsigned long elapsed = millis() - startTime;
  timeNow.timePoint = timeSnapPoint + seconds(elapsed / 1000);
  timeNow = timeService.timerFromTimePoint(timeNow.timePoint);

  String formattedTime = String(timeNow.hour) + ":" + String(timeNow.minute) + ":" + String(timeNow.second);
  String formattedTimeAndDate = String(timeNow.day) + "/" + String(timeNow.month) + "/" + String(timeNow.year) + " " + formattedTime;

  if (timeNow.second != t_timeNow.second) {
    t_timeNow.second = timeNow.second;

    setTimePoint(timer1, timeNow);
    setTimePoint(timer2, timeNow);

    oled.stack[0].text = formattedTime;
    oled.stack[3].text = String(timer1.hour);
    oled.stack[4].text = String(timer2.hour);
    oled.print();

    if (trigger(timeNow, timer1, timer2) || manual == 1) {
      analogWrite(powerPin, brightness);
      analogWrite(LED_BUILTIN, 255 - brightness);
    }
    else {
      analogWrite(powerPin, LOW);
      analogWrite(LED_BUILTIN, 255);
    }
  }
}