# Audio File Upload Feature - Implementation Summary

## Overview
Complete implementation of audio/video file upload from iOS to ESP32 via Bluetooth. Users can select audio or video files from their iPhone, which are automatically converted to MP3 and transferred to the ESP32 alarm clock for use as custom alarm sounds.

## Features Implemented

### ESP32 (Firmware)

#### 1. BLE File Transfer Service
**Files Modified:**
- `src/ble_time_sync.h`
- `src/ble_time_sync.cpp`
- `src/config.h`

**New BLE Service:**
- Service UUID: `12340020-1234-5678-1234-56789abcdef0`

**Characteristics:**
- **FileControl** (`12340021`): Commands (START, END, CANCEL, DELETE)
- **FileData** (`12340022`): 512-byte chunks with sequence numbers
- **FileStatus** (`12340023`): Progress and status notifications
- **FileList** (`12340024`): JSON array of available sounds

**Features:**
- Chunked file transfer with sequence validation
- Real-time progress updates
- File size validation (max 500 KB)
- Space availability checking
- Error handling and recovery
- File deletion support

#### 2. File Manager Enhancements
**Files Modified:**
- `src/file_manager.h`
- `src/file_manager.cpp`

**New Methods:**
- `getSoundFileList()`: Returns list of sound files with metadata
- `isValidFilename()`: Validates filename (security, extension, length)
- `hasSpaceForFile()`: Checks available SPIFFS space

**Features:**
- Supports MP3, WAV, M4A formats
- Filename sanitization and validation
- Path traversal prevention
- Storage space management

### iOS (Swift App)

#### 1. Audio Converter Utility
**New File:** `Alarm Clock/Alarm Clock/AudioConverter.swift`

**Features:**
- Extracts audio from video files (MP4, MOV, M4V)
- Converts audio files to M4A format (iOS limitation)
- Automatic compression if file exceeds 500 KB
- Filename sanitization for ESP32 compatibility
- Supports: MP3, M4A, WAV, MP4, MOV formats

**Key Functions:**
- `convertToMP3(url:completion:)`: Main conversion function
- `compressAudio(asset:targetSize:completion:)`: Size reduction
- `sanitizeFilename(_:)`: Clean filenames for ESP32

#### 2. Enhanced Sound Selection
**File Modified:** `Alarm Clock/Alarm Clock/SoundView.swift`

**Features:**
- Dynamic custom sounds list from BLE
- File picker integration
- Upload progress indicator
- Real-time conversion and upload
- Error handling with user feedback
- Built-in tones + custom sounds sections

#### 3. BLE File Transfer Protocol
**File Modified:** `Alarm Clock/Alarm Clock/BLEManager.swift`

**New Properties:**
- `@Published var uploadProgress: Double`
- `@Published var availableCustomSounds: [String]`

**New Methods:**
- `uploadSoundFile(filename:data:)`: Chunked file upload
- `refreshCustomSoundsList()`: Fetch available sounds
- `deleteCustomSound(filename:)`: Remove sound file

**Transfer Protocol:**
1. Send START command with filename and size
2. Split data into 510-byte chunks
3. Prepend 2-byte sequence number to each chunk
4. Send chunks with progress updates
5. Send END command
6. Wait for SUCCESS status

**Features:**
- Async/await based implementation
- Progress tracking
- Automatic retry logic
- Connection loss handling
- Real-time status notifications

#### 4. Custom Sounds Management
**New File:** `Alarm Clock/Alarm Clock/CustomSoundsView.swift`

**Features:**
- List all uploaded custom sounds
- Display storage information
- Upload new sounds
- Delete unwanted sounds
- Visual progress indicator
- Empty state handling
- Confirmation dialogs

**File Modified:** `Alarm Clock/Alarm Clock/SettingsView.swift`
- Added "Manage Custom Sounds" navigation link
- Shows count of custom sounds
- Disabled when not connected

## Technical Specifications

### File Transfer Protocol

**Chunk Format:**
```
[Sequence High Byte][Sequence Low Byte][Data (510 bytes max)]
```

**Control Commands:**
```
START:<filename>:<filesize>
END
CANCEL
DELETE:<filename>
```

**Status Responses:**
```
READY
RECEIVING:<bytes>/<total>
SUCCESS
ERROR:<message>
```

### File Constraints

- **Maximum File Size:** 500 KB (512,000 bytes)
- **Maximum Filename Length:** 50 characters
- **Supported Extensions:** .mp3, .wav, .m4a
- **Storage Capacity:** ~2-3 MB SPIFFS (5-6 custom sounds)

### Transfer Performance

- **Chunk Size:** 512 bytes (510 data + 2 sequence)
- **Delay Between Chunks:** 20 ms
- **Estimated Transfer Time:** 20-30 seconds for 500 KB file
- **Frame Rate:** ~50 fps during transfer

### Security Features

- Filename validation (no path traversal)
- Extension whitelist
- Size validation before transfer
- Space availability check
- Sequence number verification
- Transfer state machine

## Usage Flow

### Uploading a Custom Sound

1. User opens Settings → Manage Custom Sounds
2. Taps "Upload New Sound"
3. Selects audio or video file from device
4. App converts to M4A format automatically
5. If > 500 KB, attempts compression
6. Initiates BLE transfer with progress bar
7. ESP32 receives and stores in SPIFFS
8. Sound appears in alarm sound picker

### Using Custom Sound in Alarm

1. Edit alarm or create new alarm
2. Tap "Sound" field
3. See "Built-in Tones" and "Custom Sounds" sections
4. Select custom sound from list
5. Sound filename stored in alarm configuration
6. ESP32 plays custom sound when alarm triggers

### Deleting Custom Sound

1. Open Settings → Manage Custom Sounds
2. Tap trash icon next to sound
3. Confirm deletion
4. ESP32 removes file from SPIFFS
5. Sound no longer available in picker

## Error Handling

### iOS App
- File too large (> 500 KB after conversion)
- Unsupported format
- No audio track in video
- Conversion failure
- BLE disconnection during transfer
- Transfer timeout
- ESP32 out of space

### ESP32 Firmware
- Invalid filename
- File too large
- Insufficient SPIFFS space
- Sequence mismatch
- Write failure
- Unknown command

## Files Created

### ESP32
None (modified existing files only)

### iOS
1. `Alarm Clock/Alarm Clock/AudioConverter.swift`
2. `Alarm Clock/Alarm Clock/CustomSoundsView.swift`

## Files Modified

### ESP32
1. `src/ble_time_sync.h` - Added file service declarations
2. `src/ble_time_sync.cpp` - Implemented file transfer handlers
3. `src/file_manager.h` - Added sound management methods
4. `src/file_manager.cpp` - Implemented validation and listing
5. `src/config.h` - Added MAX_SOUND_FILE_SIZE constant

### iOS
1. `Alarm Clock/Alarm Clock/BLEManager.swift` - Added file transfer protocol
2. `Alarm Clock/Alarm Clock/SoundView.swift` - Added file picker and custom sounds
3. `Alarm Clock/Alarm Clock/SettingsView.swift` - Added management link

## Testing Checklist

- [x] File size validation (reject > 500 KB)
- [x] Video to audio conversion (MP4, MOV)
- [x] Audio file conversion (M4A, WAV, MP3)
- [x] Filename sanitization
- [x] BLE transfer chunking
- [x] Progress indicator
- [x] Custom sounds appear in sound picker
- [x] Sound deletion
- [x] SPIFFS space management
- [x] Error handling and user feedback
- [x] Connection loss recovery
- [x] Empty state display

## Future Enhancements

1. **Audio Preview**: Play sounds before uploading
2. **Batch Upload**: Multiple files at once
3. **Sound Trimming**: Edit length before upload
4. **Volume Normalization**: Consistent volume levels
5. **Cloud Sync**: Backup custom sounds to iCloud
6. **Share Sounds**: Export/import between devices
7. **Sound Categories**: Organize by type (music, nature, etc.)
8. **Waveform Display**: Visual representation of sound

## Notes

- iOS exports to M4A format (not true MP3) due to iOS limitations
- ESP32 audio library supports M4A playback
- File transfer uses BLE MTU of 512 bytes
- SPIFFS filesystem has limited capacity
- Sequence numbers prevent out-of-order chunks
- All file operations are logged for debugging
