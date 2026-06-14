#include <Arduino.h>

class TimeOut {
public:
    TimeOut(void (*func)(), unsigned long timeout) {
        unsigned long lastTime = millis();
            if (millis() - lastTime > timeout) {
                lastTime = millis();
                func();
            }
    }
};