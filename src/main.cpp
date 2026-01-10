#include <Arduino.h>
#include "config.h"
#include "button.h"

// ============================================
// Global Objects
// ============================================
Button button(BUTTON_PIN, BUTTON_DEBOUNCE_MS);

// ============================================
// Setup Function
// ============================================
void setup() {
    // Initialize Serial communication
    Serial.begin(SERIAL_BAUD);
    delay(1000); // Wait for serial to initialize

    // Print startup banner
    Serial.println("\n\n========================================");
    Serial.println(PROJECT_NAME);
    Serial.print("Version: ");
    Serial.println(VERSION);
    Serial.println("========================================");
    Serial.println("Hardware: ESP32-L with DESPI-CO2");
    Serial.println("Test: Button Input with Debouncing");
    Serial.println("========================================\n");

    // Initialize button
    Serial.print("Initializing button on pin ");
    Serial.print(BUTTON_PIN);
    Serial.println("...");
    button.begin();
    Serial.println("Button initialized!");

    // Print instructions
    Serial.println("\n========================================");
    Serial.println("READY - Press the button to test!");
    Serial.println("========================================\n");
    Serial.println("Expected behavior:");
    Serial.println("  - Press button: \">>> Button PRESSED!\" message");
    Serial.println("  - Release button: \">>> Button RELEASED!\" message");
    Serial.println("  - Debouncing prevents false triggers");
    Serial.println("  - Timing information included\n");
}

// ============================================
// Loop Function
// ============================================
void loop() {
    // Update button state (must be called every loop iteration)
    button.update();

    // Check if button was just pressed
    if (button.wasPressed()) {
        unsigned long pressTime = button.getLastPressTime();
        Serial.println("========================================");
        Serial.print(">>> Button PRESSED!");
        Serial.print(" (time: ");
        Serial.print(pressTime);
        Serial.println("ms)");
        Serial.println("========================================");
    }

    // Check if button was just released
    if (button.wasReleased()) {
        unsigned long duration = button.getPressDuration();
        Serial.println("========================================");
        Serial.print(">>> Button RELEASED!");
        Serial.print(" (duration: ");
        Serial.print(duration);
        Serial.println("ms)");
        Serial.println("========================================\n");

        // Provide feedback on press duration
        if (duration < 100) {
            Serial.println("  [Quick tap detected]");
        } else if (duration < 500) {
            Serial.println("  [Normal press detected]");
        } else if (duration < 2000) {
            Serial.println("  [Long press detected]");
        } else {
            Serial.println("  [Very long press detected!]");
        }
        Serial.println();
    }

    // Small delay to prevent overwhelming the CPU
    delay(10);
}
