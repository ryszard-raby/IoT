#include <chrono>
using namespace std::chrono;

class Timer
{
    public:
    std::string name;
    int hour;
    int minute;
    bool active;
    bool itsTime;
    system_clock::time_point timePoint;

    Timer() {
        this->name = "timer";
        this->hour = 0;
        this->minute = 0;
        this->active = false;
        this->itsTime = false;
    }
};