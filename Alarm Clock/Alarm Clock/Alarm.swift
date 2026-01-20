//
//  Alarm.swift
//  Alarm Clock
//
//  Created by Allan on 1/15/26.
//

import Foundation

/// Alarm data model matching ESP32 AlarmData struct
struct Alarm: Identifiable, Codable {
    var id: Int              // Alarm ID (0-9, max 10 alarms)
    var hour: Int            // Hour (0-23)
    var minute: Int          // Minute (0-59)
    var daysOfWeek: Int      // Bitmask for days
    var sound: String        // Sound: "tone1", "tone2", "tone3", or MP3 filename
    var enabled: Bool        // Is alarm active?
    var label: String        // Custom alarm name
    var snoozeEnabled: Bool  // Is snooze enabled for this alarm?
    var permanentlyDisabled: Bool  // One-shot alarms permanently disabled after firing
    var scheduledDate: Date?  // Absolute scheduled time for one-shot alarms (nil for repeating alarms)
    var bottomRowLabel: String  // Custom bottom row text (replaces instructions when alarm rings)

    // MARK: - Initialization

    init(id: Int = 0, hour: Int = 7, minute: Int = 0, daysOfWeek: Int = 0x7F, sound: String = "tone1", enabled: Bool = true, label: String = "Alarm", snoozeEnabled: Bool = true, permanentlyDisabled: Bool = false, scheduledDate: Date? = nil, bottomRowLabel: String = "") {
        self.id = id
        self.hour = hour
        self.minute = minute
        self.daysOfWeek = daysOfWeek
        self.sound = sound
        self.enabled = enabled
        self.label = label
        self.snoozeEnabled = snoozeEnabled
        self.permanentlyDisabled = permanentlyDisabled
        self.scheduledDate = scheduledDate
        self.bottomRowLabel = bottomRowLabel
    }

    // MARK: - Computed Properties

    /// Formatted time string (e.g., "7:00 AM")
    var timeString: String {
        let period = hour < 12 ? "AM" : "PM"
        let displayHour = hour == 0 ? 12 : (hour > 12 ? hour - 12 : hour)
        return String(format: "%d:%02d %@", displayHour, minute, period)
    }

    /// Short description of days (e.g., "Every day", "Weekdays", "Mon, Fri")
    var daysString: String {
        if daysOfWeek == 0x7F {
            return "Every day"
        } else if daysOfWeek == 0x3E {
            return "Weekdays"
        } else if daysOfWeek == 0x41 {
            return "Weekends"
        } else if daysOfWeek == 0 {
            return "Never"
        } else {
            let dayNames = selectedDayNames
            if dayNames.count == 1 {
                return dayNames[0]
            } else if dayNames.count <= 3 {
                return dayNames.joined(separator: ", ")
            } else {
                return "\(dayNames.count) days"
            }
        }
    }

    /// Array of selected day names
    var selectedDayNames: [String] {
        var days: [String] = []
        if isDayEnabled(.sunday) { days.append("Sun") }
        if isDayEnabled(.monday) { days.append("Mon") }
        if isDayEnabled(.tuesday) { days.append("Tue") }
        if isDayEnabled(.wednesday) { days.append("Wed") }
        if isDayEnabled(.thursday) { days.append("Thu") }
        if isDayEnabled(.friday) { days.append("Fri") }
        if isDayEnabled(.saturday) { days.append("Sat") }
        return days
    }

    // MARK: - Day-of-Week Bitmask Helpers

    enum DayOfWeek: Int, CaseIterable {
        case sunday = 0x01      // 1
        case monday = 0x02      // 2
        case tuesday = 0x04     // 4
        case wednesday = 0x08   // 8
        case thursday = 0x10    // 16
        case friday = 0x20      // 32
        case saturday = 0x40    // 64

        var name: String {
            switch self {
            case .sunday: return "Sunday"
            case .monday: return "Monday"
            case .tuesday: return "Tuesday"
            case .wednesday: return "Wednesday"
            case .thursday: return "Thursday"
            case .friday: return "Friday"
            case .saturday: return "Saturday"
            }
        }

        var shortName: String {
            switch self {
            case .sunday: return "Sun"
            case .monday: return "Mon"
            case .tuesday: return "Tue"
            case .wednesday: return "Wed"
            case .thursday: return "Thu"
            case .friday: return "Fri"
            case .saturday: return "Sat"
            }
        }
    }

    /// Check if a specific day is enabled
    func isDayEnabled(_ day: DayOfWeek) -> Bool {
        return (daysOfWeek & day.rawValue) != 0
    }

    /// Enable or disable a specific day
    mutating func setDay(_ day: DayOfWeek, enabled: Bool) {
        if enabled {
            daysOfWeek |= day.rawValue  // Set bit
        } else {
            daysOfWeek &= ~day.rawValue  // Clear bit
        }
    }

    /// Toggle a specific day
    mutating func toggleDay(_ day: DayOfWeek) {
        daysOfWeek ^= day.rawValue  // XOR to toggle
    }

    // MARK: - JSON Serialization

    /// Convert to JSON dictionary for BLE transmission
    /// Note: scheduledDate is NOT included - it's iOS-only metadata for auto-disable logic
    func toJSON() -> [String: Any] {
        return [
            "id": id,
            "hour": hour,
            "minute": minute,
            "days": daysOfWeek,
            "sound": sound,
            "enabled": enabled,
            "label": label,
            "snooze": snoozeEnabled,
            "perm_disabled": permanentlyDisabled,
            "bottomRowLabel": bottomRowLabel
        ]
    }

    /// Convert to JSON string for BLE transmission
    func toJSONString() -> String? {
        guard let jsonData = try? JSONSerialization.data(withJSONObject: toJSON(), options: []),
              let jsonString = String(data: jsonData, encoding: .utf8) else {
            return nil
        }
        return jsonString
    }

    /// Create from JSON dictionary
    static func fromJSON(_ json: [String: Any]) -> Alarm? {
        guard let id = json["id"] as? Int,
              let hour = json["hour"] as? Int,
              let minute = json["minute"] as? Int,
              let days = json["days"] as? Int,
              let sound = json["sound"] as? String,
              let enabled = json["enabled"] as? Bool else {
            return nil
        }

        // Optional fields with defaults for backwards compatibility
        let label = json["label"] as? String ?? "Alarm"
        let snoozeEnabled = json["snooze"] as? Bool ?? true
        let permanentlyDisabled = json["perm_disabled"] as? Bool ?? false
        let bottomRowLabel = json["bottomRowLabel"] as? String ?? ""

        // scheduledDate is not transmitted via BLE - it's iOS-only metadata
        // It will be recalculated when the alarm is saved in iOS
        return Alarm(id: id, hour: hour, minute: minute, daysOfWeek: days, sound: sound, enabled: enabled, label: label, snoozeEnabled: snoozeEnabled, permanentlyDisabled: permanentlyDisabled, scheduledDate: nil, bottomRowLabel: bottomRowLabel)
    }

    // MARK: - Validation

    /// Check if alarm data is valid
    var isValid: Bool {
        return id >= 0 && id <= 9 &&
               hour >= 0 && hour <= 23 &&
               minute >= 0 && minute <= 59 &&
               daysOfWeek >= 0 &&  // Allow 0 (Never) as valid
               !sound.isEmpty
    }
}

// MARK: - Sample Data

extension Alarm {
    static let sampleAlarms: [Alarm] = [
        Alarm(id: 0, hour: 7, minute: 0, daysOfWeek: 0x3E, sound: "tone1", enabled: true),
        Alarm(id: 1, hour: 22, minute: 30, daysOfWeek: 0x7F, sound: "tone2", enabled: true),
        Alarm(id: 2, hour: 8, minute: 30, daysOfWeek: 0x41, sound: "tone3", enabled: false)
    ]
}
