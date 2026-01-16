//
//  AlarmEditView.swift
//  Alarm Clock
//
//  Created by Allan on 1/15/26.
//

import SwiftUI

struct AlarmEditView: View {
    @ObservedObject var bleManager: BLEManager
    @Environment(\.dismiss) private var dismiss

    // Alarm being edited (nil for new alarm)
    let alarm: Alarm?

    // Form state
    @State private var hour: Int
    @State private var minute: Int
    @State private var daysOfWeek: Int
    @State private var sound: String
    @State private var enabled: Bool
    @State private var alarmId: Int

    // UI state
    @State private var showingError = false
    @State private var errorMessage = ""

    // Available sounds
    private let availableSounds = ["tone1", "tone2", "tone3"]

    init(bleManager: BLEManager, alarm: Alarm?) {
        self.bleManager = bleManager
        self.alarm = alarm

        // Initialize state from alarm or defaults
        if let alarm = alarm {
            _hour = State(initialValue: alarm.hour)
            _minute = State(initialValue: alarm.minute)
            _daysOfWeek = State(initialValue: alarm.daysOfWeek)
            _sound = State(initialValue: alarm.sound)
            _enabled = State(initialValue: alarm.enabled)
            _alarmId = State(initialValue: alarm.id)
        } else {
            _hour = State(initialValue: 7)
            _minute = State(initialValue: 0)
            _daysOfWeek = State(initialValue: 0x7F)  // Every day
            _sound = State(initialValue: "tone1")
            _enabled = State(initialValue: true)
            _alarmId = State(initialValue: -1)  // Will be assigned
        }
    }

    var body: some View {
        NavigationView {
            Form {
                // Time Picker Section
                Section {
                    timePicker
                } header: {
                    Text("Time")
                }

                // Days of Week Section
                Section {
                    daysOfWeekPicker
                } header: {
                    Text("Repeat")
                }

                // Sound Selection Section
                Section {
                    soundPicker
                } header: {
                    Text("Sound")
                }

                // Enable/Disable Section
                Section {
                    Toggle("Enabled", isOn: $enabled)
                }
            }
            .navigationTitle(alarm == nil ? "Add Alarm" : "Edit Alarm")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel") {
                        dismiss()
                    }
                }

                ToolbarItem(placement: .confirmationAction) {
                    Button("Save") {
                        saveAlarm()
                    }
                    .disabled(!isValid)
                }
            }
            .alert("Error", isPresented: $showingError) {
                Button("OK", role: .cancel) { }
            } message: {
                Text(errorMessage)
            }
        }
    }

    // MARK: - Time Picker

    private var timePicker: some View {
        HStack {
            Picker("Hour", selection: $hour) {
                ForEach(0..<24, id: \.self) { h in
                    Text(hourString(h))
                        .tag(h)
                }
            }
            .pickerStyle(.wheel)
            .frame(maxWidth: .infinity)

            Text(":")
                .font(.title)

            Picker("Minute", selection: $minute) {
                ForEach(0..<60, id: \.self) { m in
                    Text(String(format: "%02d", m))
                        .tag(m)
                }
            }
            .pickerStyle(.wheel)
            .frame(maxWidth: .infinity)

            Text(hour < 12 ? "AM" : "PM")
                .font(.title3)
                .foregroundColor(.secondary)
                .frame(width: 50)
        }
        .frame(height: 120)
    }

    private func hourString(_ hour: Int) -> String {
        if hour == 0 {
            return "12"
        } else if hour <= 12 {
            return String(hour)
        } else {
            return String(hour - 12)
        }
    }

    // MARK: - Days of Week Picker

    private var daysOfWeekPicker: some View {
        VStack(spacing: 12) {
            // Quick select buttons
            HStack(spacing: 8) {
                quickSelectButton(title: "Every Day", bitmask: 0x7F)
                quickSelectButton(title: "Weekdays", bitmask: 0x3E)
                quickSelectButton(title: "Weekends", bitmask: 0x41)
            }

            Divider()

            // Individual day toggles
            ForEach(Alarm.DayOfWeek.allCases, id: \.rawValue) { day in
                HStack {
                    Text(day.name)
                        .frame(maxWidth: .infinity, alignment: .leading)

                    Toggle("", isOn: Binding(
                        get: { (daysOfWeek & day.rawValue) != 0 },
                        set: { enabled in
                            if enabled {
                                daysOfWeek |= day.rawValue
                            } else {
                                daysOfWeek &= ~day.rawValue
                            }
                        }
                    ))
                    .labelsHidden()
                }
            }
        }
    }

    private func quickSelectButton(title: String, bitmask: Int) -> some View {
        Button(action: {
            daysOfWeek = bitmask
        }) {
            Text(title)
                .font(.caption)
                .padding(.horizontal, 12)
                .padding(.vertical, 6)
                .background(daysOfWeek == bitmask ? Color.accentColor : Color(.systemGray5))
                .foregroundColor(daysOfWeek == bitmask ? .white : .primary)
                .cornerRadius(8)
        }
    }

    // MARK: - Sound Picker

    private var soundPicker: some View {
        Picker("Sound", selection: $sound) {
            ForEach(availableSounds, id: \.self) { soundName in
                HStack {
                    Image(systemName: "speaker.wave.2.fill")
                    Text(soundName.capitalized)
                }
                .tag(soundName)
            }
        }
        .pickerStyle(.inline)
    }

    // MARK: - Validation

    private var isValid: Bool {
        // At least one day must be selected
        return daysOfWeek > 0
    }

    // MARK: - Save

    private func saveAlarm() {
        // Find available ID if creating new alarm
        var finalId = alarmId
        if finalId == -1 {
            // Find first available ID (0-9)
            let usedIds = Set(bleManager.alarms.map { $0.id })
            for id in 0...9 {
                if !usedIds.contains(id) {
                    finalId = id
                    break
                }
            }

            if finalId == -1 {
                errorMessage = "No available alarm slots (maximum 10 alarms)"
                showingError = true
                return
            }
        }

        let newAlarm = Alarm(
            id: finalId,
            hour: hour,
            minute: minute,
            daysOfWeek: daysOfWeek,
            sound: sound,
            enabled: enabled
        )

        guard newAlarm.isValid else {
            errorMessage = "Invalid alarm configuration"
            showingError = true
            return
        }

        bleManager.setAlarm(newAlarm)
        dismiss()
    }
}

// MARK: - Preview

#Preview {
    AlarmEditView(bleManager: BLEManager(), alarm: nil)
}
