#ifndef FRONTLIGHT_MANAGER_H
#define FRONTLIGHT_MANAGER_H

#include <Arduino.h>
#include "config.h"

/**
 * FrontlightManager - PWM control for DESPI-F01 frontlight via MOSFET
 *
 * Controls brightness of e-ink display frontlight using PWM signal
 * connected to 2N7000 N-channel MOSFET gate.
 */
class FrontlightManager {
public:
    FrontlightManager();

    /**
     * Initialize frontlight PWM control
     * @return true if successful
     */
    bool begin();

    /**
     * Set brightness level
     * @param brightness 0-100 (0 = off, 100 = full brightness)
     */
    void setBrightness(uint8_t brightness);

    /**
     * Get current brightness level
     * @return Brightness 0-100
     */
    uint8_t getBrightness() const;

    /**
     * Turn frontlight on (restore previous brightness)
     */
    void on();

    /**
     * Turn frontlight off (but remember brightness level)
     */
    void off();

    /**
     * Check if frontlight is on
     * @return true if on
     */
    bool isOn() const;

private:
    uint8_t _brightness;          // Current brightness (0-100)
    uint8_t _savedBrightness;     // Saved brightness when turning off
    bool _isOn;

    // PWM Configuration
    static const uint8_t PWM_CHANNEL = 0;       // LED PWM channel
    static const uint32_t PWM_FREQUENCY = 30000; // 25 kHz PWM frequency (above audible range)
    static const uint8_t PWM_RESOLUTION = 8;    // 8-bit resolution (0-255)

    void updatePWM();
    uint8_t percentToPWM(uint8_t percent);
};

#endif // FRONTLIGHT_MANAGER_H
