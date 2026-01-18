//
//  SettingsView.swift
//  Alarm Clock
//
//  Settings view for volume control and ESP32 sound preview
//

import SwiftUI

struct SettingsView: View {
    @EnvironmentObject var bleManager: BLEManager
    @State private var volume: Double = 70

    var body: some View {
        NavigationView {
            Form {
                Section(header: Text("Volume Control")) {
                    VStack(alignment: .leading, spacing: 10) {
                        HStack {
                            Text("Volume")
                            Spacer()
                            Text("\(Int(volume))%")
                                .foregroundColor(.secondary)
                                .font(.headline)
                        }

                        Slider(value: $volume, in: 0...100, step: 1)
                            .onChange(of: volume) {
                                bleManager.setVolume(Int(volume))
                            }

                        Button {
                            bleManager.testSound()
                        } label: {
                            HStack {
                                Image(systemName: "speaker.wave.3")
                                Text("Test Sound on ESP32")
                            }
                            .frame(maxWidth: .infinity)
                        }
                        .buttonStyle(.borderedProminent)
                        .disabled(!bleManager.isConnected)

                        if !bleManager.isConnected {
                            Text("Connect to ESP32 to adjust volume")
                                .font(.caption)
                                .foregroundColor(.secondary)
                        }
                    }
                }

                Section(header: Text("Display Message"), footer: Text("Custom message appears on top row of display. Long messages will scroll. Leave empty to show day of week.")) {
                    VStack(alignment: .leading, spacing: 10) {
                        TextField("Enter custom message (max 100 chars)", text: $bleManager.displayMessage)
                            .onChange(of: bleManager.displayMessage) {
                                // Truncate to 100 chars
                                if bleManager.displayMessage.count > 100 {
                                    bleManager.displayMessage = String(bleManager.displayMessage.prefix(100))
                                }
                                // Send to ESP32
                                bleManager.setDisplayMessage(bleManager.displayMessage)
                            }
                            .disabled(!bleManager.isConnected)

                        if !bleManager.isConnected {
                            Text("Connect to ESP32 to set display message")
                                .font(.caption)
                                .foregroundColor(.secondary)
                        }

                        if bleManager.displayMessage.isEmpty {
                            Text("Currently displaying: Day of Week")
                                .font(.caption)
                                .foregroundColor(.secondary)
                        } else {
                            Text("Currently displaying: \"\(bleManager.displayMessage)\"")
                                .font(.caption)
                                .foregroundColor(.secondary)
                        }
                    }
                }

                Section(header: Text("About")) {
                    HStack {
                        Text("Device")
                        Spacer()
                        Text(bleManager.isConnected ? "ESP32-L Alarm" : "Not Connected")
                            .foregroundColor(.secondary)
                    }

                    HStack {
                        Text("Connection Status")
                        Spacer()
                        Circle()
                            .fill(bleManager.isConnected ? Color.green : Color.gray)
                            .frame(width: 10, height: 10)
                    }
                }
            }
            .navigationTitle("Settings")
            .onAppear {
                // Load current volume from BLE manager
                volume = Double(bleManager.getCurrentVolume())
            }
        }
    }
}

#Preview {
    SettingsView()
        .environmentObject(BLEManager())
}
