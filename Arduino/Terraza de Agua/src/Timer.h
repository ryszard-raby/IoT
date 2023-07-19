class Timer
{
    public:
    std::string name;
    int hour_start;
    int hour_end;
    int min_start;
    int min_end;
    bool active;
    bool itsTime;

    // Timer(int hour, int value) {
    //     this->hour = hour;
    //     this->value = value;
    //     this->done = false;
    // }

    Timer() {
        this->name = "timer";
        this->hour_start = 0;
        this->hour_end = 0;
        this->min_start = 0;
        this->min_end = 0;
        this->active = false;
        this->itsTime = false;
    }
};