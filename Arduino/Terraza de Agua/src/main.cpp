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

#include <C:/Users/ryszard.raby/OneDrive/auth/auth.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET    -1  // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C // Adres I2C wyświetlacza, możesz sprawdzić go za pomocą narzędzi diagnostycznych I2C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


using namespace std::chrono;

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
system_clock::time_point currentTimePoint;
system_clock::time_point setTimePoint(Timer timer, system_clock::time_point currentTimePoint);
void setPinValue(int value);

int interval = 1000;
int activeTimersCount_temp = 0;
int seconds_temp = 0;
bool state_temp = false;
bool refresh = true; 

void setCurrentTime(long long serverTime);

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

void oled() {

}

void getData(String key, String value) {
  using namespace std::chrono;

  if (key == "timer1") {
    timer[0].hour = std::stoll(value.c_str());
    timer[0].minute = 0;
    timer[0].timePoint = setTimePoint(timer[0], currentTimePoint);
    std::time_t timerTimePoint;
    timerTimePoint = system_clock::to_time_t(timer[0].timePoint);
    Serial.println("timepoint " + String(hour(timerTimePoint)));
  }

  if (key == "timer2") {
    timer[1].hour = std::stoll(value.c_str());
    timer[1].minute = 0;
    timer[1].timePoint = setTimePoint(timer[1], currentTimePoint);
    std::time_t timerTimePoint;
    timerTimePoint = system_clock::to_time_t(timer[1].timePoint);
    Serial.println("timepoint " + String(year(timerTimePoint)));
  }

  if (key == "custom") {
    time_t currentTime = system_clock::to_time_t(currentTimePoint);
    timer[2].hour = hour(currentTime);
    timer[2].minute = minute(currentTime);
    timer[2].timePoint = setTimePoint(timer[2], currentTimePoint);
    std::time_t timerTimePoint;
    timerTimePoint = system_clock::to_time_t(timer[2].timePoint);
    Serial.println("timepoint " + String(hour(timerTimePoint)));
    if (std::stoi(value.c_str())) {
      timer[2].active = true;
    }
    else {
      timer[2].active = false;
    }
  }
  
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
  if (key == "time") {
    serverTime = std::stoll(value.c_str());
    liveTime = millis();
  }
  getData(key, value);
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

  setPinValue(0);
}

// set value to all pins
void setPinValue(int value) {
  Serial.println("setPinValue: " + String(value));
  for (int i = 0; i < pinsCount; i++) {
    digitalWrite(pin[i], value);
  }
  digitalWrite(builtInLed, 255 - value);
}

// add UTC+1, summer time and server delay to current time
system_clock::time_point synchro(system_clock::time_point currentTimePoint) {
  
  time_t currentTime = system_clock::to_time_t(currentTimePoint);

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
system_clock::time_point getCurrentTime(long long serverTime) {

  system_clock::time_point now = system_clock::now();
  time_t time = system_clock::to_time_t(now);

  time += serverTime / 1000;
  time -= liveTime / 1000;

  auto lt = localtime(&time);
  system_clock::time_point timePoint = system_clock::from_time_t(mktime(lt));

  return timePoint;
}

// set time_point for timer
system_clock::time_point setTimePoint(Timer timer, system_clock::time_point currentTimePoint) {

  system_clock::time_point hourZero = currentTimePoint;
  time_t currentTime = system_clock::to_time_t(currentTimePoint);

  hourZero -= hours(hour(currentTime));
  hourZero -= minutes(minute(currentTime));
  hourZero -= seconds(second(currentTime));

  system_clock::time_point timePoint = hourZero + hours(timer.hour) + minutes(timer.minute);

  return timePoint;
}

system_clock::time_point setIntervalTimePoint(Timer timer) {
  system_clock::time_point intervalTimePoint = timer.timePoint + minutes(interval);
  return intervalTimePoint;
}

bool itsTime(system_clock::time_point currentTimePoint, Timer timer) {

  system_clock::time_point intervalTimePoint = setIntervalTimePoint(timer);

  if (timer.active && currentTimePoint >= timer.timePoint && currentTimePoint < intervalTimePoint) {
    return true;
  }
  else {
    return false;
  }
}

void loop() {
  using namespace std::chrono;

  firebaseService.firebaseStream();

  currentTimePoint = getCurrentTime(serverTime);
  currentTimePoint = synchro(currentTimePoint);

  currentTime = system_clock::to_time_t(currentTimePoint);

  if (second(currentTime) != seconds_temp) {
    seconds_temp = second(currentTime);
    Serial.println(String(hour(currentTime)) + ":" + String(minute(currentTime)) + ":" + String(second(currentTime)) + " " + String(day(currentTime)) + "." + String(month(currentTime)) + "." + String(year(currentTime)));
  }

  if (millis() - millis_temp > 60000 * 60) {
    millis_temp = millis();
    firebaseService.setTimestamp();
  }

  if (millis() - millis_temp > 10000 && refresh) {
    firebaseService.setTimestamp();
    refresh = false;
  }

  bool state = itsTime(currentTimePoint, timer[0]) || itsTime(currentTimePoint, timer[1]) || itsTime(currentTimePoint, timer[2]);
  
  if (state != state_temp) {
    state_temp = state;
    setPinValue(state * 255);
  }
}