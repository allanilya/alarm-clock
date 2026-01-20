#include "button.h"

/**
 * Constructor
 */
Button::Button(uint8_t pin, unsigned long debounceMs)
    : _pin(pin),
      _debounceMs(debounceMs),
      _currentState(false),
      _lastState(false),
      _lastRawState(false),
      _lastDebounceTime(0),
      _pressedFlag(false),
      _releasedFlag(false),
      _pressStartTime(0),
      _lastPressTime(0),
      _pressDuration(0),
      _lastClickTime(0),
      _clickCount(0),
      _doubleClickFlag(false) {
}

/**
 * Initialize the button
 */
void Button::begin() {
    pinMode(_pin, INPUT_PULLUP);
    _lastRawState = readRaw();
    _currentState = _lastRawState;
    _lastState = _currentState;
}

/**
 * Update button state with debouncing
 * This implements a standard debouncing algorithm:
 * - Read the raw pin state
 * - If it differs from the last reading, reset the debounce timer
 * - If it's been stable for debounceMs, update the debounced state
 * - Detect edges (transitions) between states
 */
void Button::update() {
    bool rawState = readRaw();
    unsigned long currentTime = millis();

    // If the raw state has changed, reset the debounce timer
    if (rawState != _lastRawState) {
        _lastDebounceTime = currentTime;
        _lastRawState = rawState;
    }

    // If the raw state has been stable for longer than debounce time
    if ((currentTime - _lastDebounceTime) > _debounceMs) {
        // If the debounced state has changed
        if (rawState != _currentState) {
            _currentState = rawState;

            // Detect edges
            if (_currentState && !_lastState) {
                // Rising edge (button pressed)
                _pressedFlag = true;
                _pressStartTime = currentTime;
                _lastPressTime = currentTime;

#ifdef DEBUG_BUTTON
                Serial.println("[Button] Pressed (rising edge detected)");
#endif
            } else if (!_currentState && _lastState) {
                // Falling edge (button released)
                _releasedFlag = true;
                _pressDuration = currentTime - _pressStartTime;

                // Track clicks for double-click detection (700ms window)
                if (currentTime - _lastClickTime < 700) {
                    _clickCount++;
                    Serial.print("[Button] Click count: ");
                    Serial.println(_clickCount);
                    if (_clickCount >= 2) {
                        _doubleClickFlag = true;
                        _clickCount = 0;
                        Serial.println("[Button] DOUBLE-CLICK DETECTED!");
                    }
                } else {
                    _clickCount = 1;
                    Serial.println("[Button] First click");
                }
                _lastClickTime = currentTime;

#ifdef DEBUG_BUTTON
                Serial.print("[Button] Released (falling edge detected), duration: ");
                Serial.print(_pressDuration);
                Serial.println("ms");
#endif
            }

            _lastState = _currentState;
        }
    }
}

/**
 * Check if button is currently pressed
 */
bool Button::isPressed() const {
    return _currentState;
}

/**
 * Check if button was just pressed (edge detection)
 * This function returns true only once per press
 */
bool Button::wasPressed() {
    if (_pressedFlag) {
        _pressedFlag = false;
        return true;
    }
    return false;
}

/**
 * Check if button was just released (edge detection)
 * This function returns true only once per release
 */
bool Button::wasReleased() {
    if (_releasedFlag) {
        _releasedFlag = false;
        return true;
    }
    return false;
}

/**
 * Get the duration of the current or last button press
 */
unsigned long Button::getPressDuration() const {
    if (_currentState) {
        // Button is currently pressed, return current duration
        return millis() - _pressStartTime;
    } else {
        // Button is released, return last press duration
        return _pressDuration;
    }
}

/**
 * Get timestamp of when button was last pressed
 */
unsigned long Button::getLastPressTime() const {
    return _lastPressTime;
}

/**
 * Check if button was double-clicked
 */
bool Button::wasDoubleClicked(unsigned long timeoutMs) {
    // Reset click count if too much time has passed
    unsigned long currentTime = millis();
    if (currentTime - _lastClickTime > timeoutMs) {
        _clickCount = 0;
    }

    // Check and clear double-click flag
    if (_doubleClickFlag) {
        _doubleClickFlag = false;
        Serial.println("[Button] wasDoubleClicked() returning TRUE");
        return true;
    }
    return false;
}

/**
 * Reset button state
 */
void Button::reset() {
    _pressedFlag = false;
    _releasedFlag = false;
    _pressDuration = 0;
    _clickCount = 0;
    _doubleClickFlag = false;
}

/**
 * Read raw button state from pin
 * Active LOW: button pressed = LOW (0), not pressed = HIGH (1)
 * So we invert the reading with !
 */
bool Button::readRaw() const {
    return !digitalRead(_pin);
}
