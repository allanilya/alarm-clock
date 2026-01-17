PHASE B: iOS UI REDESIGN (3-4 hours)
Goal: Redesign AlarmEditView to match iPhone native alarm style

User Requirements:

Large time wheel picker (12-hour with AM/PM)
4 clickable rows: Repeat, Label, Sound, Snooze
Each row navigates to dedicated screen
"Never" as default for Repeat
Per-alarm snooze toggle
Tasks:

Update Alarm Data Model (30 min)

File: /Users/Allan/alarm-clock/Alarm Clock/Alarm Clock/Alarm.swift
Add var label: String = "Alarm" field (line 17)
Add var snoozeEnabled: Bool = true field (line 18)
Update toJSON() to include label and snooze
Update JSON parsing in fromJSON()
Create RepeatView.swift (45 min) - NEW FILE

Day selection screen with quick select buttons
"Never" (0x00), "Every Day" (0x7F), "Weekdays" (0x3E), "Weekends" (0x41)
Individual day toggles with bitmask binding
Navigation title "Repeat"
Create LabelView.swift (15 min) - NEW FILE

Simple TextField for alarm name
Text input with auto-capitalization
Navigation title "Label"
Create SoundView.swift (30 min) - NEW FILE

List of available sounds with checkmark for selected
Tap to select + preview (Phase D)
Shows: "Tone 1", "Tone 2", "Tone 3", + custom tones
Navigation title "Sound"
Rewrite AlarmEditView.swift (60 min)

Replace Form with List
Large time wheel at top (12-hour format with AM/PM)
4 NavigationLink rows:
Repeat → RepeatView (shows current: "Never", "Every day", "Weekdays", etc.)
Label → LabelView (shows current label)
Sound → SoundView (shows current sound name)
Snooze → Toggle directly in row (no navigation)
Delete button at bottom (if editing existing alarm)
Save/Cancel in toolbar
Update BLE Communication (30 min)

Modify JSON format to include label and snooze fields
Update ESP32 BLE parsing to handle new fields
Update alarm list JSON generation
Files Created:

/Users/Allan/alarm-clock/Alarm Clock/Alarm Clock/RepeatView.swift (NEW)
/Users/Allan/alarm-clock/Alarm Clock/Alarm Clock/LabelView.swift (NEW)
/Users/Allan/alarm-clock/Alarm Clock/Alarm Clock/SoundView.swift (NEW)
Files Modified:

/Users/Allan/alarm-clock/Alarm Clock/Alarm Clock/Alarm.swift (add label + snooze)
/Users/Allan/alarm-clock/Alarm Clock/Alarm Clock/AlarmEditView.swift (complete rewrite)
/Users/Allan/alarm-clock/src/alarm_manager.h (add label to AlarmData struct)
/Users/Allan/alarm-clock/src/ble_time_sync.cpp (update JSON parsing)
Success Criteria:

✅ Time picker shows 12-hour format with AM/PM
✅ Repeat shows "Never" by default, navigates to day selection
✅ Label field accepts custom text
✅ Sound selection works
✅ Snooze toggle per alarm
✅ UI matches iPhone native style
PHASE C: DISPLAY CUSTOMIZATION (2-3 hours)
Goal: Implement top row custom messaging and improve display refresh

User Requirements:

Top row shows custom message (user-defined) OR alarm label (when ringing)
If no custom message set: top row shows day of week (current behavior)
Bottom row always shows date, or date+day if custom message is active
Reduce display refresh flashing (only at 3 AM)
Display Layout Logic:


WITHOUT custom message:
  Top: "Wednesday"
  Middle: "3:45 PM" [seconds clock]
  Bottom: "January 16, 2026"

WITH custom message:
  Top: "Good Morning"
  Middle: "3:45 PM" [seconds clock]
  Bottom: "January 16, 2026 Wednesday"

ALARM RINGING:
  Top: "Morning Routine" (alarm label)
  Middle: "7:30 AM" [ringing icon]
  Bottom: "January 16, 2026 Wednesday"
Tasks:

Add Custom Message BLE Characteristic (45 min)

File: /Users/Allan/alarm-clock/src/ble_time_sync.cpp
Add DisplayMessage characteristic (UUID: 12340003-...)
Read/Write property, max 50 characters
Save to NVS for persistence
Callback updates DisplayManager
Update DisplayManager (60 min)

File: /Users/Allan/alarm-clock/src/display_manager.cpp
Add String _customMessage member variable
Add setCustomMessage() and getCustomMessage() methods
Load from NVS in begin()
Update showClock() layout logic:
If custom message set: Show message + date+day
If no custom message: Show day + date (current)
Update showAlarmRinging() to use alarm.label on top row
Move day to bottom row when custom message or alarm label active
Update Refresh Strategy (20 min)

File: /Users/Allan/alarm-clock/src/display_manager.cpp
Change full refresh from every 1 hour to only at 3 AM
Update condition: if (hour == 3 && minute == 0) trigger full refresh
Reduces disruptive flashing during day
Add Alarm Label Support in Firmware (30 min)

File: /Users/Allan/alarm-clock/src/alarm_manager.h
Add String label to AlarmData struct
File: /Users/Allan/alarm-clock/src/alarm_manager.cpp
Update NVS save/load format to include label
Update JSON parsing to handle label field
Create Settings View in iOS App (45 min)

File: /Users/Allan/alarm-clock/Alarm Clock/Alarm Clock/SettingsView.swift (NEW)
TextField for custom display message
onChange handler sends to BLE
Section header: "Display Message"
Footer: "Appears on top row when no alarm is ringing"
Add Settings Tab (10 min)

File: /Users/Allan/alarm-clock/Alarm Clock/Alarm Clock/ContentView.swift
Add third tab for Settings
Icon: "gear"
Update BLEManager (20 min)

Add displayMessageCharacteristic property
Add setDisplayMessage() method
Add UUID to discovery list
Handle characteristic in discovery callback
Files Created:

/Users/Allan/alarm-clock/Alarm Clock/Alarm Clock/SettingsView.swift (NEW)
Files Modified:

/Users/Allan/alarm-clock/src/ble_time_sync.cpp (add DisplayMessage char)
/Users/Allan/alarm-clock/src/ble_time_sync.h (add UUID)
/Users/Allan/alarm-clock/src/display_manager.cpp (update layout + refresh)
/Users/Allan/alarm-clock/src/display_manager.h (add message methods)
/Users/Allan/alarm-clock/src/alarm_manager.h (add label field)
/Users/Allan/alarm-clock/src/alarm_manager.cpp (update NVS format)
/Users/Allan/alarm-clock/Alarm Clock/Alarm Clock/ContentView.swift (add tab)
/Users/Allan/alarm-clock/Alarm Clock/Alarm Clock/BLEManager.swift (add method)
Success Criteria:

✅ Custom message appears on top row
✅ Day+date appear on bottom when custom message set
✅ Alarm label overrides custom message when ringing
✅ Full refresh only at 3 AM (no daytime flashing)
✅ Settings sync via BLE
✅ Message persists after reboot
PHASE D: SOUND SYSTEM ENHANCEMENTS (8-10 hours)
Goal: Add sound preview and custom tone upload capability

User Requirements:

Tap sound to preview it on iPhone
Upload custom MP3/WAV files via iOS app
Store custom tones on ESP32 (SPIFFS)
Manage tone library between app and device
Tasks:

Sound Preview on iPhone (1-2 hours)

File: /Users/Allan/alarm-clock/Alarm Clock/Alarm Clock/SoundView.swift
Import AVFoundation
Generate 3 tone .wav files (440Hz, 523Hz, 659Hz)
Add to iOS app assets
Implement previewSound() with AVAudioPlayer
Play 2-second sample on tap
SPIFFS File System Setup (30 min)

Create /data/alarms/ folder in project
Add sample MP3 files: "gentle_chimes.mp3", "beep.mp3"
Update platformio.ini with SPIFFS partition
Upload filesystem: pio run --target uploadfs
BLE File Transfer Protocol (3-4 hours) ⚠️ COMPLEX

Create new BLE service: File Transfer (UUID: 12340020-...)
Characteristics:
FileData (W): Write 512-byte chunks
FileControl (W): Start/end transfer with metadata
FileStatus (R/N): Progress feedback
Implement chunked transfer (BLE has ~512 byte MTU)
Add CRC validation
Handle transfer interruption/retry
SPIFFS File Management in Firmware (2-3 hours)

Create file_manager.h/cpp module
Methods: listFiles(), saveFile(), deleteFile(), getFileSize()
Integration with AudioTest for playback
Maximum file validation (limit to 5MB per file, 10 files total)
iOS File Picker & Upload UI (2-3 hours)

Add "Upload Custom Sound" button in SoundView
UIDocumentPicker for .mp3/.wav selection
Progress indicator during upload
Display uploaded sounds in list
Delete option for custom sounds
Update Audio Playback (30 min)

Modify AudioTest to support file playback
If sound contains ".mp3" or ".wav", play from SPIFFS
Otherwise, play built-in tone
Add MP3 decoder library if needed
Files Created:

/Users/Allan/alarm-clock/src/file_manager.h (NEW)
/Users/Allan/alarm-clock/src/file_manager.cpp (NEW)
Tone audio assets: tone1.wav, tone2.wav, tone3.wav (iOS app)
Files Modified:

/Users/Allan/alarm-clock/Alarm Clock/Alarm Clock/SoundView.swift (preview + upload)
/Users/Allan/alarm-clock/src/ble_time_sync.cpp (add file transfer service)
/Users/Allan/alarm-clock/src/ble_time_sync.h (add UUIDs)
/Users/Allan/alarm-clock/src/audio_test.cpp (add file playback)
/Users/Allan/alarm-clock/platformio.ini (SPIFFS config)
Success Criteria:

✅ Tap tone in app plays preview on iPhone
✅ Upload custom MP3/WAV from iPhone
✅ Custom sounds appear in sound list
✅ Alarms can use custom sounds
✅ Custom sounds persist after reboot
✅ File transfer handles errors gracefully
Risks:

BLE file transfer is complex (3-4 hours alone)
512-byte MTU means large files take time (5MB file = ~10,000 chunks)
SPIFFS has limited space (~1.5MB usable)
MP3 decoder may require additional library
IMPLEMENTATION ORDER & TIME ESTIMATES
Priority 1 - Critical Bug Fix:

✅ Phase A: Audio System Fix (30 min - 1 hour) COMPLETED
Priority 2 - Volume Control (User Requested Next):

🔊 Phase A.5: Volume Control + ESP32 Sound Preview (1-1.5 hours) ⏩ NEXT
Priority 3 - User Experience:

Phase B: iOS UI Redesign (3-4 hours)
Phase C: Display Customization (2-3 hours)
Priority 4 - Advanced Features:

Phase D: Sound Enhancements (8-10 hours)
Total Time: 15-22.5 hours (2-3 full work days)

NEXT IMMEDIATE STEPS
Volume Control + ESP32 Sound Preview (1-1.5 hours) - START HERE 🔊

Add volume support to AudioTest class (dynamic amplitude)
Add BLE characteristics for volume + test sound
Create iOS Settings view with slider + "Test Sound" button
Test: Adjust volume → tap test → hear result on ESP32
UI Redesign (3-4 hours)

Create 3 new SwiftUI views (RepeatView, LabelView, SoundView)
Update Alarm data model (add label + snooze)
Rewrite AlarmEditView to match iPhone native style
Test on iOS/macOS
Display Customization (2-3 hours)

Add custom message BLE characteristic
Update display layout logic (top row message)
Add alarm label support
Fix refresh timing (only at 3 AM)
Sound System (8-10 hours)

Add sound preview on iPhone
Build BLE file transfer protocol
Add custom tone upload UI
Test end-to-end
This plan addresses critical bugs first, then user-requested volume control, then UX enhancements, then advanced features.

BLE Interface Quick Reference (for App Development)
Service UUIDs

Time Service:    12340000-1234-5678-1234-56789abcdef0
  Time (R/W):    12340001-1234-5678-1234-56789abcdef0
  DateTime(R/W): 12340002-1234-5678-1234-56789abcdef0
  Volume (R/W):  12340003-1234-5678-1234-56789abcdef0  [Phase A.5]
  TestSound (W): 12340004-1234-5678-1234-56789abcdef0  [Phase A.5]

Alarm Service:   12340010-1234-5678-1234-56789abcdef0
  Set (W):       12340011-1234-5678-1234-56789abcdef0
  List (R):      12340012-1234-5678-1234-56789abcdef0
  Delete (W):    12340013-1234-5678-1234-56789abcdef0
Data Formats
Time Sync: 4-byte UInt32 (Unix timestamp, little-endian) or String "YYYY-MM-DD HH:MM:SS"
Volume: Single byte 0-100 (read current, write to change)
TestSound: Any byte (write to trigger 2-second test tone at 440Hz)
Alarm Set: JSON {"id":0,"hour":7,"minute":30,"days":127,"sound":"tone1","enabled":true}
Alarm List: JSON array [{...}, {...}] (max 10 alarms)
Alarm Delete: String "0" to "9" (alarm ID)
Day-of-Week Bitmask

0x01 (1)   = Sunday
0x02 (2)   = Monday
0x04 (4)   = Tuesday
0x08 (8)   = Wednesday
0x10 (16)  = Thursday
0x20 (32)  = Friday
0x40 (64)  = Saturday
0x7F (127) = Every day
Sound Options
Built-in tones: "tone1", "tone2", "tone3"
MP3 files: e.g., "gentle_chimes.mp3", "beep.mp3" (Phase 5+)
