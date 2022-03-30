// #define BLYNK_TEMPLATE_ID "TMPL-Tm7gNpK"
#define BLYNK_DEVICE_NAME "esp8266"

#include <Arduino.h>
#include <Ultrasonic.h>
#include <BlynkSimpleEsp8266.h>
#include <TimeLib.h>
#include <WidgetRTC.h>

#include <C:/auth/auth.h>
#include <C:/auth/blynkToken.h>

WidgetRTC rtc;

char auth[] = Token_Bano;
char ssid[] = AuthSsid;
char pass[] = AuthPass;

Ultrasonic ultrasonic(0, 4);
BlynkTimer timer;

int lampState = 0;
bool pause = false;

int lampMax = 1023;
int lampVal = 0;
int direction = 1;
int distance = 0;
bool on = false;
bool off = false;

void fade()
{
  int ratio = lampMax / lampMax;

  lampVal = lampVal + ratio * direction;

  lampVal < 0 ? lampVal = 0 : 0;
  lampVal > lampMax ? lampVal = lampMax : 0;

  analogWrite(2, lampVal);
  analogWrite(16, lampVal);
}

void labelUpdate(int distanceRead)
{
  Blynk.virtualWrite(V0, distanceRead);
}

void pauseTimeOut()
{
  pause = false;
}

int distanceRead;

void distanceUpdate()
{
  int distanceRead = ultrasonic.read();

  if (distanceRead <= distance && !on) {
    on = true;
    off = false;
    labelUpdate(distanceRead);
    lampState = 1;
  }

  if (distanceRead > distance && !off) {
    off = true;
    on = false;
    labelUpdate(distanceRead);
    lampState = 0;
  }

  // if (distanceRead <= distance && !pause)
  // {
  //   lampState = -lampState + 1;
  //   pause = true;
  //   timer.setTimeout(1500L, pauseTimeOut);
  //   labelUpdate(distanceRead);
  // }

  lampState ? direction = 1 : direction = -1;
}

BLYNK_CONNECTED()
{
  rtc.begin();
  Blynk.syncAll();
}

BLYNK_WRITE(V1)
{
  distance = param.asInt();
  labelUpdate(distance);
}

BLYNK_WRITE(V2)
{
  lampMax = param.asInt();
}

void setup()
{
  Serial.begin(9600);
  Blynk.begin(auth, ssid, pass);

  pinMode(2, OUTPUT);
  pinMode(16, OUTPUT);
  digitalWrite(2, lampState);
  digitalWrite(16, lampState);

  timer.setInterval(100L, distanceUpdate);
  timer.setInterval(1L, fade);
}

void loop()
{
  Blynk.run();
  timer.run();
}