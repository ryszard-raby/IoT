#include <Time.h>

class Timer {
    public:
        int hour;
        int minute;
        int second;
        bool active;
    
        Timer() {
            this->hour = 0;
            this->minute = 0;
            this->second = 0;
            this->active = false;
        }
};