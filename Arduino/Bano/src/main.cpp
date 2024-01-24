#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#include <Arduino.h>
#include <FirebaseService.h>
#include <TimeService.h>
#include <chrono>
#include <time.h>
#include <C:/Users/ryszard.raby/OneDrive/auth/auth.h>

int liveTime = 0;
int startTime = 0;

FirebaseService firebaseService;
TimeService timeService;

system_clock::time_point timeSnapPoint;

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

void callback(String key, String value) {
  if (key == "time") {
    Serial.println(std::stoll(value.c_str()));
    timeSnapPoint = timeService.setTimePoint(std::stoll(value.c_str()));
  }
  if (key == "timer1") {
    time_t currentTime = system_clock::to_time_t(timeService.getTimeNow(liveTime));
    Serial.println(ctime(&currentTime));
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(LED_BUILTIN, OUTPUT);

  connectWiFi();
  
  firebaseService.connectFirebase();
  firebaseService.setCallback([](String key, String value) {
    callback(key, value);
  });
  firebaseService.setTimestamp();

  startTime = millis();
}

void loop() {
  firebaseService.firebaseStream();
  liveTime = millis() - startTime;
}