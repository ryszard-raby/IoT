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

#include <C:/auth.h>

unsigned long secTemp = 0;
unsigned long minTemp = 0;
int cout = 0;
uint8_t builtInLed = 2;

int intData;
long long int currentTime;
long long int serverTime;

FirebaseService firebaseService;

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
    currentTime = serverTime - millis();
    DateTime dateTime = fullDate(currentTime + millis());

    Serial.println("Current time: " + String(dateTime.year) + "-" + String(dateTime.month) + "-" + String(dateTime.day) + " " + String(dateTime.hour) + ":" + String(dateTime.minute) + ":" + String(dateTime.second));
  }

  if (key == "gpio2") {
    intData = std::stoi(value.c_str());
    digitalWrite(builtInLed, intData);
    Serial.println("Pin2: " + String(value));
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
}

void loop() {
  firebaseService.firebaseStream();
  
  if (millis() - secTemp > 60000) {
    firebaseService.setTimestamp();
    secTemp = millis();
    // Serial.println("Live time: " + fullDate(millis()));
  }
}