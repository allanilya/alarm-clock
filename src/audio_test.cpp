#include "audio_test.h"
#include <math.h>

/**
 * Constructor
 */
AudioTest::AudioTest() : _initialized(false) {
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

    _initialized = true;
    Serial.println("I2S initialized successfully!");
    return true;
}

/**
 * Generate sine wave samples
 */
void AudioTest::generateSineWave(int16_t* buffer, size_t bufferSize, uint16_t frequency, float& phase) {
    const float amplitude = 20000.0;  // High volume for testing (max is 32767)
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

    Serial.println("Tone finished.");
}

/**
 * Stop audio output
 */
void AudioTest::stop() {
    if (_initialized) {
        i2s_zero_dma_buffer(I2S_PORT);
        i2s_driver_uninstall(I2S_PORT);
        _initialized = false;
        Serial.println("Audio stopped and driver uninstalled.");
    }
}
