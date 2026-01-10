# ESP32-L Alarm Clock

A modern alarm clock built with ESP32-L microcontroller, featuring a 3.7" e-ink display with frontlight, I2S audio output, and BLE connectivity for iOS app integration.

## Project Status

**Current Version**: 0.1.0
**Phase**: Initial Setup & Hardware Testing (Button)

## Hardware Components

- **Board**: ESP32-L with DESPI-CO2 E-Ink Driver
- **Display**: 3.7" E-Ink (GDEY037T03) with frontlight (DESPI-F01)
- **Audio**: I2S DAC Amplifier with 4Ω 3W Speaker
- **Input**: 60mm Green Arcade Button with microswitch
- **Power**: USB-C (5V 3A recommended)

## Pin Configuration

| Component | Pin | ESP32-L Label | Notes |
|-----------|-----|---------------|-------|
| Button | GPIO 4 | T0 | Active LOW with INPUT_PULLUP |
| I2S DOUT | GPIO 22 | SCL | Audio data output |
| I2S BCLK | GPIO 27 | T7 | Audio bit clock |
| I2S LRC | GPIO 14 | T6 | Audio left/right clock |
| E-Ink | SPI | DESPI-CO2 | Controlled via driver board |

See [src/config.h](src/config.h) for complete pin definitions.

## Project Structure

```
alarm-clock/
├── platformio.ini          # PlatformIO configuration
├── src/
│   ├── main.cpp           # Main program with button test
│   ├── config.h           # Pin definitions and constants
│   ├── button.cpp         # Button implementation
│   └── button.h           # Button interface
├── include/               # Additional headers
├── data/                  # SPIFFS data
│   └── alarms/           # Alarm sound files (future)
└── test/                  # Unit tests (future)
```

## Getting Started

### Prerequisites

1. **VSCode** with **PlatformIO** extension installed
2. **USB-C cable** for programming and power
3. **ESP32-L board** with components connected as per pin configuration

### Installation

1. Clone this repository:
   ```bash
   git clone https://github.com/allanilya/alarm-clock.git
   cd alarm-clock
   ```

2. Open the project in VSCode:
   ```bash
   code .
   ```

3. PlatformIO will automatically download dependencies

### Building the Project

```bash
pio run
```

Expected output: Clean build with no errors

### Uploading to ESP32-L

1. Connect ESP32-L via USB-C cable
2. Upload the firmware:
   ```bash
   pio run --target upload
   ```

### Monitoring Serial Output

```bash
pio device monitor
```

Or use the PlatformIO Serial Monitor in VSCode.

## Current Features (v0.1.0)

### Button Test Implementation

The current version implements a comprehensive button test with the following features:

- **Software Debouncing**: 50ms debounce time prevents false triggers
- **Edge Detection**: Reliable detection of press and release events
- **Press Duration Tracking**: Measures how long the button is held
- **Serial Output**: Real-time feedback via serial monitor

### Testing the Button

1. Upload the firmware to your ESP32-L
2. Open the serial monitor (115200 baud)
3. Press the green arcade button

**Expected Output:**
```
========================================
ESP32-L Alarm Clock
Version: 0.1.0
========================================
Hardware: ESP32-L with DESPI-CO2
Test: Button Input with Debouncing
========================================

Initializing button on pin 13...
Button initialized!

========================================
READY - Press the button to test!
========================================

>>> Button PRESSED! (time: 5432ms)
>>> Button RELEASED! (duration: 234ms)
  [Normal press detected]
```

### Button Module API

The `Button` class provides:

```cpp
Button button(BUTTON_PIN, BUTTON_DEBOUNCE_MS);

button.begin();              // Initialize button
button.update();             // Update state (call in loop)
button.isPressed();          // Check current state
button.wasPressed();         // Check if just pressed (edge)
button.wasReleased();        // Check if just released (edge)
button.getPressDuration();   // Get press duration in ms
button.getLastPressTime();   // Get timestamp of last press
```

## Configuration

Edit [src/config.h](src/config.h) to customize:

- Pin assignments
- Debounce timing
- Audio settings
- Display settings
- BLE configuration
- Debug flags

### Enabling Debug Output

Uncomment debug flags in [config.h](src/config.h):

```cpp
#define DEBUG_BUTTON
#define DEBUG_AUDIO
#define DEBUG_DISPLAY
#define DEBUG_BLE
```

## Development Phases

### Phase 1: Initial Setup ✅ (Current)
- [x] Project structure setup
- [x] PlatformIO configuration
- [x] Button test implementation

### Phase 2: Hardware Testing (Next)
- [ ] E-ink display initialization and test
- [ ] I2S audio playback test
- [ ] BLE communication test

### Phase 3: Core Functionality
- [ ] Time keeping (RTC or BLE time sync)
- [ ] Alarm storage and triggering
- [ ] Audio playback on alarm
- [ ] Display updates (time, alarm status)

### Phase 4: iOS App Integration
- [ ] BLE service for alarm configuration
- [ ] File transfer for custom alarm sounds
- [ ] Settings synchronization

## Libraries Used

- **GxEPD2** (v1.5.9+) - E-Paper display driver by Jean-Marc Zingg
- **ESP32-audioI2S** - Audio playback library by schreibfaul1
- **BLEDevice** - Built-in ESP32 BLE library
- **SPIFFS** - Built-in ESP32 file system library

## Troubleshooting

### Button Not Responding

1. Check physical connection (COM and NO terminals on microswitch)
2. Verify GPIO 13 is not used by other components
3. Enable `DEBUG_BUTTON` in config.h for detailed output
4. Test continuity with multimeter when button is pressed

### Build Errors

1. Ensure PlatformIO is up to date: `pio upgrade`
2. Clean build directory: `pio run --target clean`
3. Check that all dependencies are installed

### Upload Fails

1. Check USB-C cable (must support data, not just power)
2. Try different USB port
3. Press BOOT button on ESP32-L during upload (if required)
4. Update upload port in platformio.ini if needed

## Contributing

This is a personal project, but suggestions and bug reports are welcome via GitHub issues.

## License

MIT License - See LICENSE file for details

## Acknowledgments

- DESPI-CO2 E-Ink driver board
- ESP32 Arduino framework
- PlatformIO build system
- GxEPD2 and ESP32-audioI2S library authors

## Resources

- [ESP32 Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [GxEPD2 Library](https://github.com/ZinggJM/GxEPD2)
- [ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S)
- [PlatformIO Docs](https://docs.platformio.org/)

## Contact

Allan - [GitHub](https://github.com/allanilya)

---

**Next Steps**: Test button functionality, then proceed to display and audio testing.
