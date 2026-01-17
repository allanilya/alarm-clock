//
//  SoundView.swift
//  Alarm Clock
//
//  Created by Allan on 1/16/26.
//

import SwiftUI

struct SoundView: View {
    @Binding var selectedSound: String
    @Environment(\.dismiss) private var dismiss

    // Available alarm sounds
    let availableSounds = [
        "tone1": "Tone 1",
        "tone2": "Tone 2",
        "tone3": "Tone 3"
    ]

    var body: some View {
        List {
            Section {
                ForEach(Array(availableSounds.keys.sorted()), id: \.self) { soundKey in
                    Button(action: {
                        selectedSound = soundKey
                    }) {
                        HStack {
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

            // Placeholder for custom sounds (Phase D)
            Section {
                Text("Upload custom sounds in Phase D")
                    .foregroundColor(.secondary)
                    .font(.caption)
            } header: {
                Text("Custom Sounds")
            }
        }
        .navigationTitle("Sound")
    }
}

#Preview {
    NavigationStack {
        SoundView(selectedSound: .constant("tone1"))
    }
}
