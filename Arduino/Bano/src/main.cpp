
#include <Arduino.h>
#include <Ultrasonic.h>
#include <WiFiService.h>
#include <FirebaseService.h>
#include <TimeService.h>
#include <chrono>
#include <time.h>
#include <Oled.h>
#include <OTAService.h>

int startTime = 0;

Timer timeNow;
Timer timer1;
Timer timer2;

int led_pin = 16;

int brightness1 = 0;
int brightness2 = 0;

int state, _state = 0;

int threshold = 0;
unsigned long interval = 1000;

int _measure = 0;

int distance = 0;

Ultrasonic ultrasonic(0, 4); // (Trig PIN, Echo PIN)

WiFiService wifiService;
FirebaseService firebaseService;
TimeService timeService;
Oled oled;
OTAService otaService;

system_clock::time_point timeSnapPoint;

void setTimePoint(Timer &timer, Timer &timeNow) {
  timer.timePoint = timeNow.timePoint;

  time_t currentTime = system_clock::to_time_t(timer.timePoint);
  struct tm *timeInfo = localtime(&currentTime);
  timeInfo->tm_hour = timer.hour;
  timeInfo->tm_min = timer.minute;
  timeInfo->tm_sec = timer.second;
  timer.timePoint = system_clock::from_time_t(mktime(timeInfo));
}

void callback(String key, String value) {
  if (key == "time") {
    Serial.println(std::stoll(value.c_str()));
    timeSnapPoint = timeService.snapTimePoint(std::stoll(value.c_str()));
  }

  if (key == "timer1") {
    timer1.hour = std::stoi(value.c_str());
    timer1.minute = 0;
    timer1.second = 0; 
    setTimePoint(timer1, timeNow);
    Serial.println("timer1: " + value);
  }

  if (key == "timer2") {
    timer2.hour = std::stoi(value.c_str());
    timer2.minute = 0;
    timer2.second = 0;
    setTimePoint(timer2, timeNow);
    Serial.println("timer2: " + value);
  }

  if (key == "brightness1") {
    brightness1 = std::stoi(value.c_str());
    if (brightness1< 0) brightness1 = 0;
    if (brightness1 > 255) brightness1 = 255;
    Serial.println("brightness1: " + value);
  }

  if (key == "brightness2") {
    brightness2 = std::stoi(value.c_str());
    if (brightness2 < 0) brightness2 = 0;
    if (brightness2 > 255) brightness2 = 255;
    Serial.println("brightness2: " + value);
  }

  if (key == "threshold") {
    threshold = std::stoi(value.c_str());
    Serial.println("threshold: " + value);
  }

  if (key == "interval") {
    interval = std::stoi(value.c_str());
    Serial.println("interval: " + value);
  }

  if (key == "distance") {
    distance = std::stoi(value.c_str());
    Serial.println("trigger: " + value);
  }
}


bool trigger(Timer timeNow, Timer &timer1, Timer &timer2) {
  if (timer1.hour <= timer2.hour) {
    // Przedział nie przechodzi przez północ
    return (timeNow.hour >= timer1.hour && timeNow.hour < timer2.hour);
  } else {
    // Przedział przechodzi przez północ
    return (timeNow.hour >= timer1.hour || timeNow.hour < timer2.hour);
  }
}

bool switcher() {
  static int increaseState = 0;
  distance = ultrasonic.read(40000UL);

  distance < threshold ? state = 1 : state = 0;

  if (state != _state) {
    increaseState++;
    _state = state;
  }

  oled.stack[1].text = String(increaseState % 4);

  return increaseState % 4 != 0;
}

void setup() {
  Serial.begin(9600);

  oled.init();
  oled.add(0, 0, "time", 2);
  oled.add(0, 40, "i", 2);
  oled.add(60, 40, "is", 2);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(led_pin, OUTPUT);
  analogWrite(led_pin, 0);
  analogWrite(LED_BUILTIN, 255);

  wifiService.connect();
  
  // Inicjalizuj OTA po połączeniu z WiFi
  otaService.init();
  
  firebaseService.connectFirebase();
  firebaseService.setCallback([](String key, String value) {
    callback(key, value);
  });
  firebaseService.setTimestamp();

  startTime = millis();
}

void loop() {
  // Obsługuj żądania OTA
  otaService.handle();

  firebaseService.firebaseStream();
  unsigned long elapsed = millis() - startTime;
  timeNow.timePoint = timeSnapPoint + seconds(elapsed / 1000);
  timeNow = timeService.timerFromTimePoint(timeNow.timePoint);

  static Timer lastTimeNow = timeNow;
  if (timeNow.second != lastTimeNow.second) {

    String formattedTime = String(timeNow.hour) + ":" + String(timeNow.minute) + ":" + String(timeNow.second);

    oled.stack[0].text = formattedTime;
    oled.print();
    
    trigger(timeNow, timer1, timer2);
    
    lastTimeNow = timeNow;
  }

  // Sprawdzamy stan przełącznika raz na interval ms i zapisujemy wynik
  static bool switcherState = false;
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck >= interval) {
    switcherState = switcher();
    lastCheck = millis();
  }

  // Aktualizujemy OLED co 100ms
  static unsigned long lastOledUpdate = 0;
  if (millis() - lastOledUpdate >= 100) {
    oled.stack[2].text = String(switcherState ? "ON" : "OFF");
    lastOledUpdate = millis();
  }

  // Wysyłamy distance do Firebase co 10 sekund
  // static unsigned long lastFirebaseSend = 0;
  // if (millis() - lastFirebaseSend >= 10000) {
  //   firebaseService.setPin("distance-test", distance);
  //   lastFirebaseSend = millis();
  // }

  // Ustawiamy jasność LED w zależności od pory dnia i stanu przełącznika
  static unsigned long brightnessGlobal = 0;
  if (trigger(timeNow, timer1, timer2) && switcherState) {
    brightnessGlobal = brightness1;
  }
  else if (!trigger(timeNow, timer1, timer2) && switcherState) {
    brightnessGlobal = brightness2; 
  }
  else {
    brightnessGlobal = 0; // Domyślna jasność, jeśli nie ma aktywnego przełącznika
  }

  // Stopniowe przejście do nowej jasności
  static unsigned long brightnessTemp = 0;
  if (brightnessTemp < brightnessGlobal) {
    brightnessTemp += 1;
  } else if (brightnessTemp > brightnessGlobal) {
    // brightnessTemp <= 0 ? 0 : brightnessTemp -= 10;
    brightnessTemp = (brightnessTemp <= 10) ? 0 : brightnessTemp - 10; // Zapobiega ujemnej jasności
  }

  // Ustawiamy jasność LED
  analogWrite(led_pin, brightnessTemp);
  analogWrite(LED_BUILTIN, 255 - brightnessTemp);
}