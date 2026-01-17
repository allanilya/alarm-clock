//
//  BLEManager.swift
//  Alarm Clock
//
//  Created by Allan on 1/15/26.
//

import Foundation
import CoreBluetooth
import Combine

/// BLE Central Manager for connecting to ESP32-L alarm clock
class BLEManager: NSObject, ObservableObject {

    // MARK: - Published Properties

    @Published var isScanning = false
    @Published var isConnected = false
    @Published var discoveredDevices: [CBPeripheral] = []
    @Published var alarms: [Alarm] = []
    @Published var connectionState: ConnectionState = .disconnected
    @Published var lastError: String?
    @Published var lastSyncTime: Date?
    @Published var isBluetoothReady = false

    // MARK: - Connection State

    enum ConnectionState {
        case disconnected
        case scanning
        case connecting
        case connected
        case discovering
        case ready
    }

    // MARK: - BLE Service UUIDs

    private let timeServiceUUID = CBUUID(string: "12340000-1234-5678-1234-56789abcdef0")
    private let timeCharUUID = CBUUID(string: "12340001-1234-5678-1234-56789abcdef0")
    private let dateTimeCharUUID = CBUUID(string: "12340002-1234-5678-1234-56789abcdef0")

    private let volumeCharUUID = CBUUID(string: "12340003-1234-5678-1234-56789abcdef0")
    private let testSoundCharUUID = CBUUID(string: "12340004-1234-5678-1234-56789abcdef0")

    private let alarmServiceUUID = CBUUID(string: "12340010-1234-5678-1234-56789abcdef0")
    private let alarmSetCharUUID = CBUUID(string: "12340011-1234-5678-1234-56789abcdef0")
    private let alarmListCharUUID = CBUUID(string: "12340012-1234-5678-1234-56789abcdef0")
    private let alarmDeleteCharUUID = CBUUID(string: "12340013-1234-5678-1234-56789abcdef0")

    // MARK: - Private Properties

    private var centralManager: CBCentralManager!
    private var connectedPeripheral: CBPeripheral?

    // Characteristics
    private var timeCharacteristic: CBCharacteristic?
    private var dateTimeCharacteristic: CBCharacteristic?
    private var volumeCharacteristic: CBCharacteristic?
    private var testSoundCharacteristic: CBCharacteristic?
    private var alarmSetCharacteristic: CBCharacteristic?
    private var alarmListCharacteristic: CBCharacteristic?
    private var alarmDeleteCharacteristic: CBCharacteristic?

    // Volume tracking
    private var currentVolume: Int = 70

    // Auto-reconnect
    private var shouldAutoReconnect = true
    private var lastConnectedPeripheralIdentifier: UUID?

    // Pending scan flag
    private var pendingScan = false

    // CoreData persistence
    private let persistenceController: PersistenceController

    // MARK: - Initialization

    init(persistenceController: PersistenceController = .shared) {
        self.persistenceController = persistenceController
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)

        // Load alarms from CoreData on initialization
        loadAlarmsFromCoreData()
    }

    // MARK: - CoreData Integration

    /// Load alarms from CoreData (called on app launch)
    private func loadAlarmsFromCoreData() {
        let savedAlarms = persistenceController.fetchAlarms()
        DispatchQueue.main.async { [weak self] in
            self?.alarms = savedAlarms.sorted { $0.id < $1.id }
            print("BLEManager: Loaded \(savedAlarms.count) alarms from CoreData")

            // Auto-disable expired one-time alarms
            self?.checkAndDisableExpiredOneTimeAlarms()
        }
    }

    /// Auto-disable one-time alarms (days=0) when their time is reached
    func checkAndDisableExpiredOneTimeAlarms() {
        let now = Date()
        let calendar = Calendar.current
        let currentHour = calendar.component(.hour, from: now)
        let currentMinute = calendar.component(.minute, from: now)

        var hasChanges = false

        for i in 0..<alarms.count {
            var alarm = alarms[i]

            // Only check enabled one-time alarms (daysOfWeek == 0) that aren't already permanently disabled
            guard alarm.enabled && alarm.daysOfWeek == 0 && !alarm.permanentlyDisabled else { continue }

            // Check if current time EXACTLY matches alarm time
            let alarmTimeMatches = (alarm.hour == currentHour && alarm.minute == currentMinute)

            if alarmTimeMatches {
                print("BLEManager: Permanently disabling one-time alarm \(alarm.id) at \(alarm.timeString)")
                alarms[i].enabled = false
                alarms[i].permanentlyDisabled = true
                hasChanges = true
            }
        }

        // Save changes if any alarms were disabled
        if hasChanges {
            saveAlarmsToCoreData()

            // Also update ESP32 if connected
            if isConnected {
                for alarm in alarms where alarm.daysOfWeek == 0 && alarm.permanentlyDisabled {
                    setAlarm(alarm)
                }
            }
        }
    }

    /// Save current alarms to CoreData
    private func saveAlarmsToCoreData() {
        persistenceController.saveAlarms(alarms)
        print("BLEManager: Saved \(alarms.count) alarms to CoreData")
    }

    /// Save a single alarm to CoreData
    private func saveAlarmToCoreData(_ alarm: Alarm) {
        persistenceController.saveAlarm(alarm)
        print("BLEManager: Saved alarm \(alarm.id) to CoreData")
    }

    /// Delete alarm from CoreData
    private func deleteAlarmFromCoreData(id: Int) {
        persistenceController.deleteAlarm(id: id)
        print("BLEManager: Deleted alarm \(id) from CoreData")
    }

    // MARK: - Public Methods

    /// Start scanning for ESP32-L devices
    func startScanning() {
        guard centralManager.state == .poweredOn else {
            lastError = "Bluetooth is not ready yet. Please wait..."
            pendingScan = true
            print("BLEManager: Bluetooth not ready, scan will start when powered on")
            return
        }

        pendingScan = false
        discoveredDevices.removeAll()
        connectionState = .scanning
        isScanning = true

        // Scan for devices advertising Time or Alarm service
        centralManager.scanForPeripherals(
            withServices: [timeServiceUUID, alarmServiceUUID],
            options: [CBCentralManagerScanOptionAllowDuplicatesKey: false]
        )

        print("BLEManager: Started scanning for devices...")
    }

    /// Stop scanning
    func stopScanning() {
        centralManager.stopScan()
        isScanning = false
        if connectionState == .scanning {
            connectionState = .disconnected
        }
        print("BLEManager: Stopped scanning")
    }

    /// Connect to a discovered peripheral
    func connect(to peripheral: CBPeripheral) {
        stopScanning()
        connectionState = .connecting
        connectedPeripheral = peripheral
        peripheral.delegate = self
        centralManager.connect(peripheral, options: nil)
        lastConnectedPeripheralIdentifier = peripheral.identifier
        print("BLEManager: Connecting to \(peripheral.name ?? "Unknown")...")
    }

    /// Disconnect from current peripheral
    func disconnect() {
        shouldAutoReconnect = false
        if let peripheral = connectedPeripheral {
            centralManager.cancelPeripheralConnection(peripheral)
            print("BLEManager: Disconnecting...")
        }
        connectionState = .disconnected
        isConnected = false
    }

    /// Enable auto-reconnect
    func enableAutoReconnect() {
        shouldAutoReconnect = true
        if connectionState == .disconnected, let uuid = lastConnectedPeripheralIdentifier {
            if let peripheral = centralManager.retrievePeripherals(withIdentifiers: [uuid]).first {
                connect(to: peripheral)
            }
        }
    }

    // MARK: - Time Synchronization

    /// Sync current time to ESP32 using Unix timestamp
    func syncTime() {
        guard let characteristic = dateTimeCharacteristic else {
            lastError = "DateTime characteristic not found"
            return
        }

        // Format: "YYYY-MM-DD HH:MM:SS"
        let formatter = DateFormatter()
        formatter.dateFormat = "yyyy-MM-dd HH:mm:ss"
        let dateString = formatter.string(from: Date())

        guard let data = dateString.data(using: .utf8) else {
            lastError = "Failed to encode date string"
            return
        }

        connectedPeripheral?.writeValue(data, for: characteristic, type: .withResponse)
        lastSyncTime = Date()
        print("BLEManager: Sent time sync: \(dateString)")
    }

    /// Auto-sync time on connection
    private func autoSyncTime() {
        // Wait a moment for characteristics to be ready
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) { [weak self] in
            self?.syncTime()
        }
    }

    // MARK: - Alarm Management

    /// Read all alarms from ESP32 (ESP32 is source of truth when connected)
    func readAlarms() {
        guard let characteristic = alarmListCharacteristic else {
            lastError = "AlarmList characteristic not found"
            return
        }

        connectedPeripheral?.readValue(for: characteristic)
        print("BLEManager: Reading alarm list from ESP32...")
    }

    /// Set or update an alarm (sends to ESP32 if connected, saves to CoreData)
    func setAlarm(_ alarm: Alarm) {
        guard alarm.isValid else {
            lastError = "Invalid alarm data"
            return
        }

        // If connected to ESP32, send the alarm
        if isConnected, let characteristic = alarmSetCharacteristic {
            guard let jsonString = alarm.toJSONString(),
                  let data = jsonString.data(using: .utf8) else {
                lastError = "Failed to serialize alarm to JSON"
                return
            }

            connectedPeripheral?.writeValue(data, for: characteristic, type: .withResponse)
            print("BLEManager: Set alarm \(alarm.id): \(alarm.timeString) on ESP32")

            // Refresh alarm list after setting (which will save to CoreData)
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) { [weak self] in
                self?.readAlarms()
            }
        } else {
            // Not connected, update local state and save to CoreData
            DispatchQueue.main.async { [weak self] in
                guard let self = self else { return }
                if let index = self.alarms.firstIndex(where: { $0.id == alarm.id }) {
                    self.alarms[index] = alarm
                } else {
                    self.alarms.append(alarm)
                    self.alarms.sort { $0.id < $1.id }
                }
                self.saveAlarmToCoreData(alarm)
                print("BLEManager: Set alarm \(alarm.id): \(alarm.timeString) locally (offline)")

                // Check and disable expired one-time alarms after saving
                self.checkAndDisableExpiredOneTimeAlarms()
            }
        }
    }

    /// Delete an alarm by ID (deletes from ESP32 if connected, and from CoreData)
    func deleteAlarm(id: Int) {
        guard id >= 0 && id <= 9 else {
            lastError = "Invalid alarm ID"
            return
        }

        // If connected to ESP32, send delete command
        if isConnected, let characteristic = alarmDeleteCharacteristic {
            let idString = String(id)
            guard let data = idString.data(using: .utf8) else {
                lastError = "Failed to encode alarm ID"
                return
            }

            connectedPeripheral?.writeValue(data, for: characteristic, type: .withResponse)
            print("BLEManager: Deleted alarm \(id) from ESP32")

            // Refresh alarm list after deleting (which will save to CoreData)
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) { [weak self] in
                self?.readAlarms()
            }
        } else {
            // Not connected, update local state and delete from CoreData
            DispatchQueue.main.async { [weak self] in
                guard let self = self else { return }
                self.alarms.removeAll { $0.id == id }
                self.deleteAlarmFromCoreData(id: id)
                print("BLEManager: Deleted alarm \(id) locally (offline)")
            }
        }
    }

    // MARK: - Volume Control

    /// Set volume on ESP32 (0-100%)
    func setVolume(_ volume: Int) {
        guard let characteristic = volumeCharacteristic else {
            lastError = "Volume characteristic not found"
            return
        }

        guard volume >= 0 && volume <= 100 else {
            lastError = "Invalid volume (must be 0-100)"
            return
        }

        let data = Data([UInt8(volume)])
        connectedPeripheral?.writeValue(data, for: characteristic, type: .withResponse)
        currentVolume = volume
        print("BLEManager: Set volume to \(volume)%")
    }

    /// Get current volume level
    func getCurrentVolume() -> Int {
        return currentVolume
    }

    /// Trigger test sound on ESP32
    func testSound() {
        testSound(soundName: "tone1")  // Default to tone1
    }

    /// Trigger test sound with specific sound name (tone1, tone2, tone3)
    func testSound(soundName: String) {
        guard let characteristic = testSoundCharacteristic else {
            lastError = "TestSound characteristic not found"
            return
        }

        // First stop any ongoing sound by sending empty string
        if let stopData = "stop".data(using: .utf8) {
            connectedPeripheral?.writeValue(stopData, for: characteristic, type: .withoutResponse)
        }

        // Small delay to ensure stop command is processed
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.05) { [weak self] in
            guard let self = self else { return }
            guard let data = soundName.data(using: .utf8) else {
                self.lastError = "Failed to encode sound name"
                return
            }

            self.connectedPeripheral?.writeValue(data, for: characteristic, type: .withResponse)
            print("BLEManager: Triggered test sound '\(soundName)' on ESP32")
        }
    }

    /// Stop any currently playing test sound
    func stopTestSound() {
        guard let characteristic = testSoundCharacteristic else { return }

        if let stopData = "stop".data(using: .utf8) {
            connectedPeripheral?.writeValue(stopData, for: characteristic, type: .withoutResponse)
            print("BLEManager: Stopped test sound on ESP32")
        }
    }

    // MARK: - Helper Methods

    private func parseAlarmList(from data: Data) {
        guard let jsonString = String(data: data, encoding: .utf8) else {
            lastError = "Failed to decode alarm list data"
            return
        }

        print("BLEManager: Received alarm list JSON: \(jsonString)")

        guard let jsonData = jsonString.data(using: .utf8),
              let jsonArray = try? JSONSerialization.jsonObject(with: jsonData) as? [[String: Any]] else {
            lastError = "Failed to parse alarm list JSON"
            return
        }

        let parsedAlarms = jsonArray.compactMap { Alarm.fromJSON($0) }

        DispatchQueue.main.async { [weak self] in
            guard let self = self else { return }
            self.alarms = parsedAlarms.sorted { $0.id < $1.id }

            // Save ESP32 alarms to CoreData (ESP32 is source of truth when connected)
            self.saveAlarmsToCoreData()

            // Auto-disable expired one-time alarms
            self.checkAndDisableExpiredOneTimeAlarms()

            print("BLEManager: Parsed \(parsedAlarms.count) alarms from ESP32 and saved to CoreData")
        }
    }
}

// MARK: - CBCentralManagerDelegate

extension BLEManager: CBCentralManagerDelegate {

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            print("BLEManager: Bluetooth powered on")
            DispatchQueue.main.async { [weak self] in
                self?.isBluetoothReady = true
                self?.lastError = nil
            }

            // Start pending scan if requested
            if pendingScan {
                print("BLEManager: Starting pending scan")
                DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) { [weak self] in
                    self?.startScanning()
                }
            }

            // Try to reconnect to last device if auto-reconnect is enabled
            if shouldAutoReconnect, let uuid = lastConnectedPeripheralIdentifier {
                if let peripheral = centralManager.retrievePeripherals(withIdentifiers: [uuid]).first {
                    connect(to: peripheral)
                }
            }

        case .poweredOff:
            print("BLEManager: Bluetooth powered off")
            DispatchQueue.main.async { [weak self] in
                self?.isBluetoothReady = false
                self?.lastError = "Bluetooth is powered off"
                self?.connectionState = .disconnected
                self?.isConnected = false
            }

        case .unsupported:
            print("BLEManager: Bluetooth not supported")
            lastError = "Bluetooth is not supported on this device"

        case .unauthorized:
            print("BLEManager: Bluetooth unauthorized")
            lastError = "Bluetooth permission not granted"

        case .resetting:
            print("BLEManager: Bluetooth resetting")

        case .unknown:
            print("BLEManager: Bluetooth state unknown")

        @unknown default:
            print("BLEManager: Unknown Bluetooth state")
        }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                       advertisementData: [String: Any], rssi RSSI: NSNumber) {
        print("BLEManager: Discovered \(peripheral.name ?? "Unknown") (RSSI: \(RSSI))")

        if !discoveredDevices.contains(where: { $0.identifier == peripheral.identifier }) {
            DispatchQueue.main.async { [weak self] in
                self?.discoveredDevices.append(peripheral)
            }
        }
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        print("BLEManager: Connected to \(peripheral.name ?? "Unknown")")

        DispatchQueue.main.async { [weak self] in
            self?.connectionState = .discovering
            self?.isConnected = true
        }

        // Discover services
        peripheral.discoverServices([timeServiceUUID, alarmServiceUUID])
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        print("BLEManager: Failed to connect - \(error?.localizedDescription ?? "unknown error")")

        DispatchQueue.main.async { [weak self] in
            self?.connectionState = .disconnected
            self?.isConnected = false
            self?.lastError = "Failed to connect: \(error?.localizedDescription ?? "unknown error")"
        }
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        print("BLEManager: Disconnected from \(peripheral.name ?? "Unknown")")

        DispatchQueue.main.async { [weak self] in
            self?.connectionState = .disconnected
            self?.isConnected = false
            self?.connectedPeripheral = nil

            // Clear characteristics
            self?.timeCharacteristic = nil
            self?.dateTimeCharacteristic = nil
            self?.alarmSetCharacteristic = nil
            self?.alarmListCharacteristic = nil
            self?.alarmDeleteCharacteristic = nil
        }

        // Auto-reconnect if enabled
        if shouldAutoReconnect && error != nil {
            print("BLEManager: Attempting to reconnect...")
            DispatchQueue.main.asyncAfter(deadline: .now() + 2.0) { [weak self] in
                self?.connect(to: peripheral)
            }
        }
    }
}

// MARK: - CBPeripheralDelegate

extension BLEManager: CBPeripheralDelegate {

    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        if let error = error {
            print("BLEManager: Error discovering services - \(error.localizedDescription)")
            lastError = "Failed to discover services"
            return
        }

        guard let services = peripheral.services else { return }

        print("BLEManager: Discovered \(services.count) services")

        for service in services {
            print("BLEManager: Service UUID: \(service.uuid)")
            if service.uuid == timeServiceUUID {
                print("BLEManager: Discovering Time service characteristics...")
                peripheral.discoverCharacteristics([timeCharUUID, dateTimeCharUUID, volumeCharUUID, testSoundCharUUID], for: service)
            } else if service.uuid == alarmServiceUUID {
                print("BLEManager: Discovering Alarm service characteristics...")
                peripheral.discoverCharacteristics([alarmSetCharUUID, alarmListCharUUID, alarmDeleteCharUUID], for: service)
            } else {
                print("BLEManager: Unknown service UUID: \(service.uuid)")
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        if let error = error {
            print("BLEManager: Error discovering characteristics - \(error.localizedDescription)")
            return
        }

        guard let characteristics = service.characteristics else { return }

        print("BLEManager: Discovered \(characteristics.count) characteristics for service \(service.uuid)")

        for characteristic in characteristics {
            if characteristic.uuid == timeCharUUID {
                timeCharacteristic = characteristic
                print("BLEManager: Found Time characteristic")
            } else if characteristic.uuid == dateTimeCharUUID {
                dateTimeCharacteristic = characteristic
                print("BLEManager: Found DateTime characteristic")
            } else if characteristic.uuid == volumeCharUUID {
                volumeCharacteristic = characteristic
                print("BLEManager: Found Volume characteristic")
                // Read current volume
                peripheral.readValue(for: characteristic)
            } else if characteristic.uuid == testSoundCharUUID {
                testSoundCharacteristic = characteristic
                print("BLEManager: Found TestSound characteristic")
            } else if characteristic.uuid == alarmSetCharUUID {
                alarmSetCharacteristic = characteristic
                print("BLEManager: Found AlarmSet characteristic")
            } else if characteristic.uuid == alarmListCharUUID {
                alarmListCharacteristic = characteristic
                print("BLEManager: Found AlarmList characteristic")
            } else if characteristic.uuid == alarmDeleteCharUUID {
                alarmDeleteCharacteristic = characteristic
                print("BLEManager: Found AlarmDelete characteristic")
            }
        }

        // Check if all characteristics are discovered
        if timeCharacteristic != nil && dateTimeCharacteristic != nil &&
           volumeCharacteristic != nil && testSoundCharacteristic != nil &&
           alarmSetCharacteristic != nil && alarmListCharacteristic != nil &&
           alarmDeleteCharacteristic != nil {

            DispatchQueue.main.async { [weak self] in
                self?.connectionState = .ready
                print("BLEManager: All characteristics discovered, connection ready!")
            }

            // Auto-sync time and read alarms
            autoSyncTime()
            readAlarms()
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        if let error = error {
            print("BLEManager: Error reading characteristic - \(error.localizedDescription)")
            return
        }

        // Handle volume characteristic read
        if characteristic.uuid == volumeCharUUID {
            if let data = characteristic.value, let vol = data.first {
                currentVolume = Int(vol)
                print("BLEManager: Read volume from ESP32: \(currentVolume)%")
            }
            return
        }

        guard let data = characteristic.value else { return }

        if characteristic.uuid == alarmListCharUUID {
            parseAlarmList(from: data)
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        if let error = error {
            print("BLEManager: Error writing characteristic - \(error.localizedDescription)")
            lastError = "Failed to write: \(error.localizedDescription)"
        }
    }
}
