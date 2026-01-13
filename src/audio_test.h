#ifndef AUDIO_TEST_H
#define AUDIO_TEST_H

#include <Arduino.h>
#include <driver/i2s.h>
#include "config.h"

/**
 * Simple I2S Audio Test
 * Generates a test tone to verify I2S hardware is working
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

private:
    bool _initialized;
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
