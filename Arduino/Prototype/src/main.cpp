#include <Arduino.h>
#include <WiFiService.h>
#include <FirebaseService.h>
#include <TimeService.h>
#include <Oled.h>
#include <OTAService.h>

// ---- Zmienne globalne ----
uint64_t startUs = 0;  // czas startu w µs (micros64 – 64-bit, brak overflow)
Timer timeNow;
system_clock::time_point timeSnapPoint;
bool ledState = false;  // stan LED z Firebase (gpio2)

// ---- Serwisy ----
WiFiService wifiService;
FirebaseService firebaseService;
TimeService timeService;
Oled oled;
OTAService otaService;

// ---- Callback Firebase ----
void onFirebaseData(String key, String value) {
  if (key == "time") {
    timeSnapPoint = timeService.snapTimePoint(std::stoll(value.c_str()));
    Serial.println("Time synced: " + value);
  }
  if (key == "gpio2") {
    ledState = (value.toInt() != 0);
    // ESP8266: LED_BUILTIN aktywny LOW
    digitalWrite(LED_BUILTIN, ledState ? LOW : HIGH);
    Serial.println("LED: " + String(ledState ? "ON" : "OFF"));
  }
}

// ---- Setup ----
void setup() {
  Serial.begin(9600);

  // OLED – przygotuj sloty: godzina (wiersz 0) i stan LED (wiersz 2)
  oled.init();
  oled.add(0, 0, "00:00:00", 2);
  oled.add(0, 40, "LED: OFF", 1);

  // WiFi
  wifiService.connect();

  // OTA
  otaService.init();

  // Firebase
  firebaseService.connectFirebase();
  firebaseService.setCallback([](String key, String value) {
    onFirebaseData(key, value);
  });
  firebaseService.setDeviceTime();
  firebaseService.setOnline(true);    // zgłoś obecność w bazie

  startUs = micros64();
  pinMode(LED_BUILTIN, OUTPUT);
}

// ---- Loop ----
void loop() {
  otaService.handle();
  firebaseService.firebaseStream();

  // Oblicz aktualny czas – micros64() to 64-bit µs, brak overflow
  int64_t elapsedUs = micros64() - startUs;
  timeNow = timeService.timerFromTimePoint(timeSnapPoint + microseconds(elapsedUs));

  // Formatuj czas
  char timeBuf[16];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d",
           timeNow.hour, timeNow.minute, timeNow.second);

  // Aktualizuj OLED: godzina + stan LED z Firebase
  oled.stack[0].text = String(timeBuf);
  oled.stack[1].text = ledState ? "LED: ON " : "LED: OFF";
  oled.print();

  // Heartbeat – aktualizuj timestamp co godzinę (dowód życia + sync czasu)
  static uint64_t lastHeartbeatUs = 0;
  if (micros64() - lastHeartbeatUs >= 3600000000ULL && firebaseService.isConnected()) {
    lastHeartbeatUs = micros64();
    firebaseService.setDeviceTime();
  }

  delay(100);
}