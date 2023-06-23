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

#include <C:/auth.h>

unsigned long secTemp = 0;
unsigned long minTemp = 0;
int cout = 0;
uint8_t builtInLed = 2;

int intData;
long long int serverTime;
unsigned long liveTime;

FirebaseService firebaseService;

int interval = 0;
int brightness = 0;


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

String fullTime(long long int millis) {
  int seconds = (millis / 1000) % 60;
  int minutes = (millis / (1000 * 60)) % 60;
  int hours = (millis / (1000 * 60 * 60)) % 24;
  return String(hours) + ":" + String(minutes) + ":" + String(seconds) + ":" + String(millis);
}

DateTime fullDate(long long int millis) {
  time_t t = millis / 1000;
  struct tm *tm = localtime(&t);
  tm->tm_hour += 2;
  time_t t2 = mktime(tm);
  tm = localtime(&t2);

  return DateTime(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
}

void callback(String key, String value) {
  Serial.println("\nCallbackFunction \n Key: " + key + " Value: " + value + "\n");

  if (key == "time") {
    serverTime = std::stoll(value.c_str());
  }

  if (key == "interval") {
    intData = std::stoi(value.c_str());
    interval = intData;
  }

  if (key == "gpio2") {
    intData = std::stoi(value.c_str());
    analogWrite(builtInLed, brightness);
  }

  if (key == "brightness") {
    intData = std::stoi(value.c_str());
    brightness = intData;
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(builtInLed, OUTPUT);

  connectWiFi();
  firebaseService.connectFirebase();
  firebaseService.setCallback([](String key, String value) {
    callback(key, value);
  });
  firebaseService.setTimestamp();

  liveTime = millis();
}

void loop() {
  firebaseService.firebaseStream();
  DateTime currentTime = fullDate(serverTime + millis() - liveTime);

  if (millis() - liveTime > 60000) {
    firebaseService.setTimestamp();
    liveTime = millis();
    Serial.println(String(currentTime.hour) + ":" + String(currentTime.minute) + ":" + String(currentTime.second));
  }
}