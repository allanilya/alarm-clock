#include <Arduino.h>
#include "config.h"
#include "time_manager.h"
#include "display_manager.h"
#include "ble_time_sync.h"

// ============================================
// Global Objects
// ============================================
TimeManager timeManager;
DisplayManager displayManager;
BLETimeSync bleSync;

// ============================================
// Setup Function
// ============================================
void setup() {
    // Initialize Serial communication
    Serial.begin(SERIAL_BAUD);
    delay(1000);

    // Print startup banner
    Serial.println("\n\n========================================");
    Serial.println(PROJECT_NAME);
    Serial.print("Version: ");
    Serial.println(VERSION);
    Serial.println("========================================");
    Serial.println("Phase 2: BLE Time Sync Test");
    Serial.println("========================================\n");

    // Initialize TimeManager
    Serial.println("Initializing TimeManager...");
    if (timeManager.begin()) {
        Serial.println("TimeManager initialized!");
    } else {
        Serial.println("ERROR: Failed to initialize TimeManager!");
    }

    // Initialize DisplayManager
    Serial.println("\nInitializing DisplayManager...");
    if (displayManager.begin()) {
        Serial.println("DisplayManager initialized!");
    } else {
        Serial.println("ERROR: Failed to initialize DisplayManager!");
    }

    // Initialize BLE Time Sync
    Serial.println("\nInitializing BLE Time Sync...");
    if (bleSync.begin(BLE_DEVICE_NAME)) {
        Serial.println("BLE Time Sync initialized!");
    } else {
        Serial.println("ERROR: Failed to initialize BLE Time Sync!");
    }

    // Set BLE callback to update time
    bleSync.setTimeSyncCallback([](time_t timestamp) {
        timeManager.setTimestamp(timestamp);
        Serial.println(">>> Time synchronized from BLE!");
    });

    // Set initial status indicators
    displayManager.setBLEStatus(false);     // Will update when connected
    displayManager.setTimeSyncStatus(false); // Not synced yet

    // Display initial clock (will show default time)
    Serial.println("\nDisplaying initial clock...");
    uint8_t hour, minute, second;
    timeManager.getTime(hour, minute, second);
    displayManager.showClock(
        timeManager.getTimeString(true),  // 12-hour format with AM/PM
        timeManager.getDateString(),
        timeManager.getDayOfWeekString(),
        second
    );

    Serial.println("\n========================================");
    Serial.println("READY - Waiting for BLE time sync!");
    Serial.println("========================================");
    Serial.println("Instructions:");
    Serial.println("1. Open BLE app on your phone (LightBlue or nRF Connect)");
    Serial.println("2. Scan for 'ESP32-L Alarm'");
    Serial.println("3. Connect to the device");
    Serial.println("4. Find 'DateTime' characteristic");
    Serial.println("5. Write: YYYY-MM-DD HH:MM:SS");
    Serial.println("   Example: 2026-01-14 15:30:00");
    Serial.println("\nDisplay shows:");
    Serial.println("  - BLE: --- (not connected)");
    Serial.println("  - SYNC: ???? (not synced)");
    Serial.println("\nAfter sync, will show:");
    Serial.println("  - BLE: BLE (connected)");
    Serial.println("  - SYNC: SYNC (synced)");
    Serial.println("========================================\n");
}

// ============================================
// Loop Function
// ============================================
void loop() {
    static unsigned long lastUpdate = 0;
    static bool lastBLEStatus = false;
    unsigned long now = millis();

    // Update BLE
    bleSync.update();

    // Check if BLE connection status changed
    bool bleConnected = bleSync.isConnected();
    if (bleConnected != lastBLEStatus) {
        lastBLEStatus = bleConnected;
        displayManager.setBLEStatus(bleConnected);

        if (bleConnected) {
            Serial.println("\n>>> BLE STATUS: Connected");
        } else {
            Serial.println("\n>>> BLE STATUS: Disconnected");
        }
    }

    // Update time sync status
    displayManager.setTimeSyncStatus(timeManager.isSynced());

    // Update display every second
    if (now - lastUpdate >= 1000) {
        lastUpdate = now;

        // Get current time
        uint8_t hour, minute, second;
        timeManager.getTime(hour, minute, second);
        String timeStr = timeManager.getTimeString(true);  // 12-hour with AM/PM
        String dateStr = timeManager.getDateString();
        String dayStr = timeManager.getDayOfWeekString();

        // Update display
        displayManager.showClock(timeStr, dateStr, dayStr, second);

        // Print to serial (for debugging)
        Serial.print("Clock: ");
        Serial.print(timeStr);
        Serial.print(" | BLE: ");
        Serial.print(bleConnected ? "Connected" : "---");
        Serial.print(" | Sync: ");
        Serial.println(timeManager.isSynced() ? "YES" : "NO");
    }

    // Small delay to prevent overwhelming CPU
    delay(10);
}
