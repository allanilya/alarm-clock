#include <Arduino.h>
#include "config.h"
#include "button.h"
#include "audio_test.h"

// ============================================
// Global Objects
// ============================================
Button button(BUTTON_PIN, BUTTON_DEBOUNCE_MS);
AudioTest audio;

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
    Serial.println("Test: Button + I2S Audio");
    Serial.println("========================================\n");

    // Initialize button
    Serial.print("Initializing button on pin ");
    Serial.print(BUTTON_PIN);
    Serial.println("...");
    button.begin();
    Serial.println("Button initialized!");

    // Initialize I2S audio
    Serial.println("\nInitializing I2S audio...");
    Serial.print("  BCLK: GPIO ");
    Serial.println(I2S_BCLK);
    Serial.print("  LRC:  GPIO ");
    Serial.println(I2S_LRC);
    Serial.print("  DOUT: GPIO ");
    Serial.println(I2S_DOUT);

    if (audio.begin()) {
        Serial.println("I2S audio initialized successfully!\n");
    } else {
        Serial.println("ERROR: Failed to initialize I2S audio!\n");
    }

    // Print instructions
    Serial.println("========================================");
    Serial.println("READY - Press button to play test tone!");
    Serial.println("========================================\n");
    Serial.println("Expected behavior:");
    Serial.println("  - Short press: Play 440 Hz tone (A4 note)");
    Serial.println("  - Long press: Play 880 Hz tone (A5 note)");
    Serial.println("  - Very long press: Play chord\n");
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

        // Play different tones based on press duration
        if (duration < 500) {
            Serial.println("  [Short press - Playing A4 (440 Hz) for 2 seconds]");
            audio.playTone(440, 2000);  // A4 note for 2 seconds (easier to hear)
        } else if (duration < 2000) {
            Serial.println("  [Long press - Playing A5 (880 Hz) for 2 seconds]");
            audio.playTone(880, 2000);  // A5 note for 2 seconds
        } else {
            Serial.println("  [Very long press - Playing chord]");
            audio.playTone(523, 800);  // C5
            audio.playTone(659, 800);  // E5
            audio.playTone(784, 800);  // G5
        }
        Serial.println();
    }

    // Small delay to prevent overwhelming the CPU
    delay(10);
}
