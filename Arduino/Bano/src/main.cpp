#include <ESP8266WiFi.h>

#include <Arduino.h>
#include <ArduinoOTA.h>
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

int led_pin = 16;

FirebaseService firebaseService;
TimeService timeService;

system_clock::time_point timeSnapPoint;

void connectWiFi() {
  WiFi.mode(WIFI_STA);
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

void ota() {
  // ArduinoOTA.setHostname("bano");
  // ArduinoOTA.setPassword("admin");
  // ArduinoOTA.onStart([]() {
  //   Serial.println("Start");
  //   String type;
  //   if (ArduinoOTA.getCommand() == U_FLASH) {
  //     type = "sketch";
  //   } else { // U_SPIFFS
  //     type = "filesystem";
  //   }
  // });
  ArduinoOTA.begin();
}

void callback(String key, String value) {
  if (key == "time") {
    Serial.println(std::stoll(value.c_str()));
    timeSnapPoint = timeService.setTimePoint(std::stoll(value.c_str()));
  }
  if (key == "timer1") {
    timer1.hour = timeNow.hour;
    timer1.minute = timeNow.minute;
    timer1.second = std::stoi(value.c_str());
    timer1.active = true;
  }
  if (key == "timer2") {
    timer2.hour = timeNow.hour;
    timer2.minute = timeNow.minute;
    timer2.second = std::stoi(value.c_str());
    timer2.active = true;
  }
  if (key == "power1") {
    power1 = std::stoi(value.c_str());
    analogWrite(led_pin, 255 - power1);
  }
  if (key == "power2") {
    power2 = std::stoi(value.c_str());
  }
}

void trigger(Timer timeNow, Timer &timer, int power) {
  if (timeNow.hour >= timer.hour && timeNow.minute >= timer.minute && timeNow.second >= timer.second && timer.active == true) {
    powerGlobal = power;
    timer.active = false;
  }
}

void reset(Timer timeNow, Timer &timer) {
  // if (timeNow.hour == 0 && timeNow.minute == 0 && timeNow.second == 0) {
  if (timeNow.second == 0) {
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

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(led_pin, OUTPUT);

  connectWiFi();
  ota();
  
  firebaseService.connectFirebase();
  firebaseService.setCallback([](String key, String value) {
    callback(key, value);
  });
  firebaseService.setTimestamp();

  startTime = millis();

}

void loop() {
  ArduinoOTA.handle();


  firebaseService.firebaseStream();
  timeNow = timeService.getTimer(millis() - startTime);

  if (timeNow.second != t_timeNow.second) {
    Serial.println(timeNow.second);
    trigger(timeNow, timer1, power1);
    trigger(timeNow, timer2, power2);
    reset(timeNow, timer1);
    reset(timeNow, timer2);
    
    t_timeNow = timeNow;
  }

  increase(powerNew);
  analogWrite(LED_BUILTIN, powerNew);

}