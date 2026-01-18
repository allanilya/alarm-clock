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
    @Published var displayMessage: String = ""

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
    private let displayMessageCharUUID = CBUUID(string: "12340005-1234-5678-1234-56789abcdef0")

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
    private var displayMessageCharacteristic: CBCharacteristic?
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

    /// Auto-disable one-time alarms (days=0) when their time has passed
    func checkAndDisableExpiredOneTimeAlarms() {
        let now = Date()

        var hasChanges = false

        for i in 0..<alarms.count {
            var alarm = alarms[i]

            // Only check enabled one-time alarms (daysOfWeek == 0) that aren't already permanently disabled
            guard alarm.enabled && alarm.daysOfWeek == 0 && !alarm.permanentlyDisabled else { continue }

            // Check if scheduledDate has passed
            if let scheduledDate = alarm.scheduledDate {
                if now >= scheduledDate {
                    print("BLEManager: Permanently disabling expired one-time alarm \(alarm.id) - scheduled: \(scheduledDate), now: \(now)")
                    alarms[i].enabled = false
                    alarms[i].permanentlyDisabled = true
                    hasChanges = true
                }
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

    /// Push all iOS alarms to ESP32 (iOS is the source of truth)
    private func pushAlarmsToESP32() {
        guard isConnected else { return }

        print("BLEManager: Pushing \(alarms.count) alarms from iOS to ESP32...")

        // Send each alarm to ESP32
        for alarm in alarms {
            setAlarm(alarm)
            // Small delay between writes to avoid overwhelming ESP32
            Thread.sleep(forTimeInterval: 0.1)
        }

        print("BLEManager: Finished pushing alarms to ESP32")
    }

    /// Read all alarms from ESP32 (DEPRECATED: iOS is now source of truth)
    /// This is kept only for manual refresh UI, but no longer used on connection
    func readAlarms() {
        // iOS is source of truth - we don't import from ESP32 anymore
        print("BLEManager: readAlarms() called but iOS is source of truth - no action taken")
    }

    /// Set or update an alarm (sends to ESP32 if connected, saves to CoreData)
    func setAlarm(_ alarm: Alarm) {
        guard alarm.isValid else {
            lastError = "Invalid alarm data"
            return
        }

        // Update local iOS state first (iOS is source of truth)
        DispatchQueue.main.async { [weak self] in
            guard let self = self else { return }
            if let index = self.alarms.firstIndex(where: { $0.id == alarm.id }) {
                self.alarms[index] = alarm
            } else {
                self.alarms.append(alarm)
                self.alarms.sort { $0.id < $1.id }
            }
            self.saveAlarmToCoreData(alarm)
            print("BLEManager: Set alarm \(alarm.id): \(alarm.timeString) in iOS")
        }

        // If connected to ESP32, also send the alarm
        if isConnected, let characteristic = alarmSetCharacteristic {
            guard let jsonString = alarm.toJSONString(),
                  let data = jsonString.data(using: String.Encoding.utf8) else {
                lastError = "Failed to serialize alarm to JSON"
                return
            }

            connectedPeripheral?.writeValue(data, for: characteristic, type: .withResponse)
            print("BLEManager: Pushed alarm \(alarm.id) to ESP32")
        }
    }

    /// Delete an alarm by ID (deletes from iOS and ESP32 if connected)
    func deleteAlarm(id: Int) {
        guard id >= 0 && id <= 9 else {
            lastError = "Invalid alarm ID"
            return
        }

        // Delete from iOS first (iOS is source of truth)
        DispatchQueue.main.async { [weak self] in
            guard let self = self else { return }
            self.alarms.removeAll { $0.id == id }
            self.deleteAlarmFromCoreData(id: id)
            print("BLEManager: Deleted alarm \(id) from iOS")
        }

        // If connected to ESP32, also send delete command
        if isConnected, let characteristic = alarmDeleteCharacteristic {
            let idString = String(id)
            guard let data = idString.data(using: String.Encoding.utf8) else {
                lastError = "Failed to encode alarm ID"
                return
            }

            connectedPeripheral?.writeValue(data, for: characteristic, type: .withResponse)
            print("BLEManager: Sent delete command for alarm \(id) to ESP32")
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

    /// Set display message on ESP32
    func setDisplayMessage(_ message: String) {
        guard let characteristic = displayMessageCharacteristic else {
            lastError = "Display message characteristic not found"
            return
        }

        // Truncate to 100 characters (ESP32 supports scrolling for long messages)
        let truncatedMessage = String(message.prefix(100))

        guard let data = truncatedMessage.data(using: .utf8) else {
            lastError = "Failed to encode display message"
            return
        }

        connectedPeripheral?.writeValue(data, for: characteristic, type: .withResponse)
        displayMessage = truncatedMessage
        print("BLEManager: Set display message: \"\(truncatedMessage)\"")
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
        // iOS is source of truth - we don't import alarms from ESP32
        print("BLEManager: Received alarm list from ESP32 but ignoring (iOS is source of truth)")
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
                peripheral.discoverCharacteristics([timeCharUUID, dateTimeCharUUID, volumeCharUUID, testSoundCharUUID, displayMessageCharUUID], for: service)
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
            } else if characteristic.uuid == displayMessageCharUUID {
                displayMessageCharacteristic = characteristic
                print("BLEManager: Found DisplayMessage characteristic")
                // Read current display message
                peripheral.readValue(for: characteristic)
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

            // Auto-sync time and push iOS alarms to ESP32
            autoSyncTime()
            pushAlarmsToESP32()
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

        // Handle display message characteristic read
        if characteristic.uuid == displayMessageCharUUID {
            if let data = characteristic.value, let message = String(data: data, encoding: .utf8) {
                DispatchQueue.main.async { [weak self] in
                    self?.displayMessage = message
                }
                print("BLEManager: Read display message from ESP32: \"\(message)\"")
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
