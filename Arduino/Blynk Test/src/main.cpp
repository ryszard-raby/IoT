#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

#include <C:/auth/auth.h>
#include <C:/auth/blynkToken.h>

BlynkTimer timer;

char auth[] = Token_BlynkTest;
char ssid[] = AuthSsid;
char pass[] = AuthPass;

BLYNK_WRITE(V5)
{
  Blynk.virtualWrite(V0, param.asInt());
  Serial.print("Got a value: ");
  Serial.println(param.asStr());

  digitalWrite(2, 1 - param.asInt());
}

void setup()
{
  Serial.begin(115200);
  Blynk.begin(auth, ssid, pass);

  pinMode(2, OUTPUT);
  digitalWrite(2, 1);
}

void loop()
{
  Blynk.run();
}