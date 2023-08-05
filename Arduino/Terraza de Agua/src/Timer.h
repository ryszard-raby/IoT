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
    std::chrono::system_clock::time_point timePoint;

    // Timer(int hour, int value) {
    //     this->hour = hour;
    //     this->value = value;
    //     this->done = false;
    // }

    Timer() {
        this->name = "timer";
        this->hour = 0;
        this->minute = 0;
        this->active = false;
        this->itsTime = false;
    }
};