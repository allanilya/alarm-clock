#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>

/**
 * Button Class
 *
 * Handles button input with software debouncing and edge detection.
 * Designed for active-LOW buttons (button connects to GND when pressed).
 *
 * Features:
 * - Software debouncing to prevent false triggers
 * - Edge detection for reliable press/release events
 * - Non-blocking operation using millis()
 * - Support for press duration measurement
 */
class Button {
public:
    /**
     * Constructor
     * @param pin GPIO pin number for the button
     * @param debounceMs Debounce time in milliseconds (default: 50ms)
     */
    Button(uint8_t pin, unsigned long debounceMs = 50);

    /**
     * Initialize the button
     * Sets up the pin with INPUT_PULLUP mode
     */
    void begin();

    /**
     * Update button state
     * Must be called regularly in loop() to maintain accurate state
     */
    void update();

    /**
     * Check if button is currently pressed (debounced)
     * @return true if button is pressed, false otherwise
     */
    bool isPressed() const;

    /**
     * Check if button was just pressed (edge detection)
     * Returns true only once per press
     * @return true if button was just pressed since last check
     */
    bool wasPressed();

    /**
     * Check if button was just released (edge detection)
     * Returns true only once per release
     * @return true if button was just released since last check
     */
    bool wasReleased();

    /**
     * Get the duration of the current or last button press
     * @return Duration in milliseconds
     */
    unsigned long getPressDuration() const;

    /**
     * Get timestamp of when button was last pressed
     * @return Timestamp in milliseconds (from millis())
     */
    unsigned long getLastPressTime() const;

    /**
     * Check if button was double-clicked
     * Returns true only once per double-click
     * @param timeoutMs Maximum time between clicks (default: 500ms)
     * @return true if double-click detected
     */
    bool wasDoubleClicked(unsigned long timeoutMs = 700);

    /**
     * Reset button state
     * Clears all edge detection flags
     */
    void reset();

private:
    uint8_t _pin;                    // GPIO pin number
    unsigned long _debounceMs;       // Debounce time in milliseconds

    // State tracking
    bool _currentState;              // Current debounced button state (true = pressed)
    bool _lastState;                 // Previous button state for edge detection
    bool _lastRawState;              // Last raw pin reading
    unsigned long _lastDebounceTime; // Last time the pin state changed

    // Edge detection flags
    bool _pressedFlag;               // Flag for wasPressed()
    bool _releasedFlag;              // Flag for wasReleased()

    // Timing
    unsigned long _pressStartTime;   // When button was pressed
    unsigned long _lastPressTime;    // Timestamp of last press
    unsigned long _pressDuration;    // Duration of last press

    // Double-click detection
    unsigned long _lastClickTime;    // Timestamp of last click
    uint8_t _clickCount;             // Number of clicks in sequence
    bool _doubleClickFlag;           // Flag for wasDoubleClicked()

    /**
     * Read raw button state from pin
     * @return true if button is pressed (active LOW, so returns !digitalRead)
     */
    bool readRaw() const;
};

#endif // BUTTON_H
