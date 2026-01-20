#include "frontlight_manager.h"

FrontlightManager::FrontlightManager()
    : _brightness(50),           // Default 50% brightness
      _savedBrightness(50),
      _isOn(true) {
}

bool FrontlightManager::begin() {
    Serial.println("FrontlightManager: Initializing...");

    // Configure LED PWM channel
    ledcSetup(PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);

    // Attach channel to GPIO pin
    ledcAttachPin(FRONTLIGHT_PIN, PWM_CHANNEL);

    // Set initial brightness
    updatePWM();

    Serial.print("FrontlightManager: Initialized on GPIO ");
    Serial.println(FRONTLIGHT_PIN);
    Serial.print("FrontlightManager: Default brightness: ");
    Serial.print(_brightness);
    Serial.println("%");

    return true;
}

void FrontlightManager::setBrightness(uint8_t brightness) {
    // Clamp to 0-100 range
    if (brightness > 100) {
        brightness = 100;
    }

    _brightness = brightness;
    _savedBrightness = brightness;

    // If setting brightness > 0, turn on
    if (brightness > 0) {
        _isOn = true;
    } else {
        _isOn = false;
    }

    updatePWM();

    Serial.print("FrontlightManager: Brightness set to ");
    Serial.print(_brightness);
    Serial.println("%");
}

uint8_t FrontlightManager::getBrightness() const {
    return _brightness;
}

void FrontlightManager::on() {
    _isOn = true;
    _brightness = _savedBrightness;

    // Ensure minimum brightness when turning on
    if (_brightness == 0) {
        _brightness = 50;  // Default to 50%
        _savedBrightness = 50;
    }

    updatePWM();

    Serial.println("FrontlightManager: Turned ON");
}

void FrontlightManager::off() {
    _isOn = false;
    _savedBrightness = _brightness;  // Remember current brightness
    _brightness = 0;
    updatePWM();

    Serial.println("FrontlightManager: Turned OFF");
}

bool FrontlightManager::isOn() const {
    return _isOn;
}

void FrontlightManager::updatePWM() {
    uint8_t pwmValue = percentToPWM(_brightness);
    ledcWrite(PWM_CHANNEL, pwmValue);
}

uint8_t FrontlightManager::percentToPWM(uint8_t percent) {
    // Convert 0-100 percent to 0-255 PWM value
    return (uint8_t)((percent * 255) / 100);
}
