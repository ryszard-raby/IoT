#define BLYNK_DEVICE_NAME "esp8266"

#include <Arduino.h>
#include <BlynkSimpleEsp8266.h>
#include <TimeLib.h>
#include <WidgetRTC.h>

#include <C:/auth/auth.h>
#include <C:/auth/blynkToken.h>

#include <Config.h>

WidgetRTC rtc;

char auth[] = Token_Guardarropa;
char ssid[] = AuthSsid;
char pass[] = AuthPass;

BlynkTimer timer;

Config config;

BLYNK_CONNECTED()
{
  rtc.begin();
  Blynk.syncAll();
}

void setPins(int value) {
  analogWrite(1, value);
  analogWrite(14, value);
  analogWrite(3, value);
  analogWrite(4, value);
  analogWrite(5, value);
}

int getTimeNow() {
  return hour() * 3600 + minute() * 60 + second();
}

int getDayNow() {
  return day();
}

void setBrightness(int brightness, int storage, int slot) {
    int value = 1024 * brightness / 100;
    Blynk.virtualWrite(V0, value);
    Blynk.virtualWrite(slot, storage);
    setPins(value);
}

void timeCheck() { 
  int timeNow = getTimeNow();

  if (timeNow >= config.timer1 && config.storage1 != getDayNow()) {
    config.storage1 = getDayNow();
    setBrightness(config.brightness1, config.storage1, 10);
  }
  if (timeNow >= config.timer2 && config.storage2 != getDayNow()) {
    config.storage2 = getDayNow();
    setBrightness(config.brightness2, config.storage2, 11);
  }
}

BLYNK_WRITE(V0)
{
  setPins(param.asInt());
}

BLYNK_WRITE(V1)
{
  config.timer1 = param.asInt();
}

BLYNK_WRITE(V2)
{
  config.brightness1 = param.asInt();
}

BLYNK_WRITE(V3)
{
  config.timer2 = param.asInt();
}

BLYNK_WRITE(V4)
{
  config.brightness2 = param.asInt();
}

BLYNK_WRITE(V10) {
  config.storage1 = param.asInt();
}

BLYNK_WRITE(V11) {
  config.storage2 = param.asInt();
}

void setup() {
  Blynk.begin(auth, ssid, pass);

  pinMode(1, OUTPUT);
  pinMode(14, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  
  pinMode(2, OUTPUT);

  analogWrite(2, 1023);

  timer.setInterval(5000L, timeCheck);
}

void loop()
{
  Blynk.run();
  timer.run();
}