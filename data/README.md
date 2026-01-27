# Data Directory

This directory contains files uploaded to ESP32's SPIFFS (Serial Peripheral Interface Flash File System) storage.

## Structure

```
data/
└── alarms/
    ├── *.mp3    # Alarm sound files
    └── *.wav    # Alarm sound files
```

## Alarm Sounds

The `alarms/` directory stores alarm sound files uploaded via:
- iOS app (recommended): Use the File Upload feature
- PlatformIO: `pio run --target uploadfs`

### File Requirements

- **Formats**: MP3, WAV, M4A
- **Max File Size**: 500 KB per file
- **Sample Rate**: 44.1 kHz (recommended)
- **Bitrate**: 128 kbps (MP3, recommended)

### Storage Capacity

- **Available SPIFFS**: ~1.5 MB
- **Recommended**: 5-10 sound files

## Uploading Files

### Via iOS App (Recommended)

1. Open the alarm clock iOS app
2. Go to "Files" tab
3. Tap "Upload Audio/Video File"
4. Select your audio file
5. File is automatically converted and transferred

### Via PlatformIO

1. Place files in `data/alarms/` directory
2. Run: `pio run --target uploadfs`
3. Wait for upload to complete

## File Management

- Files can be deleted via the iOS app
- File list is accessible via BLE characteristic `12340024`
- Storage space is checked before uploads
