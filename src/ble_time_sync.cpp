#include "ble_time_sync.h"
#include "alarm_manager.h"
#include "audio_test.h"
#include "file_manager.h"
#include "display_manager.h"
#include "frontlight_manager.h"
#include <SPIFFS.h>

// External references
extern AlarmManager alarmManager;
extern AudioTest audioObj;
extern FileManager fileManager;
extern DisplayManager displayManager;
extern FrontlightManager frontlightManager;

// BLE Service UUID: Custom time sync service
const char* BLETimeSync::SERVICE_UUID = "12340000-1234-5678-1234-56789abcdef0";
const char* BLETimeSync::TIME_CHAR_UUID = "12340001-1234-5678-1234-56789abcdef0";
const char* BLETimeSync::DATETIME_CHAR_UUID = "12340002-1234-5678-1234-56789abcdef0";
const char* BLETimeSync::VOLUME_CHAR_UUID = "12340003-1234-5678-1234-56789abcdef0";
const char* BLETimeSync::TEST_SOUND_CHAR_UUID = "12340004-1234-5678-1234-56789abcdef0";
const char* BLETimeSync::DISPLAY_MESSAGE_CHAR_UUID = "12340005-1234-5678-1234-56789abcdef0";
const char* BLETimeSync::BOTTOM_ROW_LABEL_CHAR_UUID = "12340006-1234-5678-1234-56789abcdef0";
const char* BLETimeSync::BRIGHTNESS_CHAR_UUID = "12340007-1234-5678-1234-56789abcdef0";

// BLE Alarm Service UUID: Custom alarm management service
const char* BLETimeSync::ALARM_SERVICE_UUID = "12340010-1234-5678-1234-56789abcdef0";
const char* BLETimeSync::ALARM_SET_CHAR_UUID = "12340011-1234-5678-1234-56789abcdef0";
const char* BLETimeSync::ALARM_LIST_CHAR_UUID = "12340012-1234-5678-1234-56789abcdef0";
const char* BLETimeSync::ALARM_DELETE_CHAR_UUID = "12340013-1234-5678-1234-56789abcdef0";

// BLE File Service UUID: File transfer for custom alarm sounds
const char* BLETimeSync::FILE_SERVICE_UUID = "12340020-1234-5678-1234-56789abcdef0";
const char* BLETimeSync::FILE_CONTROL_CHAR_UUID = "12340021-1234-5678-1234-56789abcdef0";
const char* BLETimeSync::FILE_DATA_CHAR_UUID = "12340022-1234-5678-1234-56789abcdef0";
const char* BLETimeSync::FILE_STATUS_CHAR_UUID = "12340023-1234-5678-1234-56789abcdef0";
const char* BLETimeSync::FILE_LIST_CHAR_UUID = "12340024-1234-5678-1234-56789abcdef0";

BLETimeSync::BLETimeSync()
    : _pServer(nullptr),
      _pTimeService(nullptr),
      _pAlarmService(nullptr),
      _pFileService(nullptr),
      _pTimeCharacteristic(nullptr),
      _pDateTimeCharacteristic(nullptr),
      _pVolumeCharacteristic(nullptr),
      _pTestSoundCharacteristic(nullptr),
      _pBrightnessCharacteristic(nullptr),
      _pAlarmSetCharacteristic(nullptr),
      _pAlarmListCharacteristic(nullptr),
      _pAlarmDeleteCharacteristic(nullptr),
      _pFileControlCharacteristic(nullptr),
      _pFileDataCharacteristic(nullptr),
      _pFileStatusCharacteristic(nullptr),
      _pFileListCharacteristic(nullptr),
      _deviceConnected(false),
      _connectionCount(0),
      _timeSyncCallback(nullptr),
      _fileTransferState(FILE_IDLE),
      _receivingFilename(""),
      _receivingFileSize(0),
      _receivedBytes(0),
      _expectedSequence(0) {
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

    // Create Display Message Characteristic (Read/Write: custom message for top row, max 100 chars)
    _pDisplayMessageCharacteristic = _pTimeService->createCharacteristic(
        DISPLAY_MESSAGE_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );
    _pDisplayMessageCharacteristic->setCallbacks(new DisplayMessageCharCallbacks(this));
    _pDisplayMessageCharacteristic->setValue(displayManager.getCustomMessage().c_str());
    Serial.println("BLE: Created DisplayMessage characteristic (12340005)");

    // Create Bottom Row Label Characteristic (Read/Write: custom label for bottom row, max 50 chars)
    _pBottomRowLabelCharacteristic = _pTimeService->createCharacteristic(
        BOTTOM_ROW_LABEL_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );
    _pBottomRowLabelCharacteristic->setCallbacks(new BottomRowLabelCharCallbacks(this));
    _pBottomRowLabelCharacteristic->setValue(displayManager.getBottomRowLabel().c_str());
    Serial.println("BLE: Created BottomRowLabel characteristic (12340006)");

    // Create Brightness Characteristic (Read/Write: 0-100%)
    _pBrightnessCharacteristic = _pTimeService->createCharacteristic(
        BRIGHTNESS_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );
    _pBrightnessCharacteristic->setCallbacks(new BrightnessCharCallbacks(this));
    _pBrightnessCharacteristic->addDescriptor(new BLE2902());
    uint32_t initialBrightness = (uint32_t)frontlightManager.getBrightness();
    _pBrightnessCharacteristic->setValue(initialBrightness);
    Serial.println("BLE: Created Brightness characteristic (12340007)");

    // Set initial value
    time_t currentTime = time(nullptr);
    uint32_t timeValue = (uint32_t)currentTime;
    _pTimeCharacteristic->setValue(timeValue);

    // Start the time service
    Serial.println("BLE: Starting Time service with 7 characteristics...");
    _pTimeService->start();
    Serial.println("BLE: Time service started successfully");

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

    // Create BLE File Service
    _pFileService = _pServer->createService(FILE_SERVICE_UUID);

    // Create File Control Characteristic (Write: START:<filename>:<size>, END, CANCEL)
    _pFileControlCharacteristic = _pFileService->createCharacteristic(
        FILE_CONTROL_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    _pFileControlCharacteristic->setCallbacks(new FileControlCharCallbacks(this));

    // Create File Data Characteristic (Write: 512-byte chunks with 2-byte sequence number)
    _pFileDataCharacteristic = _pFileService->createCharacteristic(
        FILE_DATA_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    _pFileDataCharacteristic->setCallbacks(new FileDataCharCallbacks(this));

    // Create File Status Characteristic (Read/Notify: READY, RECEIVING, SUCCESS, ERROR)
    _pFileStatusCharacteristic = _pFileService->createCharacteristic(
        FILE_STATUS_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    _pFileStatusCharacteristic->addDescriptor(new BLE2902());
    _pFileStatusCharacteristic->setValue("READY");

    // Create File List Characteristic (Read + Notify: JSON array of available sounds)
    _pFileListCharacteristic = _pFileService->createCharacteristic(
        FILE_LIST_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    _pFileListCharacteristic->addDescriptor(new BLE2902());

    // Start the file service
    _pFileService->start();

    // Start advertising
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->addServiceUUID(ALARM_SERVICE_UUID);
    pAdvertising->addServiceUUID(FILE_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // 7.5ms minimum interval
    pAdvertising->setMaxPreferred(0x12);  // 22.5ms maximum interval
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

    // Initialize file list characteristic
    updateFileList();

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
        json += ",\"bottomRowLabel\":\"";
        json += alarms[i].bottomRowLabel;
        json += "\"}";
    }

    json += "]";

    _pAlarmListCharacteristic->setValue(json.c_str());
    Serial.print("BLE: Updated alarm list (");
    Serial.print(alarms.size());
    Serial.println(" alarms)");
}

void BLETimeSync::updateFileList() {
    if (!_pFileListCharacteristic) return;

    // Build JSON array of all sound files
    String json = "[";
    std::vector<SoundFileInfo> files = fileManager.getSoundFileList();

    for (size_t i = 0; i < files.size(); i++) {
        if (i > 0) json += ",";

        json += "{\"filename\":\"";
        json += files[i].filename;
        json += "\",\"size\":";
        json += String(files[i].fileSize);
        json += "}";
    }

    json += "]";

    _pFileListCharacteristic->setValue(json.c_str());
    _pFileListCharacteristic->notify();  // Notify iOS app of file list update
    Serial.print("BLE: Updated file list (");
    Serial.print(files.size());
    Serial.println(" files)");
    Serial.print("BLE: File list JSON: ");
    Serial.println(json);
}

bool BLETimeSync::isFileTransferring() {
    return _fileTransferState == FILE_RECEIVING;
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

        // Extract bottomRowLabel (optional for backward compatibility)
        int bottomRowIdx = json.indexOf("\"bottomRowLabel\":\"");
        if (bottomRowIdx >= 0) {
            int bottomRowEnd = json.indexOf("\"", bottomRowIdx + 18);
            alarm.bottomRowLabel = json.substring(bottomRowIdx + 18, bottomRowEnd);
        } else {
            alarm.bottomRowLabel = "";  // Default empty
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

void BLETimeSync::DisplayMessageCharCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    String message = String(value.c_str());

    Serial.print("\n>>> BLE: Display message set to: ");
    Serial.println(message.length() > 0 ? message : "(empty - using day of week)");

    // Update DisplayManager with new message
    displayManager.setCustomMessage(message);
}

void BLETimeSync::BottomRowLabelCharCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    String label = String(value.c_str());

    Serial.print("\n>>> BLE: Bottom row label set to: ");
    Serial.println(label.length() > 0 ? label : "(empty - using default layout)");

    // Update DisplayManager with new bottom row label
    displayManager.setBottomRowLabel(label);
}

// ============================================
// Brightness Characteristic Callbacks
// ============================================

void BLETimeSync::BrightnessCharCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();

    if (value.length() > 0) {
        uint8_t brightness = (uint8_t)value[0];

        if (brightness <= 100) {
            frontlightManager.setBrightness(brightness);
            Serial.print("\n>>> BLE: Frontlight brightness set to ");
            Serial.print(brightness);
            Serial.println("%");
        } else {
            Serial.println("\n>>> BLE: ERROR - Invalid brightness (must be 0-100)");
        }
    }
}

// ============================================
// File Transfer Callbacks
// ============================================

void BLETimeSync::FileControlCharCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    String command = String(value.c_str());

    Serial.print("\n>>> BLE FILE: Control command: ");
    Serial.println(command);

    if (command.startsWith("START:")) {
        // Parse: START:<filename>:<filesize>
        int firstColon = command.indexOf(':', 6);
        if (firstColon > 0) {
            String filename = command.substring(6, firstColon);
            String sizeStr = command.substring(firstColon + 1);
            size_t fileSize = sizeStr.toInt();

            _parent->startFileTransfer(filename, fileSize);
        } else {
            _parent->updateFileStatus("ERROR:Invalid START format");
        }
    } else if (command == "END") {
        // Finish file transfer
        if (_parent->_fileTransferState == FILE_RECEIVING) {
            if (_parent->_receivingFile) {
                _parent->_receivingFile.flush();  // Ensure all data is written
                _parent->_receivingFile.close();
            }

            if (_parent->_receivedBytes == _parent->_receivingFileSize) {
                _parent->_fileTransferState = FILE_COMPLETE;
                _parent->updateFileStatus("SUCCESS");
                Serial.println(">>> BLE FILE: Transfer complete successfully!");
                Serial.printf(">>> BLE FILE: Saved file: %s (%u bytes)\n", _parent->_receivingFilename.c_str(), _parent->_receivedBytes);

                // Small delay to ensure SPIFFS commits the file
                delay(100);

                // Verify file was actually saved - try both path formats
                String pathWithPrefix = String(ALARM_SOUNDS_DIR) + "/" + _parent->_receivingFilename;
                String pathWithoutPrefix = "/alarms/" + _parent->_receivingFilename;

                Serial.printf(">>> BLE FILE: Checking if file exists...\n");
                Serial.printf(">>> BLE FILE:   Path with /spiffs prefix: %s\n", pathWithPrefix.c_str());
                Serial.printf(">>> BLE FILE:   Path without prefix: %s\n", pathWithoutPrefix.c_str());

                bool existsWithPrefix = SPIFFS.exists(pathWithPrefix.c_str());
                bool existsWithoutPrefix = SPIFFS.exists(pathWithoutPrefix.c_str());

                Serial.printf(">>> BLE FILE:   Exists with prefix: %s\n", existsWithPrefix ? "YES" : "NO");
                Serial.printf(">>> BLE FILE:   Exists without prefix: %s\n", existsWithoutPrefix ? "YES" : "NO");

                if (existsWithoutPrefix) {
                    File verifyFile = SPIFFS.open(pathWithoutPrefix.c_str(), "r");
                    if (verifyFile) {
                        size_t actualSize = verifyFile.size();
                        verifyFile.close();
                        Serial.printf(">>> BLE FILE: VERIFIED - File exists at %s with size %u\n", pathWithoutPrefix.c_str(), actualSize);
                    } else {
                        Serial.printf(">>> BLE FILE: WARNING - File exists but cannot open: %s\n", pathWithoutPrefix.c_str());
                    }
                } else if (existsWithPrefix) {
                    File verifyFile = SPIFFS.open(pathWithPrefix.c_str(), "r");
                    if (verifyFile) {
                        size_t actualSize = verifyFile.size();
                        verifyFile.close();
                        Serial.printf(">>> BLE FILE: VERIFIED - File exists at %s with size %u\n", pathWithPrefix.c_str(), actualSize);
                    } else {
                        Serial.printf(">>> BLE FILE: WARNING - File exists but cannot open: %s\n", pathWithPrefix.c_str());
                    }
                } else {
                    Serial.printf(">>> BLE FILE: ERROR - File does NOT exist with either path format!\n");
                    Serial.println(">>> BLE FILE: Listing /alarms directory to debug...");
                    File debugDir = SPIFFS.open("/alarms");
                    if (debugDir && debugDir.isDirectory()) {
                        File debugFile = debugDir.openNextFile();
                        while (debugFile) {
                            Serial.printf(">>> BLE FILE:   Found: %s (%u bytes)\n", debugFile.name(), debugFile.size());
                            debugFile = debugDir.openNextFile();
                        }
                        debugDir.close();
                    }
                }

                // Update file list so iOS can see the new file
                _parent->updateFileList();
            } else {
                _parent->_fileTransferState = FILE_ERROR;
                _parent->updateFileStatus("ERROR:Size mismatch");
                Serial.println(">>> BLE FILE: ERROR - Size mismatch!");
                
                // Delete incomplete file
                String deletePathOpen = "/alarms/" + _parent->_receivingFilename;
                SPIFFS.remove(deletePathOpen.c_str());
            }

            // Reset state
            _parent->_receivingFilename = "";
            _parent->_receivingFileSize = 0;
            _parent->_receivedBytes = 0;
            _parent->_expectedSequence = 0;
        }
    } else if (command == "CANCEL") {
        _parent->cancelFileTransfer();
    } else if (command.startsWith("DELETE:")) {
        // Parse: DELETE:<filename>
        String filename = command.substring(7);
        String deletePath = "/alarms/" + filename;

        Serial.printf(">>> BLE FILE: Delete request for: %s\n", filename.c_str());

        if (SPIFFS.remove(deletePath.c_str())) {
            _parent->updateFileStatus("SUCCESS");
            Serial.printf(">>> BLE FILE: Deleted file: %s\n", filename.c_str());

            // Update file list after deletion
            _parent->updateFileList();
        } else {
            _parent->updateFileStatus("ERROR:Delete failed");
            Serial.printf(">>> BLE FILE: ERROR - Failed to delete file: %s\n", deletePath.c_str());
        }
    } else {
        _parent->updateFileStatus("ERROR:Unknown command");
        Serial.printf(">>> BLE FILE: ERROR - Unknown command: %s\n", command.c_str());
    }
}

void BLETimeSync::FileDataCharCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    if (_parent->_fileTransferState != FILE_RECEIVING) {
        Serial.println(">>> BLE FILE: ERROR - Not in receiving state");
        return;
    }

    std::string value = pCharacteristic->getValue();
    
    if (value.length() < 2) {
        Serial.println(">>> BLE FILE: ERROR - Chunk too small");
        return;
    }

    // Extract sequence number (first 2 bytes)
    uint16_t sequence = ((uint8_t)value[0] << 8) | (uint8_t)value[1];
    
    // Check sequence
    if (sequence != _parent->_expectedSequence) {
        Serial.print(">>> BLE FILE: ERROR - Sequence mismatch. Expected ");
        Serial.print(_parent->_expectedSequence);
        Serial.print(", got ");
        Serial.println(sequence);
        _parent->updateFileStatus("ERROR:Sequence mismatch");
        _parent->cancelFileTransfer();
        return;
    }

    // Write data (skip first 2 bytes which are sequence number)
    size_t dataLen = value.length() - 2;
    const uint8_t* data = (const uint8_t*)value.c_str() + 2;
    
    if (_parent->_receivingFile) {
        size_t written = _parent->_receivingFile.write(data, dataLen);
        if (written != dataLen) {
            Serial.println(">>> BLE FILE: ERROR - Failed to write data");
            _parent->updateFileStatus("ERROR:Write failed");
            _parent->cancelFileTransfer();
            return;
        }
        
        _parent->_receivedBytes += written;
        _parent->_expectedSequence++;

        // Flush file every 5 chunks to ensure data is written promptly
        // With 254-byte chunks, this flushes every ~1.3KB
        if (sequence % 5 == 0) {
            _parent->_receivingFile.flush();

            String status = "RECEIVING:" + String(_parent->_receivedBytes) + "/" + String(_parent->_receivingFileSize);
            _parent->updateFileStatus(status);
            Serial.print(">>> BLE FILE: Progress: ");
            Serial.print(_parent->_receivedBytes);
            Serial.print(" / ");
            Serial.println(_parent->_receivingFileSize);
        }
    }
}

// ============================================
// File Transfer Helper Methods
// ============================================

void BLETimeSync::startFileTransfer(const String& filename, size_t fileSize) {
    Serial.print(">>> BLE FILE: Starting transfer - ");
    Serial.print(filename);
    Serial.print(" (");
    Serial.print(fileSize);
    Serial.println(" bytes)");

    // Validate filename
    if (!fileManager.isValidFilename(filename)) {
        updateFileStatus("ERROR:Invalid filename");
        Serial.println(">>> BLE FILE: ERROR - Invalid filename");
        return;
    }

    // Check file size (max 1 MB)
    if (fileSize > 1048576) {
        updateFileStatus("ERROR:File too large");
        Serial.println(">>> BLE FILE: ERROR - File too large (max 1 MB)");
        return;
    }

    // Check available space
    if (!fileManager.hasSpaceForFile(fileSize)) {
        updateFileStatus("ERROR:Not enough space");
        Serial.println(">>> BLE FILE: ERROR - Not enough space");
        return;
    }

    // Cancel any existing transfer
    if (_fileTransferState == FILE_RECEIVING) {
        cancelFileTransfer();
    }

    // Open file for writing (use path without /spiffs prefix for SPIFFS.open)
    String relativePath = "/alarms/" + filename;

    // Debug: Print the actual path being used
    Serial.print(">>> BLE FILE: Opening file path: ");
    Serial.println(relativePath);
    Serial.printf(">>> BLE FILE: SPIFFS Free: %d / Total: %d bytes\n",
                  SPIFFS.totalBytes() - SPIFFS.usedBytes(), SPIFFS.totalBytes());

    // Ensure directory exists
    Serial.println(">>> BLE FILE: Checking /alarms directory...");
    if (!SPIFFS.exists("/alarms/.placeholder")) {
        Serial.println(">>> BLE FILE: Creating /alarms directory structure...");
        File placeholder = SPIFFS.open("/alarms/.placeholder", "w");
        if (placeholder) {
            placeholder.print("This file ensures the directory exists in SPIFFS");
            placeholder.close();
            Serial.printf(">>> BLE FILE: Directory structure created successfully at %s\n", ALARM_SOUNDS_DIR);
        } else {
            Serial.printf(">>> BLE FILE: ERROR - Could not create directory structure at %s\n", ALARM_SOUNDS_DIR);
            updateFileStatus("ERROR:Cannot create directory");
            return;
        }
    } else {
        Serial.println(">>> BLE FILE: Directory structure already exists");
    }
    
    _receivingFile = SPIFFS.open(relativePath.c_str(), "w");
    
    if (!_receivingFile) {
        updateFileStatus("ERROR:Cannot create file");
        Serial.printf(">>> BLE FILE: ERROR - Cannot create file at: %s\n", relativePath.c_str());
        
        // Try to diagnose the issue
        Serial.println(">>> BLE FILE: Attempting diagnostics...");
        File testFile = SPIFFS.open("/test.txt", "w");
        if (testFile) {
            testFile.print("test");
            testFile.close();
            Serial.println(">>> BLE FILE: Can write to root directory - issue is with /alarms path");
            SPIFFS.remove("/test.txt");
        } else {
            Serial.println(">>> BLE FILE: Cannot write to SPIFFS at all - filesystem may be read-only or full");
        }
        return;
    }
    
    Serial.println(">>> BLE FILE: File opened successfully!");

    // Initialize transfer state
    _fileTransferState = FILE_RECEIVING;
    _receivingFilename = filename;
    _receivingFileSize = fileSize;
    _receivedBytes = 0;
    _expectedSequence = 0;

    updateFileStatus("READY");
    Serial.println(">>> BLE FILE: Ready to receive data");
}

void BLETimeSync::cancelFileTransfer() {
    Serial.println(">>> BLE FILE: Canceling transfer");

    if (_receivingFile) {
        _receivingFile.close();
    }

    // Delete partial file
    if (_receivingFilename.length() > 0) {
        String relativePath = "/alarms/" + _receivingFilename;
        SPIFFS.remove(relativePath.c_str());
    }

    _fileTransferState = FILE_IDLE;
    _receivingFilename = "";
    _receivingFileSize = 0;
    _receivedBytes = 0;
    _expectedSequence = 0;

    updateFileStatus("READY");
}

void BLETimeSync::updateFileStatus(const String& status) {
    if (_pFileStatusCharacteristic) {
        _pFileStatusCharacteristic->setValue(status.c_str());
        _pFileStatusCharacteristic->notify();
    }
}
