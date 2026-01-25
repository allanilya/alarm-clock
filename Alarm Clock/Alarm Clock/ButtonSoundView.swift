//
//  ButtonSoundView.swift
//  Alarm Clock
//
//  Created by Allan on 1/24/26.
//

import SwiftUI

struct ButtonSoundView: View {
    @Binding var selectedSound: String
    @EnvironmentObject var bleManager: BLEManager
    @Environment(\.dismiss) private var dismiss

    // Available alarm sounds (built-in tones)
    let availableSounds = [
        "tone1": "Tone 1 (Low)",
        "tone2": "Tone 2 (Medium)",
        "tone3": "Tone 3 (High)"
    ]

    var body: some View {
        List {
            // None option
            Section {
                Button(action: {
                    selectedSound = ""
                    bleManager.setButtonSound("")
                }) {
                    HStack {
                        Image(systemName: "speaker.slash")
                            .foregroundColor(.secondary)
                        Text("None")
                            .foregroundColor(.primary)
                        Spacer()
                        if selectedSound.isEmpty {
                            Image(systemName: "checkmark")
                                .foregroundColor(.blue)
                        }
                    }
                }
            } header: {
                Text("No Sound")
            } footer: {
                Text("Button presses will be silent")
            }

            // Built-in tones
            Section {
                ForEach(Array(availableSounds.keys.sorted()), id: \.self) { soundKey in
                    Button(action: {
                        selectedSound = soundKey
                        bleManager.setButtonSound(soundKey)
                    }) {
                        HStack {
                            Image(systemName: "waveform")
                                .foregroundColor(.secondary)
                            Text(availableSounds[soundKey] ?? soundKey)
                                .foregroundColor(.primary)
                            Spacer()
                            if selectedSound == soundKey {
                                Image(systemName: "checkmark")
                                    .foregroundColor(.blue)
                            }
                        }
                    }
                }
            } header: {
                Text("Built-in Tones")
            }

            // Custom sounds section
            Section {
                if bleManager.availableCustomSounds.isEmpty {
                    Text("No custom sounds available")
                        .foregroundColor(.secondary)
                        .font(.callout)
                } else {
                    ForEach(bleManager.availableCustomSounds, id: \.self) { soundFile in
                        Button(action: {
                            selectedSound = soundFile
                            bleManager.setButtonSound(soundFile)
                        }) {
                            HStack {
                                Image(systemName: "music.note")
                                    .foregroundColor(.secondary)
                                Text(displayName(for: soundFile))
                                    .foregroundColor(.primary)
                                Spacer()
                                if selectedSound == soundFile {
                                    Image(systemName: "checkmark")
                                        .foregroundColor(.blue)
                                }
                            }
                        }
                    }
                }

                NavigationLink(destination: CustomSoundsView()) {
                    HStack {
                        Image(systemName: "folder.badge.plus")
                            .foregroundColor(.blue)
                        Text("Manage Custom Sounds")
                    }
                }
                .disabled(!bleManager.isConnected)
            } header: {
                Text("Custom Sounds")
            } footer: {
                Text("Upload custom sounds from the Settings tab. Maximum 5 seconds recommended for button feedback.")
            }
        }
        .navigationTitle("Button Sound")
        .navigationBarTitleDisplayMode(.inline)
        .onAppear {
            // Refresh custom sounds list
            Task {
                await bleManager.refreshCustomSoundsList()
            }
        }
    }

    /// Generate display name from filename
    private func displayName(for filename: String) -> String {
        var name = filename

        // Remove extension
        if let dotIndex = name.lastIndex(of: ".") {
            name = String(name[..<dotIndex])
        }

        // Replace underscores with spaces
        name = name.replacingOccurrences(of: "_", with: " ")

        // Capitalize first letter
        if let first = name.first {
            name = first.uppercased() + name.dropFirst()
        }

        return name
    }
}

#Preview {
    NavigationStack {
        ButtonSoundView(selectedSound: .constant(""))
            .environmentObject(BLEManager())
    }
}
