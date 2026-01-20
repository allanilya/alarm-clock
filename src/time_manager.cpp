#include "time_manager.h"

// Day and month names for formatting
static const char* DAYS_OF_WEEK[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

static const char* MONTHS[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

TimeManager::TimeManager()
    : _synced(false),
      _lastSyncMillis(0) {
    memset(&_timeinfo, 0, sizeof(struct tm));
}

bool TimeManager::begin() {
    // Initialize with default time (Jan 1, 2026, 00:00:00)
    _timeinfo.tm_year = 2026 - 1900;  // Years since 1900
    _timeinfo.tm_mon = 0;             // January (0-11)
    _timeinfo.tm_mday = 1;            // Day 1
    _timeinfo.tm_hour = 0;
    _timeinfo.tm_min = 0;
    _timeinfo.tm_sec = 0;
    _timeinfo.tm_wday = 3;            // Wednesday (0=Sunday)

    // Set ESP32 RTC
    time_t t = mktime(&_timeinfo);
    struct timeval now = { .tv_sec = t };
    settimeofday(&now, NULL);

    Serial.println("TimeManager: Initialized with default time (2026-01-01 00:00:00)");
    return true;
}

void TimeManager::setTime(uint8_t hour, uint8_t minute, uint8_t second) {
    // Update current timeinfo
    updateTimeinfo();

    // Set new time
    _timeinfo.tm_hour = hour;
    _timeinfo.tm_min = minute;
    _timeinfo.tm_sec = second;

    // Update ESP32 RTC
    time_t t = mktime(&_timeinfo);
    struct timeval now = { .tv_sec = t };
    settimeofday(&now, NULL);

    // Mark as synced
    _synced = true;
    _lastSyncMillis = millis();

    Serial.printf("TimeManager: Time set to %02d:%02d:%02d\n", hour, minute, second);
}

void TimeManager::setDate(uint8_t day, uint8_t month, uint16_t year) {
    // Update current timeinfo
    updateTimeinfo();

    // Set new date
    _timeinfo.tm_mday = day;
    _timeinfo.tm_mon = month - 1;     // tm_mon is 0-11
    _timeinfo.tm_year = year - 1900;  // Years since 1900

    // Update ESP32 RTC
    time_t t = mktime(&_timeinfo);
    struct timeval now = { .tv_sec = t };
    settimeofday(&now, NULL);

    // Mark as synced
    _synced = true;
    _lastSyncMillis = millis();

    Serial.printf("TimeManager: Date set to %04d-%02d-%02d\n", year, month, day);
}

void TimeManager::setTimestamp(time_t timestamp) {
    // Set RTC from Unix timestamp
    struct timeval now = { .tv_sec = timestamp };
    settimeofday(&now, NULL);

    // Update local timeinfo
    updateTimeinfo();

    // Mark as synced
    _synced = true;
    _lastSyncMillis = millis();

    Serial.printf("TimeManager: Timestamp set to %ld\n", (long)timestamp);
}

void TimeManager::getTime(uint8_t& hour, uint8_t& minute, uint8_t& second) {
    updateTimeinfo();
    hour = _timeinfo.tm_hour;
    minute = _timeinfo.tm_min;
    second = _timeinfo.tm_sec;
}

void TimeManager::getDate(uint8_t& day, uint8_t& month, uint16_t& year) {
    updateTimeinfo();
    day = _timeinfo.tm_mday;
    month = _timeinfo.tm_mon + 1;     // tm_mon is 0-11
    year = _timeinfo.tm_year + 1900;  // Years since 1900
}

time_t TimeManager::getTimestamp() {
    time_t now;
    time(&now);
    return now;
}

String TimeManager::getTimeString(bool format12Hour) {
    updateTimeinfo();

    char buffer[16];

    if (format12Hour) {
        // 12-hour format with AM/PM
        uint8_t hour12 = _timeinfo.tm_hour % 12;
        if (hour12 == 0) hour12 = 12;  // 0:00 becomes 12:00 AM
        const char* ampm = (_timeinfo.tm_hour < 12) ? "AM" : "PM";

        snprintf(buffer, sizeof(buffer), "%d:%02d %s",
                 hour12, _timeinfo.tm_min, ampm);
    } else {
        // 24-hour format
        snprintf(buffer, sizeof(buffer), "%02d:%02d",
                 _timeinfo.tm_hour, _timeinfo.tm_min);
    }

    return String(buffer);
}

String TimeManager::getDateString() {
    updateTimeinfo();

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%s %d, %d",
             MONTHS[_timeinfo.tm_mon],
             _timeinfo.tm_mday,
             _timeinfo.tm_year + 1900);

    return String(buffer);
}

String TimeManager::getDayOfWeekString() {
    updateTimeinfo();
    return String(DAYS_OF_WEEK[_timeinfo.tm_wday]);
}

bool TimeManager::isSynced() {
    return _synced;
}

unsigned long TimeManager::getTimeSinceSync() {
    if (!_synced) return 0;
    return millis() - _lastSyncMillis;
}

void TimeManager::updateTimeinfo() {
    time_t now;
    time(&now);
    localtime_r(&now, &_timeinfo);
}
