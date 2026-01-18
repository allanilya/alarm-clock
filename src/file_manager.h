#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <Arduino.h>
#include <SPIFFS.h>
#include <vector>

// Default alarm sounds directory
#define ALARM_SOUNDS_DIR "/spiffs/alarms"

/**
 * @brief File information structure
 */
struct SoundFileInfo {
    String filename;      // e.g., "alarm1.mp3"
    size_t fileSize;      // bytes
    String displayName;   // e.g., "Alarm 1"
};

/**
 * @brief FileManager handles SPIFFS operations for alarm sound files
 *
 * Provides file system mounting, CRUD operations, and space management
 * for custom alarm sound files stored in SPIFFS.
 */
class FileManager {
public:
    FileManager();
    ~FileManager();

    /**
     * @brief Initialize SPIFFS and create alarm directory
     * @return true if successful, false otherwise
     */
    bool begin();

    /**
     * @brief Check if a file exists
     * @param path Full path to file (e.g., "/spiffs/alarms/alarm1.mp3")
     * @return true if file exists, false otherwise
     */
    bool fileExists(const String& path);

    /**
     * @brief Get file size in bytes
     * @param path Full path to file
     * @return File size in bytes, 0 if file doesn't exist
     */
    size_t getFileSize(const String& path);

    /**
     * @brief Delete a file
     * @param path Full path to file
     * @return true if successful, false otherwise
     */
    bool deleteFile(const String& path);

    /**
     * @brief List all sound files in alarm directory
     * @return Vector of filenames (not full paths)
     */
    std::vector<String> listSounds();

    /**
     * @brief Get free space in SPIFFS
     * @return Free space in bytes
     */
    size_t getFreeSpace();

    /**
     * @brief Get total SPIFFS capacity
     * @return Total space in bytes
     */
    size_t getTotalSpace();

    /**
     * @brief Write a chunk of data to a file
     * @param path Full path to file
     * @param data Pointer to data buffer
     * @param len Length of data
     * @param append If true, append to existing file; if false, create new file
     * @return true if successful, false otherwise
     */
    bool writeChunk(const String& path, const uint8_t* data, size_t len, bool append);

    /**
     * @brief Read entire file into buffer
     * @param path Full path to file
     * @param buffer Buffer to store file data
     * @param maxLen Maximum buffer length
     * @return Number of bytes read, 0 on failure
     */
    size_t readFile(const String& path, uint8_t* buffer, size_t maxLen);

private:
    bool _initialized;

    /**
     * @brief Create directory if it doesn't exist
     * @param path Directory path
     * @return true if exists or created successfully
     */
    bool ensureDirectory(const String& path);
};

#endif // FILE_MANAGER_H
