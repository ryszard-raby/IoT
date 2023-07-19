class DateTime {
    public:
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;

    DateTime(int year, int month, int day, int hour, int minute, int second) {
        this->year = year;
        this->month = month;
        this->day = day;
        this->hour = hour;
        this->minute = minute;
        this->second = second;
    }
};