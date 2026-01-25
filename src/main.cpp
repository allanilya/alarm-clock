#include <Arduino.h>
#include <Preferences.h>
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
Button button(BUTTON_PIN, 1);  // 1ms debounce for better sensitivity
AudioTest audioObj;
FileManager fileManager;
FrontlightManager frontlightManager;

// ============================================
// Button Sound State
// ============================================
String buttonSoundFile = "";  // Filename of button press sound (empty = disabled)
String buttonSoundPath = "";  // Full path to button sound file (cached for performance)
uint8_t savedBrightnessBeforeAlarm = 255;  // Saved brightness before alarm boost (255 = not set)

// Button sound PCM buffer (for instant playback of preloaded WAV files)
uint8_t* buttonSoundPCMBuffer = nullptr;  // PCM data in PSRAM
size_t buttonSoundPCMSize = 0;            // Size of PCM data in bytes
uint32_t buttonSoundSampleRate = 44100;   // Sample rate
uint8_t buttonSoundBits = 16;             // Bits per sample (8 or 16)
uint8_t buttonSoundChannels = 2;          // Channels (1=mono, 2=stereo)

// ============================================
// WAV File Parsing Structures
// ============================================
struct WAVHeader {
    // RIFF chunk
    char riffID[4];       // "RIFF"
    uint32_t riffSize;    // File size - 8
    char waveID[4];       // "WAVE"

    // fmt chunk
    char fmtID[4];        // "fmt "
    uint32_t fmtSize;     // fmt chunk size (16 for PCM)
    uint16_t audioFormat; // Audio format (1 = PCM)
    uint16_t numChannels; // Number of channels
    uint32_t sampleRate;  // Sample rate
    uint32_t byteRate;    // Byte rate
    uint16_t blockAlign;  // Block align
    uint16_t bitsPerSample; // Bits per sample

    // data chunk
    char dataID[4];       // "data"
    uint32_t dataSize;    // Size of PCM data
};

/**
 * Parse WAV file header and extract PCM parameters
 * Returns true if valid WAV file, false otherwise
 */
bool parseWAVHeader(File& file, uint32_t& sampleRate, uint8_t& bits, uint8_t& channels, size_t& pcmDataSize, size_t& pcmDataOffset) {
    // Read RIFF header
    char riffID[4];
    file.read((uint8_t*)riffID, 4);
    if (memcmp(riffID, "RIFF", 4) != 0) {
        Serial.println("ERROR: Not a RIFF file");
        return false;
    }

    file.read((uint8_t*)&pcmDataSize, 4);  // File size (not used)

    char waveID[4];
    file.read((uint8_t*)waveID, 4);
    if (memcmp(waveID, "WAVE", 4) != 0) {
        Serial.println("ERROR: Not a WAVE file");
        return false;
    }

    // Find fmt chunk
    char chunkID[4];
    uint32_t chunkSize;
    bool foundFmt = false;

    while (file.available()) {
        file.read((uint8_t*)chunkID, 4);
        file.read((uint8_t*)&chunkSize, 4);

        if (memcmp(chunkID, "fmt ", 4) == 0) {
            foundFmt = true;

            // Read fmt chunk data
            uint16_t audioFormat;
            file.read((uint8_t*)&audioFormat, 2);
            if (audioFormat != 1) {
                Serial.printf("ERROR: Unsupported audio format: %d (only PCM supported)\n", audioFormat);
                return false;
            }

            uint16_t numChannels;
            file.read((uint8_t*)&numChannels, 2);
            channels = (uint8_t)numChannels;

            file.read((uint8_t*)&sampleRate, 4);

            uint32_t byteRate;
            file.read((uint8_t*)&byteRate, 4);

            uint16_t blockAlign;
            file.read((uint8_t*)&blockAlign, 2);

            uint16_t bitsPerSample;
            file.read((uint8_t*)&bitsPerSample, 2);
            bits = (uint8_t)bitsPerSample;

            // Skip any extra fmt bytes
            if (chunkSize > 16) {
                file.seek(file.position() + (chunkSize - 16));
            }

            break;
        } else {
            // Skip this chunk
            file.seek(file.position() + chunkSize);
        }
    }

    if (!foundFmt) {
        Serial.println("ERROR: fmt chunk not found");
        return false;
    }

    // Find data chunk
    while (file.available()) {
        file.read((uint8_t*)chunkID, 4);
        file.read((uint8_t*)&chunkSize, 4);

        if (memcmp(chunkID, "data", 4) == 0) {
            pcmDataSize = chunkSize;
            pcmDataOffset = file.position();

            Serial.printf("WAV: %dHz, %d-bit, %d-channel, %d bytes PCM\n",
                         sampleRate, bits, channels, pcmDataSize);
            return true;
        } else {
            // Skip this chunk
            file.seek(file.position() + chunkSize);
        }
    }

    Serial.println("ERROR: data chunk not found");
    return false;
}

/**
 * Load WAV file into PSRAM buffer for instant playback
 * Returns true if successful, false otherwise
 */
bool loadButtonSoundWAV(const String& filePath) {
    // Free any existing buffer
    if (buttonSoundPCMBuffer != nullptr) {
        free(buttonSoundPCMBuffer);
        buttonSoundPCMBuffer = nullptr;
        buttonSoundPCMSize = 0;
    }

    // Strip /spiffs prefix if present
    String spiffsPath = filePath;
    if (spiffsPath.startsWith("/spiffs")) {
        spiffsPath = spiffsPath.substring(7);
    }

    // Open file
    File file = SPIFFS.open(spiffsPath, "r");
    if (!file) {
        Serial.printf("ERROR: Could not open WAV file: %s\n", spiffsPath.c_str());
        return false;
    }

    // Parse WAV header
    uint32_t sampleRate;
    uint8_t bits, channels;
    size_t pcmDataSize, pcmDataOffset;

    if (!parseWAVHeader(file, sampleRate, bits, channels, pcmDataSize, pcmDataOffset)) {
        file.close();
        return false;
    }

    // Validate parameters
    if (bits != 8 && bits != 16) {
        Serial.printf("ERROR: Unsupported bit depth: %d (only 8 or 16 supported)\n", bits);
        file.close();
        return false;
    }

    if (channels != 1 && channels != 2) {
        Serial.printf("ERROR: Unsupported channels: %d (only 1 or 2 supported)\n", channels);
        file.close();
        return false;
    }

    // Check 5-second limit (5 seconds * sample rate * channels * bytes per sample)
    size_t maxSize = 5 * sampleRate * channels * (bits / 8);
    if (pcmDataSize > maxSize) {
        Serial.printf("WARNING: WAV file exceeds 5 seconds (%d bytes, max %d bytes)\n",
                     pcmDataSize, maxSize);
        pcmDataSize = maxSize;  // Truncate to 5 seconds
    }

    // Allocate PSRAM buffer
    buttonSoundPCMBuffer = (uint8_t*)ps_malloc(pcmDataSize);
    if (buttonSoundPCMBuffer == nullptr) {
        Serial.printf("ERROR: Failed to allocate %d bytes of PSRAM for button sound\n", pcmDataSize);
        file.close();
        return false;
    }

    // Read PCM data into buffer
    file.seek(pcmDataOffset);
    size_t bytesRead = file.read(buttonSoundPCMBuffer, pcmDataSize);
    file.close();

    if (bytesRead != pcmDataSize) {
        Serial.printf("ERROR: Failed to read PCM data (expected %d, got %d)\n",
                     pcmDataSize, bytesRead);
        free(buttonSoundPCMBuffer);
        buttonSoundPCMBuffer = nullptr;
        return false;
    }

    // Store parameters
    buttonSoundPCMSize = pcmDataSize;
    buttonSoundSampleRate = sampleRate;
    buttonSoundBits = bits;
    buttonSoundChannels = channels;

    Serial.printf("Button sound WAV preloaded: %d bytes in PSRAM\n", pcmDataSize);
    return true;
}

// ============================================
// FreeRTOS Audio Task
// ============================================
// Dedicated task for continuous MP3 decoding
// Runs independently from main loop to prevent choppy playback
void audioTask(void* pvParameters) {
    Serial.println(">>> AUDIO TASK: Started");
    while (true) {
        audioObj.loop();  // Keep MP3 decoder buffer full
        vTaskDelay(1);    // Short delay to yield CPU (1ms)
    }
}

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

        // Boost brightness to 100% during alarm (temporarily, without saving to NVS)
        // Only save brightness if not already saved (prevents overwriting with boosted value)
        if (savedBrightnessBeforeAlarm == 255) {
            savedBrightnessBeforeAlarm = frontlightManager.getBrightness();
            Serial.printf(">>> ALARM: Saved current brightness: %d%%\n", savedBrightnessBeforeAlarm);
        }
        frontlightManager.setBrightnessTemporary(100);
        Serial.printf(">>> ALARM: Brightness boosted to 100%%\n");

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
                    // Give audio task 100ms to prime the decoder
                    delay(100);
                    Serial.println(">>> AUDIO: File playback started, audio task priming decoder");
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

        // Create dedicated FreeRTOS task for continuous MP3 decoding
        // Task name: "AudioTask", Stack: 4KB, Priority: 2 (higher than idle)
        xTaskCreate(
            audioTask,      // Task function
            "AudioTask",    // Task name (for debugging)
            4096,           // Stack size (4KB)
            NULL,           // Task parameters (none)
            2,              // Priority (2 = above normal, below critical tasks)
            NULL            // Task handle (not needed)
        );
        Serial.println("Audio task created!");
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

    // Load button sound from NVS
    Serial.println("\nLoading button sound setting from NVS...");
    Preferences buttonPrefs;
    buttonPrefs.begin("button", true);  // Read-only
    buttonSoundFile = buttonPrefs.getString("sound", "");
    buttonPrefs.end();
    if (buttonSoundFile.length() > 0) {
        // Cache the full path for fast playback (avoid file system checks in hot path)
        buttonSoundPath = String(ALARM_SOUNDS_DIR) + "/" + buttonSoundFile;
        Serial.printf("Button sound loaded: %s\n", buttonSoundFile.c_str());

        // Check if it's a WAV file - preload into PSRAM for instant playback
        String lowerPath = buttonSoundFile;
        lowerPath.toLowerCase();
        if (lowerPath.endsWith(".wav")) {
            Serial.println("Preloading WAV file into PSRAM for instant playback...");
            if (loadButtonSoundWAV(buttonSoundPath)) {
                Serial.println("WAV preloading successful!");
            } else {
                Serial.println("WAV preloading failed - will use normal file playback");
            }
        } else if (lowerPath.endsWith(".mp3")) {
            Serial.println("MP3 file - will use streaming playback (~2 second delay)");
        }
    } else {
        buttonSoundPath = "";
        Serial.println("Button sound: disabled (no sound set)");
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
    static uint8_t alarmStartVolume = 0;  // Capture volume when alarm starts
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
    // Store button states to avoid consuming flags multiple times
    bool buttonWasPressed = button.wasPressed();
    bool buttonWasDoubleClicked = button.wasDoubleClicked();

    // Play button sound on any button press (if configured)
    if ((buttonWasPressed || buttonWasDoubleClicked) && buttonSoundPath.length() > 0) {
        audioObj.stop();
        if (audioObj.getCurrentSoundType() == SOUND_TYPE_FILE) {
            audioObj.stopFile();
        }

        // Check if we have a preloaded PCM buffer (instant playback for WAV files)
        if (buttonSoundPCMBuffer != nullptr && buttonSoundPCMSize > 0) {
            // Instant playback from PSRAM (~10-30ms latency)
            audioObj.playPCMBuffer(buttonSoundPCMBuffer, buttonSoundPCMSize,
                                  buttonSoundSampleRate, buttonSoundBits, buttonSoundChannels);
            Serial.printf(">>> BUTTON SOUND: Playing WAV from PSRAM (%d bytes)\n", buttonSoundPCMSize);
        } else {
            // Fall back to file playback (MP3 or WAV that failed to preload)
            audioObj.playFile(buttonSoundPath, false);  // Non-looping
            Serial.printf(">>> BUTTON SOUND: Playing file %s (streaming)\n", buttonSoundFile.c_str());
        }
    }

    // CRITICAL: Check double-click for BOTH ringing and snoozed alarms
    if (buttonWasDoubleClicked) {
        // Double-click ALWAYS dismisses alarm (whether ringing or snoozed)
        if (alarmManager.isAlarmRinging() || alarmManager.isAlarmSnoozed()) {
            alarmManager.dismissAlarm();
            audioObj.stop();
            lastToneStart = 0;
            pendingSingleClickTime = 0;  // Cancel any pending snooze
            Serial.println("\n>>> BUTTON: ===== ALARM DISMISSED (double-click) =====");
            Serial.println(">>> AUDIO: Stopped");

            // Restore brightness
            if (savedBrightnessBeforeAlarm != 255) {
                frontlightManager.setBrightness(savedBrightnessBeforeAlarm);
                Serial.printf(">>> ALARM DISMISSED: Brightness restored to %d%%\n", savedBrightnessBeforeAlarm);
                savedBrightnessBeforeAlarm = 255;  // Reset to "not set"
            }
        }
    }
    else if (alarmManager.isAlarmRinging() && buttonWasPressed) {
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

            // Restore brightness
            if (savedBrightnessBeforeAlarm != 255) {
                frontlightManager.setBrightness(savedBrightnessBeforeAlarm);
                Serial.printf(">>> ALARM SNOOZED: Brightness restored to %d%%\n", savedBrightnessBeforeAlarm);
                savedBrightnessBeforeAlarm = 255;  // Reset to "not set"
            }
        }
        pendingSingleClickTime = 0;  // Clear pending state
    }

    // Handle alarm audio (runs every loop for responsiveness)
    if (alarmManager.isAlarmRinging()) {
        // If alarm just started, initialize timer and show alarm display
        if (!wasRingingLastLoop) {
            alarmStartVolume = audioObj.getVolume();  // Capture volume at alarm start
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
                    // Temporarily set volume to what it was when alarm started
                    uint8_t currentUserVolume = audioObj.getVolume();
                    audioObj.setVolume(alarmStartVolume);

                    // Use distinct frequencies: low (262), middle (440), high (880)
                    uint16_t frequency = (alarm.sound == "tone2") ? 440 :
                                       (alarm.sound == "tone3") ? 880 : 262;
                    audioObj.playTone(frequency, 50);  // 50ms burst

                    // Restore user's current volume setting
                    audioObj.setVolume(currentUserVolume);
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

    // Handle test sound requests from BLE (queued to prevent stack overflow in BLE callback)
    if (bleSync.hasTestSoundRequest()) {
        String soundFile = bleSync.getPendingTestSound();
        Serial.printf(">>> MAIN: Processing test sound request: %s\n", soundFile.c_str());

        // Stop any current playback first
        audioObj.stop();
        if (audioObj.getCurrentSoundType() == SOUND_TYPE_FILE) {
            audioObj.stopFile();
        }

        // Play the test sound
        String filePath = String(ALARM_SOUNDS_DIR) + "/" + soundFile;
        if (fileManager.fileExists(filePath)) {
            Serial.printf(">>> MAIN: Playing test file: %s\n", soundFile.c_str());
            audioObj.playFile(filePath, false);  // Don't loop test sounds
            // Give audio task 100ms to prime the decoder (task runs every 1ms)
            delay(100);
            Serial.println(">>> MAIN: File playback started, audio task priming decoder");
        } else {
            Serial.printf(">>> MAIN: Test file not found: %s\n", soundFile.c_str());
        }
    }

    // Handle serial commands for debugging
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();

        if (command.startsWith("b")) {
            // Brightness command: b0 to b100
            int brightness = command.substring(1).toInt();
            if (brightness >= 0 && brightness <= 100) {
                frontlightManager.setBrightness(brightness);
                Serial.printf(">>> SERIAL: Set brightness to %d%%\n", brightness);
            } else {
                Serial.println(">>> SERIAL: ERROR - Brightness must be 0-100");
            }
        } else if (command.startsWith("v")) {
            // Volume command: v0 to v100
            int volume = command.substring(1).toInt();
            if (volume >= 0 && volume <= 100) {
                audioObj.setVolume(volume);
                Serial.printf(">>> SERIAL: Set volume to %d%%\n", volume);
            } else {
                Serial.println(">>> SERIAL: ERROR - Volume must be 0-100");
            }
        } else if (command == "restart" || command == "r") {
            Serial.println(">>> SERIAL: Restarting ESP32...");
            delay(500);
            ESP.restart();
        } else if (command == "help") {
            Serial.println(">>> SERIAL COMMANDS:");
            Serial.println("  b<0-100>  - Set brightness (e.g., b50 for 50%)");
            Serial.println("  v<0-100>  - Set volume (e.g., v75 for 75%)");
            Serial.println("  restart   - Restart ESP32 (clears BLE cache)");
            Serial.println("  help      - Show this help message");
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

    // Audio decoding now handled by dedicated FreeRTOS task (audioTask)
    // No need to call audioObj.loop() here - task runs continuously

    // Small delay to prevent overwhelming CPU
    delay(10);  // Normal delay (audio task handles MP3 decoding independently)
}
