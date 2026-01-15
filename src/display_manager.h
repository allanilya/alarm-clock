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
     */
    void showAlarmRinging(const String& timeStr);

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
     * Force a full refresh on next update
     */
    void forceFullRefresh();

private:
    GxEPD2_BW<GxEPD2_370_GDEY037T03, GxEPD2_370_GDEY037T03::HEIGHT>* _display;
    bool _initialized;
    bool _bleConnected;
    bool _timeSynced;
    unsigned long _lastFullRefresh;
    bool _forceFullRefresh;
    String _lastTimeStr;

    static const unsigned long FULL_REFRESH_INTERVAL = 3600000;  // 1 hour

    void partialUpdate();
    void fullUpdate();
    void drawStatusIcons();
};

#endif // DISPLAY_MANAGER_H
