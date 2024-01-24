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

#include <C:/Users/ryszard.raby/OneDrive/auth/auth.h>

uint8_t builtInLed = 2;

long long int serverTime = 0;
unsigned long liveTime = 0;

FirebaseService firebaseService;
DateTime currentTime = DateTime(0, 0, 0, 0, 0, 0);

int morning_start, evening_start, morning_power, evening_power;
int secondTemp = 0;

const int timersCount = 2;
Timer timer[timersCount];

const int pinsCount = 3;
int pin[pinsCount] = ;

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

void getData(String key, String value) {
  if (key == "morning") {
    timer[0].hour = std::stoi(value.c_str());
    timer[0].done = false;
  }
  if (key == "evening") {
    timer[1].hour = std::stoi(value.c_str());
    timer[1].done = false;
  }
  if (key == "morning-power") {
    timer[0].value = std::stoi(value.c_str());
    timer[0].done = false;
  }
  if (key == "evening-power") {
    timer[1].value = std::stoi(value.c_str());
    timer[1].done = false;
  }
  if (key == "gpio2") {
    analogWrite(builtInLed, 255 - value.toInt());
  }

  Serial.println(key + " : " + value);
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

void setPinValue(int value) {
  for (int i = 0; i < pinsCount; i++) {
    analogWrite(pin[i], value);
  }
}

void loop() {
  firebaseService.firebaseStream();
  currentTime = fullDate(serverTime + millis() - liveTime);

  if (millis() - liveTime > 600000) {
    firebaseService.setTimestamp();
    liveTime = millis();
    Serial.println(String(currentTime.hour) + ":" + String(currentTime.minute) + ":" + String(currentTime.second));
  }

  for(int i = 0; i < timersCount; i++) {
    if (currentTime.hour >= timer[i].hour) {
      if (!timer[i].done) {
        setPinValue(timer[i].value);
      }
      timer[i].done = true;
    }
  }
}