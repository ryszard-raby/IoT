#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

#include <C:/auth/auth.h>
#include <C:/auth/blynkToken.h>

BlynkTimer timer;

char auth[] = Token_WaterTank;

char ssid[] = AuthSsid;
char pass[] = AuthPass;

void OTAsetup(){
  Serial.begin(115200);
  Blynk.begin(auth, ssid, pass);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  digitalWrite(5, param.asInt());
}

void setup()
{
  Serial.begin(115200);
  Blynk.begin(auth, ssid, pass);

  pinMode(5, OUTPUT);
  digitalWrite(2, 1);
  digitalWrite(5, 0);
}

void loop()
{
  ArduinoOTA.handle();
  Blynk.run();
  timer.run();
}