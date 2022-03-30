#define BLYNK_DEVICE_NAME "esp8266"

#include <Arduino.h>
#include <Ultrasonic.h>
#include <BlynkSimpleEsp8266.h>
#include <TimeLib.h>
#include <WidgetRTC.h>

// #include <ESP8266WiFi.h>
// #include <WiFiUdp.h>
// #include <WiFiNINA.h>
// #include <ArduinoOTA.h>

#include <C:/auth/auth.h>
#include <C:/auth/blynkToken.h>

#include <Config.h>

WidgetRTC rtc;

char auth[] = Token_Bano;
char ssid[] = AuthSsid;
char pass[] = AuthPass;

Ultrasonic ultrasonic(0, 4);
BlynkTimer timer;

Config config;

BLYNK_CONNECTED()
{
  rtc.begin();
  Blynk.syncAll();
}

BLYNK_WRITE(V1)
{
  // param.asInt() ? distanceUpdate(config.distance_min - 1) : distanceUpdate(config.distance_max + 1);
}

BLYNK_WRITE(V2)
{
  config.brightness_max = param.asInt();
}

BLYNK_WRITE(V3)
{
  config.distance_min = param.asInt();
}

BLYNK_WRITE(V4)
{
  config.distance_max = param.asInt();
}

BLYNK_WRITE(V10)
{
  config.timer1 = param.asInt();
}

BLYNK_WRITE(V11)
{
  config.brightness1 = param.asInt();
}

BLYNK_WRITE(V12)
{
  config.timer2 = param.asInt();
}

BLYNK_WRITE(V13)
{
  config.brightness2 = param.asInt();
}

BLYNK_WRITE(V50) {
  config.storage1 = param.asInt();
}

BLYNK_WRITE(V51) {
  config.storage2 = param.asInt();
}

void fade() {

  int ratio = config.brightness_max / config.brightness_max;

  config.brightness = config.brightness + ratio * config.direction;

  config.brightness < config.brightness_min ? config.brightness = config.brightness_min : 0;
  config.brightness > config.brightness_max ? config.brightness = config.brightness_max : 0;

  analogWrite(2, config.brightness);
  analogWrite(16, config.brightness);
}

void distanceUpdate() {

  int distance = ultrasonic.read();

  Blynk.virtualWrite(V0, distance);

  if (distance < config.distance_min && !config.state_temp && config.pause == 5) {config.state_temp = true; config.light += 1; config.pause = 0;}
  if (distance > config.distance_max && config.state_temp && config.pause == 5) {config.state_temp = false; config.light += 1; config.pause = 0;}

  config.pause = config.pause + 1;
  config.pause >= 5 ? config.pause = 5 : 0;

  config.direction = config.light % 4 ? 1 : -1;
}

int getTimeNow() {
  return hour() * 3600 + minute() * 60 + second();
}

int getDayNow() {
  return day();
}

void setBrightness(int brightness, int storage, int slot) {
    config.brightness_max = 1024 * brightness / 100;
    Blynk.virtualWrite(V2, config.brightness_max);
    storage = getDayNow();
    Blynk.virtualWrite(slot, storage); // check slot! V50 -> 50
}

void timeCheck() { 
  int timeNow = getTimeNow();

  if (timeNow >= config.timer1 && config.storage1 != getDayNow()) {
    setBrightness(config.brightness1, config.storage1, 50);
  }
  if (timeNow >= config.timer2 && config.storage2 != getDayNow()) {
    setBrightness(config.brightness2, config.storage2, 51);
  }
}

void setup()
{
  // WiFi.begin(ssid, pass);
  // ArduinoOTA.begin(WiFi.localIP(), "Bano", pass, InternalStorage);

  pinMode(2, OUTPUT);
  pinMode(16, OUTPUT);

  // config.distance_min = 60;
  // config.distance_max = 80;
  config.brightness_max = 1023;
  config.brightness_min = 0;

  // Serial.begin(9600);
  Blynk.begin(auth, ssid, pass);

  timer.setInterval(350L, distanceUpdate);
  timer.setInterval(1L, fade);
  timer.setInterval(5000L, timeCheck);
}

void loop()
{
  // ArduinoOTA.poll();
  Blynk.run();
  timer.run();
}