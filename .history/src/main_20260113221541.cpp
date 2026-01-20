#include <Arduino.h>
#include <GxEPD2_BW.h>

// DESPI-C02 pin configuration for ESP32-L
#define EPD_CS    5
#define EPD_DC    17
#define EPD_RST   16
#define EPD_BUSY  15

// Create display object for GDEY037T03 (3.7" e-ink)
GxEPD2_BW<GxEPD2_370_GDEY037T03, GxEPD2_370_GDEY037T03::HEIGHT> display(
    GxEPD2_370_GDEY037T03(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n========================================");
    Serial.println("E-INK DISPLAY - HELLO WORLD TEST");
    Serial.println("========================================");
    Serial.println("Display: GDEY037T03 (3.7\")");
    Serial.println("Driver: UC8253");
    Serial.println("DESPI-C02: RESE = 0.47 ohm");
    Serial.println("========================================\n");

    // Initialize display
    Serial.println("Initializing display...");
    display.init(115200);
    display.setRotation(1);  // Landscape
    display.setTextColor(GxEPD_BLACK);
    Serial.println("Display initialized!\n");

    // Draw "Hello World"
    Serial.println("Drawing 'Hello World' on display...");
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(10, 50);
        display.setTextSize(3);
        display.print("Hello World!");
        display.setCursor(10, 100);
        display.setTextSize(2);
        display.print("GDEY037T03 Test");
    } while (display.nextPage());

    Serial.println("Display update complete!");
    Serial.println("\n========================================");
    Serial.println("CHECK THE DISPLAY NOW!");
    Serial.println("========================================");
    Serial.println("Expected: White background with black text");
    Serial.println("  Line 1: 'Hello World!' (large)");
    Serial.println("  Line 2: 'GDEY037T03 Test' (medium)");
    Serial.println("\nIf you see the text:");
    Serial.println("  SUCCESS! Display is working!");
    Serial.println("\nIf display unchanged:");
    Serial.println("  Hardware problem detected");
    Serial.println("========================================\n");
}

void loop() {
    // Nothing - one-shot test
    delay(1000);
}
