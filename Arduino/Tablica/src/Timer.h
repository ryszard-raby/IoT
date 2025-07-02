#include <Time.h>

class Timer {
    public:
        int hour;
        int minute;
        int second;
        int millisecond;
        int remaining;
    
        Timer() {
            this->hour = 0;
            this->minute = 0;
            this->second = 0;
            this->millisecond = 0;
            this->remaining = 0;
        }
};