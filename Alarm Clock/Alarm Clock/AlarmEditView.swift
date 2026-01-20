//
//  AlarmEditView.swift
//  Alarm Clock
//
//  Created by Allan on 1/15/26.
//  Redesigned to match iPhone native alarm style - 1/16/26
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
    @State private var label: String
    @State private var snoozeEnabled: Bool
    @State private var permanentlyDisabled: Bool
    @State private var bottomRowLabel: String

    // Infinite scroll state for pickers
    @State private var scrollHour: Int = 100
    @State private var scrollMinute: Int = 90

    // UI state
    @State private var showingError = false
    @State private var errorMessage = ""

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
            _label = State(initialValue: alarm.label)
            _snoozeEnabled = State(initialValue: alarm.snoozeEnabled)
            _permanentlyDisabled = State(initialValue: alarm.permanentlyDisabled)
            _bottomRowLabel = State(initialValue: alarm.bottomRowLabel)
            // Initialize infinite scroll positions (middle of range)
            // Hour picker cycles 0-11, so use alarm.hour % 12
            _scrollHour = State(initialValue: 96 + (alarm.hour % 12))  // 96 = 8*12, middle of 200-item range
            _scrollMinute = State(initialValue: 60 + (alarm.minute % 60))  // 60 = middle of 180-item range
        } else {
            _hour = State(initialValue: 7)
            _minute = State(initialValue: 0)
            _daysOfWeek = State(initialValue: 0x00)  // Never (default per plan)
            _sound = State(initialValue: "tone1")
            _enabled = State(initialValue: true)
            _alarmId = State(initialValue: -1)  // Will be assigned
            _label = State(initialValue: "Alarm")
            _snoozeEnabled = State(initialValue: true)
            _permanentlyDisabled = State(initialValue: false)  // New alarms never permanently disabled
            _bottomRowLabel = State(initialValue: "")  // Default empty
            // Initialize infinite scroll positions (middle of range)
            _scrollHour = State(initialValue: 96 + 7)  // 7 AM
            _scrollMinute = State(initialValue: 90 + 0)
        }
    }

    var body: some View {
        NavigationView {
            List {
                // Large time picker section
                Section {
                    timePicker
                }
                .listRowInsets(EdgeInsets())
                .listRowBackground(Color.clear)

                // Navigation rows section
                Section {
                    // Repeat row
                    NavigationLink(destination: RepeatView(daysOfWeek: $daysOfWeek)) {
                        HStack {
                            Text("Repeat")
                            Spacer()
                            Text(repeatDescription)
                                .foregroundColor(.secondary)
                        }
                    }

                    // Label row (inline editing)
                    HStack {
                        Text("Label")
                        Spacer()
                        TextField("Alarm", text: $label)
                            .multilineTextAlignment(.trailing)
                            .foregroundColor(.secondary)
                            .textInputAutocapitalization(.sentences)
                    }

                    // Sound row
                    NavigationLink(destination: SoundView(selectedSound: $sound)) {
                        HStack {
                            Text("Sound")
                            Spacer()
                            Text(soundDisplayName)
                                .foregroundColor(.secondary)
                        }
                    }

                    // Bottom Row Label (inline editing)
                    HStack {
                        Text("Bottom Row")
                        Spacer()
                        TextField("Instructions", text: $bottomRowLabel)
                            .multilineTextAlignment(.trailing)
                            .foregroundColor(.secondary)
                            .textInputAutocapitalization(.sentences)
                    }

                    // Snooze toggle (inline, no navigation)
                    HStack {
                        Text("Snooze")
                        Spacer()
                        Toggle("", isOn: $snoozeEnabled)
                            .labelsHidden()
                    }
                }

                // Delete button section (only for existing alarms)
                if alarm != nil {
                    Section {
                        Button(role: .destructive, action: deleteAlarm) {
                            HStack {
                                Spacer()
                                Text("Delete Alarm")
                                Spacer()
                            }
                        }
                    }
                }
            }
            .navigationTitle(alarm == nil ? "Add Alarm" : "Edit Alarm")
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

    // MARK: - Large Time Picker (12-hour with AM/PM) - Infinite Scrolling

    private var timePicker: some View {
        HStack(spacing: 0) {
            // Hour picker (1-12) with infinite scrolling (200 items = ~8 full cycles)
            Picker("Hour", selection: Binding(
                get: { scrollHour },
                set: { newValue in
                    scrollHour = newValue
                    // Convert scroll position to 0-11 hour (12-hour format base)
                    let hourIn12 = ((newValue % 12) + 12) % 12
                    // Preserve current AM/PM state from hour value
                    let currentPeriod = hour >= 12 ? 12 : 0
                    hour = hourIn12 + currentPeriod
                }
            )) {
                ForEach(0..<200, id: \.self) { index in
                    let h12 = ((index % 12) + 12) % 12
                    let displayHour = h12 == 0 ? 12 : h12
                    Text(String(displayHour))
                        .tag(index)
                }
            }
            .pickerStyle(.wheel)
            .frame(maxWidth: .infinity)

            // Minute picker (00-59) with infinite scrolling (180 items = 3 full cycles)
            Picker("Minute", selection: Binding(
                get: { scrollMinute },
                set: { newValue in
                    scrollMinute = newValue
                    // Convert to 0-59 minute
                    let normalizedMinute = ((newValue % 60) + 60) % 60
                    minute = normalizedMinute
                }
            )) {
                ForEach(0..<180, id: \.self) { index in
                    let m = ((index % 60) + 60) % 60
                    Text(String(format: "%02d", m))
                        .tag(index)
                }
            }
            .pickerStyle(.wheel)
            .frame(maxWidth: .infinity)

            // AM/PM picker
            Picker("Period", selection: Binding(
                get: { hour < 12 ? 0 : 1 },
                set: { newValue in
                    let hourIn12 = hour % 12
                    if newValue == 0 {
                        // Switch to AM (0-11)
                        hour = hourIn12
                    } else {
                        // Switch to PM (12-23)
                        hour = hourIn12 + 12
                    }
                }
            )) {
                Text("AM").tag(0)
                Text("PM").tag(1)
            }
            .pickerStyle(.wheel)
            .frame(maxWidth: .infinity)
        }
        .frame(height: 200)
    }

    // MARK: - Helper Properties

    private var repeatDescription: String {
        if daysOfWeek == 0x7F {
            return "Every day"
        } else if daysOfWeek == 0x3E {
            return "Weekdays"
        } else if daysOfWeek == 0x41 {
            return "Weekends"
        } else if daysOfWeek == 0x00 {
            return "Never"
        } else {
            let alarm = Alarm(id: 0, hour: 0, minute: 0, daysOfWeek: daysOfWeek, sound: "", enabled: true)
            let dayNames = alarm.selectedDayNames
            if dayNames.count == 1 {
                return dayNames[0]
            } else {
                return dayNames.joined(separator: ", ")
            }
        }
    }

    private var soundDisplayName: String {
        switch sound {
        case "tone1": return "Tone 1"
        case "tone2": return "Tone 2"
        case "tone3": return "Tone 3"
        default: return sound.capitalized
        }
    }

    // MARK: - Validation

    private var isValid: Bool {
        // Must have a non-empty label
        return !label.isEmpty
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

        // Calculate scheduledDate for one-shot alarms
        var scheduledDate: Date? = nil
        if daysOfWeek == 0 {
            // One-shot alarm - calculate next occurrence
            let calendar = Calendar.current
            var components = calendar.dateComponents([.year, .month, .day], from: Date())
            components.hour = hour
            components.minute = minute
            components.second = 0

            if var scheduled = calendar.date(from: components) {
                // If selected time <= now, schedule for tomorrow
                if scheduled <= Date() {
                    scheduled = calendar.date(byAdding: .day, value: 1, to: scheduled)!
                }
                scheduledDate = scheduled
            }
        }

        // Always enable alarms when user edits/creates them
        // Auto-disable logic only runs when scheduled time is actually reached
        let newAlarm = Alarm(
            id: finalId,
            hour: hour,
            minute: minute,
            daysOfWeek: daysOfWeek,
            sound: sound,
            enabled: true,  // User is actively setting this alarm, so enable it
            label: label,
            snoozeEnabled: snoozeEnabled,
            permanentlyDisabled: false,  // Clear permanent disable flag when editing
            scheduledDate: scheduledDate,
            bottomRowLabel: bottomRowLabel
        )

        guard newAlarm.isValid else {
            errorMessage = "Invalid alarm configuration"
            showingError = true
            return
        }

        bleManager.setAlarm(newAlarm)
        dismiss()
    }

    // MARK: - Delete

    private func deleteAlarm() {
        guard let alarm = alarm else { return }
        bleManager.deleteAlarm(id: alarm.id)
        dismiss()
    }
}

// MARK: - Preview

#Preview {
    AlarmEditView(bleManager: BLEManager(), alarm: nil)
}
