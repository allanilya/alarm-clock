#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// ESP32-L Alarm Clock Configuration
// ============================================

// Project Version
#define PROJECT_NAME "ESP32-L Alarm Clock"
#define VERSION "0.1.0"

// ============================================
// Hardware Pin Assignments
// ============================================
// Note: ESP32-L board uses special pin labels (T pins = Touch pins)
// The comments show the physical label on the ESP32-L board

// Button Configuration
#define BUTTON_PIN          4     // GPIO 4 (Labeled "T0" on ESP32-L)
                                  // Green arcade button with microswitch
                                  // Active LOW (connects to GND when pressed)

// I2S Audio Pins
#define I2S_DOUT            22    // Labeled as "SCL" on ESP32-L board
#define I2S_BCLK            27    // Labeled as "T7" on ESP32-L board
#define I2S_LRC             14    // Labeled as "T6" on ESP32-L board

// E-Ink Display
// Controlled via DESPI-CO2 driver board using SPI
// Pin assignments handled by DESPI-CO2 library
// Display: GDEY037T03 (3.7" with frontlight)

// Frontlight (DESPI-F01)
// Currently powered via 5V/GND (always-on)
// Optional: Add GPIO control for on/off switching

// ============================================
// Button Configuration
// ============================================
#define BUTTON_DEBOUNCE_MS  50    // Debounce time in milliseconds
#define BUTTON_LONG_PRESS_MS 2000 // Long press threshold (future use)

// ============================================
// Serial Configuration
// ============================================
#define SERIAL_BAUD         115200

// ============================================
// Audio Configuration
// ============================================
#define AUDIO_VOLUME        15    // Default volume (0-21)
#define AUDIO_SAMPLE_RATE   44100 // Sample rate in Hz

// ============================================
// Display Configuration
// ============================================
#define DISPLAY_ROTATION    0     // Display rotation (0, 1, 2, 3)
#define PARTIAL_UPDATE_INTERVAL 60000  // Full refresh interval (ms)

// ============================================
// Alarm Configuration
// ============================================
#define MAX_ALARMS          10    // Maximum number of alarms
#define SNOOZE_DURATION_MS  300000 // Snooze duration (5 minutes)
#define ALARM_TIMEOUT_MS    600000 // Auto-stop after 10 minutes

// ============================================
// BLE Configuration
// ============================================
#define BLE_DEVICE_NAME     "ESP32-L Alarm"
#define BLE_MTU_SIZE        512   // Maximum BLE packet size

// ============================================
// File System Configuration
// ============================================
#define SPIFFS_MOUNT_POINT  "/spiffs"
#define ALARM_SOUNDS_DIR    "/spiffs/alarms"

// ============================================
// Debug Configuration
// ============================================
// Uncomment to enable debug output
// #define DEBUG_BUTTON
// #define DEBUG_AUDIO
// #define DEBUG_DISPLAY
// #define DEBUG_BLE
// #define DEBUG_ALARM

#endif // CONFIG_H
