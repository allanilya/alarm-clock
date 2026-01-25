#ifndef AUDIO_TEST_H
#define AUDIO_TEST_H

#include <Arduino.h>
#include <driver/i2s.h>
#include "config.h"

// Forward declaration for Audio library
class Audio;

/**
 * Sound type enumeration
 */
enum SoundType {
    SOUND_TYPE_NONE,
    SOUND_TYPE_TONE,
    SOUND_TYPE_FILE,
    SOUND_TYPE_PCM  // Raw PCM buffer playback (for preloaded WAV)
};

/**
 * AudioTest handles both tone generation and MP3/WAV file playback
 * Supports tone generation via I2S and file playback via ESP32-audioI2S library
 */
class AudioTest {
public:
    AudioTest();

    /**
     * Initialize I2S for audio output
     * @return true if successful, false otherwise
     */
    bool begin();

    /**
     * Play a test tone at the specified frequency
     * @param frequency Frequency in Hz (e.g., 440 for A4 note)
     * @param duration Duration in milliseconds
     */
    void playTone(uint16_t frequency, uint32_t duration);

    /**
     * Stop audio output
     */
    void stop();

    /**
     * Set volume level (0-100%)
     * @param volume Volume level 0-100
     */
    void setVolume(uint8_t volume);

    /**
     * Get current volume level
     * @return Current volume (0-100%)
     */
    uint8_t getVolume();

    /**
     * Play MP3/WAV file from SPIFFS
     * @param path Full path to audio file (e.g., "/spiffs/alarms/alarm1.mp3")
     * @param loop If true, loop the file continuously
     * @return true if playback started successfully, false otherwise
     */
    bool playFile(const String& path, bool loop = false);

    /**
     * Stop file playback
     */
    void stopFile();

    /**
     * Play raw PCM data from RAM buffer
     * Used for preloaded WAV files for instant button feedback
     * @param buffer Pointer to PCM data in RAM (16-bit stereo, 44.1kHz)
     * @param sizeBytes Size of PCM data in bytes
     * @param sampleRate Sample rate (default: 44100 Hz)
     * @param bits Bits per sample (8 or 16, default: 16)
     * @param channels Number of channels (1=mono, 2=stereo, default: 2)
     * @return true if playback started successfully
     */
    bool playPCMBuffer(const uint8_t* buffer, size_t sizeBytes, uint32_t sampleRate = 44100, uint8_t bits = 16, uint8_t channels = 2);

    /**
     * Check if audio is currently playing
     * @return true if any audio is playing (tone or file)
     */
    bool isPlaying();

    /**
     * Get current sound type being played
     * @return Current sound type (NONE, TONE, or FILE)
     */
    SoundType getCurrentSoundType();

    /**
     * Loop method - must be called regularly to process audio playback
     * This keeps the MP3/WAV decoder running
     */
    void loop();

private:
    bool _initialized;
    uint8_t _volume;  // Volume level 0-100 (default: 70)
    volatile bool _volumeChanged;  // Flag: volume changed, needs audioOut update
    volatile SoundType _currentSoundType;  // Track what's currently playing (volatile for multi-core)
    Audio* _audioLib;  // ESP32-audioI2S library instance for file playback
    bool _loopFile;  // Whether to loop file playback
    String _currentFilePath;  // Current file being played (for looping)
    SemaphoreHandle_t _audioMutex;  // Mutex for thread-safe audio operations

    // PCM buffer playback state
    const uint8_t* _pcmBuffer;  // Pointer to PCM data in RAM
    size_t _pcmSizeBytes;       // Size of PCM data
    size_t _pcmPosition;        // Current playback position in bytes
    uint32_t _pcmSampleRate;    // Sample rate of PCM data
    uint8_t _pcmBits;           // Bits per sample (8 or 16)
    uint8_t _pcmChannels;       // Number of channels (1 or 2)
    bool _pcmPlaying;           // Flag: PCM playback active

    static const i2s_port_t I2S_PORT = I2S_NUM_0;
    static const uint32_t SAMPLE_RATE = 44100;

    /**
     * Generate sine wave samples
     * @param buffer Output buffer
     * @param bufferSize Size of buffer in samples
     * @param frequency Frequency of the tone
     * @param phase Current phase (updated by function)
     */
    void generateSineWave(int16_t* buffer, size_t bufferSize, uint16_t frequency, float& phase);
};

#endif // AUDIO_TEST_H
