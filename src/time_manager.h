#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include <time.h>

/**
 * TimeManager - ESP32 RTC-based timekeeping with BLE sync support
 *
 * Uses the ESP32's built-in RTC to maintain time without WiFi/NTP.
 * Time is synchronized via BLE from iOS app.
 */
class TimeManager {
public:
    TimeManager();

    /**
     * Initialize the time manager
     * @return true if successful
     */
    bool begin();

    /**
     * Set current time
     * @param hour Hour (0-23)
     * @param minute Minute (0-59)
     * @param second Second (0-59)
     */
    void setTime(uint8_t hour, uint8_t minute, uint8_t second);

    /**
     * Set current date
     * @param day Day of month (1-31)
     * @param month Month (1-12)
     * @param year Year (e.g., 2026)
     */
    void setDate(uint8_t day, uint8_t month, uint16_t year);

    /**
     * Set time from Unix timestamp
     * @param timestamp Unix timestamp (seconds since Jan 1, 1970)
     */
    void setTimestamp(time_t timestamp);

    /**
     * Get current time
     * @param hour Output: Hour (0-23)
     * @param minute Output: Minute (0-59)
     * @param second Output: Second (0-59)
     */
    void getTime(uint8_t& hour, uint8_t& minute, uint8_t& second);

    /**
     * Get current date
     * @param day Output: Day of month (1-31)
     * @param month Output: Month (1-12)
     * @param year Output: Year (e.g., 2026)
     */
    void getDate(uint8_t& day, uint8_t& month, uint16_t& year);

    /**
     * Get current Unix timestamp
     * @return Unix timestamp (seconds since Jan 1, 1970)
     */
    time_t getTimestamp();

    /**
     * Get formatted time string
     * @param format12Hour If true, returns 12-hour format (e.g., "3:45 PM"), otherwise 24-hour (e.g., "15:45")
     * @return Time string
     */
    String getTimeString(bool format12Hour = false);

    /**
     * Get formatted date string
     * @return Date string (e.g., "Jan 14, 2026")
     */
    String getDateString();

    /**
     * Get day of week string
     * @return Day name (e.g., "Monday")
     */
    String getDayOfWeekString();

    /**
     * Check if time has been synchronized
     * @return true if time was set (either manually or via BLE)
     */
    bool isSynced();

    /**
     * Get time since last sync
     * @return Milliseconds since last sync, or 0 if never synced
     */
    unsigned long getTimeSinceSync();

private:
    struct tm _timeinfo;
    bool _synced;
    unsigned long _lastSyncMillis;

    void updateTimeinfo();
};

#endif // TIME_MANAGER_H
