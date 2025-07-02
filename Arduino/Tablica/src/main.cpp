#include <Arduino.h>
#include <Oled.h>
#include <WiFiService.h>
#include <FirebaseService.h>
#include <TimeService.h>
#include <chrono>
#include <time.h>

int startTime = 0;

Oled oled;
WiFiService wifiService;
FirebaseService firebaseService;
TimeService timeService;

Timer timeNow;
Timer t_timeNow;

Timer timer1;
Timer timer2;
Timer timer3;
Timer timer4;

system_clock::time_point timeSnapPoint;

struct ScrollInfo {
  int row;
  String id;
  String text;
  String remaining;
  bool active;
};

ScrollInfo scrollAnimations[4];
unsigned long lastScrollTime = 0;
const unsigned long scrollInterval = 200; // Adjust interval for scroll speed

void handleScrollAnimations() {
  if (millis() - lastScrollTime >= scrollInterval) {
    lastScrollTime = millis();

    for (int i = 0; i < 4; i++) {
      if (scrollAnimations[i].active) {
        String paddedText = scrollAnimations[i].text + "   ";
        static int scrollIndex[4] = {0, 0, 0, 0};

        String scrollText = paddedText.substring(scrollIndex[i]) + paddedText.substring(0, scrollIndex[i]);
        oled.updateRow(scrollAnimations[i].row, scrollAnimations[i].id, scrollText, scrollAnimations[i].remaining);
        oled.print();

        scrollIndex[i] = (scrollIndex[i] + 1) % paddedText.length();
      }
    }
  }
}

void oledSetup() {
  oled.add(0, 0, "--:--", 2);
  oled.add(80, 0, "Arkady", 1);
  oled.add(80, 9, "Capitol", 1);

  oled.addRow(0, "-", "---", String(timer1.minute) + " min");
  oled.addRow(1, "-", "---", String(timer2.minute) + " min");
  oled.addRow(2, "-", "---", String(timer3.minute) + " min");
  oled.addRow(3, "-", "---", String(timer4.minute) + " min");
}

void callback(String key, String value) {
  Serial.println(key + ": " + value);
  if (key == "time") {
    Serial.println(std::stoll(value.c_str()));
    timeSnapPoint = timeService.setTimePoint(std::stoll(value.c_str()));
  }
}

void setup() {
  Serial.begin(9600);
  oled.init();

  timer1.minute = 2;
  timer2.minute = 3;
  timer3.minute = 4;
  timer4.minute = 5;

  oledSetup();
  oled.print();

  wifiService.connect();

  firebaseService.connectFirebase();
  firebaseService.setCallback([](String key, String value) {
    callback(key, value);
  });
  firebaseService.setTimestamp();

  startTime = millis();
}

void updateAndSortTimers() {
  timer1.remaining = timeNow.minute % timer1.minute;
  timer1.remaining > 0 ? timer1.remaining = timer1.minute - timer1.remaining : timer1.remaining = 0;

  timer2.remaining = timeNow.minute % timer2.minute;
  timer2.remaining > 0 ? timer2.remaining = timer2.minute - timer2.remaining : timer2.remaining = 0;

  timer3.remaining = timeNow.minute % timer3.minute;
  timer3.remaining > 0 ? timer3.remaining = timer3.minute - timer3.remaining : timer3.remaining = 0;

  timer4.remaining = timeNow.minute % timer4.minute;
  timer4.remaining > 0 ? timer4.remaining = timer4.minute - timer4.remaining : timer4.remaining = 0;

  struct TimerInfo {
    int id;
    String destination;
    Timer timer;
  };

  TimerInfo timers[] = {
    {17, "Klecina", timer1},
    {20, "Oporow", timer2},
    {6, "Krzyki", timer3},
    {7, "Klecina", timer4}
  };

  std::sort(std::begin(timers), std::end(timers), [](const TimerInfo &a, const TimerInfo &b) {
    return a.timer.remaining < b.timer.remaining;
  });

  for (int i = 0; i < 4; i++) {
    if (timers[i].timer.remaining == 0) {
      scrollAnimations[i] = {i, String(timers[i].id), timers[i].destination, String(timers[i].timer.remaining) + " min", true};
    } else {
      scrollAnimations[i].active = false;
      oled.updateRow(i, String(timers[i].id), timers[i].destination, String(timers[i].timer.remaining) + " min");
    }
  }

  oled.print();
}

void loop() {
  firebaseService.firebaseStream();
  timeNow = timeService.getTimer(millis() - startTime);

  if (timeNow.second != t_timeNow.second) {
    String formatedTime = (timeNow.minute < 10 ? "0" : "") + String(timeNow.minute) + ":" + (timeNow.second < 10 ? "0" : "") + String(timeNow.second);
    oled.stack[0].text = formatedTime;

    updateAndSortTimers();

    t_timeNow = timeNow;
  }

  handleScrollAnimations();
}

