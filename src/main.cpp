#include <Arduino.h>
#include "config.h"
#include "time_manager.h"
#include "display_manager.h"
#include "ble_time_sync.h"
#include "alarm_manager.h"
#include "button.h"
#include "audio_test.h"
#include "file_manager.h"
#include "frontlight_manager.h"

// ============================================
// Global Objects
// ============================================
TimeManager timeManager;
DisplayManager displayManager;
BLETimeSync bleSync;
AlarmManager alarmManager;
Button button(BUTTON_PIN);
AudioTest audioObj;
FileManager fileManager;
FrontlightManager frontlightManager;

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
    Serial.println(PROJECT_VERSION);
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

    // Initialize AlarmManager
    Serial.println("\nInitializing AlarmManager...");
    if (alarmManager.begin()) {
        Serial.println("AlarmManager initialized!");
    } else {
        Serial.println("ERROR: Failed to initialize AlarmManager!");
    }

    // Set alarm callback
    alarmManager.setAlarmCallback([](uint8_t alarmId) {
        Serial.print(">>> ALARM CALLBACK: Alarm ");
        Serial.print(alarmId);
        Serial.println(" is ringing!");

        // Get alarm data to determine which sound to play
        AlarmData alarm;
        if (alarmManager.getAlarm(alarmId, alarm)) {
            // Check if it's a built-in tone or custom file
            if (alarm.sound == "tone1" || alarm.sound == "tone2" || alarm.sound == "tone3") {
                // Play built-in tone
                uint16_t frequency;
                if (alarm.sound == "tone2") {
                    frequency = 440;  // A4 note (middle)
                } else if (alarm.sound == "tone3") {
                    frequency = 880;  // A5 note (high)
                } else {
                    frequency = 262;  // C4 note (low, default tone1)
                }

                // Play very short tone burst (non-blocking approach)
                audioObj.playTone(frequency, 50);  // 50ms burst only
                Serial.print(">>> AUDIO: Playing tone at ");
                Serial.print(frequency);
                Serial.println(" Hz (50ms burst)");
            } else {
                // Try to play custom sound file from SPIFFS
                String filePath = String(ALARM_SOUNDS_DIR) + "/" + alarm.sound;
                if (fileManager.fileExists(filePath)) {
                    Serial.printf(">>> AUDIO: Playing custom sound file: %s\n", alarm.sound.c_str());
                    audioObj.playFile(filePath, true);  // Loop continuously
                } else {
                    // File not found - fallback to tone1
                    Serial.printf(">>> AUDIO: File not found '%s', using tone1 fallback\n", alarm.sound.c_str());
                    audioObj.playTone(262, 50);  // Fallback to tone1
                }
            }
        }
    });

    // Initialize Button
    Serial.println("\nInitializing Button...");
    button.begin();
    Serial.println("Button initialized!");

    // Initialize Audio
    Serial.println("\nInitializing Audio...");
    if (audioObj.begin()) {
        Serial.println("Audio initialized!");
    } else {
        Serial.println("ERROR: Failed to initialize Audio!");
    }

    // Initialize FileManager (for custom alarm sounds)
    Serial.println("\nInitializing FileManager (SPIFFS)...");
    if (fileManager.begin()) {
        Serial.println("FileManager initialized!");
        // List available sound files
        std::vector<String> sounds = fileManager.listSounds();
        if (sounds.size() > 0) {
            Serial.printf("Found %d custom sound file(s):\n", sounds.size());
            for (const String& sound : sounds) {
                Serial.printf("  - %s\n", sound.c_str());
            }
        } else {
            Serial.println("No custom sound files found (upload via PlatformIO)");
        }

        // Update BLE file list now that FileManager is ready
        Serial.println("\nUpdating BLE file list...");
        bleSync.updateFileList();
    } else {
        Serial.println("ERROR: Failed to initialize FileManager!");
    }

    // Initialize FrontlightManager (PWM control for e-ink frontlight)
    Serial.println("\nInitializing FrontlightManager...");
    if (frontlightManager.begin()) {
        Serial.println("FrontlightManager initialized!");
    } else {
        Serial.println("ERROR: Failed to initialize FrontlightManager!");
    }

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
    static unsigned long lastToneStart = 0;  // Track when tone was started
    static bool wasRingingLastLoop = false;  // Track alarm state
    static bool displayUpdatedForAlarm = false;  // Track if alarm display shown
    static unsigned long pendingSingleClickTime = 0;  // Track pending snooze
    unsigned long now = millis();

    // Update BLE
    bleSync.update();

    // Update button
    button.update();

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

    // Update alarm status display (replaces sync indicator)
    if (alarmManager.isAlarmSnoozed()) {
        displayManager.setAlarmStatus("SNOOZE");
    } else if (alarmManager.hasEnabledAlarm()) {
        displayManager.setAlarmStatus("ALARM");
    } else {
        displayManager.setAlarmStatus("");
    }

    // Handle button presses for alarm control
    // CRITICAL: Check double-click for BOTH ringing and snoozed alarms
    if (button.wasDoubleClicked()) {
        // Double-click ALWAYS dismisses alarm (whether ringing or snoozed)
        if (alarmManager.isAlarmRinging() || alarmManager.isAlarmSnoozed()) {
            alarmManager.dismissAlarm();
            audioObj.stop();
            lastToneStart = 0;
            pendingSingleClickTime = 0;  // Cancel any pending snooze
            Serial.println("\n>>> BUTTON: ===== ALARM DISMISSED (double-click) =====");
            Serial.println(">>> AUDIO: Stopped");
            // Consume any pending single press flag
            button.wasPressed();
        }
    }
    else if (alarmManager.isAlarmRinging() && button.wasPressed()) {
        // Single press detected - record time but DON'T execute snooze yet
        // Wait 700ms to see if a second click comes (double-click)
        pendingSingleClickTime = now;
        Serial.println("\n>>> BUTTON: Single press detected - waiting for potential double-click...");
    }

    // Execute pending snooze if 700ms has passed without a double-click
    if (pendingSingleClickTime > 0 && (now - pendingSingleClickTime) >= 700) {
        // Only execute if alarm is still ringing (not already dismissed)
        if (alarmManager.isAlarmRinging()) {
            alarmManager.snoozeAlarm();
            audioObj.stop();
            lastToneStart = 0;
            Serial.println(">>> BUTTON: Alarm snoozed for 5 minutes (single press confirmed after timeout)");
            Serial.println(">>> AUDIO: Stopped");
        }
        pendingSingleClickTime = 0;  // Clear pending state
    }

    // Handle alarm audio (runs every loop for responsiveness)
    if (alarmManager.isAlarmRinging()) {
        // If alarm just started, initialize timer and show alarm display
        if (!wasRingingLastLoop) {
            lastToneStart = 0;  // Force immediate play
            wasRingingLastLoop = true;
            displayUpdatedForAlarm = false;  // Need to show alarm screen

            // Show alarm screen immediately (only once)
            uint8_t hour, minute, second;
            timeManager.getTime(hour, minute, second);
            String timeStr = timeManager.getTimeString(true);

            // Get alarm label and bottom row label to display
            uint8_t alarmId = alarmManager.getRingingAlarmId();
            AlarmData alarm;
            String alarmLabel = "ALARM";  // Default fallback
            String bottomRowLabel = "";    // Default empty (shows instructions)
            if (alarmManager.getAlarm(alarmId, alarm)) {
                alarmLabel = alarm.label;
                bottomRowLabel = alarm.bottomRowLabel;
            }

            displayManager.showAlarmRinging(timeStr, alarmLabel, bottomRowLabel);
            displayUpdatedForAlarm = true;
        }

        // Play audio bursts for tone alarms (file alarms loop automatically)
        if (now - lastToneStart >= 60) {  // Restart every 60ms
            uint8_t alarmId = alarmManager.getRingingAlarmId();
            AlarmData alarm;
            if (alarmManager.getAlarm(alarmId, alarm)) {
                // Only play bursts for built-in tones (file playback handles looping)
                if (alarm.sound == "tone1" || alarm.sound == "tone2" || alarm.sound == "tone3") {
                    // Use distinct frequencies: low (262), middle (440), high (880)
                    uint16_t frequency = (alarm.sound == "tone2") ? 440 :
                                       (alarm.sound == "tone3") ? 880 : 262;
                    audioObj.playTone(frequency, 50);  // 50ms burst
                }
                // For file playback, Audio library handles looping automatically
            }
            lastToneStart = now;
        }
    } else {
        // Reset state when alarm stops
        if (wasRingingLastLoop) {
            wasRingingLastLoop = false;
            lastToneStart = 0;
            displayUpdatedForAlarm = false;

            // Force display update to return to clock
            lastUpdate = 0;
        }
    }

    // Update display every second (only for normal clock, not alarm screen)
    // Skip display updates during file transfers to avoid blocking BLE
    if (now - lastUpdate >= 1000 && !bleSync.isFileTransferring()) {
        lastUpdate = now;

        // Get current time
        uint8_t hour, minute, second;
        timeManager.getTime(hour, minute, second);
        String timeStr = timeManager.getTimeString(true);  // 12-hour with AM/PM
        String dateStr = timeManager.getDateString();
        String dayStr = timeManager.getDayOfWeekString();

        // Check alarms (gets day of week from tm struct)
        struct tm timeinfo;
        time_t now_t = time(nullptr);
        localtime_r(&now_t, &timeinfo);
        alarmManager.checkAlarms(hour, minute, timeinfo.tm_wday);

        // Force full refresh at 3 AM to prevent ghosting (once per day)
        if (hour == 3 && minute == 0) {
            displayManager.forceFullRefresh();
        }

        // Only update display if not showing alarm (alarm display updates once above)
        if (!alarmManager.isAlarmRinging()) {
            displayManager.showClock(timeStr, dateStr, dayStr, second);
        }

        // Print to serial (for debugging)
        Serial.print("Clock: ");
        Serial.print(timeStr);
        Serial.print(" | BLE: ");
        Serial.print(bleConnected ? "Connected" : "---");
        Serial.print(" | Sync: ");
        Serial.print(timeManager.isSynced() ? "YES" : "NO");
        Serial.print(" | Alarm: ");
        Serial.println(alarmManager.isAlarmRinging() ? "RINGING" : "---");
    }

    // Call Audio library loop for file playback processing
    audioObj.loop();  // Process MP3/WAV decoding every loop iteration

    // Small delay to prevent overwhelming CPU
    // BUT: Reduce delay when playing MP3/WAV files for smooth playback
    if (audioObj.getCurrentSoundType() == SOUND_TYPE_FILE) {
        delay(1);  // Minimal delay for smooth MP3 decoding
    } else {
        delay(10);  // Normal delay when not playing files
    }
}
