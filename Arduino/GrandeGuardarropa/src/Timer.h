#include <Time.h>
#include <chrono>
using namespace std::chrono;

class Timer {
    public:
        int day;
        int month;
        int year;
        int hour;
        int minute;
        int second;
        int millisecond;
        bool active;
        bool enabled;
        system_clock::time_point timePoint;
    
        Timer() {
            this->day = 0;
            this->month = 0;
            this->year = 0;
            this->hour = 0;
            this->minute = 0;
            this->second = 0;
            this->millisecond = 0;
            this->active = false;
            this->enabled = false;
            this->timePoint = system_clock::now();
        }
};