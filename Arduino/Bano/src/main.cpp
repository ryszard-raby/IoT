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

class Config {
  public:
    int distance_min;
    int distance_max;
    int light;
    int brightness;
    int brightness_max;
    int brightness_min;
    int direction;
    long refresh_time;
    bool state_temp;
    bool pause;
};

Config config;

void pauseTimeOut()
{
  config.pause = false;
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

  if (distance < config.distance_min && !config.state_temp && !config.pause) {config.state_temp = true; config.light += 1; config.pause = true;}
  if (distance > config.distance_max && config.state_temp && !config.pause) {config.state_temp = false; config.light += 1; config.pause = true;}

  config.pause ? timer.setTimeout(500L, pauseTimeOut) : 0;

  config.direction = config.light % 4 ? -1 : 1;
 
  Blynk.virtualWrite(V0, distance);
}

BLYNK_CONNECTED()
{
  rtc.begin();
  Blynk.syncAll();
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

BLYNK_WRITE(V5)
{
  config.refresh_time = param.asLong();
}

void setup()
{
  pinMode(2, OUTPUT);
  pinMode(16, OUTPUT);

  config.distance_min = 60;
  config.distance_max = 80;
  config.brightness_max = 1023;
  config.brightness_min = 0;

  Serial.begin(9600);
  Blynk.begin(auth, ssid, pass);

  timer.setInterval(350L, distanceUpdate);
  timer.setInterval(1L, fade);
}

void loop()
{
  Blynk.run();
  timer.run();
}