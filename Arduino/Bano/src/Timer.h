#include <Time.h>

class Timer {
    public:
        int hour;
        int minute;
        int second;
        int millisecond;
        bool active;
    
        Timer() {
            this->hour = 0;
            this->minute = 0;
            this->second = 0;
            this->millisecond = 0;
            this->active = false;
        }
};