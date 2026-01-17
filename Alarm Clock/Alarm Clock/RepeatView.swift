//
//  RepeatView.swift
//  Alarm Clock
//
//  Created by Allan on 1/16/26.
//

import SwiftUI

struct RepeatView: View {
    @Binding var daysOfWeek: Int
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        List {
            // Quick select buttons section
            Section {
                QuickSelectButton(title: "Never", value: 0x00, current: $daysOfWeek)
                QuickSelectButton(title: "Every Day", value: 0x7F, current: $daysOfWeek)
                QuickSelectButton(title: "Weekdays", value: 0x3E, current: $daysOfWeek)
                QuickSelectButton(title: "Weekends", value: 0x41, current: $daysOfWeek)
            }

            // Individual day toggles
            Section {
                ForEach(Alarm.DayOfWeek.allCases, id: \.rawValue) { day in
                    DayToggleRow(day: day, daysOfWeek: $daysOfWeek)
                }
            }
        }
        .navigationTitle("Repeat")
    }
}

// Quick select button that highlights when active
struct QuickSelectButton: View {
    let title: String
    let value: Int
    @Binding var current: Int

    var isSelected: Bool {
        current == value
    }

    var body: some View {
        Button(action: {
            current = value
        }) {
            HStack {
                Text(title)
                    .foregroundColor(.primary)
                Spacer()
                if isSelected {
                    Image(systemName: "checkmark")
                        .foregroundColor(.blue)
                }
            }
        }
    }
}

// Individual day toggle row
struct DayToggleRow: View {
    let day: Alarm.DayOfWeek
    @Binding var daysOfWeek: Int

    var isEnabled: Bool {
        (daysOfWeek & day.rawValue) != 0
    }

    var body: some View {
        Toggle(day.name, isOn: Binding(
            get: { isEnabled },
            set: { newValue in
                if newValue {
                    daysOfWeek |= day.rawValue  // Set bit
                } else {
                    daysOfWeek &= ~day.rawValue  // Clear bit
                }
            }
        ))
    }
}

#Preview {
    NavigationStack {
        RepeatView(daysOfWeek: .constant(0x7F))
    }
}
