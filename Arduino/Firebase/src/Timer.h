class Timer
{
    public:
    int hour;
    int value;
    bool done;

    // Timer(int hour, int value) {
    //     this->hour = hour;
    //     this->value = value;
    //     this->done = false;
    // }

    Timer() {
        this->hour = 0;
        this->value = 0;
        this->done = false;
    }
};