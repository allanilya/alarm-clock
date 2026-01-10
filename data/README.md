# Data Directory

This directory contains files that will be uploaded to the ESP32's SPIFFS (Serial Peripheral Interface Flash File System) storage.

## Structure

```
data/
└── alarms/
    ├── alarm1.mp3    (future: default alarm sound)
    ├── alarm2.mp3    (future: alternative alarm sound)
    └── ...           (future: custom alarm sounds)
```

## Alarm Sounds Directory

The `alarms/` directory will store alarm sound files in MP3 format.

### File Format Requirements
- **Format**: MP3
- **Bitrate**: 128 kbps (recommended for balance of quality and file size)
- **Duration**: 15-30 seconds (will loop if needed)
- **Sample Rate**: 44.1 kHz
- **File Size**: Aim for < 500 KB per file

### Storage Capacity
- **Available SPIFFS**: ~2-3 MB (depending on partition scheme)
- **Estimated capacity**: 5-10 alarm sound files

## Uploading to ESP32

To upload files to SPIFFS:

### Using PlatformIO:
```bash
pio run --target uploadfs
```

### Using Arduino IDE:
1. Install "ESP32 Sketch Data Upload" plugin
2. Tools → ESP32 Sketch Data Upload

## Adding Custom Alarm Sounds

1. Place MP3 files in the `data/alarms/` directory
2. Name them descriptively (e.g., `gentle_chimes.mp3`, `upbeat_morning.mp3`)
3. Upload to SPIFFS using the command above
4. Reference them in your iOS app or firmware

## Current Status

**Note**: This directory is currently empty as we're in the initial setup phase. Alarm sound files will be added in a future development phase.

## Future Enhancements

- Support for WAV format (higher quality, larger files)
- Support for custom alarm sounds uploaded via BLE from iOS app
- Ability to preview alarm sounds before setting them
- Volume adjustment per alarm sound
