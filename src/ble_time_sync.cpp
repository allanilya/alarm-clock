#include "ble_time_sync.h"

// BLE Service UUID: Custom time sync service
const char* BLETimeSync::SERVICE_UUID = "12340000-1234-5678-1234-56789abcdef0";
const char* BLETimeSync::TIME_CHAR_UUID = "12340001-1234-5678-1234-56789abcdef0";
const char* BLETimeSync::DATETIME_CHAR_UUID = "12340002-1234-5678-1234-56789abcdef0";

BLETimeSync::BLETimeSync()
    : _pServer(nullptr),
      _pTimeService(nullptr),
      _pTimeCharacteristic(nullptr),
      _pDateTimeCharacteristic(nullptr),
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

    // Set initial value
    time_t currentTime = time(nullptr);
    uint32_t timeValue = (uint32_t)currentTime;
    _pTimeCharacteristic->setValue(timeValue);

    // Start the service
    _pTimeService->start();

    // Start advertising
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("BLETimeSync: BLE service started!");
    Serial.print("BLETimeSync: Device name: ");
    Serial.println(deviceName);
    Serial.println("\nConnect with your phone using a BLE app like:");
    Serial.println("  - iOS: LightBlue or nRF Connect");
    Serial.println("  - Android: nRF Connect");
    Serial.println("\nTo set time, write to the DateTime characteristic:");
    Serial.println("  Format: YYYY-MM-DD HH:MM:SS");
    Serial.println("  Example: 2026-01-14 15:30:00\n");

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
