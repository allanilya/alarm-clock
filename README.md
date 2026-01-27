# ESP32-L Alarm Clock

A phone-independent alarm clock built with ESP32 microcontroller, featuring a low-power 3.7" e-ink display, custom alarm sounds, and wireless configuration via companion iOS app.

## Motivation

Built to eliminate phone dependency in morning routines and reduce nighttime screen exposure. The e-ink display stays readable without constant backlighting, and custom alarm sounds create a more personalized wake-up experience.

## Features

- **E-Ink Display**: 3.7" low-power display with adjustable frontlight
- **Custom Alarm Sounds**: Upload your own MP3/WAV files via iOS app
- **BLE Configuration**: Wireless alarm scheduling and settings management
- **Physical Controls**: Large arcade button for snooze/dismiss
- **Persistent Storage**: Alarm settings and sounds survive power loss
- **Brightness Control**: Automatic brightness boost during alarms

## Hardware Components

- **Board**: ESP32-L with DESPI-CO2 E-Ink Driver
- **Display**: 3.7" E-Ink (GDEY037T03) with frontlight (DESPI-F01)
- **Audio**: I2S DAC Amplifier with 4Ω 3W Speaker
- **Input**: 60mm Arcade Button with microswitch
- **Power**: USB-C (5V 3A recommended)

## Pin Configuration

| Component | Pin | ESP32-L Label | Notes |
|-----------|-----|---------------|-------|
| Button | GPIO 4 | T0 | Active LOW with INPUT_PULLUP |
| Frontlight | GPIO 5 | T1 | PWM-controlled LED |
| I2S DOUT | GPIO 22 | SCL | Audio data output |
| I2S BCLK | GPIO 27 | T7 | Audio bit clock |
| I2S LRC | GPIO 14 | T6 | Audio left/right clock |
| E-Ink | SPI | DESPI-CO2 | Controlled via driver board |

See [src/config.h](src/config.h) for complete configuration.

## Project Structure

```
alarm-clock/
├── platformio.ini          # PlatformIO configuration
├── src/                    # ESP32 firmware
│   ├── main.cpp           # Main program loop
│   ├── config.h           # Pin definitions and constants
│   ├── alarm_manager.*    # Alarm scheduling and triggering
│   ├── audio_test.*       # I2S audio playback (MP3/WAV)
│   ├── ble_time_sync.*    # BLE service implementation
│   ├── button.*           # Button debouncing and detection
│   ├── display_manager.*  # E-ink display control
│   ├── file_manager.*     # SPIFFS file operations
│   ├── frontlight_manager.* # PWM frontlight control
│   └── time_manager.*     # RTC and time synchronization
├── data/                  # SPIFFS data
│   └── alarms/           # Alarm sound files (MP3/WAV)
└── Alarm Clock/          # iOS companion app (Swift)
    └── Alarm Clock/
        ├── ContentView.swift      # Main tab view
        ├── AlarmListView.swift    # Alarm list display
        ├── AlarmEditView.swift    # Alarm editor
        ├── SettingsView.swift     # App settings
        ├── FileUploadView.swift   # Sound file upload
        ├── BLEManager.swift       # Bluetooth communication
        └── Alarm.swift            # Alarm data model
```

## Getting Started

### Prerequisites

1. **VSCode** with **PlatformIO** extension
2. **Xcode** for iOS app development (optional)
3. **USB-C cable** for programming
4. **ESP32-L board** with components connected

### Firmware Installation

1. Clone the repository:
   ```bash
   git clone https://github.com/allanilya/alarm-clock.git
   cd alarm-clock
   ```

2. Build and upload firmware:
   ```bash
   pio run --target upload
   ```

3. Monitor serial output:
   ```bash
   pio device monitor
   ```

### iOS App Installation

1. Open `Alarm Clock/Alarm Clock.xcodeproj` in Xcode
2. Select your development team
3. Build and run on your iPhone

## Usage

### Setting Alarms

1. Open the iOS app
2. Tap the device in the list to connect
3. Tap "+" to create a new alarm
4. Set time, repeat days, label, and sound
5. Enable the alarm and save

### Uploading Custom Sounds

1. In the iOS app, go to "Files" tab
2. Tap "Upload Audio/Video File"
3. Select an MP3, WAV, or video file (audio will be extracted)
4. File is automatically converted and transferred to ESP32

### Button Controls

- **Single Click**: Snooze for 5 minutes (during alarm)
- **Double Click**: Dismiss alarm

### Adjusting Brightness

1. Open iOS app and connect
2. Go to "Settings" tab
3. Use the brightness slider to adjust frontlight

## BLE Interface

### Services

**Time Service** (`12340000-1234-5678-1234-56789abcdef0`)
- Time Sync (`12340001`): Unix timestamp or datetime string
- Display Message (`12340033`): Custom top-row message
- Bottom Row Label (`12340034`): Custom bottom-row text
- Brightness (`12340035`): 0-100 brightness level
- Volume (`12340032`): 0-100 volume level

**Alarm Service** (`12340010-1234-5678-1234-56789abcdef0`)
- Set Alarm (`12340011`): JSON alarm configuration
- List Alarms (`12340012`): JSON array of alarms
- Delete Alarm (`12340013`): Alarm ID to delete

**File Transfer Service** (`12340020-1234-5678-1234-56789abcdef0`)
- File Control (`12340021`): START/END/CANCEL commands
- File Data (`12340022`): 512-byte chunks
- File Status (`12340023`): Transfer progress
- File List (`12340024`): Available sound files

### Alarm JSON Format

```json
{
  "id": 0,
  "hour": 7,
  "minute": 30,
  "days": 62,
  "sound": "gentle_chimes.mp3",
  "label": "Morning",
  "snooze": true,
  "enabled": true
}
```

**Days bitmask**: Sun=1, Mon=2, Tue=4, Wed=8, Thu=16, Fri=32, Sat=64

## Libraries Used

- **GxEPD2** (v1.5.9) - E-Paper display driver
- **ESP8266Audio** (v1.9.7) - MP3/WAV audio playback
- **BLEDevice** - ESP32 Bluetooth Low Energy

## Troubleshooting

### Display Not Updating

1. Check SPI connections to DESPI-CO2
2. Verify display power supply (3.3V)
3. Try full refresh: Power cycle the device

### Audio Not Playing

1. Check I2S wiring (DOUT, BCLK, LRC pins)
2. Verify speaker connection
3. Adjust volume in iOS app settings
4. Ensure sound file is valid MP3/WAV

### BLE Connection Issues

1. Ensure Bluetooth is enabled on iPhone
2. Try restarting the ESP32
3. Check serial monitor for BLE initialization errors
4. Stay within 10-meter range

### Button Not Responding

1. Check wiring (COM and NO terminals)
2. Verify GPIO 4 connection
3. Test button continuity with multimeter

## Development

### Building Firmware

```bash
pio run
```

### Uploading Filesystem

```bash
pio run --target uploadfs
```

### Cleaning Build

```bash
pio run --target clean
```

## License

MIT License - See LICENSE file for details

## Acknowledgments

- DESPI-CO2 E-Ink driver board
- ESP32 Arduino framework
- GxEPD2 and ESP8266Audio library authors
- PlatformIO build system

## Contact

Allan - [GitHub](https://github.com/allanilya)
