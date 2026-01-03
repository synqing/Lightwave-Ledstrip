// ============================================================================
// DisplayUI - Main UI controller implementation
// ============================================================================
// 4x4 grid of encoder gauges with status bar
// Clean sprite-based rendering, no bullshit animations
// ============================================================================

#include "DisplayUI.h"
#include "ZoneComposerUI.h"
#include "../parameters/ParameterMap.h"
#include "../config/Config.h"
#include <M5Unified.h>
#include <esp_system.h>
#include <cstdio>
#include <cstring>
#include <Arduino.h>

DisplayUI::DisplayUI(M5GFX& display)
    : _display(display)
    , _currentScreen(UIScreen::GLOBAL)
{
    _header = nullptr;
    _zoneComposer = nullptr;
    for (int i = 0; i < 16; i++) {
        _gauges[i] = nullptr;
    }
}

DisplayUI::~DisplayUI() {
    if (_header) {
        delete _header;
        _header = nullptr;
    }
    if (_zoneComposer) {
        delete _zoneComposer;
        _zoneComposer = nullptr;
    }
    for (int i = 0; i < 16; i++) {
        if (_gauges[i]) {
            delete _gauges[i];
            _gauges[i] = nullptr;
        }
    }
}

void DisplayUI::begin() {
#if ENABLE_UI_DIAGNOSTICS
    Serial.printf("[DBG] begin_start cols=%d rows=%d cellW=%d cellH=%d\n", Theme::GRID_COLS, Theme::GRID_ROWS, Theme::CELL_W, Theme::CELL_H);
#endif

    _display.fillScreen(Theme::BG_DARK);

    // Create header
    _header = new UIHeader(&_display);
#if ENABLE_UI_DIAGNOSTICS
    Serial.printf("[DBG] header_created ptr=%p\n", _header);
#endif

    // Create 8x1 grid of gauges (8 global parameters only)
    int yOffset = Theme::STATUS_BAR_H;

    for (int i = 0; i < 8; i++) {
        int row = i / Theme::GRID_COLS;
        int col = i % Theme::GRID_COLS;

        int x = col * Theme::CELL_W;
        int y = yOffset + row * Theme::CELL_H;

#if ENABLE_UI_DIAGNOSTICS
        Serial.printf("[DBG] creating_gauge i=%d x=%d y=%d w=%d h=%d\n", i, x, y, Theme::CELL_W, Theme::CELL_H);
#endif

        _gauges[i] = new GaugeWidget(&_display, x, y, Theme::CELL_W, Theme::CELL_H, i);
        
        // Set initial max value from ParameterMap
        uint8_t maxValue = getParameterMax(i);
        _gauges[i]->setMaxValue(maxValue);
    }
    
#if ENABLE_UI_DIAGNOSTICS
    Serial.printf("[DBG] gauges_created count=8\n");
#endif
    
    // Clear remaining gauge slots (8-15) - not used in global view
    for (int i = 8; i < 16; i++) {
        _gauges[i] = nullptr;
    }

    // Create zone composer UI
    _zoneComposer = new ZoneComposerUI(_display);
    _zoneComposer->setHeader(_header);  // Share header instance
    _zoneComposer->begin();

#if ENABLE_UI_DIAGNOSTICS
    Serial.printf("[DBG] zonecomposer_created ptr=%p\n", _zoneComposer);
#endif

    // Initial render
    renderCurrentScreen();

#if ENABLE_UI_DIAGNOSTICS
    Serial.printf("[DBG] begin_complete\n");
#endif
}

void DisplayUI::setScreen(UIScreen screen) {
    if (_currentScreen != screen) {
        _currentScreen = screen;
        
        // When switching to GLOBAL, force full redraw to ensure clean transition
        if (_currentScreen == UIScreen::GLOBAL) {
            // Clear screen to remove any Zone Composer artifacts
            _display.fillScreen(Theme::BG_DARK);
            
            // Force all widgets to redraw
            if (_header) {
                _header->markDirty();
            }
            for (int i = 0; i < 8; i++) {
                if (_gauges[i]) {
                    _gauges[i]->markDirty();
                }
            }
        }
        
        renderCurrentScreen();
    }
}

void DisplayUI::renderCurrentScreen() {
#if ENABLE_UI_DIAGNOSTICS
    Serial.printf("[DBG] renderCurrentScreen screen=%d\n", (int)_currentScreen);
#endif
    
    switch (_currentScreen) {
        case UIScreen::GLOBAL:
            if (_header) {
#if ENABLE_UI_DIAGNOSTICS
                Serial.printf("[DBG] rendering header\n");
#endif
                _header->render();
            }
            // Only 8 global gauges exist (indices 0-7)
            for (int i = 0; i < 8; i++) {
                if (_gauges[i]) {
#if ENABLE_UI_DIAGNOSTICS
                    Serial.printf("[DBG] rendering gauge %d\n", i);
#endif
                    _gauges[i]->render();
                }
            }
#if ENABLE_UI_DIAGNOSTICS
            Serial.printf("[DBG] renderCurrentScreen complete\n");
#endif
            break;
        case UIScreen::ZONE_COMPOSER:
            if (_zoneComposer) {
                _zoneComposer->markDirty();
                _zoneComposer->loop();
            }
            break;
    }
}

void DisplayUI::loop() {
    uint32_t now = millis();

    // Update header (power + connection) every 500ms
    if (now - _lastStatsUpdate >= 500) {
        _lastStatsUpdate = now;
        updateHeader();
    }

    // Clear highlight after 300ms (only for global screen)
    if (_currentScreen == UIScreen::GLOBAL && _highlightIdx < 8 && _gauges[_highlightIdx] && now - _highlightTime >= 300) {
        _gauges[_highlightIdx]->setHighlight(false);
        _gauges[_highlightIdx]->render();
        _highlightIdx = 255;
    }

    // Render current screen
    switch (_currentScreen) {
        case UIScreen::GLOBAL:
            // Render header (checks dirty flag internally)
            if (_header) {
                _header->render();
            }
            // Render gauges (each checks dirty flag internally) - only 8 global parameters
            for (int i = 0; i < 8; i++) {
                if (_gauges[i]) {
                    _gauges[i]->render();
                }
            }
            break;
        case UIScreen::ZONE_COMPOSER:
            // Render header on Zone Composer screen too
            if (_header) {
                _header->render();
            }
            if (_zoneComposer) {
                _zoneComposer->loop();
            }
            break;
    }
}

void DisplayUI::updateEncoder(uint8_t index, int32_t value) {
    // Only handle global parameters (0-7)
    if (index >= 8) return;

    if (!_gauges[index]) return;

    // Sync max value from ParameterMap (in case it was updated dynamically)
    uint8_t maxValue = getParameterMax(index);
    _gauges[index]->setMaxValue(maxValue);
    
    // Always update the gauge's stored value (cache it even when not on GLOBAL screen)
    _gauges[index]->setValue(value);

    // Only perform expensive rendering/highlighting when on GLOBAL screen
    if (_currentScreen == UIScreen::GLOBAL) {
        // Clear previous highlight
        if (_highlightIdx < 8 && _highlightIdx != index && _gauges[_highlightIdx]) {
            _gauges[_highlightIdx]->setHighlight(false);
        }

        // Update and highlight
        _gauges[index]->setHighlight(true);
        _gauges[index]->render();
        
        _highlightIdx = index;
        _highlightTime = millis();
    }
}

void DisplayUI::handleTouch(int16_t x, int16_t y) {
    // Forward touch to ZoneComposerUI if on that screen
    if (_currentScreen == UIScreen::ZONE_COMPOSER && _zoneComposer) {
        _zoneComposer->handleTouch(x, y);
    }
}

void DisplayUI::setConnectionState(bool wifi, bool ws, bool encA, bool encB) {
    if (!_header) return;
    
    DeviceConnState state;
    state.wifi = wifi;
    state.ws = ws;
    state.encA = encA;
    state.encB = encB;
    _header->setConnection(state);
}

void DisplayUI::updateStats() {
    // Legacy method - kept for compatibility but no longer used
    // Stats (heap/psram/uptime) removed from simplified header
}

void DisplayUI::updateHeader() {
    if (!_header) return;
    
    // Update power state from M5.Power
    int8_t batteryPercent = M5.Power.getBatteryLevel();
    bool isCharging = M5.Power.isCharging();
    // Get voltage in volts (M5.Power.getBatteryVoltage() returns millivolts)
    float voltage = M5.Power.getBatteryVoltage() / 1000.0f;
    _header->setPower(batteryPercent, isCharging, voltage);
}

// ============================================================================
// Metadata stubs (for effect/palette names from server - not yet wired to UI)
// ============================================================================

void DisplayUI::setCurrentEffect(uint8_t id, const char* name) {
    // TODO: Display current effect name in status bar or dedicated area
    (void)id;
    (void)name;
}

void DisplayUI::setCurrentPalette(uint8_t id, const char* name) {
    // TODO: Display current palette name in status bar or dedicated area
    (void)id;
    (void)name;
}

void DisplayUI::setWiFiInfo(const char* ip, const char* ssid, int32_t rssi) {
    // TODO: Display WiFi info in status bar
    (void)ip;
    (void)ssid;
    (void)rssi;
}
