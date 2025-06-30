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

const int pinsCount = 3;
int pin[pinsCount] = {5, 4, 12};

int brightness1 = 1;
int brightness2 = 1;

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
    timeSnapPoint = timeService.snapTimePoint(std::stoll(value.c_str()));
  }
  
  if (key == "timer1") {
    timer1.hour = std::stoi(value.c_str());
    timer1.minute = 0;
    timer1.second = 0; 
    setTimePoint(timer1, timeNow);
    Serial.println("timer1: " + value);
  }

  if (key == "timer2") {
    timer2.hour = std::stoi(value.c_str());
    timer2.minute = 0;
    timer2.second = 0;
    setTimePoint(timer2, timeNow);
    Serial.println("timer2: " + value);
  }

  if (key == "brightness1") {
    brightness1 = std::stoi(value.c_str());
    if (brightness1< 0) brightness1 = 0;
    if (brightness1 > 255) brightness1 = 255;
    Serial.println("brightness1: " + value);
  }

  if (key == "brightness2") {
    brightness2 = std::stoi(value.c_str());
    if (brightness2 < 0) brightness2 = 0;
    if (brightness2 > 255) brightness2 = 255;
    Serial.println("brightness2: " + value);
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
  for (int i = 0; i < pinsCount; i++) {
    pinMode(pin[i], OUTPUT);
  }
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
  }

  if (trigger(timeNow, timer1, timer2)) {
    for (int i = 0; i < pinsCount; i++) {
      analogWrite(pin[i], brightness1); // Comment this line for debugging - pin (12) is used for oled screen
    }
    analogWrite(LED_BUILTIN, 255 - brightness1);
  }
  else {
    for (int i = 0; i < pinsCount; i++) {
      analogWrite(pin[i], brightness2); // Comment this line for debugging - pin (12) is used for oled screen
    }
    analogWrite(LED_BUILTIN, 255 - brightness2);
  }

}