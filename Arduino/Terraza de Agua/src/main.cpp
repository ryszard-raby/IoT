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

#include <C:/Users/rysza/OneDrive/auth/auth.h>

uint8_t builtInLed = 2;

long long int serverTime = 0;
unsigned long liveTime = 0;
long long int currentTimeMillis = 0;

FirebaseService firebaseService;
DateTime currentTime = DateTime(0, 0, 0, 0, 0, 0);

int interval = 0;

const int timersCount = 3;
Timer timer[timersCount];
TimerObserver timerObserver;

bool synchro = false;

const int pinsCount = 1;
int pin[pinsCount] = {5};

int activeTimersCount_temp = 0;

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

DateTime fullDate(long long int millis) {
  time_t t = millis / 1000;
  struct tm *tm = localtime(&t);
  tm->tm_hour += 2;
  time_t t2 = mktime(tm);
  tm = localtime(&t2);

  return DateTime(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
}

void printTimersValues(int i) {
  Serial.println("timer" + String(i) + ": " + String(timer[i].hour_start) + ":" + String(timer[i].min_start) + " - " + String(timer[i].hour_end) + ":" + String(timer[i].min_end));
}

void getData(String key, String value) {
  if (key == "timer1") {
    timer[0].hour_start = std::stoi(value.c_str());
    timer[0].hour_end = std::stoi(value.c_str());
    timer[0].min_start = 0;
    timer[0].min_end = interval;
    printTimersValues(0);
  }

  if (key == "timer2") {
    timer[1].hour_start = std::stoi(value.c_str());
    timer[1].hour_end = std::stoi(value.c_str());
    timer[1].min_start = 0;
    timer[1].min_end = interval;
    printTimersValues(1);
  }

  if (key == "custom") {
    if (std::stoi(value.c_str())) {
      timer[2].name = "custom";
      timer[2].hour_start = currentTime.hour;
      timer[2].hour_end = currentTime.hour;
      timer[2].min_start = currentTime.minute;
      timer[2].min_end = currentTime.minute + interval;
      timer[2].active = true;
      if (timer[2].min_end >= 60) {
        timer[2].hour_end = currentTime.hour + 1;
        timer[2].min_end = timer[2].min_end - 60;
      }
      }
    else {
      timer[2].active = false;
    }
    printTimersValues(2);
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

    timer[0].min_end = interval;
    timer[1].min_end = interval;

    timer[2].hour_start = currentTime.hour;
    timer[2].hour_end = currentTime.hour;
    timer[2].min_start = currentTime.minute;
    timer[2].min_end = currentTime.minute + interval;

    if (timer[2].min_end >= 60) {
      timer[2].hour_end = currentTime.hour + 1;
      timer[2].min_end -= 60;
    }

    for (int i = 0; i < timersCount; i++) {
      printTimersValues(i);
    }
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

void itsTime() {
  if (timerObserver.activeTimersCount > 0) {
    setPinValue(255);
    
    if (activeTimersCount_temp != timerObserver.activeTimersCount) {
      String log = String(currentTime.hour) + ":" + String(currentTime.minute) + ":" + String(currentTime.second) + " | " + String(currentTime.day) + "." + String(currentTime.month) + "." + String(currentTime.year) + " | " + String(interval) + " min" + " | " + String(timerObserver.activeTimersCount) + " timer";
      firebaseService.setPinString("log", log.c_str());
    }

    activeTimersCount_temp = timerObserver.activeTimersCount;
  }
  else {
    setPinValue(0);
    activeTimersCount_temp = timerObserver.activeTimersCount;
  }
}

void checkTimer(int i) {
  if (timer[i].active) {
    if ((timer[i].hour_start <= currentTime.hour && timer[i].min_start <= currentTime.minute)
    && (timer[i].hour_end >= currentTime.hour && timer[i].min_end > currentTime.minute))
    {
      timerObserver.count(true);
    }
    else if (timer[i].hour_end <= currentTime.hour && timer[i].min_end <= currentTime.minute) {
      timerObserver.count(false);

      if (timer[i].name == "custom") {
        timer[i].active = false;
        firebaseService.setPin("custom", 0);
        firebaseService.setTimestamp();
        liveTime = millis();
      }
    }
  }
}

int seconds_temp = 0;

void loop() {
  firebaseService.firebaseStream();
  currentTime = fullDate(serverTime + millis() - liveTime);
  currentTimeMillis = serverTime + millis() - liveTime;

  if (millis() - liveTime > 600000) {
    firebaseService.setTimestamp();
    liveTime = millis();
    Serial.println(String(currentTime.hour) + ":" + String(currentTime.minute) + ":" + String(currentTime.second));
  }

  // check all timers
  timerObserver.activeTimersCount = 0;
  for (int i = 0; i < timersCount; i++) {
    checkTimer(i);
  }

  itsTime();

  // if (currentTime.second != seconds_temp) {
  //   seconds_temp = currentTime.second;
  //   Serial.println(String(currentTime.hour) + ":" + String(currentTime.minute) + ":" + String(currentTime.second));
  // }
}

