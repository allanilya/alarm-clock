#include "display_manager.h"
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Preferences.h>

DisplayManager::DisplayManager()
    : _display(nullptr),
      _initialized(false),
      _bleConnected(false),
      _timeSynced(false),
      _alarmStatus(""),
      _customMessage(""),
      _bottomRowLabel(""),
      _lastFullRefresh(0),
      _forceFullRefresh(false),
      _lastTimeStr(""),
      _scrollPixelOffset(0),
      _lastScrollTime(0) {
}

bool DisplayManager::begin() {
    Serial.println("DisplayManager: Initializing e-ink display...");

    // Create display object
    _display = new GxEPD2_BW<GxEPD2_370_GDEY037T03, GxEPD2_370_GDEY037T03::HEIGHT>(
        GxEPD2_370_GDEY037T03(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
    );

    if (!_display) {
        Serial.println("DisplayManager: ERROR - Failed to create display object!");
        return false;
    }

    // Initialize display
    _display->init(115200);
    _display->setRotation(1);  // Landscape mode
    _display->setTextColor(GxEPD_BLACK);
    _display->setTextWrap(false);

    Serial.println("DisplayManager: Display initialized successfully!");

    // Do initial full refresh to clear screen
    _display->setFullWindow();
    _display->firstPage();
    do {
        _display->fillScreen(GxEPD_WHITE);
    } while (_display->nextPage());

    // Load custom message and bottom row label from NVS
    Preferences prefs;
    prefs.begin("display", true);  // Read-only
    _customMessage = prefs.getString("customMsg", "");
    _bottomRowLabel = prefs.getString("bottomLabel", "");
    prefs.end();

    if (_customMessage.length() > 0) {
        Serial.print("DisplayManager: Loaded custom message: ");
        Serial.println(_customMessage);
    }

    if (_bottomRowLabel.length() > 0) {
        Serial.print("DisplayManager: Loaded bottom row label: ");
        Serial.println(_bottomRowLabel);
    }

    _lastFullRefresh = millis();
    _initialized = true;

    return true;
}

void DisplayManager::showClock(const String& timeStr, const String& dateStr, const String& dayStr, uint8_t second) {
    if (!_initialized) return;

    // Check if we need a full refresh (only when forced, e.g., at 3 AM)
    if (_forceFullRefresh) {
        Serial.println("DisplayManager: Performing full refresh (3 AM daily refresh)...");
        _display->setFullWindow();
        _lastFullRefresh = millis();
        _forceFullRefresh = false;
    } else {
        // Partial update for time changes
        _display->setPartialWindow(0, 0, _display->width(), _display->height());
    }

    _display->firstPage();
    do {
        // Clear screen
        _display->fillScreen(GxEPD_WHITE);

        // Draw decorative border
        _display->drawRect(5, 5, _display->width() - 10, _display->height() - 10, GxEPD_BLACK);
        _display->drawRect(7, 7, _display->width() - 14, _display->height() - 14, GxEPD_BLACK);

        // Draw status icons at top
        drawStatusIcons();

        // Top row: Custom message (if set) or day of week
        _display->setFont(&FreeMonoBold12pt7b);
        int16_t x1, y1;
        uint16_t w, h;
        
        if (_customMessage.length() > 0) {
            // Check if message needs scrolling
            _display->getTextBounds(_customMessage.c_str(), 0, 0, &x1, &y1, &w, &h);
            int16_t availableWidth = _display->width() - 40;  // Leave margin for borders
            int16_t messageWidth = w;
            
            if (messageWidth > availableWidth) {
                // Message is too long - implement smooth pixel-based scrolling with clipping
                unsigned long currentTime = millis();
                
                // Update scroll position periodically (smooth pixel scrolling)
                if (currentTime - _lastScrollTime > SCROLL_DELAY) {
                    _scrollPixelOffset += SCROLL_SPEED;
                    _lastScrollTime = currentTime;
                }
                
                // Calculate total width including spacing
                String spacedMessage = _customMessage + "     ";  // 5 spaces between loops
                _display->getTextBounds(spacedMessage.c_str(), 0, 0, &x1, &y1, &w, &h);
                int16_t totalScrollWidth = w;
                
                // Loop back to start when we've scrolled through the entire message
                if (_scrollPixelOffset >= totalScrollWidth) {
                    _scrollPixelOffset = 0;
                }
                
                // Create extended message for seamless looping
                String displayText = _customMessage + "     " + _customMessage + "     ";
                
                // Define clipping boundaries (inside the borders)
                int16_t clipLeft = 20;
                int16_t clipRight = _display->width() - 20;
                int16_t clipTop = 25;
                int16_t clipBottom = 55;
                
                // Calculate start position - text scrolls from left to right edge
                int16_t startX = clipLeft - _scrollPixelOffset;
                
                // Draw the scrolling text
                _display->setCursor(startX, 45);
                _display->print(displayText);
                
                // Mask overflow areas with white rectangles
                // Left mask - from left edge to clip boundary
                _display->fillRect(0, clipTop, clipLeft, clipBottom - clipTop, GxEPD_WHITE);
                // Right mask - from clip boundary to right edge
                _display->fillRect(clipRight, clipTop, _display->width() - clipRight, clipBottom - clipTop, GxEPD_WHITE);
                
                // Redraw the borders on top so they're not covered by masks
                _display->drawRect(5, 5, _display->width() - 10, _display->height() - 10, GxEPD_BLACK);
                _display->drawRect(7, 7, _display->width() - 14, _display->height() - 14, GxEPD_BLACK);
                
            } else {
                // Message fits - display normally (centered)
                _display->getTextBounds(_customMessage.c_str(), 0, 0, &x1, &y1, &w, &h);
                int16_t topX = (_display->width() - w) / 2;
                _display->setCursor(topX, 45);
                _display->print(_customMessage);
                _scrollPixelOffset = 0;
            }
        } else {
            // No custom message - show day of week (centered)
            _display->getTextBounds(dayStr.c_str(), 0, 0, &x1, &y1, &w, &h);
            int16_t topX = (_display->width() - w) / 2;
            _display->setCursor(topX, 45);
            _display->print(dayStr);
            _scrollPixelOffset = 0;
        }

        // Draw horizontal line under top row
        _display->drawLine(20, 60, _display->width() - 20, 60, GxEPD_BLACK);

        // Display large time in center
        _display->setFont(&FreeSansBold24pt7b);
        _display->getTextBounds(timeStr.c_str(), 0, 0, &x1, &y1, &w, &h);
        int16_t timeX = (_display->width() - w) / 2;
        int16_t timeY = (_display->height() / 2) + 20;
        _display->setCursor(timeX, timeY);
        _display->print(timeStr);

        // Draw small analog seconds clock to the right of time
        int16_t clockCenterX = timeX + w + 35;  // Position to the right of time
        int16_t clockCenterY = timeY - 20;      // Vertically aligned with time
        int16_t clockRadius = 20;               // Small clock radius

        // Draw clock circle
        _display->drawCircle(clockCenterX, clockCenterY, clockRadius, GxEPD_BLACK);

        // Calculate hand angle (seconds: 0 = top, clockwise)
        // Convert seconds (0-59) to angle in radians
        // 0 seconds = -90 degrees (top), each second = 6 degrees
        float angle = (second * 6.0 - 90.0) * PI / 180.0;

        // Calculate hand endpoint (hand length = radius - 2)
        int16_t handLength = clockRadius - 3;
        int16_t handX = clockCenterX + handLength * cos(angle);
        int16_t handY = clockCenterY + handLength * sin(angle);

        // Draw the seconds hand
        _display->drawLine(clockCenterX, clockCenterY, handX, handY, GxEPD_BLACK);

        // Draw center dot
        _display->fillCircle(clockCenterX, clockCenterY, 2, GxEPD_BLACK);

        // Check if bottom row label is set for dynamic layout
        if (_bottomRowLabel.length() > 0) {
            // LAYOUT WITH BOTTOM LABEL:
            // - Draw day and date UNDER the time (smaller font)
            _display->setFont(&FreeMono9pt7b);
            String dayDateStr = dayStr + ", " + dateStr;
            _display->getTextBounds(dayDateStr.c_str(), 0, 0, &x1, &y1, &w, &h);
            int16_t dayDateX = (_display->width() - w) / 2;
            int16_t dayDateY = timeY + 35;  // Below the time
            _display->setCursor(dayDateX, dayDateY);
            _display->print(dayDateStr);

            // - Draw bottom row label at the bottom
            _display->setFont(&FreeMonoBold12pt7b);
            _display->getTextBounds(_bottomRowLabel.c_str(), 0, 0, &x1, &y1, &w, &h);
            int16_t bottomX = (_display->width() - w) / 2;
            _display->setCursor(bottomX, _display->height() - 30);
            _display->print(_bottomRowLabel);

            // Draw horizontal line above bottom label
            _display->drawLine(20, _display->height() - 50, _display->width() - 20, _display->height() - 50, GxEPD_BLACK);
        } else {
            // DEFAULT LAYOUT (no bottom label):
            // - Bottom row shows: Day+Date (if custom message) OR just Date
            _display->setFont(&FreeMonoBold12pt7b);
            String bottomText = (_customMessage.length() > 0) ? (dayStr + " " + dateStr) : dateStr;
            _display->getTextBounds(bottomText.c_str(), 0, 0, &x1, &y1, &w, &h);
            int16_t bottomX = (_display->width() - w) / 2;
            _display->setCursor(bottomX, _display->height() - 30);
            _display->print(bottomText);

            // Draw horizontal line above date
            _display->drawLine(20, _display->height() - 50, _display->width() - 20, _display->height() - 50, GxEPD_BLACK);
        }

    } while (_display->nextPage());

    _lastTimeStr = timeStr;
}

void DisplayManager::showAlarmRinging(const String& timeStr, const String& alarmLabel, const String& bottomRowLabel) {
    if (!_initialized) return;

    Serial.print("DisplayManager: Showing alarm ringing screen for: ");
    Serial.println(alarmLabel);

    _display->setFullWindow();
    _display->firstPage();
    do {
        _display->fillScreen(GxEPD_WHITE);

        // Draw thick border
        for (int i = 0; i < 5; i++) {
            _display->drawRect(5 + i, 5 + i,
                             _display->width() - 10 - (i * 2),
                             _display->height() - 10 - (i * 2),
                             GxEPD_BLACK);
        }

        // Display alarm label (truncate if too long)
        _display->setFont(&FreeMonoBold24pt7b);
        int16_t x1, y1;
        uint16_t w, h;
        String displayLabel = alarmLabel;

        // Check if label fits, use smaller font if needed
        _display->getTextBounds(displayLabel.c_str(), 0, 0, &x1, &y1, &w, &h);
        if (w > (_display->width() - 40)) {
            // Try smaller font
            _display->setFont(&FreeMonoBold12pt7b);
            _display->getTextBounds(displayLabel.c_str(), 0, 0, &x1, &y1, &w, &h);
            // If still too long, truncate
            if (w > (_display->width() - 40)) {
                while (displayLabel.length() > 0 && w > (_display->width() - 40)) {
                    displayLabel = displayLabel.substring(0, displayLabel.length() - 1);
                    _display->getTextBounds(displayLabel.c_str(), 0, 0, &x1, &y1, &w, &h);
                }
            }
        }

        _display->getTextBounds(displayLabel.c_str(), 0, 0, &x1, &y1, &w, &h);
        int16_t alarmX = (_display->width() - w) / 2;
        _display->setCursor(alarmX, 80);
        _display->print(displayLabel);

        // Current time - USE SAME FONT AS NORMAL CLOCK (FreeSansBold24pt7b)
        _display->setFont(&FreeSansBold24pt7b);
        _display->getTextBounds(timeStr.c_str(), 0, 0, &x1, &y1, &w, &h);
        int16_t timeX = (_display->width() - w) / 2;
        int16_t timeY = (_display->height() / 2) + 20;
        _display->setCursor(timeX, timeY);
        _display->print(timeStr);

        // Bottom row: Show custom label if set, otherwise show instructions
        if (bottomRowLabel.length() > 0) {
            // Show custom bottom row label
            _display->setFont(&FreeMonoBold12pt7b);
            _display->getTextBounds(bottomRowLabel.c_str(), 0, 0, &x1, &y1, &w, &h);
            int16_t labelX = (_display->width() - w) / 2;
            _display->setCursor(labelX, _display->height() - 30);
            _display->print(bottomRowLabel);
        } else {
            // Show default instructions
            _display->setFont(&FreeMono9pt7b);
            const char* instr1 = "Single click: Snooze 5 min";
            _display->getTextBounds(instr1, 0, 0, &x1, &y1, &w, &h);
            int16_t instr1X = (_display->width() - w) / 2;
            _display->setCursor(instr1X, _display->height() - 50);
            _display->print(instr1);

            const char* instr2 = "Double click: Dismiss";
            _display->getTextBounds(instr2, 0, 0, &x1, &y1, &w, &h);
            int16_t instr2X = (_display->width() - w) / 2;
            _display->setCursor(instr2X, _display->height() - 30);
            _display->print(instr2);
        }

    } while (_display->nextPage());
}

void DisplayManager::setBLEStatus(bool connected) {
    _bleConnected = connected;
}

void DisplayManager::setTimeSyncStatus(bool synced) {
    _timeSynced = synced;
}

void DisplayManager::setAlarmStatus(const String& status) {
    _alarmStatus = status;
}

void DisplayManager::setCustomMessage(const String& message) {
    // Allow longer messages now that we support scrolling (max 100 chars)
    _customMessage = message.substring(0, 100);
    
    // Reset scroll position when message changes
    _scrollPixelOffset = 0;
    _lastScrollTime = 0;

    // Save to NVS
    Preferences prefs;
    prefs.begin("display", false);
    prefs.putString("customMsg", _customMessage);
    prefs.end();

    Serial.print("DisplayManager: Custom message set to: ");
    Serial.println(_customMessage.length() > 0 ? _customMessage : "(empty - using day of week)");
}

String DisplayManager::getCustomMessage() const {
    return _customMessage;
}

void DisplayManager::setBottomRowLabel(const String& label) {
    // Max 50 chars for bottom row label
    _bottomRowLabel = label.substring(0, 50);

    // Save to NVS
    Preferences prefs;
    prefs.begin("display", false);
    prefs.putString("bottomLabel", _bottomRowLabel);
    prefs.end();

    Serial.print("DisplayManager: Bottom row label set to: ");
    Serial.println(_bottomRowLabel.length() > 0 ? _bottomRowLabel : "(empty)");
}

String DisplayManager::getBottomRowLabel() const {
    return _bottomRowLabel;
}

void DisplayManager::forceFullRefresh() {
    _forceFullRefresh = true;
}

void DisplayManager::drawStatusIcons() {
    // Draw BLE status icon (top left)
    _display->setFont(&FreeMono9pt7b);
    _display->setCursor(15, 25);
    if (_bleConnected) {
        _display->print("BLE");
    } else {
        _display->print("---");
    }

    // Draw alarm status icon (top right) - replaces sync indicator
    _display->setCursor(_display->width() - 80, 25);
    if (_alarmStatus.length() > 0) {
        _display->print(_alarmStatus);  // "ALARM" or "SNOOZE"
    } else {
        _display->print("     ");  // Empty space if no alarm
    }
}
