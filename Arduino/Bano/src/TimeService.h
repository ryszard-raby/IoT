#include <chrono>
#include <Timer.h>

using namespace std::chrono;

class TimeService {
    public:

    system_clock::time_point timeSnapPoint;

    system_clock::time_point snapTimePoint(long long timeStamp) {
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

    system_clock::time_point timePointNow(int liveTime) {
        return timeSnapPoint + seconds(liveTime/1000);
    }

    Timer timerFromTimePoint(system_clock::time_point timePoint) {
        Timer Timer;
        time_t currentTime = system_clock::to_time_t(timePoint);
        auto lt = localtime(&currentTime);
        Timer.day = lt->tm_mday;
        Timer.month = lt->tm_mon + 1;  // tm_mon is 0-11, we need 1-12
        Timer.year = lt->tm_year + 1900;  // tm_year is years since 1900
        Timer.hour = lt->tm_hour;
        Timer.minute = lt->tm_min;
        Timer.second = lt->tm_sec;
        Timer.millisecond = 0;
        Timer.timePoint = timePoint;
        return Timer;
    }

    system_clock::time_point timePointFromTimer(Timer Timer) {
        time_t currentTime = time(0);
        auto lt = localtime(&currentTime);
        lt->tm_hour = Timer.hour;
        lt->tm_min = Timer.minute;
        lt->tm_sec = Timer.second;
        return system_clock::from_time_t(mktime(lt));
    }
};