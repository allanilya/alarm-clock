//
//  AlarmListView.swift
//  Alarm Clock
//
//  Created by Allan on 1/15/26.
//

import SwiftUI

struct AlarmListView: View {
    @ObservedObject var bleManager: BLEManager
    @State private var showingAddAlarm = false
    @State private var editingAlarm: Alarm?

    var body: some View {
        NavigationView {
            ZStack {
                if bleManager.alarms.isEmpty {
                    emptyStateView
                } else {
                    alarmListView
                }
            }
            .navigationTitle("Alarms")
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    addButton
                }
            }
            .sheet(item: $editingAlarm) { alarm in
                AlarmEditView(bleManager: bleManager, alarm: alarm)
            }
            .sheet(isPresented: $showingAddAlarm) {
                AlarmEditView(bleManager: bleManager, alarm: nil)
            }
        }
    }

    // MARK: - Connection Status Bar

    private var connectionStatusBar: some View {
        HStack {
            Circle()
                .fill(bleManager.isConnected ? Color.green : Color.red)
                .frame(width: 10, height: 10)

            Text(bleManager.isConnected ? "Connected" : "Disconnected")
                .font(.caption)
                .foregroundColor(.secondary)

            Spacer()

            if let lastSync = bleManager.lastSyncTime {
                Text("Synced \(timeAgo(lastSync))")
                    .font(.caption2)
                    .foregroundColor(.secondary)
            }
        }
        .padding(.horizontal)
        .padding(.vertical, 8)
        .background(Color(.systemGray6))
    }

    // MARK: - Alarm List View

    private var alarmListView: some View {
        VStack(spacing: 0) {
            connectionStatusBar

            List {
                ForEach(bleManager.alarms) { alarm in
                    AlarmRow(alarm: alarm, bleManager: bleManager)
                        .contentShape(Rectangle())
                        .onTapGesture {
                            editingAlarm = alarm
                        }
                }
                .onDelete(perform: deleteAlarms)
            }
            .refreshable {
                bleManager.readAlarms()
            }
        }
    }

    // MARK: - Empty State View

    private var emptyStateView: some View {
        VStack(spacing: 20) {
            connectionStatusBar

            Spacer()

            Image(systemName: "alarm.fill")
                .font(.system(size: 60))
                .foregroundColor(.gray)

            Text("No Alarms")
                .font(.title2)
                .fontWeight(.semibold)

            Text(bleManager.isConnected ?
                 "Tap + to add your first alarm" :
                 "Connect to your alarm clock to view alarms")
                .font(.body)
                .foregroundColor(.secondary)
                .multilineTextAlignment(.center)
                .padding(.horizontal, 40)

            Spacer()
        }
    }

    // MARK: - Add Button

    private var addButton: some View {
        Button(action: {
            if bleManager.alarms.count < 10 {
                showingAddAlarm = true
            }
        }) {
            Image(systemName: "plus")
        }
        .disabled(bleManager.alarms.count >= 10 || !bleManager.isConnected)
    }

    // MARK: - Actions

    private func deleteAlarms(at offsets: IndexSet) {
        for index in offsets {
            let alarm = bleManager.alarms[index]
            bleManager.deleteAlarm(id: alarm.id)
        }
    }

    private func timeAgo(_ date: Date) -> String {
        let seconds = Int(Date().timeIntervalSince(date))
        if seconds < 60 {
            return "just now"
        } else if seconds < 3600 {
            let minutes = seconds / 60
            return "\(minutes)m ago"
        } else {
            let hours = seconds / 3600
            return "\(hours)h ago"
        }
    }
}

// MARK: - Alarm Row

struct AlarmRow: View {
    let alarm: Alarm
    @ObservedObject var bleManager: BLEManager

    var body: some View {
        HStack(alignment: .top, spacing: 12) {
            VStack(alignment: .leading, spacing: 4) {
                // Time
                Text(alarm.timeString)
                    .font(.system(size: 32, weight: .light, design: .default))
                    .foregroundColor(alarm.enabled ? .primary : .gray)

                // Days
                Text(alarm.daysString)
                    .font(.subheadline)
                    .foregroundColor(.secondary)

                // Sound
                HStack(spacing: 4) {
                    Image(systemName: soundIcon(for: alarm.sound))
                        .font(.caption)
                        .foregroundColor(.secondary)

                    Text(soundName(for: alarm.sound))
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
            }

            Spacer()

            // Enable/Disable Toggle
            Toggle("", isOn: Binding(
                get: { alarm.enabled },
                set: { newValue in
                    var updatedAlarm = alarm
                    updatedAlarm.enabled = newValue
                    bleManager.setAlarm(updatedAlarm)
                }
            ))
            .labelsHidden()
        }
        .padding(.vertical, 8)
        .opacity(alarm.enabled ? 1.0 : 0.5)
    }

    private func soundIcon(for sound: String) -> String {
        if sound.hasPrefix("tone") {
            return "speaker.wave.2.fill"
        } else {
            return "music.note"
        }
    }

    private func soundName(for sound: String) -> String {
        if sound.hasPrefix("tone") {
            return sound.capitalized
        } else {
            return sound.replacingOccurrences(of: "_", with: " ")
                       .replacingOccurrences(of: ".mp3", with: "")
                       .capitalized
        }
    }
}

// MARK: - Preview

#Preview {
    AlarmListView(bleManager: {
        let manager = BLEManager()
        // Simulate connected state with sample data
        manager.alarms = Alarm.sampleAlarms
        return manager
    }())
}
