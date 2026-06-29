#pragma once

#include <Arduino.h>
#include <vector>
#include <set>
#include <algorithm>

#include "FirebaseService.h"
#include "Timer.h"
#include "PinManager.h"

// ---- Struktura revertu (powrót pinu do minValue po czasie) ----
struct PendingRevert {
  String pinName;        // wirtualna nazwa pinu – do zapisu w Firebase
  int revertValue;
  uint64_t revertAtUs;   // micros64() moment revertu (monotoniczny)
};

/// Zarządza harmonogramami, revertami i deduplikacją zdarzeń.
class ScheduleManager {
public:
  std::vector<ScheduleEntry> schedules;
  std::set<String> firedToday;           // klucze "pin:HH:MM" już odpalone dziś
  std::vector<PendingRevert> reverts;    // aktywne reverty

  /// Buduje klucz do zbioru firedToday: "pin:HH:MM"
  static String firedKey(const String& pin, int hour, int minute) {
    char buf[20];
    snprintf(buf, sizeof(buf), "%s:%02d:%02d", pin.c_str(), hour, minute);
    return String(buf);
  }

  // ========== Callbacki ==========

  /// Callback – odbiera harmonogramy z Firebase i przebudowuje wektor schedules.
  void onScheduleEntry(const ScheduleEntry& entry) {
    schedules.push_back(entry);
    Serial.printf("[SCHEDULE] pin=%s trigger=%02d:%02d value=%d dur=%dmin",
                  entry.pin.c_str(), entry.triggerHour, entry.triggerMinute,
                  entry.value, entry.durationMinutes);
    if (entry.dependencyPin.length() > 0) {
      Serial.printf(" dep=%s==%d", entry.dependencyPin.c_str(), entry.dependencyValue);
    }
    Serial.println();
  }

  /// Callback – czyści harmonogramy przed załadowaniem nowych z Firebase.
  void onScheduleClear(const String& pin) {
    if (pin.length() == 0) {
      schedules.clear();
      Serial.println("[SCHEDULE] Cleared schedules (reverts preserved)");
    } else {
      schedules.erase(
        std::remove_if(schedules.begin(), schedules.end(),
          [&pin](const ScheduleEntry& e) { return e.pin == pin; }),
        schedules.end()
      );
      reverts.erase(
        std::remove_if(reverts.begin(), reverts.end(),
          [&pin](const PendingRevert& r) { return r.pinName == pin; }),
        reverts.end()
      );
      Serial.printf("[SCHEDULE] Cleared pin=%s\n", pin.c_str());
    }
  }

  // ========== Główna logika pętli ==========

  /// Sprawdza i wykonuje reverty (powrót pinu do minValue) – używa micros64().
  void checkReverts(FirebaseService& firebase) {
    uint64_t nowUs = micros64();
    for (auto it = reverts.begin(); it != reverts.end(); ) {
      if (nowUs >= it->revertAtUs) {
        int physicalPin = PinManager::virtualPinToPhysical(it->pinName);
        if (physicalPin >= 0) {
          digitalWrite(physicalPin, it->revertValue ? HIGH : LOW);
          Serial.printf("[SCHEDULE] Revert %s (pin=%d) → %d\n",
                        it->pinName.c_str(), physicalPin, it->revertValue);
        } else {
          Serial.printf("[SCHEDULE] Revert virtual %s → %d\n",
                        it->pinName.c_str(), it->revertValue);
        }
        firebase.setData("state/" + std::string(it->pinName.c_str()), it->revertValue);
        it = reverts.erase(it);
      } else {
        ++it;
      }
    }
  }

  /// Sprawdza harmonogramy czasowe – odpala zdarzenia, planuje reverty.
  /// Resetuje firedToday o północy.
  void checkTriggers(const Timer& timeNow, const std::map<String, int>& pinStates,
                     FirebaseService& firebase) {
    // Reset firedToday o północy
    if (timeNow.hour == 0 && timeNow.minute == 0 && !firedToday.empty()) {
      firedToday.clear();
      Serial.println("[SCHEDULE] Nowy dzień – reset firedToday");
    }

    for (const auto& entry : schedules) {
      if (timeNow.hour != entry.triggerHour || timeNow.minute != entry.triggerMinute)
        continue;

      String key = firedKey(entry.pin, entry.triggerHour, entry.triggerMinute);
      if (firedToday.count(key)) continue;  // już odpalone dziś

      // ── Sprawdź zależność AND na poziomie schedule entry ──
      if (entry.dependencyPin.length() > 0) {
        int depState = pinStates.count(entry.dependencyPin) ? pinStates.at(entry.dependencyPin) : -1;
        if (depState != entry.dependencyValue) {
          Serial.printf("[SCHEDULE] SKIP %s – AND dependency %s==%d (actual=%d)\n",
                        entry.pin.c_str(), entry.dependencyPin.c_str(),
                        entry.dependencyValue, depState);
          continue;
        }
      }

      // Aktualizuj Firebase i oznacz jako odpalone
      firebase.setData("state/" + std::string(entry.pin.c_str()), entry.value);
      firedToday.insert(key);

      // Steruj sprzętem
      int physicalPin = PinManager::virtualPinToPhysical(entry.pin);
      if (physicalPin >= 0) {
        pinMode(physicalPin, OUTPUT);
        digitalWrite(physicalPin, entry.value ? HIGH : LOW);
        Serial.printf("[SCHEDULE] FIRE %s (pin=%d) → %d", entry.pin.c_str(), physicalPin, entry.value);
      } else {
        Serial.printf("[SCHEDULE] FIRE virtual %s → %d", entry.pin.c_str(), entry.value);
      }

      // Zaplanuj revert jeśli durationMinutes > 0
      if (entry.durationMinutes > 0) {
        PendingRevert rev;
        rev.pinName = entry.pin;
        rev.revertValue = entry.minValue;
        rev.revertAtUs = micros64() + uint64_t(entry.durationMinutes) * 60ULL * 1000000ULL;
        reverts.push_back(rev);
        Serial.printf(" | revert za %dmin → %d", entry.durationMinutes, entry.minValue);
      }
      Serial.println();
    }
  }
};
