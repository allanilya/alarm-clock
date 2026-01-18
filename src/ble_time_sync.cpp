#include "ble_time_sync.h"
#include "alarm_manager.h"
#include "audio_test.h"
#include "file_manager.h"

// External references
extern AlarmManager alarmManager;
extern AudioTest audioObj;
extern FileManager fileManager;

// BLE Service UUID: Custom time sync service
const char* BLETimeSync::SERVICE_UUID = "12340000-1234-5678-1234-56789abcdef0";
const char* BLETimeSync::TIME_CHAR_UUID = "12340001-1234-5678-1234-56789abcdef0";
const char* BLETimeSync::DATETIME_CHAR_UUID = "12340002-1234-5678-1234-56789abcdef0";
const char* BLETimeSync::VOLUME_CHAR_UUID = "12340003-1234-5678-1234-56789abcdef0";
const char* BLETimeSync::TEST_SOUND_CHAR_UUID = "12340004-1234-5678-1234-56789abcdef0";

// BLE Alarm Service UUID: Custom alarm management service
const char* BLETimeSync::ALARM_SERVICE_UUID = "12340010-1234-5678-1234-56789abcdef0";
const char* BLETimeSync::ALARM_SET_CHAR_UUID = "12340011-1234-5678-1234-56789abcdef0";
const char* BLETimeSync::ALARM_LIST_CHAR_UUID = "12340012-1234-5678-1234-56789abcdef0";
const char* BLETimeSync::ALARM_DELETE_CHAR_UUID = "12340013-1234-5678-1234-56789abcdef0";

BLETimeSync::BLETimeSync()
    : _pServer(nullptr),
      _pTimeService(nullptr),
      _pAlarmService(nullptr),
      _pTimeCharacteristic(nullptr),
      _pDateTimeCharacteristic(nullptr),
      _pVolumeCharacteristic(nullptr),
      _pTestSoundCharacteristic(nullptr),
      _pAlarmSetCharacteristic(nullptr),
      _pAlarmListCharacteristic(nullptr),
      _pAlarmDeleteCharacteristic(nullptr),
      _deviceConnected(false),
      _connectionCount(0),
      _timeSyncCallback(nullptr) {
}

bool BLETimeSync::begin(const char* deviceName) {
    Serial.println("BLETimeSync: Initializing BLE...");

    // Initialize BLE
    BLEDevice::init(deviceName);

    // Create BLE Server
    _pServer = BLEDevice::createServer();
    _pServer->setCallbacks(new ServerCallbacks(this));

    // Create BLE Service
    _pTimeService = _pServer->createService(SERVICE_UUID);

    // Create Time Characteristic (Unix timestamp - 32-bit integer)
    _pTimeCharacteristic = _pTimeService->createCharacteristic(
        TIME_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );
    _pTimeCharacteristic->setCallbacks(new TimeCharCallbacks(this));
    _pTimeCharacteristic->addDescriptor(new BLE2902());

    // Create DateTime Characteristic (String format: "YYYY-MM-DD HH:MM:SS")
    _pDateTimeCharacteristic = _pTimeService->createCharacteristic(
        DATETIME_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );
    _pDateTimeCharacteristic->setCallbacks(new DateTimeCharCallbacks(this));
    _pDateTimeCharacteristic->addDescriptor(new BLE2902());

    // Create Volume Characteristic (0-100%)
    _pVolumeCharacteristic = _pTimeService->createCharacteristic(
        VOLUME_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );
    _pVolumeCharacteristic->setCallbacks(new VolumeCharCallbacks(this));
    _pVolumeCharacteristic->addDescriptor(new BLE2902());
    uint32_t initialVolume = (uint32_t)audioObj.getVolume();
    _pVolumeCharacteristic->setValue(initialVolume);

    // Create Test Sound Characteristic (Write to trigger 2-second test tone)
    _pTestSoundCharacteristic = _pTimeService->createCharacteristic(
        TEST_SOUND_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    _pTestSoundCharacteristic->setCallbacks(new TestSoundCharCallbacks(this));

    // Set initial value
    time_t currentTime = time(nullptr);
    uint32_t timeValue = (uint32_t)currentTime;
    _pTimeCharacteristic->setValue(timeValue);

    // Start the time service
    _pTimeService->start();

    // Create BLE Alarm Service
    _pAlarmService = _pServer->createService(ALARM_SERVICE_UUID);

    // Create Alarm Set Characteristic (JSON: {"id":0,"hour":7,"minute":30,"days":127,"sound":"tone1","enabled":true,"label":"Alarm","snooze":true})
    _pAlarmSetCharacteristic = _pAlarmService->createCharacteristic(
        ALARM_SET_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    _pAlarmSetCharacteristic->setCallbacks(new AlarmSetCharCallbacks(this));

    // Create Alarm List Characteristic (Read: JSON array of all alarms)
    _pAlarmListCharacteristic = _pAlarmService->createCharacteristic(
        ALARM_LIST_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ
    );

    // Create Alarm Delete Characteristic (Write: alarm ID to delete)
    _pAlarmDeleteCharacteristic = _pAlarmService->createCharacteristic(
        ALARM_DELETE_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    _pAlarmDeleteCharacteristic->setCallbacks(new AlarmDeleteCharCallbacks(this));

    // Start the alarm service
    _pAlarmService->start();

    // NOTE: Don't send alarm list initially - iOS will push its alarms on connection
    // updateAlarmList();  // Disabled: iOS is source of truth and will push on connect

    // Start advertising
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->addServiceUUID(ALARM_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("BLETimeSync: BLE services started!");
    Serial.print("BLETimeSync: Device name: ");
    Serial.println(deviceName);
    Serial.println("\nTime Service:");
    Serial.println("  - DateTime characteristic: Write YYYY-MM-DD HH:MM:SS");
    Serial.println("\nAlarm Service:");
    Serial.println("  - AlarmSet: Write JSON alarm data");
    Serial.println("  - AlarmList: Read JSON array of alarms");
    Serial.println("  - AlarmDelete: Write alarm ID to delete\n");

    return true;
}

void BLETimeSync::update() {
    // Nothing to do in loop for now
    // Future: Could notify time updates
}

bool BLETimeSync::isConnected() {
    return _deviceConnected;
}

void BLETimeSync::setTimeSyncCallback(TimeSyncCallback callback) {
    _timeSyncCallback = callback;
}

uint32_t BLETimeSync::getConnectionCount() {
    return _connectionCount;
}

void BLETimeSync::updateAlarmList() {
    if (!_pAlarmListCharacteristic) return;

    // Build JSON array of all alarms
    String json = "[";
    std::vector<AlarmData> alarms = alarmManager.getAllAlarms();

    for (size_t i = 0; i < alarms.size(); i++) {
        if (i > 0) json += ",";

        json += "{\"id\":";
        json += String(alarms[i].id);
        json += ",\"hour\":";
        json += String(alarms[i].hour);
        json += ",\"minute\":";
        json += String(alarms[i].minute);
        json += ",\"days\":";
        json += String(alarms[i].daysOfWeek);
        json += ",\"sound\":\"";
        json += alarms[i].sound;
        json += "\",\"enabled\":";
        json += alarms[i].enabled ? "true" : "false";
        json += ",\"label\":\"";
        json += alarms[i].label;
        json += "\",\"snooze\":";
        json += alarms[i].snoozeEnabled ? "true" : "false";
        json += ",\"perm_disabled\":";
        json += alarms[i].permanentlyDisabled ? "true" : "false";
        json += "}";
    }

    json += "]";

    _pAlarmListCharacteristic->setValue(json.c_str());
    Serial.print("BLE: Updated alarm list (");
    Serial.print(alarms.size());
    Serial.println(" alarms)");
}

// ============================================
// Server Callbacks
// ============================================

void BLETimeSync::ServerCallbacks::onConnect(BLEServer* pServer) {
    _parent->_deviceConnected = true;
    _parent->_connectionCount++;
    Serial.println("\n>>> BLE Client Connected!");
    Serial.print("Total connections: ");
    Serial.println(_parent->_connectionCount);
}

void BLETimeSync::ServerCallbacks::onDisconnect(BLEServer* pServer) {
    _parent->_deviceConnected = false;
    Serial.println("\n>>> BLE Client Disconnected!");
    // Restart advertising
    BLEDevice::startAdvertising();
    Serial.println("BLE advertising restarted");
}

// ============================================
// Time Characteristic Callbacks
// ============================================

void BLETimeSync::TimeCharCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();

    if (value.length() >= 4) {
        // Receive Unix timestamp (32-bit integer)
        uint32_t timestamp = *((uint32_t*)value.data());

        Serial.println("\n>>> Received Unix timestamp via BLE");
        Serial.print("Timestamp: ");
        Serial.println(timestamp);

        // Call callback if set
        if (_parent->_timeSyncCallback) {
            _parent->_timeSyncCallback((time_t)timestamp);
            Serial.println("Time synchronized successfully!");
        }
    }
}

// ============================================
// DateTime Characteristic Callbacks
// ============================================

void BLETimeSync::DateTimeCharCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();

    if (value.length() > 0) {
        Serial.println("\n>>> Received DateTime string via BLE");
        Serial.print("Received: ");
        Serial.println(value.c_str());

        // Parse format: "YYYY-MM-DD HH:MM:SS"
        int year, month, day, hour, minute, second;
        int parsed = sscanf(value.c_str(), "%d-%d-%d %d:%d:%d",
                           &year, &month, &day, &hour, &minute, &second);

        if (parsed == 6) {
            // Convert to Unix timestamp
            struct tm timeinfo;
            timeinfo.tm_year = year - 1900;
            timeinfo.tm_mon = month - 1;
            timeinfo.tm_mday = day;
            timeinfo.tm_hour = hour;
            timeinfo.tm_min = minute;
            timeinfo.tm_sec = second;
            time_t timestamp = mktime(&timeinfo);

            // Call callback if set
            if (_parent->_timeSyncCallback) {
                _parent->_timeSyncCallback(timestamp);
                Serial.println("Time synchronized successfully!");
                Serial.print("New time: ");
                Serial.print(year);
                Serial.print("-");
                Serial.print(month);
                Serial.print("-");
                Serial.print(day);
                Serial.print(" ");
                Serial.print(hour);
                Serial.print(":");
                Serial.print(minute);
                Serial.print(":");
                Serial.println(second);
            }
        } else {
            Serial.println("ERROR: Invalid datetime format!");
            Serial.println("Expected format: YYYY-MM-DD HH:MM:SS");
            Serial.println("Example: 2026-01-14 15:30:00");
        }
    }
}

// ============================================
// Alarm Set Characteristic Callbacks
// ============================================

void BLETimeSync::AlarmSetCharCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();

    if (value.length() > 0) {
        Serial.println("\n>>> Received Alarm Set via BLE");
        Serial.print("JSON: ");
        Serial.println(value.c_str());

        // Parse JSON: {"id":0,"hour":7,"minute":30,"days":127,"sound":"tone1","enabled":true,"label":"Alarm","snooze":true}
        // Simple manual parsing (to avoid ArduinoJson dependency)

        AlarmData alarm;
        String json = String(value.c_str());

        // Extract id
        int idIdx = json.indexOf("\"id\":");
        if (idIdx >= 0) {
            alarm.id = json.substring(idIdx + 5).toInt();
        }

        // Extract hour
        int hourIdx = json.indexOf("\"hour\":");
        if (hourIdx >= 0) {
            alarm.hour = json.substring(hourIdx + 7).toInt();
        }

        // Extract minute
        int minIdx = json.indexOf("\"minute\":");
        if (minIdx >= 0) {
            alarm.minute = json.substring(minIdx + 9).toInt();
        }

        // Extract days
        int daysIdx = json.indexOf("\"days\":");
        if (daysIdx >= 0) {
            alarm.daysOfWeek = json.substring(daysIdx + 7).toInt();
        }

        // Extract sound
        int soundIdx = json.indexOf("\"sound\":\"");
        if (soundIdx >= 0) {
            int soundEnd = json.indexOf("\"", soundIdx + 9);
            alarm.sound = json.substring(soundIdx + 9, soundEnd);
        }

        // Extract enabled
        int enabledIdx = json.indexOf("\"enabled\":");
        if (enabledIdx >= 0) {
            alarm.enabled = json.substring(enabledIdx + 10, enabledIdx + 14) == "true";
        }

        // Extract label (optional for backward compatibility)
        int labelIdx = json.indexOf("\"label\":\"");
        if (labelIdx >= 0) {
            int labelEnd = json.indexOf("\"", labelIdx + 9);
            alarm.label = json.substring(labelIdx + 9, labelEnd);
        } else {
            alarm.label = "Alarm";  // Default
        }

        // Extract snooze (optional for backward compatibility)
        int snoozeIdx = json.indexOf("\"snooze\":");
        if (snoozeIdx >= 0) {
            alarm.snoozeEnabled = json.substring(snoozeIdx + 9, snoozeIdx + 13) == "true";
        } else {
            alarm.snoozeEnabled = true;  // Default
        }

        // Extract perm_disabled (optional for backward compatibility)
        int permDisabledIdx = json.indexOf("\"perm_disabled\":");
        if (permDisabledIdx >= 0) {
            alarm.permanentlyDisabled = json.substring(permDisabledIdx + 16, permDisabledIdx + 20) == "true";
        } else {
            alarm.permanentlyDisabled = false;  // Default
        }

        // Set the alarm
        if (alarmManager.setAlarm(alarm)) {
            Serial.println("BLE: Alarm set successfully!");
            // NOTE: Don't send alarm list back to iOS - iOS is the source of truth
            // _parent->updateAlarmList();  // Disabled: iOS ignores this data + can exceed BLE MTU
        } else {
            Serial.println("BLE: ERROR - Failed to set alarm!");
        }
    }
}

// ============================================
// Alarm Delete Characteristic Callbacks
// ============================================

void BLETimeSync::AlarmDeleteCharCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();

    if (value.length() > 0) {
        uint8_t alarmId = atoi(value.c_str());

        Serial.print("\n>>> Received Alarm Delete via BLE: ID=");
        Serial.println(alarmId);

        if (alarmManager.deleteAlarm(alarmId)) {
            Serial.println("BLE: Alarm deleted successfully!");
            // NOTE: Don't send alarm list back to iOS - iOS is the source of truth
            // _parent->updateAlarmList();  // Disabled: iOS ignores this data + can exceed BLE MTU
        } else {
            Serial.println("BLE: ERROR - Failed to delete alarm!");
        }
    }
}

// ============================================
// Volume Characteristic Callbacks
// ============================================

void BLETimeSync::VolumeCharCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();

    if (value.length() > 0) {
        uint8_t volume = (uint8_t)value[0];

        if (volume <= 100) {
            audioObj.setVolume(volume);
            Serial.print("\n>>> BLE: Volume set to ");
            Serial.print(volume);
            Serial.println("%");
        } else {
            Serial.println("\n>>> BLE: ERROR - Invalid volume (must be 0-100)");
        }
    }
}

// ============================================
// Test Sound Characteristic Callbacks
// ============================================

void BLETimeSync::TestSoundCharCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    String soundName = String(value.c_str());

    // Check for stop command
    if (soundName == "stop") {
        audioObj.stop();
        Serial.println("\n>>> BLE: Test sound stopped");
        return;
    }

    // Check if it's a built-in tone
    if (soundName == "tone1" || soundName == "tone2" || soundName == "tone3") {
        // Play built-in tone for 2 seconds
        uint16_t frequency;
        if (soundName == "tone2") {
            frequency = 440;  // A4 note (middle)
        } else if (soundName == "tone3") {
            frequency = 880;  // A5 note (high)
        } else {
            frequency = 262;  // C4 note (tone1)
        }

        Serial.print("\n>>> BLE: Playing test tone '");
        Serial.print(soundName);
        Serial.print("' (");
        Serial.print(frequency);
        Serial.println(" Hz for 2 seconds)");

        audioObj.playTone(frequency, 2000);
    } else {
        // Check if already playing a file - prevent concurrent test sounds
        if (audioObj.getCurrentSoundType() == SOUND_TYPE_FILE) {
            Serial.println("\n>>> BLE: Test sound already playing, ignoring request");
            return;
        }

        // Try to play custom sound file from SPIFFS
        String filePath = String(ALARM_SOUNDS_DIR) + "/" + soundName;
        if (fileManager.fileExists(filePath)) {
            Serial.print("\n>>> BLE: Playing test file '");
            Serial.print(soundName);
            Serial.println("' (will auto-stop when finished)");

            // Play file without looping - will auto-stop when complete
            // Don't use delay() or stopFile() here to avoid race condition with main loop
            audioObj.playFile(filePath, false);
        } else {
            // File not found - play tone1 as fallback
            Serial.print("\n>>> BLE: File not found '");
            Serial.print(soundName);
            Serial.println("', using tone1 fallback (2 seconds)");

            audioObj.playTone(262, 2000);
        }
    }
}
