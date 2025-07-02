#include <chrono>
#include <Timer.h>

using namespace std::chrono;

class TimeService {
    public:

    system_clock::time_point timeSnapPoint;

    system_clock::time_point setTimePoint(long long timeStamp) {
        system_clock::time_point timePoint = system_clock::from_time_t(timeStamp/1000);
        
        // UTC+1
        timePoint += hours(1);

        //get month from timePoint
        time_t timePoint_t = system_clock::to_time_t(timePoint);
        auto lt = localtime(&timePoint_t);
        int month = lt->tm_mon;

        //check if summer time
        if (month > 3 && month < 10) {
            timePoint += hours(1);
        }

        timeSnapPoint = timePoint;

        return timePoint;
    }

    system_clock::time_point getTimePointNow(int liveTime) {
        return timeSnapPoint + seconds(liveTime/1000);
    }

    Timer getTimer(int liveTime) {
        Timer Timer;
        time_t currentTime = system_clock::to_time_t(getTimePointNow(liveTime));
        auto lt = localtime(&currentTime);
        Timer.hour = lt->tm_hour;
        Timer.minute = lt->tm_min;
        Timer.second = lt->tm_sec;
        Timer.millisecond = liveTime % 1000;
        Timer.millisecond = (liveTime % 1000) / 100 * 100;
        return Timer;
    }
};