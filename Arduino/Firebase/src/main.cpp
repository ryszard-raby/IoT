#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#include <Arduino.h>
#include <firebaseService.h>
#include <time.h>

#include <C:/auth.h>

unsigned long dataMillis = 0;
int cout = 0;
uint8_t builtInLed = 2;

int intData;

FirebaseService firebaseService;

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

void setup() {
  Serial.begin(9600);

  pinMode(builtInLed, OUTPUT);

  connectWiFi();
  firebaseService.connectFirebase();
  firebaseService.setCallback([](String key, String value) {
    Serial.println("\nCallbackFunction \n Key: " + key + " Value: " + value + "\n");
    intData = value.toInt();
    if (intData == 1) {
      digitalWrite(builtInLed, HIGH);
    } else {
      digitalWrite(builtInLed, LOW);
    }
  });
}

void loop() {
  firebaseService.firebaseStream();
}