#ifndef BLE_TIME_SYNC_H
#define BLE_TIME_SYNC_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

/**
 * BLETimeSync - BLE service for time synchronization from iOS/Android
 *
 * Implements BLE Current Time Service for syncing ESP32 RTC from mobile devices.
 * Uses callback pattern to decouple from TimeManager.
 */
class BLETimeSync {
public:
    /**
     * Callback function type for time sync
     * @param timestamp Unix timestamp to set
     */
    typedef void (*TimeSyncCallback)(time_t timestamp);

    BLETimeSync();

    /**
     * Initialize BLE server and services
     * @param deviceName BLE device name (visible to iOS/Android)
     * @return true if successful
     */
    bool begin(const char* deviceName);

    /**
     * Update BLE (call in loop)
     */
    void update();

    /**
     * Check if BLE device is connected
     * @return true if connected
     */
    bool isConnected();

    /**
     * Set callback for time sync events
     * @param callback Function to call when time is received
     */
    void setTimeSyncCallback(TimeSyncCallback callback);

    /**
     * Get number of successful connections since boot
     * @return Connection count
     */
    uint32_t getConnectionCount();

    /**
     * Update alarm list characteristic (call after alarm changes)
     */
    void updateAlarmList();

private:
    BLEServer* _pServer;
    BLEService* _pTimeService;
    BLEService* _pAlarmService;
    BLECharacteristic* _pTimeCharacteristic;
    BLECharacteristic* _pDateTimeCharacteristic;
    BLECharacteristic* _pVolumeCharacteristic;
    BLECharacteristic* _pTestSoundCharacteristic;
    BLECharacteristic* _pDisplayMessageCharacteristic;
    BLECharacteristic* _pAlarmSetCharacteristic;
    BLECharacteristic* _pAlarmListCharacteristic;
    BLECharacteristic* _pAlarmDeleteCharacteristic;
    bool _deviceConnected;
    uint32_t _connectionCount;
    TimeSyncCallback _timeSyncCallback;

    // BLE UUIDs
    static const char* SERVICE_UUID;
    static const char* TIME_CHAR_UUID;
    static const char* DATETIME_CHAR_UUID;
    static const char* VOLUME_CHAR_UUID;
    static const char* TEST_SOUND_CHAR_UUID;
    static const char* DISPLAY_MESSAGE_CHAR_UUID;
    static const char* ALARM_SERVICE_UUID;
    static const char* ALARM_SET_CHAR_UUID;
    static const char* ALARM_LIST_CHAR_UUID;
    static const char* ALARM_DELETE_CHAR_UUID;

    // Server callbacks
    class ServerCallbacks : public BLEServerCallbacks {
    public:
        ServerCallbacks(BLETimeSync* parent) : _parent(parent) {}
        void onConnect(BLEServer* pServer);
        void onDisconnect(BLEServer* pServer);
    private:
        BLETimeSync* _parent;
    };

    // Time characteristic callbacks
    class TimeCharCallbacks : public BLECharacteristicCallbacks {
    public:
        TimeCharCallbacks(BLETimeSync* parent) : _parent(parent) {}
        void onWrite(BLECharacteristic* pCharacteristic);
    private:
        BLETimeSync* _parent;
    };

    // DateTime characteristic callbacks
    class DateTimeCharCallbacks : public BLECharacteristicCallbacks {
    public:
        DateTimeCharCallbacks(BLETimeSync* parent) : _parent(parent) {}
        void onWrite(BLECharacteristic* pCharacteristic);
    private:
        BLETimeSync* _parent;
    };

    // Alarm Set characteristic callbacks
    class AlarmSetCharCallbacks : public BLECharacteristicCallbacks {
    public:
        AlarmSetCharCallbacks(BLETimeSync* parent) : _parent(parent) {}
        void onWrite(BLECharacteristic* pCharacteristic);
    private:
        BLETimeSync* _parent;
    };

    // Alarm Delete characteristic callbacks
    class AlarmDeleteCharCallbacks : public BLECharacteristicCallbacks {
    public:
        AlarmDeleteCharCallbacks(BLETimeSync* parent) : _parent(parent) {}
        void onWrite(BLECharacteristic* pCharacteristic);
    private:
        BLETimeSync* _parent;
    };

    // Volume characteristic callbacks
    class VolumeCharCallbacks : public BLECharacteristicCallbacks {
    public:
        VolumeCharCallbacks(BLETimeSync* parent) : _parent(parent) {}
        void onWrite(BLECharacteristic* pCharacteristic);
    private:
        BLETimeSync* _parent;
    };

    // Test Sound characteristic callbacks
    class TestSoundCharCallbacks : public BLECharacteristicCallbacks {
    public:
        TestSoundCharCallbacks(BLETimeSync* parent) : _parent(parent) {}
        void onWrite(BLECharacteristic* pCharacteristic);
    private:
        BLETimeSync* _parent;
    };

    // Display Message characteristic callbacks
    class DisplayMessageCharCallbacks : public BLECharacteristicCallbacks {
    public:
        DisplayMessageCharCallbacks(BLETimeSync* parent) : _parent(parent) {}
        void onWrite(BLECharacteristic* pCharacteristic);
    private:
        BLETimeSync* _parent;
    };
};

#endif // BLE_TIME_SYNC_H
