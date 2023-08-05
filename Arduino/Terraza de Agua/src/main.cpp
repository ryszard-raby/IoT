#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#include <Arduino.h>
#include <FirebaseService.h>
#include <DateTime.h>
#include <time.h>
#include <cstdlib>
#include <iostream>
#include <Timer.h>
#include <TimerObserver.h>
#include <chrono>
#include <TimeLib.h>
#include <ctime>

#include <C:/Users/rysza/OneDrive/auth/auth.h>

uint8_t builtInLed = 2;

FirebaseService firebaseService;

int millis_temp = 0;
long long serverTime = 0;
long long liveTime = 0;
std::time_t currentTime;

const int pinsCount = 1;
int pin[pinsCount] = {5};
const int timersCount = 3;
Timer timer[timersCount];
TimerObserver timerObserver;
std::chrono::system_clock::time_point currentTimePoint;

int interval = 1000;
int activeTimersCount_temp = 0;
int seconds_temp = 0;
bool state_temp = false;

void setCurrentTime(long long serverTime);
std::chrono::system_clock::time_point setTimePoint(Timer timer, auto currentTimePoint);

void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println(".......");
  Serial.println("Connected with IP:");
  Serial.println(WiFi.localIP());
  Serial.println();
}

void getData(String key, String value) {
  using namespace std::chrono;

  if (key == "timer1") {
    timer[0].hour = std::stoll(value.c_str());
    timer[0].minute = 0;
    timer[0].timePoint = setTimePoint(timer[0], currentTimePoint);
    std::time_t timerTimePoint;
    timerTimePoint = std::chrono::system_clock::to_time_t(timer[0].timePoint);
    Serial.println("Timer 1 timepoint " + String(hour(timerTimePoint)));
  }

  if (key == "timer2") {
    timer[1].hour = std::stoll(value.c_str());
    timer[1].minute = 0;
  }

  // if (key == "custom") {
  //   if (std::stoi(value.c_str())) {
  //     timer[2].name = "custom";
  //     timer[2].hour = currentTime->tm_hour;
  //     timer[2].minute = currentTime->tm_min;
  //     timer[2].active = true;
  //   }
  //   else {
  //     timer[2].active = false;
  //   }
  // }
  
  if (key == "timer1-activate") {
    if (std::stoi(value.c_str())) {
      timer[0].active = true;
    }
    else {
      timer[0].active = false;
    }
  }

  if (key == "timer2-activate") {
    if (std::stoi(value.c_str())) {
      timer[1].active = true;
    }
    else {
      timer[1].active = false;
    }
  }

  if (key == "interval") {
    interval = std::stoi(value.c_str());
  }

  if (key == "gpio2") {
    analogWrite(builtInLed, 255 - value.toInt());
  }

  // Serial.println(key + " : " + value);
}

void callback(String key, String value) {
  getData(key, value);
  if (key == "time") {
    serverTime = std::stoll(value.c_str());
    liveTime = millis();
    // setCurrentTime(serverTime);
    // Serial.println("Server time: " + String(serverTime));
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(builtInLed, OUTPUT);

  for (int i = 0; i < pinsCount; i++) {
    pinMode(pin[i], OUTPUT);
  }

  connectWiFi();
  firebaseService.connectFirebase();
  firebaseService.setCallback([](String key, String value) {
    callback(key, value);
  });

  firebaseService.setTimestamp();
  liveTime = millis();
}

// set value to all pins
void setPinValue(int value) {
  for (int i = 0; i < pinsCount; i++) {
    digitalWrite(pin[i], value);
  }
  digitalWrite(builtInLed, 255 - value);
}

// add UTC+1, summer time and server delay to current time
std::chrono::system_clock::time_point synchro(std::chrono::system_clock::time_point currentTimePoint) {
  using namespace std::chrono;

  auto currentTime = std::chrono::system_clock::to_time_t(currentTimePoint);

  // UTC+1
  currentTimePoint += hours(1);

  // spring time
  if (month(currentTime) > 3 && month(currentTime) < 10) {
    currentTimePoint += hours(1);
  }

  // server delay
  currentTimePoint += seconds(2);

  return currentTimePoint;
}

// set current time as time_poin from server time
std::chrono::system_clock::time_point getCurrentTime(long long serverTime) {
  using namespace std::chrono;

  auto now = system_clock::now();
  auto time = system_clock::to_time_t(now);

  time += serverTime / 1000;
  time -= liveTime / 1000;

  auto lt = localtime(&time);
  auto timePoint = system_clock::from_time_t(mktime(lt));

  return timePoint;
}

// set time point from timer to check if it is time to turn on/off. Example: timer[0].timePoint = 12:00, set timepoint to 12:00:00 today. currenTimePoint could be any hour of day
std::chrono::system_clock::time_point setTimePoint(Timer timer, auto currentTimePoint) {
  using namespace std::chrono;

  auto dayStart = currentTimePoint;

  // std::time_t cT = std::chrono::system_clock::to_time_t(currentTimePoint);
  
  // dayStart -= hours(cT);
  // dayStart -= minutes(cT);
  // dayStart -= seconds(cT);

  auto timePoint = dayStart + hours(timer.hour) + minutes(timer.minute);

  return timePoint;
}

void loop() {
  using namespace std::chrono;

  firebaseService.firebaseStream();

  currentTimePoint = getCurrentTime(serverTime);
  currentTimePoint = synchro(currentTimePoint);

  currentTime = std::chrono::system_clock::to_time_t(currentTimePoint);

  if (second(currentTime) != seconds_temp) {
    seconds_temp = second(currentTime);
    Serial.println(String(hour(currentTime)) + ":" + String(minute(currentTime)) + ":" + String(second(currentTime)) + " " + String(day(currentTime)) + "." + String(month(currentTime)) + "." + String(year(currentTime)));
  }

  // bool state = setTimePoint(serverTime, timer[0], interval) || setTimePoint(serverTime, timer[1], interval) || setTimePoint(serverTime, timer[2], interval);

  if (millis() - millis_temp > 60000 * 60) {
    millis_temp = millis();
    firebaseService.setTimestamp();
  }

  // if (state != state_temp) {
  //   state_temp = state;

  //   setPinValue(state * 255);

  //   Serial.println(state);
  // }
}