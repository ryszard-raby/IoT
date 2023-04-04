#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#include <Arduino.h>
#include <firebaseService.h>
#include <time.h>
#include <cstdlib>

#include <C:/auth.h>

unsigned long secTemp = 0;
unsigned long minTemp = 0;
int cout = 0;
uint8_t builtInLed = 2;

int intData;
int delayTime = 0;
long long int currentTime;
long long int serverTime;
bool synchronized = false;

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

String fullTime(int millis) {
  int seconds = (millis / 1000) % 60;
  int minutes = (millis / (1000 * 60)) % 60;
  int hours = (millis / (1000 * 60 * 60)) % 24;
  return String(hours) + ":" + String(minutes) + ":" + String(seconds) + ":" + String(millis);
}

String fullDate(long long int millis) {
  time_t t = millis / 1000;
  struct tm *tm = localtime(&t);
  tm->tm_hour += 2;
  time_t t2 = mktime(tm);
  tm = localtime(&t2);

  return String(tm->tm_year + 1900) + "-" + 
         String(tm->tm_mon + 1) + "-" + 
         String(tm->tm_mday) + " " + 
         String(tm->tm_hour) + ":" + 
         String(tm->tm_min) + ":" + 
         String(tm->tm_sec);
}

long long int delay(long long int millis) {
  time_t t = millis / 1000;
  struct tm *tm = localtime(&t);
  tm->tm_hour += 2;
  return ((60 - tm->tm_min % 60 - 1) * 60000) + ((60 - tm->tm_sec % 60 - 1 - 5) * 1000); // 5 seconds before full hour
}

void setup() {
  Serial.begin(9600);

  pinMode(builtInLed, OUTPUT);

  connectWiFi();
  firebaseService.connectFirebase();
  firebaseService.setCallback([](String key, String value) {
    // Serial.println("\nCallbackFunction \n Key: " + key + " Value: " + value + "\n");

    if (key == "time") {
      serverTime = std::stoll(value.c_str());
      currentTime = serverTime - millis();
      
      Serial.println("Server time: " + fullDate(serverTime));
      Serial.println("Delay: " + fullTime(delay(serverTime)));
    }

    if (key == "gpio2") {
      intData = std::stoi(value.c_str());
      digitalWrite(builtInLed, intData);
      Serial.println("Pin2: " + String(value));
    }
  });
  firebaseService.setTimestamp();
}

void loop() {
  firebaseService.firebaseStream();
  
  if (millis() - secTemp > 1000) {
    Serial.println(" Current time: " + fullDate(currentTime + millis()));
    secTemp = millis();
  }

  if (millis() - minTemp >= delay(serverTime)) {
    firebaseService.setTimestamp();
    minTemp = millis();
    Serial.println("Synchronized");
  }
}