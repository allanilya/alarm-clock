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

    return SPIFFS.exists(path);
}

size_t FileManager::getFileSize(const String& path) {
    if (!_initialized || !fileExists(path)) {
        return 0;
    }

    File file = SPIFFS.open(path, "r");
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

    if (SPIFFS.remove(path)) {
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

    File root = SPIFFS.open(ALARM_SOUNDS_DIR);
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

    const char* mode = append ? "a" : "w";
    File file = SPIFFS.open(path, mode);
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

    File file = SPIFFS.open(path, "r");
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
    // We just need to verify SPIFFS is mounted
    // Directory "creation" happens automatically when files are written

    Serial.printf("Directory path registered: %s\n", path.c_str());
    return true;
}
