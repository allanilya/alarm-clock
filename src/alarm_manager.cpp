#include "alarm_manager.h"

AlarmManager::AlarmManager()
    : _alarmRinging(false),
      _ringingAlarmId(255),
      _lastCheckedMinute(255),
      _snoozed(false),
      _snoozeHour(0),
      _snoozeMinute(0),
      _alarmCallback(nullptr) {
}

bool AlarmManager::begin() {
    Serial.println("AlarmManager: Initializing...");

    // Open NVS in read-write mode
    if (!_prefs.begin("alarms", false)) {
        Serial.println("AlarmManager: ERROR - Failed to open NVS!");
        return false;
    }

    // Load alarms from NVS
    loadFromNVS();

    Serial.print("AlarmManager: Loaded ");
    Serial.print(_alarms.size());
    Serial.println(" alarms from NVS");

    return true;
}

bool AlarmManager::setAlarm(const AlarmData& alarm) {
    if (alarm.id >= MAX_ALARMS) {
        Serial.println("AlarmManager: ERROR - Invalid alarm ID!");
        return false;
    }

    // Find existing alarm or add new one
    bool found = false;
    for (size_t i = 0; i < _alarms.size(); i++) {
        if (_alarms[i].id == alarm.id) {
            _alarms[i] = alarm;
            found = true;
            break;
        }
    }

    if (!found) {
        _alarms.push_back(alarm);
    }

    // Save to NVS
    saveToNVS();

    Serial.print("AlarmManager: Alarm ");
    Serial.print(alarm.id);
    Serial.print(" set for ");
    Serial.print(alarm.hour);
    Serial.print(":");
    Serial.print(alarm.minute);
    Serial.print(" (");
    Serial.print(alarm.enabled ? "enabled" : "disabled");
    Serial.println(")");

    return true;
}

bool AlarmManager::getAlarm(uint8_t id, AlarmData& alarm) {
    for (const auto& a : _alarms) {
        if (a.id == id) {
            alarm = a;
            return true;
        }
    }
    return false;
}

bool AlarmManager::deleteAlarm(uint8_t id) {
    for (size_t i = 0; i < _alarms.size(); i++) {
        if (_alarms[i].id == id) {
            _alarms.erase(_alarms.begin() + i);

            // Remove from NVS
            String key = getAlarmKey(id);
            _prefs.remove(key.c_str());

            Serial.print("AlarmManager: Deleted alarm ");
            Serial.println(id);
            return true;
        }
    }
    return false;
}

std::vector<AlarmData> AlarmManager::getAllAlarms() {
    return _alarms;
}

void AlarmManager::checkAlarms(uint8_t hour, uint8_t minute, uint8_t dayOfWeek) {
    // Only check once per minute
    if (minute == _lastCheckedMinute) {
        return;
    }
    _lastCheckedMinute = minute;

    // Check if snoozed alarm should trigger
    if (_snoozed && hour == _snoozeHour && minute == _snoozeMinute) {
        _alarmRinging = true;
        _snoozed = false;

        Serial.println("\n>>> ALARM: Snoozed alarm triggering!");

        if (_alarmCallback) {
            _alarmCallback(_ringingAlarmId);
        }
        return;
    }

    // Check all enabled alarms
    for (size_t i = 0; i < _alarms.size(); i++) {
        auto& alarm = _alarms[i];
        if (!alarm.enabled || alarm.permanentlyDisabled) continue;

        if (shouldAlarmTrigger(alarm, hour, minute, dayOfWeek)) {
            // Auto-disable one-shot alarms (daysOfWeek == 0) BEFORE ringing
            if (alarm.daysOfWeek == 0) {
                _alarms[i].enabled = false;
                _alarms[i].permanentlyDisabled = true;
                saveToNVS();
                Serial.print(">>> One-time alarm ID=");
                Serial.print(alarm.id);
                Serial.println(" permanently disabled (will ring once)");
            }

            _alarmRinging = true;
            _ringingAlarmId = alarm.id;

            Serial.print("\n>>> ALARM TRIGGERED: ID=");
            Serial.print(alarm.id);
            Serial.print(" Time=");
            Serial.print(alarm.hour);
            Serial.print(":");
            Serial.print(alarm.minute);
            Serial.print(" Sound=");
            Serial.println(alarm.sound);

            if (_alarmCallback) {
                _alarmCallback(alarm.id);
            }

            break;  // Only one alarm at a time
        }
    }
}

void AlarmManager::snoozeAlarm() {
    if (!_alarmRinging) return;

    _alarmRinging = false;
    _snoozed = true;

    // Calculate snooze time (current time + 5 minutes)
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    timeinfo.tm_min += SNOOZE_MINUTES;
    if (timeinfo.tm_min >= 60) {
        timeinfo.tm_hour++;
        timeinfo.tm_min -= 60;
        if (timeinfo.tm_hour >= 24) {
            timeinfo.tm_hour = 0;
        }
    }

    _snoozeHour = timeinfo.tm_hour;
    _snoozeMinute = timeinfo.tm_min;

    Serial.print("AlarmManager: Snoozed until ");
    Serial.print(_snoozeHour);
    Serial.print(":");
    Serial.println(_snoozeMinute);
}

void AlarmManager::dismissAlarm() {
    // Always clear both ringing and snooze states
    _alarmRinging = false;
    _snoozed = false;
    _ringingAlarmId = 255;

    Serial.println("AlarmManager: Alarm dismissed (cleared ringing + snooze)");
}

bool AlarmManager::isAlarmRinging() {
    return _alarmRinging;
}

uint8_t AlarmManager::getRingingAlarmId() {
    return _ringingAlarmId;
}

String AlarmManager::getRingingAlarmSound() {
    if (!_alarmRinging) return "";

    for (const auto& alarm : _alarms) {
        if (alarm.id == _ringingAlarmId) {
            return alarm.sound;
        }
    }
    return "tone1";  // Default fallback
}

void AlarmManager::setAlarmCallback(AlarmCallback callback) {
    _alarmCallback = callback;
}

bool AlarmManager::isAlarmSnoozed() {
    return _snoozed;
}

bool AlarmManager::hasEnabledAlarm() {
    for (const auto& alarm : _alarms) {
        if (alarm.enabled) {
            return true;
        }
    }
    return false;
}

// ============================================
// Private Methods
// ============================================

void AlarmManager::loadFromNVS() {
    _alarms.clear();

    // Try to load each possible alarm slot
    for (uint8_t id = 0; id < MAX_ALARMS; id++) {
        String key = getAlarmKey(id);

        if (_prefs.isKey(key.c_str())) {
            // Load alarm data (format: "hour,minute,days,enabled,sound,label,snooze,perm_disabled,bottomRowLabel")
            String data = _prefs.getString(key.c_str(), "");

            if (data.length() > 0) {
                AlarmData alarm;
                alarm.id = id;

                // Parse comma-separated values
                int idx1 = data.indexOf(',');
                int idx2 = data.indexOf(',', idx1 + 1);
                int idx3 = data.indexOf(',', idx2 + 1);
                int idx4 = data.indexOf(',', idx3 + 1);
                int idx5 = data.indexOf(',', idx4 + 1);
                int idx6 = data.indexOf(',', idx5 + 1);
                int idx7 = data.indexOf(',', idx6 + 1);
                int idx8 = data.indexOf(',', idx7 + 1);

                if (idx1 > 0 && idx2 > 0 && idx3 > 0 && idx4 > 0) {
                    alarm.hour = data.substring(0, idx1).toInt();
                    alarm.minute = data.substring(idx1 + 1, idx2).toInt();
                    alarm.daysOfWeek = data.substring(idx2 + 1, idx3).toInt();
                    alarm.enabled = data.substring(idx3 + 1, idx4).toInt() == 1;

                    // Handle different formats
                    if (idx5 > 0 && idx6 > 0 && idx7 > 0 && idx8 > 0) {
                        // Newest format with label, snooze, perm_disabled, and bottomRowLabel
                        alarm.sound = data.substring(idx4 + 1, idx5);
                        alarm.label = data.substring(idx5 + 1, idx6);
                        alarm.snoozeEnabled = data.substring(idx6 + 1, idx7).toInt() == 1;
                        alarm.permanentlyDisabled = data.substring(idx7 + 1, idx8).toInt() == 1;
                        alarm.bottomRowLabel = data.substring(idx8 + 1);
                    } else if (idx5 > 0 && idx6 > 0 && idx7 > 0) {
                        // Previous format with label, snooze, and perm_disabled (no bottomRowLabel)
                        alarm.sound = data.substring(idx4 + 1, idx5);
                        alarm.label = data.substring(idx5 + 1, idx6);
                        alarm.snoozeEnabled = data.substring(idx6 + 1, idx7).toInt() == 1;
                        alarm.permanentlyDisabled = data.substring(idx7 + 1).toInt() == 1;
                        alarm.bottomRowLabel = "";  // Default empty
                    } else if (idx5 > 0 && idx6 > 0) {
                        // Old format with label and snooze
                        alarm.sound = data.substring(idx4 + 1, idx5);
                        alarm.label = data.substring(idx5 + 1, idx6);
                        alarm.snoozeEnabled = data.substring(idx6 + 1).toInt() == 1;
                        alarm.permanentlyDisabled = false;  // Default
                        alarm.bottomRowLabel = "";  // Default empty
                    } else {
                        // Oldest format - just sound
                        alarm.sound = data.substring(idx4 + 1);
                        alarm.label = "Alarm";  // Default label
                        alarm.snoozeEnabled = true;  // Default snooze enabled
                        alarm.permanentlyDisabled = false;  // Default
                        alarm.bottomRowLabel = "";  // Default empty
                    }

                    _alarms.push_back(alarm);
                }
            }
        }
    }
}

void AlarmManager::saveToNVS() {
    // Save each alarm
    for (const auto& alarm : _alarms) {
        String key = getAlarmKey(alarm.id);

        // Format: "hour,minute,days,enabled,sound,label,snooze,perm_disabled,bottomRowLabel"
        String data = String(alarm.hour) + "," +
                     String(alarm.minute) + "," +
                     String(alarm.daysOfWeek) + "," +
                     String(alarm.enabled ? 1 : 0) + "," +
                     alarm.sound + "," +
                     alarm.label + "," +
                     String(alarm.snoozeEnabled ? 1 : 0) + "," +
                     String(alarm.permanentlyDisabled ? 1 : 0) + "," +
                     alarm.bottomRowLabel;

        _prefs.putString(key.c_str(), data);
    }
}

String AlarmManager::getAlarmKey(uint8_t id) {
    return "alarm_" + String(id);
}

bool AlarmManager::shouldAlarmTrigger(const AlarmData& alarm, uint8_t hour, uint8_t minute, uint8_t dayOfWeek) {
    // Check time match
    if (alarm.hour != hour || alarm.minute != minute) {
        return false;
    }

    // Special case: One-time alarm (daysOfWeek == 0)
    // Triggers on ANY day at the specified time, then auto-disables
    if (alarm.daysOfWeek == 0) {
        return true;
    }

    // Check day of week (0=Sunday, 1=Monday, etc.)
    uint8_t dayBit = 1 << dayOfWeek;
    if ((alarm.daysOfWeek & dayBit) == 0) {
        return false;  // Not scheduled for this day
    }

    return true;
}
