//
//  TimeView.swift
//  Alarm Clock
//
//  Created by Allan on 1/15/26.
//

import SwiftUI

struct TimeView: View {
    @ObservedObject var bleManager: BLEManager
    @State private var showingScanner = false

    var body: some View {
        NavigationView {
            List {
                // Connection Status Section
                Section {
                    connectionStatusRow
                } header: {
                    Text("Connection")
                }

                // Time Sync Section
                if bleManager.isConnected {
                    Section {
                        syncStatusRow
                        syncButton
                    } header: {
                        Text("Time Synchronization")
                    } footer: {
                        Text("Time is automatically synchronized when you connect. You can manually sync if needed.")
                    }
                }

                // Bluetooth Status Section (if not ready)
                if !bleManager.isBluetoothReady {
                    Section {
                        HStack {
                            ProgressView()
                                .padding(.trailing, 8)
                            Text("Waiting for Bluetooth...")
                                .foregroundColor(.secondary)
                        }
                    } header: {
                        Text("Bluetooth Status")
                    } footer: {
                        Text("Please ensure Bluetooth is enabled in Settings.")
                    }
                }

                // Device Scanner Section
                if !bleManager.isConnected && bleManager.isBluetoothReady {
                    Section {
                        scanButton
                    } header: {
                        Text("Connect to Alarm Clock")
                    } footer: {
                        Text("Make sure your ESP32-L alarm clock is powered on and nearby.")
                    }
                }

                // Error Section
                if let error = bleManager.lastError {
                    Section {
                        Text(error)
                            .foregroundColor(.red)
                            .font(.caption)
                    } header: {
                        Text("Last Error")
                    }
                }
            }
            .navigationTitle("Time Sync")
            .sheet(isPresented: $showingScanner) {
                DeviceScannerView(bleManager: bleManager, isPresented: $showingScanner)
            }
        }
    }

    // MARK: - Connection Status Row

    private var connectionStatusRow: some View {
        HStack {
            Circle()
                .fill(bleManager.isConnected ? Color.green : Color.red)
                .frame(width: 12, height: 12)

            Text(bleManager.isConnected ? "Connected to ESP32-L" : "Not Connected")
                .font(.body)

            Spacer()

            if bleManager.isConnected {
                Button("Disconnect") {
                    bleManager.disconnect()
                }
                .font(.caption)
                .foregroundColor(.red)
            }
        }
    }

    // MARK: - Sync Status Row

    private var syncStatusRow: some View {
        HStack {
            Image(systemName: "clock.arrow.circlepath")
                .foregroundColor(.accentColor)

            VStack(alignment: .leading, spacing: 4) {
                Text("Last Sync")
                    .font(.body)

                if let lastSync = bleManager.lastSyncTime {
                    Text(formatSyncTime(lastSync))
                        .font(.caption)
                        .foregroundColor(.secondary)
                } else {
                    Text("Never")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
            }

            Spacer()
        }
    }

    // MARK: - Sync Button

    private var syncButton: some View {
        Button(action: {
            bleManager.syncTime()
        }) {
            HStack {
                Spacer()
                Image(systemName: "arrow.clockwise")
                Text("Sync Now")
                Spacer()
            }
        }
        .disabled(!bleManager.isConnected)
    }

    // MARK: - Scan Button

    private var scanButton: some View {
        Button(action: {
            showingScanner = true
        }) {
            HStack {
                Spacer()
                Image(systemName: "magnifyingglass")
                Text("Scan for Devices")
                Spacer()
            }
        }
        .disabled(!bleManager.isBluetoothReady)
    }

    // MARK: - Helper Methods

    private func formatSyncTime(_ date: Date) -> String {
        let formatter = DateFormatter()
        formatter.dateStyle = .medium
        formatter.timeStyle = .short
        return formatter.string(from: date)
    }
}

// MARK: - Device Scanner View

struct DeviceScannerView: View {
    @ObservedObject var bleManager: BLEManager
    @Binding var isPresented: Bool

    var body: some View {
        NavigationView {
            VStack {
                if bleManager.isScanning {
                    ProgressView()
                        .padding()
                    Text("Scanning for devices...")
                        .foregroundColor(.secondary)
                }

                List(bleManager.discoveredDevices, id: \.identifier) { peripheral in
                    Button(action: {
                        bleManager.connect(to: peripheral)
                        isPresented = false
                    }) {
                        HStack {
                            Image(systemName: "antenna.radiowaves.left.and.right")
                                .foregroundColor(.accentColor)

                            VStack(alignment: .leading, spacing: 4) {
                                Text(peripheral.name ?? "Unknown Device")
                                    .font(.body)

                                Text(peripheral.identifier.uuidString)
                                    .font(.caption2)
                                    .foregroundColor(.secondary)
                            }

                            Spacer()

                            Image(systemName: "chevron.right")
                                .font(.caption)
                                .foregroundColor(.secondary)
                        }
                    }
                }

                if bleManager.discoveredDevices.isEmpty && !bleManager.isScanning {
                    VStack(spacing: 20) {
                        Image(systemName: "wifi.slash")
                            .font(.system(size: 50))
                            .foregroundColor(.gray)

                        Text("No devices found")
                            .font(.title3)

                        VStack(alignment: .leading, spacing: 8) {
                            Text("Troubleshooting:")
                                .font(.caption)
                                .fontWeight(.semibold)
                            Text("• ESP32-L must be powered on")
                            Text("• Firmware must be running")
                            Text("• Device should advertise as 'ESP32-L Alarm'")
                            Text("• Try tapping 'Scan' again")
                        }
                        .font(.caption)
                        .foregroundColor(.secondary)
                        .padding(.horizontal, 40)
                    }
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                }
            }
            .navigationTitle("Select Device")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel") {
                        bleManager.stopScanning()
                        isPresented = false
                    }
                }

                ToolbarItem(placement: .confirmationAction) {
                    Button(action: {
                        if bleManager.isScanning {
                            bleManager.stopScanning()
                        } else {
                            bleManager.startScanning()
                        }
                    }) {
                        Text(bleManager.isScanning ? "Stop" : "Scan")
                    }
                }
            }
            .onAppear {
                // Wait a moment for Bluetooth to initialize, then start scanning
                DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) {
                    bleManager.startScanning()
                }
            }
            .onDisappear {
                bleManager.stopScanning()
            }
        }
    }
}

// MARK: - Preview

#Preview {
    TimeView(bleManager: BLEManager())
}
