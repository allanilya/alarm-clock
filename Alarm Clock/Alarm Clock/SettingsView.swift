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
    @State private var brightness: Double = 50

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

                Section(header: Text("Display Brightness")) {
                    VStack(alignment: .leading, spacing: 10) {
                        HStack {
                            Text("Brightness")
                            Spacer()
                            Text("\(Int(brightness))%")
                                .foregroundColor(.secondary)
                                .font(.headline)
                        }

                        Slider(value: $brightness, in: 0...100, step: 1)
                            .onChange(of: brightness) {
                                bleManager.setBrightness(Int(brightness))
                            }

                        HStack(spacing: 10) {
                            Button {
                                brightness = 0
                                bleManager.setBrightness(0)
                            } label: {
                                HStack {
                                    Image(systemName: "lightbulb.slash")
                                    Text("Off")
                                }
                                .frame(maxWidth: .infinity)
                            }
                            .buttonStyle(.bordered)

                            Button {
                                brightness = 100
                                bleManager.setBrightness(100)
                            } label: {
                                HStack {
                                    Image(systemName: "lightbulb.max")
                                    Text("Max")
                                }
                                .frame(maxWidth: .infinity)
                            }
                            .buttonStyle(.bordered)
                        }
                        .disabled(!bleManager.isConnected)

                        if !bleManager.isConnected {
                            Text("Connect to ESP32 to adjust brightness")
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

                Section(header: Text("Bottom Row Label"), footer: Text("Custom label appears on bottom row. Day and date move under time. Leave empty for default layout.")) {
                    VStack(alignment: .leading, spacing: 10) {
                        TextField("Enter bottom label (max 50 chars)", text: $bleManager.bottomRowLabel)
                            .onChange(of: bleManager.bottomRowLabel) {
                                // Truncate to 50 chars
                                if bleManager.bottomRowLabel.count > 50 {
                                    bleManager.bottomRowLabel = String(bleManager.bottomRowLabel.prefix(50))
                                }
                                // Send to ESP32
                                bleManager.setBottomRowLabel(bleManager.bottomRowLabel)
                            }
                            .disabled(!bleManager.isConnected)

                        if !bleManager.isConnected {
                            Text("Connect to ESP32 to set bottom label")
                                .font(.caption)
                                .foregroundColor(.secondary)
                        }

                        if bleManager.bottomRowLabel.isEmpty {
                            Text("Currently using default layout")
                                .font(.caption)
                                .foregroundColor(.secondary)
                        } else {
                            Text("Bottom row: \"\(bleManager.bottomRowLabel)\"")
                                .font(.caption)
                                .foregroundColor(.secondary)
                        }
                    }
                }

                Section(header: Text("Custom Sounds")) {
                    NavigationLink(destination: CustomSoundsView().environmentObject(bleManager)) {
                        HStack {
                            Image(systemName: "music.note.list")
                                .foregroundColor(.blue)
                            Text("Manage Custom Sounds")
                            Spacer()
                            if !bleManager.availableCustomSounds.isEmpty {
                                Text("\(bleManager.availableCustomSounds.count)")
                                    .foregroundColor(.secondary)
                            }
                        }
                    }
                    .disabled(!bleManager.isConnected)
                    
                    if !bleManager.isConnected {
                        Text("Connect to ESP32 to manage sounds")
                            .font(.caption)
                            .foregroundColor(.secondary)
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
                // Load current volume and brightness from BLE manager
                volume = Double(bleManager.getCurrentVolume())
                brightness = Double(bleManager.getCurrentBrightness())
            }
        }
    }
}

#Preview {
    SettingsView()
        .environmentObject(BLEManager())
}
