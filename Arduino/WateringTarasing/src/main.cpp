#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <TimeLib.h>
#include <WidgetRTC.h>

#include <C:/auth/auth.h>
#include <C:/auth/blynkToken.h>

BlynkTimer timer;

WidgetRTC rtc;

char auth[] = Token_WateringTarasing;
char ssid[] = AuthSsid;
char pass[] = AuthPass;

int interval = 60 * 10;
int watering1 = 3600 * 7;
int watering2 = 3600 * 19;
bool setWatering = false;
bool canceled = false;
int start;
int stop;



void startWatering(){
  digitalWrite(2, 0);
  Blynk.virtualWrite(V0, 1);
}

void stopWatering(){
  digitalWrite(2, 1);
  Blynk.virtualWrite(V0, 0);
}

int getTimeNow() {
  return hour() * 3600 + minute() * 60 + second();
}

void printClock()
{
  String currentTime = String(hour()) + ":" + minute() + ":" + second();
  String currentDate = String(day()) + " " + month() + " " + year();
  Blynk.virtualWrite(V1, currentTime);
  // Blynk.setProperty(V1, 'title', currentDate);
}

void timeRefresh()
{
  int TimeNow = getTimeNow();
  if (TimeNow > watering1 && TimeNow < watering1 + interval) {
    if (!canceled) {
      start = watering1;
      stop = watering1 + interval;
    }
  }
  else if (TimeNow > watering2 && TimeNow < watering2 + interval) {
    if (!canceled) {
      start = watering2;
      stop = watering2 + interval;
    }
  }
  else {
    canceled = false;
  }

  if (!canceled && TimeNow >= start && TimeNow < stop) {
    startWatering();
  }
  else {
    stopWatering();
    setWatering = false;
    start = 0; 
    stop = 0;
  }

  printClock();
}

BLYNK_WRITE(V0){
  if (param.asInt()) {
    start = getTimeNow();
    stop = start + interval;
    canceled = false;
  }
  else {
    start = 0;
    stop = 0;
    canceled = true;
  }
  timeRefresh();
}

BLYNK_WRITE(V10){
  interval = param.asInt() / 60;
  timeRefresh();
}

BLYNK_WRITE(V11){
  watering1 = param.asInt();
  timeRefresh();
}

BLYNK_WRITE(V12){
  watering2 = param.asInt();
  timeRefresh();
}

BLYNK_CONNECTED() {
  rtc.begin();
  Blynk.syncAll();
}

void setup()
{
  // Debug console
  Serial.begin(9600);

  Blynk.begin(auth, ssid, pass);

  pinMode(2, OUTPUT);
  digitalWrite(2, 1);

  timer.setInterval(8000L, timeRefresh);
}

void loop()
{
  Blynk.run();
  timer.run();
}