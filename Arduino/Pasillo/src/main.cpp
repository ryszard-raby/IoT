#include <Arduino.h>
#include <WiFiService.h>
#include <FirebaseService.h>
#include <TimeService.h>
#include <chrono>
#include <time.h>
#include <Oled.h>
#include <OTAService.h>

int startTime = 0;

Timer timeNow;
Timer t_timeNow;
Timer timer0;
Timer timer1;
Timer timer2;

int powerPin = 4;

int manual = 0;
int power0 = 0;
int power1 = 0;
int power2 = 0;

FirebaseService firebaseService;
WiFiService wifiService;
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

  if (key == "timer0") {
    timer0.hour = std::stoi(value.c_str());
    timer0.minute = 0;
    timer0.second = 0;
    setTimePoint(timer0, timeNow);
    Serial.println("timer0: " + String(timer0.hour));
  }

  if (key == "timer1") {
    timer1.hour = std::stoi(value.c_str());
    timer1.minute = 0;
    timer1.second = 0;
    setTimePoint(timer1, timeNow);
    Serial.println("timer1: " + String(timer1.hour));
  }

  if (key == "timer2") {
    timer2.hour = std::stoi(value.c_str());
    timer2.minute = 0;
    timer2.second = 0;
    setTimePoint(timer2, timeNow);
    Serial.println("timer2: " + String(timer2.hour));
  }

  if (key == "manual") {
    manual = std::stoi(value.c_str());
  }

  if (key == "power0") {
    power0 = std::stoi(value.c_str());
    if (power0 < 0) power0 = 0;
    if (power0 > 255) power0 = 255;
    Serial.println("power0: " + String(power0));
  }

  if (key == "power1") {
    power1 = std::stoi(value.c_str());
    if (power1 < 0) power1 = 0;
    if (power1 > 255) power1 = 255;
    Serial.println("power1: " + String(power1));
  }

  if (key == "power2") {
    power2 = std::stoi(value.c_str());
    if (power2 < 0) power2 = 0;
    if (power2 > 255) power2 = 255;
    Serial.println("power2: " + String(power2));
  }
}

struct TimerPower {
  int hour;
  int powerValue;
};

int getPower(Timer timeNow, Timer &timer0, Timer &timer1, Timer &timer2, int power0, int power1, int power2) {
  TimerPower schedule[] = {
    {timer0.hour, power0},  // Od timer0 - power0
    {timer1.hour, power1},  // Od timer1 - power1  
    {timer2.hour, power2}   // Od timer2 - power2
  };
  
  // Znajdź aktualną moc
  for (int i = 0; i < 3; i++) {
    int nextIndex = (i + 1) % 3;
    int currentHour = schedule[i].hour;
    int nextHour = schedule[nextIndex].hour;
    
    bool inRange = false;
    if (currentHour <= nextHour) {
      // Przedział nie przechodzi przez północ
      inRange = (timeNow.hour >= currentHour && timeNow.hour < nextHour);
    } else {
      // Przedział przechodzi przez północ
      inRange = (timeNow.hour >= currentHour || timeNow.hour < nextHour);
    }
    
    if (inRange) {
      return schedule[i].powerValue;
    }
  }
  
  return 0; // Poza wszystkimi przedziałami
}

void setup() {
  Serial.begin(9600);

  oled.init();
  oled.add(0, 0, "time", 2);
  oled.add(0, 25, "timer0", 1);
  oled.add(30, 25, "timer1", 1);
  oled.add(60, 25, "timer2", 1);
  oled.add(0, 40, "0", 1);
  oled.add(30, 40, "0", 1);
  oled.add(60, 40, "0", 1);

  wifiService.connect();
  
  // Inicjalizuj OTA po połączeniu z WiFi
  otaService.init();
  
  firebaseService.connectFirebase();
  firebaseService.setCallback([](String key, String value) {
    callback(key, value);
  });
  firebaseService.setTimestamp();

  startTime = millis();

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(powerPin, OUTPUT);
}

void loop() {
  // Obsługuj żądania OTA
  otaService.handle();

  firebaseService.firebaseStream();
  
  unsigned long elapsed = millis() - startTime;
  timeNow.timePoint = timeSnapPoint + seconds(elapsed / 1000);
  timeNow = timeService.timerFromTimePoint(timeNow.timePoint);

  String formattedTime = String(timeNow.hour) + ":" + String(timeNow.minute) + ":" + String(timeNow.second);
  String formattedTimeAndDate = String(timeNow.day) + "/" + String(timeNow.month) + "/" + String(timeNow.year) + " " + formattedTime;

  // if (timeNow.second != t_timeNow.second) {
  //   t_timeNow.second = timeNow.second;

  //   setTimePoint(timer0, timeNow);
  //   setTimePoint(timer1, timeNow);
  //   setTimePoint(timer2, timeNow);

  //   oled.stack[0].text = formattedTime;
  //   oled.stack[1].text = String(timer0.hour);
  //   oled.stack[2].text = String(timer1.hour);
  //   oled.stack[3].text = String(timer2.hour);
  //   oled.stack[4].text = String(power0);
  //   oled.stack[5].text = String(power1);
  //   oled.stack[6].text = String(power2);
  //   oled.print();
  // }

  // Oblicz moc na podstawie aktualnego czasu i timerów
  int powerGlobal = getPower(timeNow, timer0, timer1, timer2, power0, power1, power2);

  // Stopniowe przejście do nowej jasności
  static unsigned int powerTemp = 0;
  if (powerTemp < powerGlobal) {
    powerTemp += 1;
  } else if (powerTemp > powerGlobal) {
    powerTemp -= 1;
  }

  analogWrite(powerPin, powerTemp);
  analogWrite(LED_BUILTIN, 255 - powerTemp);
}