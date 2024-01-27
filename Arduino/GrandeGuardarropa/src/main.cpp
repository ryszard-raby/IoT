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

int startTime = 0;

Timer timeNow;
Timer t_timeNow;
Timer timer1;
Timer timer2;

int power1 = 0;
int power2 = 0;

int powerNew = 0;
int powerGlobal = 0;

const int pinsCount = 3;
int pin[pinsCount] = {5, 4, 12};

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
    timeSnapPoint = timeService.setTimePoint(std::stoll(value.c_str()));
  }
  if (key == "timer1") {
    timer1.hour = std::stoi(value.c_str());
    timer1.minute = timeNow.minute;
    timer1.second = timeNow.second; 
    timer1.active = true;
    Serial.println("timer1: " + value);
  }
  if (key == "timer2") {
    timer2.hour = std::stoi(value.c_str());
    timer2.minute = timeNow.minute;
    timer2.second = timeNow.second;
    timer2.active = true;
    Serial.println("timer2: " + value);
  }
  if (key == "power1") {
    power1 = std::stoi(value.c_str());
    timer1.active = true;
    Serial.println("power1: " + value);
  }
  if (key == "power2") {
    power2 = std::stoi(value.c_str());
    timer2.active = true;
    Serial.println("power2: " + value);
  }
  if (key == "gpio2") {
    analogWrite(LED_BUILTIN,255 - std::stoi(value.c_str()));
    Serial.println("gpio2: " + value);
  }
}

void trigger(Timer timeNow, Timer &timer, int power) {
  if (timeNow.hour >= timer.hour && timeNow.minute >= timer.minute && timeNow.second >= timer.second && timer.active == true) {
    powerGlobal = power;
    timer.active = false;
  }
}

void reset(Timer timeNow, Timer &timer) {
  if (timeNow.hour == 0 && timeNow.minute == 0 && timeNow.second == 0) {
    timer.active = true;
  }
}

void increase(int &powerNew) {
  if (powerNew < powerGlobal) {
    powerNew++;
  }
  if (powerNew > powerGlobal) {
    powerNew--;
  }
}

void setup() {
  Serial.begin(9600);


  connectWiFi();
  
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
  timeNow = timeService.getTimer(millis() - startTime);

  if (timeNow.second != t_timeNow.second) {
    trigger(timeNow, timer1, power1);
    trigger(timeNow, timer2, power2);
    reset(timeNow, timer1);
    reset(timeNow, timer2);
    
    t_timeNow = timeNow;
  }

  increase(powerNew);

  for (int i = 0; i < pinsCount; i++) {
    analogWrite(pin[i], powerNew);
  }
}