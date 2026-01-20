#include "audio_test.h"
#include <math.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include "AudioFileSourceSPIFFS.h"
#include "AudioGeneratorMP3.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

// ESP8266Audio library components
AudioOutputI2S* audioOut = nullptr;
AudioFileSourceSPIFFS* audioFile = nullptr;
AudioGeneratorMP3* mp3 = nullptr;
AudioGeneratorWAV* wav = nullptr;

/**
 * Constructor
 */
AudioTest::AudioTest() : _initialized(false), _volume(70), _currentSoundType(SOUND_TYPE_NONE), _audioLib(nullptr), _loopFile(false) {
}

/**
 * Initialize I2S for audio output
 */
bool AudioTest::begin() {
    // I2S configuration
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    // I2S pin configuration
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK,
        .ws_io_num = I2S_LRC,
        .data_out_num = I2S_DOUT,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    // Install and start I2S driver
    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.print("Failed to install I2S driver: ");
        Serial.println(err);
        return false;
    }

    err = i2s_set_pin(I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        Serial.print("Failed to set I2S pins: ");
        Serial.println(err);
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }

    i2s_zero_dma_buffer(I2S_PORT);

    // Load volume from NVS
    Preferences prefs;
    prefs.begin("audio", false);
    _volume = prefs.getUChar("volume", 70);  // Default 70%
    prefs.end();

    // Note: We'll create AudioOutputI2S on-demand when playing files
    // Don't initialize it here because it conflicts with tone I2S driver

    Serial.println("Audio library ready (will initialize on-demand for file playback)");

    _initialized = true;
    Serial.print("I2S initialized successfully! Volume: ");
    Serial.print(_volume);
    Serial.println("%");
    return true;
}

/**
 * Generate sine wave samples
 */
void AudioTest::generateSineWave(int16_t* buffer, size_t bufferSize, uint16_t frequency, float& phase) {
    // Dynamic amplitude based on volume (0-100) -> (0-32767)
    const float amplitude = (_volume / 100.0) * 32767.0;
    const float phaseIncrement = 2.0 * PI * frequency / SAMPLE_RATE;

    for (size_t i = 0; i < bufferSize; i += 2) {
        // Generate sine wave sample
        int16_t sample = (int16_t)(amplitude * sin(phase));

        // Stereo output (same sample for both channels)
        buffer[i] = sample;      // Left channel
        buffer[i + 1] = sample;  // Right channel

        // Increment phase
        phase += phaseIncrement;
        if (phase >= 2.0 * PI) {
            phase -= 2.0 * PI;
        }
    }
}

/**
 * Play a test tone
 */
void AudioTest::playTone(uint16_t frequency, uint32_t duration) {
    if (!_initialized) {
        Serial.println("Audio not initialized!");
        return;
    }

    // Stop any file playback first (this will reinstall tone I2S driver)
    if (_currentSoundType == SOUND_TYPE_FILE) {
        stopFile();
    }

    // Clear DMA buffer (I2S driver is already installed for tones)
    i2s_zero_dma_buffer(I2S_PORT);

    _currentSoundType = SOUND_TYPE_TONE;

    Serial.print("Playing ");
    Serial.print(frequency);
    Serial.print(" Hz tone for ");
    Serial.print(duration);
    Serial.println(" ms...");

    const size_t BUFFER_SIZE = 256;  // samples (128 per channel)
    int16_t buffer[BUFFER_SIZE];
    float phase = 0.0;

    uint32_t startTime = millis();
    size_t bytesWritten;

    while (millis() - startTime < duration) {
        // Generate sine wave samples
        generateSineWave(buffer, BUFFER_SIZE, frequency, phase);

        // Write to I2S
        i2s_write(I2S_PORT, buffer, sizeof(buffer), &bytesWritten, portMAX_DELAY);
    }

    // Clear DMA buffer to stop sound
    i2s_zero_dma_buffer(I2S_PORT);

    _currentSoundType = SOUND_TYPE_NONE;
    Serial.println("Tone finished.");
}

/**
 * Stop audio output (just clear the buffer, keep driver running)
 */
void AudioTest::stop() {
    if (_initialized) {
        // Only clear I2S buffer if we're currently in tone mode
        // (AudioOutputI2S manages its own I2S state for file playback)
        if (_currentSoundType == SOUND_TYPE_TONE) {
            // Try to clear DMA buffer if I2S driver is installed
            // Ignore errors if driver not in expected state
            i2s_zero_dma_buffer(I2S_PORT);
        }

        stopFile();  // Also stop file playback if active
        _currentSoundType = SOUND_TYPE_NONE;
        Serial.println("Audio stopped (buffer cleared).");
    }
}

/**
 * Set volume level (0-100%)
 */
void AudioTest::setVolume(uint8_t volume) {
    if (volume > 100) {
        volume = 100;
    }

    _volume = volume;

    // Update Audio library volume (gain scale 0-4)
    if (audioOut != nullptr) {
        audioOut->SetGain((_volume / 100.0f) * 4.0f);
    }

    // Save to NVS
    Preferences prefs;
    prefs.begin("audio", false);
    prefs.putUChar("volume", _volume);
    prefs.end();

    Serial.print("Volume set to: ");
    Serial.print(_volume);
    Serial.println("%");
}

/**
 * Get current volume level
 */
uint8_t AudioTest::getVolume() {
    return _volume;
}

/**
 * Play MP3/WAV file from SPIFFS
 */
bool AudioTest::playFile(const String& path, bool loop) {
    if (!_initialized) {
        Serial.println("ERROR: Audio not initialized!");
        return false;
    }

    // Stop any existing file playback
    if (_currentSoundType == SOUND_TYPE_FILE) {
        stopFile();
        // stopFile() will have reinstalled tone I2S driver
    }

    // Uninstall tone I2S driver before creating AudioOutputI2S
    // This is needed whether we're switching from tone mode OR from idle (first playback)
    if (audioOut == nullptr) {
        Serial.println("Need to switch from tone I2S to file I2S...");

        // Clear buffer and uninstall tone I2S driver
        i2s_zero_dma_buffer(I2S_PORT);
        esp_err_t err = i2s_driver_uninstall(I2S_PORT);
        if (err == ESP_OK) {
            Serial.println("Uninstalled tone I2S driver successfully");
        } else {
            Serial.printf("Warning: i2s_driver_uninstall returned error %d (may already be uninstalled)\n", err);
        }
        delay(100);  // Give I2S hardware time to fully reset

        // Create AudioOutputI2S for file playback
        Serial.println("Creating AudioOutputI2S for file playback...");
        audioOut = new AudioOutputI2S();
        audioOut->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
        audioOut->SetGain((_volume / 100.0f) * 4.0f);
        Serial.println("AudioOutputI2S created successfully");
    }

    // Strip /spiffs prefix if present (SPIFFS.exists doesn't use it)
    String spiffsPath = path;
    if (spiffsPath.startsWith("/spiffs")) {
        spiffsPath = spiffsPath.substring(7);  // Remove "/spiffs"
    }

    // Check if file exists
    if (!SPIFFS.exists(spiffsPath)) {
        Serial.printf("ERROR: File not found: %s (checked: %s)\n", path.c_str(), spiffsPath.c_str());
        return false;
    }

    Serial.printf("Playing file: %s (loop=%d)\n", path.c_str(), loop);

    // Store file path for looping (use SPIFFS path without /spiffs prefix)
    _currentFilePath = spiffsPath;

    // Create file source
    audioFile = new AudioFileSourceSPIFFS(spiffsPath.c_str());
    if (!audioFile) {
        Serial.println("ERROR: Failed to open audio file!");
        return false;
    }

    // Determine file type and create appropriate generator
    String lowerPath = path;
    lowerPath.toLowerCase();

    if (lowerPath.endsWith(".mp3")) {
        mp3 = new AudioGeneratorMP3();
        if (!mp3->begin(audioFile, audioOut)) {
            Serial.println("ERROR: Failed to start MP3 playback!");
            delete audioFile;
            delete mp3;
            audioFile = nullptr;
            mp3 = nullptr;
            return false;
        }
    } else if (lowerPath.endsWith(".wav")) {
        wav = new AudioGeneratorWAV();
        if (!wav->begin(audioFile, audioOut)) {
            Serial.println("ERROR: Failed to start WAV playback!");
            delete audioFile;
            delete wav;
            audioFile = nullptr;
            wav = nullptr;
            return false;
        }
    } else {
        Serial.println("ERROR: Unsupported file format! Use .mp3 or .wav");
        delete audioFile;
        audioFile = nullptr;
        return false;
    }

    _loopFile = loop;
    _currentSoundType = SOUND_TYPE_FILE;
    Serial.println("File playback started");
    return true;
}

/**
 * Stop file playback
 */
void AudioTest::stopFile() {
    if (_currentSoundType == SOUND_TYPE_FILE) {
        // Stop generators
        if (mp3 != nullptr) {
            mp3->stop();
            delete mp3;
            mp3 = nullptr;
        }
        if (wav != nullptr) {
            wav->stop();
            delete wav;
            wav = nullptr;
        }

        // Close file source
        if (audioFile != nullptr) {
            audioFile->close();
            delete audioFile;
            audioFile = nullptr;
        }

        // Clean up AudioOutputI2S
        if (audioOut != nullptr) {
            delete audioOut;
            audioOut = nullptr;
            Serial.println("Deleted AudioOutputI2S");
        }

        // Reinstall I2S driver for tone generation
        i2s_config_t i2s_config = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
            .sample_rate = SAMPLE_RATE,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 8,
            .dma_buf_len = 64,
            .use_apll = false,
            .tx_desc_auto_clear = true,
            .fixed_mclk = 0
        };

        i2s_pin_config_t pin_config = {
            .bck_io_num = I2S_BCLK,
            .ws_io_num = I2S_LRC,
            .data_out_num = I2S_DOUT,
            .data_in_num = I2S_PIN_NO_CHANGE
        };

        i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
        i2s_set_pin(I2S_PORT, &pin_config);
        Serial.println("Reinstalled tone I2S driver");

        _currentSoundType = SOUND_TYPE_NONE;
        _loopFile = false;
        _currentFilePath = "";
        Serial.println("File playback stopped");
    }
}

/**
 * Check if audio is currently playing
 */
bool AudioTest::isPlaying() {
    if (!_initialized) {
        return false;
    }

    if (_currentSoundType == SOUND_TYPE_FILE) {
        // Check if either MP3 or WAV generator is running
        if (mp3 != nullptr && mp3->isRunning()) {
            return true;
        }
        if (wav != nullptr && wav->isRunning()) {
            return true;
        }
        // If generator finished, handle looping or stop
        if (_loopFile && audioFile != nullptr) {
            // Restart playback for looping
            audioFile->seek(0, SEEK_SET);
            return true;
        } else {
            // Playback finished, clean up
            stopFile();
            return false;
        }
    }

    return _currentSoundType != SOUND_TYPE_NONE;
}

/**
 * Get current sound type
 */
SoundType AudioTest::getCurrentSoundType() {
    return _currentSoundType;
}

/**
 * Loop method - must be called regularly to process audio playback
 * This keeps the MP3/WAV decoder running
 */
void AudioTest::loop() {
    if (_currentSoundType == SOUND_TYPE_FILE) {
        bool isRunning = false;

        // Process MP3 playback
        if (mp3 != nullptr && mp3->isRunning()) {
            if (mp3->loop()) {
                isRunning = true;
            } else {
                // File finished
                if (_loopFile) {
                    // Restart for looping
                    mp3->stop();
                    delete mp3;
                    mp3 = nullptr;

                    if (audioFile != nullptr) {
                        audioFile->close();
                        delete audioFile;

                        // Reopen and restart
                        audioFile = new AudioFileSourceSPIFFS(_currentFilePath.c_str());
                        mp3 = new AudioGeneratorMP3();
                        mp3->begin(audioFile, audioOut);
                    }
                } else {
                    // Finished, stop playback
                    stopFile();
                }
            }
        }

        // Process WAV playback
        if (wav != nullptr && wav->isRunning()) {
            if (wav->loop()) {
                isRunning = true;
            } else {
                // File finished
                if (_loopFile) {
                    // Restart for looping
                    wav->stop();
                    delete wav;
                    wav = nullptr;

                    if (audioFile != nullptr) {
                        audioFile->close();
                        delete audioFile;

                        // Reopen and restart
                        audioFile = new AudioFileSourceSPIFFS(_currentFilePath.c_str());
                        wav = new AudioGeneratorWAV();
                        wav->begin(audioFile, audioOut);
                    }
                } else {
                    // Finished, stop playback
                    stopFile();
                }
            }
        }
    }
}
