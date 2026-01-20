#ifndef ALARM_MANAGER_H
#define ALARM_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <vector>
#include "config.h"

/**
 * AlarmData - Single alarm configuration
 */
struct AlarmData {
    uint8_t id;           // Alarm ID (0-9, max 10 alarms)
    uint8_t hour;         // Hour (0-23)
    uint8_t minute;       // Minute (0-59)
    uint8_t daysOfWeek;   // Bitmask: 0x01=Sun, 0x02=Mon, 0x04=Tue, 0x08=Wed, 0x10=Thu, 0x20=Fri, 0x40=Sat
    String sound;         // Sound: "tone1", "tone2", "tone3", or MP3 filename
    bool enabled;         // Is alarm active?
    String label;         // Custom alarm name/label
    bool snoozeEnabled;   // Is snooze enabled for this alarm?
    bool permanentlyDisabled;  // One-shot alarms permanently disabled after firing
    String bottomRowLabel;     // Custom bottom row text (replaces instructions when alarm rings)

    AlarmData() : id(0), hour(0), minute(0), daysOfWeek(0), sound("tone1"), enabled(false), label("Alarm"), snoozeEnabled(true), permanentlyDisabled(false), bottomRowLabel("") {}
};

/**
 * AlarmManager - Alarm scheduling and NVS storage
 *
 * Manages up to 10 alarms with NVS persistence.
 * Checks every second for alarm triggers.
 */
class AlarmManager {
public:
    /**
     * Callback function type for alarm events
     * @param alarmId ID of triggered alarm
     */
    typedef void (*AlarmCallback)(uint8_t alarmId);

    AlarmManager();

    /**
     * Initialize alarm manager and load from NVS
     * @return true if successful
     */
    bool begin();

    /**
     * Add or update an alarm
     * @param alarm Alarm data to save
     * @return true if successful
     */
    bool setAlarm(const AlarmData& alarm);

    /**
     * Get alarm by ID
     * @param id Alarm ID (0-9)
     * @param alarm Output: Alarm data
     * @return true if alarm exists
     */
    bool getAlarm(uint8_t id, AlarmData& alarm);

    /**
     * Delete alarm
     * @param id Alarm ID to delete
     * @return true if successful
     */
    bool deleteAlarm(uint8_t id);

    /**
     * Get all alarms
     * @return Vector of all alarms
     */
    std::vector<AlarmData> getAllAlarms();

    /**
     * Check if any alarms should trigger (call every second)
     * @param hour Current hour (0-23)
     * @param minute Current minute (0-59)
     * @param dayOfWeek Day of week (0=Sun, 1=Mon, ..., 6=Sat)
     */
    void checkAlarms(uint8_t hour, uint8_t minute, uint8_t dayOfWeek);

    /**
     * Snooze current alarm (reschedule for 5 minutes later)
     */
    void snoozeAlarm();

    /**
     * Dismiss current alarm
     */
    void dismissAlarm();

    /**
     * Check if alarm is currently ringing
     * @return true if alarm active
     */
    bool isAlarmRinging();

    /**
     * Get current ringing alarm ID
     * @return Alarm ID or 255 if none ringing
     */
    uint8_t getRingingAlarmId();

    /**
     * Get sound for current ringing alarm
     * @return Sound string
     */
    String getRingingAlarmSound();

    /**
     * Set callback for alarm trigger
     * @param callback Function to call when alarm triggers
     */
    void setAlarmCallback(AlarmCallback callback);

    /**
     * Check if any alarm is snoozed
     * @return true if an alarm is snoozed
     */
    bool isAlarmSnoozed();

    /**
     * Check if any enabled alarm exists
     * @return true if at least one alarm is enabled
     */
    bool hasEnabledAlarm();

private:
    static const uint8_t SNOOZE_MINUTES = 5;

    Preferences _prefs;
    std::vector<AlarmData> _alarms;
    bool _alarmRinging;
    uint8_t _ringingAlarmId;
    uint8_t _lastCheckedMinute;
    bool _snoozed;
    uint8_t _snoozeHour;
    uint8_t _snoozeMinute;
    AlarmCallback _alarmCallback;

    void loadFromNVS();
    void saveToNVS();
    String getAlarmKey(uint8_t id);
    bool shouldAlarmTrigger(const AlarmData& alarm, uint8_t hour, uint8_t minute, uint8_t dayOfWeek);
};

#endif // ALARM_MANAGER_H
