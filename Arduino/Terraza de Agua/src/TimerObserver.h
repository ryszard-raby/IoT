class TimerObserver {
public:
    int activeTimersCount = 0;

    void count(bool isActive) {
        if (isActive) activeTimersCount++;
    }
};