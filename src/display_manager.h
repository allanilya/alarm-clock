#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include "config.h"

/**
 * DisplayManager - E-ink display abstraction with smart refresh logic
 *
 * Manages the 3.7" GDEY037T03 e-ink display with efficient partial updates
 * for time changes and periodic full refreshes to prevent ghosting.
 */
class DisplayManager {
public:
    DisplayManager();

    /**
     * Initialize the display
     * @return true if successful
     */
    bool begin();

    /**
     * Show main clock screen
     * @param timeStr Time string (e.g., "12:34" or "3:45 PM")
     * @param dateStr Date string (e.g., "Jan 14, 2026")
     * @param dayStr Day of week string (e.g., "Wednesday")
     * @param second Current second (0-59) for analog seconds indicator
     */
    void showClock(const String& timeStr, const String& dateStr, const String& dayStr, uint8_t second);

    /**
     * Show alarm ringing screen
     * @param timeStr Current time
     * @param alarmLabel Alarm label to display (e.g., "Morning Routine")
     * @param bottomRowLabel Custom bottom row text (or empty to show instructions)
     */
    void showAlarmRinging(const String& timeStr, const String& alarmLabel, const String& bottomRowLabel);

    /**
     * Set BLE connection status
     * @param connected true if BLE connected
     */
    void setBLEStatus(bool connected);

    /**
     * Set time sync status
     * @param synced true if time has been synced
     */
    void setTimeSyncStatus(bool synced);

    /**
     * Set alarm status (replaces sync indicator)
     * @param status "ALARM" if alarm set, "SNOOZE" if snoozed, "" if none
     */
    void setAlarmStatus(const String& status);

    /**
     * Set custom message for top row of display
     * @param message Custom message (max 50 chars, empty string to disable)
     */
    void setCustomMessage(const String& message);

    /**
     * Get current custom message
     * @return Custom message string
     */
    String getCustomMessage() const;

    /**
     * Set custom label for bottom row of display
     * @param label Custom label (max 50 chars, empty string to disable)
     */
    void setBottomRowLabel(const String& label);

    /**
     * Get current bottom row label
     * @return Bottom row label string
     */
    String getBottomRowLabel() const;

    /**
     * Force a full refresh on next update
     */
    void forceFullRefresh();

private:
    GxEPD2_BW<GxEPD2_370_GDEY037T03, GxEPD2_370_GDEY037T03::HEIGHT>* _display;
    bool _initialized;
    bool _bleConnected;
    bool _timeSynced;
    String _alarmStatus;  // "ALARM", "SNOOZE", or ""
    String _customMessage;  // Custom message for top row (empty = use day of week)
    String _bottomRowLabel;  // Custom label for bottom row (empty = use default layout)
    unsigned long _lastFullRefresh;
    bool _forceFullRefresh;
    String _lastTimeStr;
    
    // Scrolling state for long messages
    int _scrollPixelOffset;       // Current scroll position (in pixels)
    unsigned long _lastScrollTime;  // Last time scroll was updated
    static const unsigned long SCROLL_DELAY = 0;  // Milliseconds between scroll steps
    static const int SCROLL_SPEED = 25;  // Pixels to scroll per update

    static const unsigned long FULL_REFRESH_INTERVAL = 3600000;  // 1 hour

    void partialUpdate();
    void fullUpdate();
    void drawStatusIcons();
};

#endif // DISPLAY_MANAGER_H
