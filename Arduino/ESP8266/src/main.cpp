#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <C:/auth/auth.h>
#include <C:/auth/blynkToken.h>

char auth[] = Token_WaterTank;

char ssid[] = AuthSsid;
char pass[] = AuthPass;

void OTAsetup(){
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(5000);
    ESP.restart();
  }

  // ArduinoOTA.setHostname("ArduinoOta");
  // ArduinoOTA.setPassword("admin");

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {
      type = "filesystem";
    }
  });
}

void setup()
{
  OTAsetup();

  ArduinoOTA.begin();
  Blynk.begin(auth, ssid, pass);

  Blynk.virtualWrite(V0, WiFi.localIP().toString());
}

BLYNK_WRITE(V0)
{
  int pinValue = param.asInt();
  if (pinValue) digitalWrite(2, HIGH);
  else digitalWrite(2, LOW);
}

void loop()
{
  Blynk.run();
  double val = analogRead(A0);
  Blynk.virtualWrite(V1, val);
}