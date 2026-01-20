#include "file_manager.h"

FileManager::FileManager() : _initialized(false) {
}

FileManager::~FileManager() {
    if (_initialized) {
        SPIFFS.end();
    }
}

bool FileManager::begin() {
    Serial.println("\n=== FileManager Initialization ===");

    // Mount SPIFFS
    if (!SPIFFS.begin(true)) {  // true = format if mount fails
        Serial.println("ERROR: Failed to mount SPIFFS!");
        return false;
    }

    Serial.println("SPIFFS mounted successfully");

    // Print SPIFFS info
    size_t total = SPIFFS.totalBytes();
    size_t used = SPIFFS.usedBytes();
    size_t free = total - used;

    Serial.printf("SPIFFS Total: %d KB\n", total / 1024);
    Serial.printf("SPIFFS Used:  %d KB\n", used / 1024);
    Serial.printf("SPIFFS Free:  %d KB\n", free / 1024);

    // Create alarm sounds directory if it doesn't exist
    if (!ensureDirectory(ALARM_SOUNDS_DIR)) {
        Serial.println("ERROR: Failed to create alarm sounds directory!");
        return false;
    }

    Serial.println("Alarm sounds directory ready");

    _initialized = true;
    Serial.println("=== FileManager Ready ===\n");

    return true;
}

bool FileManager::fileExists(const String& path) {
    if (!_initialized) {
        Serial.println("ERROR: FileManager not initialized!");
        return false;
    }

    // Strip /spiffs prefix if present for SPIFFS.exists()
    String checkPath = path;
    if (checkPath.startsWith(SPIFFS_MOUNT_POINT)) {
        checkPath = checkPath.substring(strlen(SPIFFS_MOUNT_POINT));
    }

    return SPIFFS.exists(checkPath.c_str());
}

size_t FileManager::getFileSize(const String& path) {
    if (!_initialized || !fileExists(path)) {
        return 0;
    }

    // Strip /spiffs prefix if present for SPIFFS.open()
    String openPath = path;
    if (openPath.startsWith(SPIFFS_MOUNT_POINT)) {
        openPath = openPath.substring(strlen(SPIFFS_MOUNT_POINT));
    }

    File file = SPIFFS.open(openPath.c_str(), "r");
    if (!file) {
        return 0;
    }

    size_t size = file.size();
    file.close();

    return size;
}

bool FileManager::deleteFile(const String& path) {
    if (!_initialized) {
        Serial.println("ERROR: FileManager not initialized!");
        return false;
    }

    if (!fileExists(path)) {
        Serial.printf("File does not exist: %s\n", path.c_str());
        return false;
    }

    // Strip /spiffs prefix if present for SPIFFS.remove()
    String removePath = path;
    if (removePath.startsWith(SPIFFS_MOUNT_POINT)) {
        removePath = removePath.substring(strlen(SPIFFS_MOUNT_POINT));
    }

    if (SPIFFS.remove(removePath.c_str())) {
        Serial.printf("Deleted file: %s\n", path.c_str());
        return true;
    } else {
        Serial.printf("ERROR: Failed to delete file: %s\n", path.c_str());
        return false;
    }
}

std::vector<String> FileManager::listSounds() {
    std::vector<String> sounds;

    if (!_initialized) {
        Serial.println("ERROR: FileManager not initialized!");
        return sounds;
    }

    // Use path without /spiffs prefix for SPIFFS.open
    File root = SPIFFS.open("/alarms");
    if (!root) {
        Serial.println("ERROR: Failed to open alarm sounds directory!");
        return sounds;
    }

    if (!root.isDirectory()) {
        Serial.println("ERROR: Alarm sounds path is not a directory!");
        root.close();
        return sounds;
    }

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String filename = String(file.name());
            // Remove directory path, keep only filename
            int lastSlash = filename.lastIndexOf('/');
            if (lastSlash >= 0) {
                filename = filename.substring(lastSlash + 1);
            }

            // Only include MP3 and WAV files
            if (filename.endsWith(".mp3") || filename.endsWith(".wav") ||
                filename.endsWith(".MP3") || filename.endsWith(".WAV")) {
                sounds.push_back(filename);
                Serial.printf("Found sound file: %s (%d bytes)\n", filename.c_str(), file.size());
            }
        }
        file = root.openNextFile();
    }

    root.close();

    Serial.printf("Total sound files found: %d\n", sounds.size());
    return sounds;
}

size_t FileManager::getFreeSpace() {
    if (!_initialized) {
        return 0;
    }

    return SPIFFS.totalBytes() - SPIFFS.usedBytes();
}

size_t FileManager::getTotalSpace() {
    if (!_initialized) {
        return 0;
    }

    return SPIFFS.totalBytes();
}

bool FileManager::writeChunk(const String& path, const uint8_t* data, size_t len, bool append) {
    if (!_initialized) {
        Serial.println("ERROR: FileManager not initialized!");
        return false;
    }

    if (data == nullptr || len == 0) {
        Serial.println("ERROR: Invalid data or length!");
        return false;
    }

    // Check free space
    if (!append) {
        if (getFreeSpace() < len) {
            Serial.printf("ERROR: Insufficient space! Need %d bytes, have %d bytes\n", len, getFreeSpace());
            return false;
        }
    }

    // Strip /spiffs prefix if present for SPIFFS.open()
    String openPath = path;
    if (openPath.startsWith(SPIFFS_MOUNT_POINT)) {
        openPath = openPath.substring(strlen(SPIFFS_MOUNT_POINT));
    }

    const char* mode = append ? "a" : "w";
    File file = SPIFFS.open(openPath.c_str(), mode);
    if (!file) {
        Serial.printf("ERROR: Failed to open file for writing: %s\n", path.c_str());
        return false;
    }

    size_t written = file.write(data, len);
    file.close();

    if (written != len) {
        Serial.printf("ERROR: Write incomplete! Wrote %d of %d bytes\n", written, len);
        return false;
    }

    Serial.printf("Wrote %d bytes to %s (append=%d)\n", written, path.c_str(), append);
    return true;
}

size_t FileManager::readFile(const String& path, uint8_t* buffer, size_t maxLen) {
    if (!_initialized) {
        Serial.println("ERROR: FileManager not initialized!");
        return 0;
    }

    if (!fileExists(path)) {
        Serial.printf("ERROR: File does not exist: %s\n", path.c_str());
        return 0;
    }

    // Strip /spiffs prefix if present for SPIFFS.open()
    String openPath = path;
    if (openPath.startsWith(SPIFFS_MOUNT_POINT)) {
        openPath = openPath.substring(strlen(SPIFFS_MOUNT_POINT));
    }

    File file = SPIFFS.open(openPath.c_str(), "r");
    if (!file) {
        Serial.printf("ERROR: Failed to open file for reading: %s\n", path.c_str());
        return 0;
    }

    size_t fileSize = file.size();
    size_t toRead = (fileSize < maxLen) ? fileSize : maxLen;

    size_t bytesRead = file.read(buffer, toRead);
    file.close();

    Serial.printf("Read %d bytes from %s\n", bytesRead, path.c_str());
    return bytesRead;
}

bool FileManager::ensureDirectory(const String& path) {
    // SPIFFS doesn't have real directories, just path prefixes
    // Create a placeholder file to establish the directory path
    
    Serial.printf("Directory path: %s\n", path.c_str());
    
    // Extract the relative path (remove /spiffs prefix if present)
    String relativePath = path;
    if (relativePath.startsWith(SPIFFS_MOUNT_POINT)) {
        relativePath = relativePath.substring(strlen(SPIFFS_MOUNT_POINT));
    }
    
    // Create placeholder file to establish directory
    String placeholderPath = relativePath + "/.placeholder";
    
    Serial.printf("Creating directory structure with placeholder: %s\n", placeholderPath.c_str());
    
    File placeholder = SPIFFS.open(placeholderPath.c_str(), "w");
    if (!placeholder) {
        Serial.printf("ERROR: Failed to create placeholder file at: %s\n", placeholderPath.c_str());
        return false;
    }
    
    placeholder.print("This file ensures the directory exists in SPIFFS");
    placeholder.close();
    
    Serial.println("Directory structure created successfully");
    return true;
}

std::vector<SoundFileInfo> FileManager::getSoundFileList() {
    std::vector<SoundFileInfo> soundFiles;

    if (!_initialized) {
        Serial.println("ERROR: FileManager not initialized!");
        return soundFiles;
    }

    Serial.println(">>> FileManager: Listing files in /alarms directory...");
    // Use path without /spiffs prefix for SPIFFS.open
    File root = SPIFFS.open("/alarms");
    if (!root) {
        Serial.println("ERROR: Failed to open alarm sounds directory!");
        return soundFiles;
    }

    if (!root.isDirectory()) {
        Serial.println("ERROR: Alarm sounds path is not a directory!");
        root.close();
        return soundFiles;
    }

    File file = root.openNextFile();
    while (file) {
        String fullPath = String(file.name());
        Serial.printf(">>> FileManager: Examining file: %s (isDir=%d)\n", fullPath.c_str(), file.isDirectory());

        if (!file.isDirectory()) {
            String filename = fullPath;
            // Remove directory path, keep only filename
            int lastSlash = filename.lastIndexOf('/');
            if (lastSlash >= 0) {
                filename = filename.substring(lastSlash + 1);
            }

            Serial.printf(">>> FileManager: Extracted filename: %s\n", filename.c_str());

            // Skip placeholder file
            if (filename == ".placeholder") {
                Serial.println(">>> FileManager: Skipping placeholder file");
                file = root.openNextFile();
                continue;
            }

            // Only include audio files
            if (filename.endsWith(".mp3") || filename.endsWith(".wav") ||
                filename.endsWith(".m4a") ||
                filename.endsWith(".MP3") || filename.endsWith(".WAV") ||
                filename.endsWith(".M4A")) {

                SoundFileInfo info;
                info.filename = filename;
                info.fileSize = file.size();

                // Generate display name (remove extension, capitalize first letter)
                info.displayName = filename;
                int dotPos = info.displayName.lastIndexOf('.');
                if (dotPos > 0) {
                    info.displayName = info.displayName.substring(0, dotPos);
                }
                // Replace underscores with spaces
                info.displayName.replace('_', ' ');

                soundFiles.push_back(info);
                Serial.printf("Found sound: %s (%d bytes)\n", filename.c_str(), file.size());
            } else {
                Serial.printf(">>> FileManager: Skipping non-audio file: %s\n", filename.c_str());
            }
        }
        file = root.openNextFile();
    }

    root.close();

    Serial.printf("Total sound files: %d\n", soundFiles.size());
    return soundFiles;
}

bool FileManager::isValidFilename(const String& filename) {
    // Check for empty filename
    if (filename.length() == 0) {
        return false;
    }

    // Check for path traversal attempts
    if (filename.indexOf("..") >= 0 || filename.indexOf('/') >= 0 || filename.indexOf('\\') >= 0) {
        Serial.println("ERROR: Invalid filename - contains path characters");
        return false;
    }

    // Check for valid file extension
    String lowerFilename = filename;
    lowerFilename.toLowerCase();
    if (!lowerFilename.endsWith(".mp3") && !lowerFilename.endsWith(".wav") && !lowerFilename.endsWith(".m4a")) {
        Serial.println("ERROR: Invalid filename - unsupported extension");
        return false;
    }

    // Check filename length (SPIFFS path limit is 31 chars, /alarms/ = 8 chars, so filename max is 23)
    // Note: filename includes extension (e.g., "myfile.m4a" = 11 chars)
    if (filename.length() > 23) {
        Serial.println("ERROR: Invalid filename - too long (max 23 chars total including extension)");
        return false;
    }

    return true;
}

bool FileManager::hasSpaceForFile(size_t fileSize) {
    if (!_initialized) {
        Serial.println("ERROR: FileManager not initialized!");
        return false;
    }

    size_t freeSpace = getFreeSpace();
    
    // Add 10% buffer for filesystem overhead
    size_t requiredSpace = fileSize + (fileSize / 10);
    
    if (freeSpace < requiredSpace) {
        Serial.printf("ERROR: Insufficient space! Need %d bytes, have %d bytes\n", requiredSpace, freeSpace);
        return false;
    }

    return true;
}
