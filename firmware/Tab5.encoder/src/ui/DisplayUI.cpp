// ============================================================================
// DisplayUI - Main UI controller implementation
// ============================================================================
// 4x4 grid of encoder gauges with status bar
// Clean sprite-based rendering, no bullshit animations
// ============================================================================

#include "DisplayUI.h"

#ifdef SIMULATOR_BUILD
    // Minimal includes for simulator
    #include "../config/Config.h"
#else
    #include "ZoneComposerUI.h"
    #include "../parameters/ParameterMap.h"
    #include "../config/Config.h"
    #include "../presets/PresetManager.h"
#endif

#ifdef SIMULATOR_BUILD
    #include "M5GFX_Mock.h"
    #include "../hal/SimHal.h"
#else
    #include <M5Unified.h>
    #include <esp_system.h>
    #include <ESP.h>  // For heap monitoring
    #include <Arduino.h>
#endif

#include "../hal/EspHal.h"
#include <cstdio>
#include <cstring>

DisplayUI::DisplayUI(M5GFX& display)
    : _display(display)
    , _currentScreen(UIScreen::GLOBAL)
{
    _header = nullptr;
    #ifndef SIMULATOR_BUILD
    _zoneComposer = nullptr;
    #endif
    for (int i = 0; i < 16; i++) {
        _gauges[i] = nullptr;
    }
    for (int i = 0; i < 8; i++) {
        _presetSlots[i] = nullptr;
    }
}

DisplayUI::~DisplayUI() {
    if (_header) {
        delete _header;
        _header = nullptr;
    }
    #ifndef SIMULATOR_BUILD
    if (_zoneComposer) {
        delete _zoneComposer;
        _zoneComposer = nullptr;
    }
    #endif
    for (int i = 0; i < 16; i++) {
        if (_gauges[i]) {
            delete _gauges[i];
            _gauges[i] = nullptr;
        }
    }
    for (int i = 0; i < 8; i++) {
        if (_presetSlots[i]) {
            delete _presetSlots[i];
            _presetSlots[i] = nullptr;
        }
    }
    #ifndef SIMULATOR_BUILD
    if (_actionRow) {
        delete _actionRow;
        _actionRow = nullptr;
    }
    #endif
}

void DisplayUI::begin() {
    // #region agent log
    EspHal::log("[DEBUG] DisplayUI::begin entry - Heap: free=%u minFree=%u largest=%u\n",
                  EspHal::getFreeHeap(), EspHal::getMinFreeHeap(), EspHal::getMaxAllocHeap());
    EspHal::log("[DEBUG] Sprite memory estimate: %u gauges * %dx%d + %u slots * %dx%d + header %dx%d = ~%u KB\n",
                  8, Theme::CELL_W, Theme::CELL_H,
                  8, Theme::PRESET_SLOT_W, Theme::PRESET_SLOT_H,
                  Theme::SCREEN_W, Theme::STATUS_BAR_H,
                  ((8 * Theme::CELL_W * Theme::CELL_H * 2) + 
                   (8 * Theme::PRESET_SLOT_W * Theme::PRESET_SLOT_H * 2) +
                   (Theme::SCREEN_W * Theme::STATUS_BAR_H * 2)) / 1024);
    // #endregion
#if ENABLE_UI_DIAGNOSTICS
    EspHal::log("[DBG] begin_start cols=%d rows=%d cellW=%d cellH=%d\n", Theme::GRID_COLS, Theme::GRID_ROWS, Theme::CELL_W, Theme::CELL_H);
#endif

    _display.fillScreen(Theme::BG_DARK);

    // Create header
    // #region agent log
    EspHal::log("[DEBUG] Before UIHeader creation - Heap: free=%u minFree=%u\n",
                  EspHal::getFreeHeap(), EspHal::getMinFreeHeap());
    // #endregion
    _header = new UIHeader(&_display);
    // #region agent log
    EspHal::log("[DEBUG] After UIHeader creation - Heap: free=%u minFree=%u\n",
                  EspHal::getFreeHeap(), EspHal::getMinFreeHeap());
    // #endregion
#if ENABLE_UI_DIAGNOSTICS
    EspHal::log("[DBG] header_created ptr=%p\n", _header);
#endif

    // Create 8x1 grid of gauges (8 global parameters only)
    int yOffset = Theme::STATUS_BAR_H;

    for (int i = 0; i < 8; i++) {
        int row = i / Theme::GRID_COLS;
        int col = i % Theme::GRID_COLS;

        int x = col * Theme::CELL_W;
        int y = yOffset + row * Theme::CELL_H;

#if ENABLE_UI_DIAGNOSTICS
        EspHal::log("[DBG] creating_gauge i=%d x=%d y=%d w=%d h=%d\n", i, x, y, Theme::CELL_W, Theme::CELL_H);
#endif

        // #region agent log
        if (i == 0 || i == 7) {  // Log first and last gauge creation
            EspHal::log("[DEBUG] Creating gauge %d - Heap before: free=%u minFree=%u\n",
                          i, EspHal::getFreeHeap(), EspHal::getMinFreeHeap());
        }
        // #endregion
        _gauges[i] = new GaugeWidget(&_display, x, y, Theme::CELL_W, Theme::CELL_H, i);
        // #region agent log
        if (i == 0 || i == 7) {
            EspHal::log("[DEBUG] Gauge %d created - Heap after: free=%u minFree=%u\n",
                          i, EspHal::getFreeHeap(), EspHal::getMinFreeHeap());
        }
        // #endregion
        
        // Set initial max value from ParameterMap
        #ifdef SIMULATOR_BUILD
            uint8_t maxValue = 255;  // Default in simulator
        #else
            uint8_t maxValue = getParameterMax(i);
        #endif
        _gauges[i]->setMaxValue(maxValue);
    }
    
    // #region agent log
    EspHal::log("[DEBUG] All 8 gauges created - Heap: free=%u minFree=%u largest=%u\n",
                  EspHal::getFreeHeap(), EspHal::getMinFreeHeap(), EspHal::getMaxAllocHeap());
    // #endregion
#if ENABLE_UI_DIAGNOSTICS
    EspHal::log("[DBG] gauges_created count=8\n");
#endif
    
    // Clear remaining gauge slots (8-15) - not used in global view
    for (int i = 8; i < 16; i++) {
        _gauges[i] = nullptr;
    }

    // Create 8 preset slot widgets below gauge row
    // #region agent log
    EspHal::log("[DEBUG] Before preset slots creation - Heap: free=%u minFree=%u\n",
                  EspHal::getFreeHeap(), EspHal::getMinFreeHeap());
    // #endregion
    for (int i = 0; i < 8; i++) {
        int x = i * Theme::PRESET_SLOT_W;
        int y = Theme::PRESET_ROW_Y;
        EspHal::log("[DEBUG] Creating preset slot %d (P%d) at x=%d y=%d width=%d\n", 
                      i, i+1, x, y, Theme::PRESET_SLOT_W);
        _presetSlots[i] = new PresetSlotWidget(&_display, x, y, i);
        if (!_presetSlots[i]) {
            EspHal::log("[ERROR] Failed to create preset slot %d!\n", i);
        }
    }
    // #region agent log
    EspHal::log("[DEBUG] All 8 preset slots created - Heap: free=%u minFree=%u largest=%u\n",
                  EspHal::getFreeHeap(), EspHal::getMinFreeHeap(), EspHal::getMaxAllocHeap());
    // #endregion
#if ENABLE_UI_DIAGNOSTICS
    EspHal::log("[DBG] preset_slots_created count=8\n");
#endif

    // Create touch action row (third row)
    #ifndef SIMULATOR_BUILD
    _actionRow = new ActionRowWidget(&_display, 0, Theme::ACTION_ROW_Y, Theme::SCREEN_W, Theme::ACTION_ROW_H);
    #endif

    // Create zone composer UI
    #ifndef SIMULATOR_BUILD
    _zoneComposer = new ZoneComposerUI(_display);
    _zoneComposer->setHeader(_header);  // Share header instance
    _zoneComposer->begin();

    #if ENABLE_UI_DIAGNOSTICS
    EspHal::log("[DBG] zonecomposer_created ptr=%p\n", _zoneComposer);
    #endif
    #endif  // !SIMULATOR_BUILD

    // Clear entire screen to black background
    _display.fillScreen(Theme::BG_DARK);
    
    // Mark all widgets as dirty to force initial render
    if (_header) _header->markDirty();
    for (int i = 0; i < 8; i++) {
        if (_gauges[i]) _gauges[i]->markDirty();
        if (_presetSlots[i]) _presetSlots[i]->markDirty();
    }
    
    // Initial render
    renderCurrentScreen();

#if ENABLE_UI_DIAGNOSTICS
    EspHal::log("[DBG] begin_complete\n");
#endif
}

void DisplayUI::setScreen(UIScreen screen) {
    if (_currentScreen != screen) {
        _currentScreen = screen;
        
        // Clear screen for ANY transition to ensure clean rendering
        _display.fillScreen(Theme::BG_DARK);
        
        if (_currentScreen == UIScreen::GLOBAL) {
            // Force all widgets to redraw
            if (_header) {
                _header->markDirty();
            }
            for (int i = 0; i < 8; i++) {
                if (_gauges[i]) {
                    _gauges[i]->markDirty();
                }
                if (_presetSlots[i]) {
                    _presetSlots[i]->markDirty();
                }
            }
            #ifndef SIMULATOR_BUILD
            if (_actionRow) {
                _actionRow->markDirty();
            }
            #endif
        } else if (_currentScreen == UIScreen::ZONE_COMPOSER) {
            // Force Zone Composer to redraw by setting _dirty directly
            #ifndef SIMULATOR_BUILD
            if (_zoneComposer) {
                _zoneComposer->forceDirty();  // New method to bypass pending
            }
            #endif
        }

        renderCurrentScreen();
    }
}

void DisplayUI::renderCurrentScreen() {
#if ENABLE_UI_DIAGNOSTICS
    EspHal::log("[DBG] renderCurrentScreen screen=%d\n", (int)_currentScreen);
#endif
    
    switch (_currentScreen) {
        case UIScreen::GLOBAL:
            if (_header) {
#if ENABLE_UI_DIAGNOSTICS
                EspHal::log("[DBG] rendering header\n");
#endif
                _header->render();
            }
            // Only 8 global gauges exist (indices 0-7)
            for (int i = 0; i < 8; i++) {
                if (_gauges[i]) {
#if ENABLE_UI_DIAGNOSTICS
                    EspHal::log("[DBG] rendering gauge %d\n", i);
#endif
                    _gauges[i]->render();
                }
            }
            // Render preset slots below gauges
            for (int i = 0; i < 8; i++) {
                if (_presetSlots[i]) {
                    #ifdef SIMULATOR_BUILD
                    printf("[DEBUG] Calling render() on preset slot %d (P%d)\n", i, i+1);
                    #endif
                    _presetSlots[i]->render();
                } else {
                    #ifdef SIMULATOR_BUILD
                    printf("[ERROR] Preset slot %d (P%d) is NULL!\n", i, i+1);
                    #endif
                }
            }
            #ifndef SIMULATOR_BUILD
            if (_actionRow) {
                _actionRow->render();
            }
            #endif
#if ENABLE_UI_DIAGNOSTICS
            EspHal::log("[DBG] renderCurrentScreen complete\n");
#endif
            break;
        case UIScreen::ZONE_COMPOSER:
            #ifndef SIMULATOR_BUILD
            if (_zoneComposer) {
                _zoneComposer->forceDirty();  // Immediate redraw, bypass pending
                _zoneComposer->loop();
            }
            #endif
            break;
    }
}

void DisplayUI::loop() {
    uint32_t now = EspHal::millis();

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
            // Update and render preset slots
            for (int i = 0; i < 8; i++) {
                if (_presetSlots[i]) {
                    _presetSlots[i]->update();  // Handle animations
                    _presetSlots[i]->render();
                }
            }
            #ifndef SIMULATOR_BUILD
            if (_actionRow) {
                _actionRow->render();
            }
            #endif
            break;
        case UIScreen::ZONE_COMPOSER:
            // Render header on Zone Composer screen too
            if (_header) {
                _header->render();
            }
            #ifndef SIMULATOR_BUILD
            #ifndef SIMULATOR_BUILD
            if (_zoneComposer) {
                _zoneComposer->loop();
            }
            #endif
            #endif
            break;
    }
}

void DisplayUI::updateEncoder(uint8_t index, int32_t value, bool highlight) {
    // Only handle global parameters (0-7)
    if (index >= 8) return;

    if (!_gauges[index]) return;

    // Sync max value from ParameterMap (in case it was updated dynamically)
    #ifdef SIMULATOR_BUILD
        uint8_t maxValue = 255;  // Default in simulator
    #else
        uint8_t maxValue = getParameterMax(index);
    #endif
    _gauges[index]->setMaxValue(maxValue);
    
    // Always update the gauge's stored value (cache it even when not on GLOBAL screen)
    _gauges[index]->setValue(value);

    // Only perform expensive rendering/highlighting when on GLOBAL screen
    if (_currentScreen == UIScreen::GLOBAL) {
        if (!highlight) {
            _gauges[index]->render();
            return;
        }

        // Clear previous highlight
        if (_highlightIdx < 8 && _highlightIdx != index && _gauges[_highlightIdx]) {
            _gauges[_highlightIdx]->setHighlight(false);
        }

        // Update and highlight
        _gauges[index]->setHighlight(true);
        _gauges[index]->render();
        
        _highlightIdx = index;
        _highlightTime = EspHal::millis();
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
    
    // Update power state from HAL
    int8_t batteryPercent = EspHal::getBatteryLevel();
    bool isCharging = EspHal::isCharging();
    // Get voltage in volts
    float voltage = EspHal::getBatteryVoltage();
    _header->setPower(batteryPercent, isCharging, voltage);
}

#ifndef SIMULATOR_BUILD
void DisplayUI::setColourCorrectionState(const ColorCorrectionState& state) {
    if (!_actionRow) return;

    _actionRow->setGamma(state.gammaValue, state.gammaEnabled);
    _actionRow->setColourMode(state.mode);
    _actionRow->setAutoExposure(state.autoExposureEnabled);
    _actionRow->setBrownGuardrail(state.brownGuardrailEnabled);
}
#endif

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

// ============================================================================
// Preset Bank UI Methods
// ============================================================================

void DisplayUI::updatePresetSlot(uint8_t slot, bool occupied, uint8_t effectId, uint8_t paletteId, uint8_t brightness) {
    if (slot >= 8 || !_presetSlots[slot]) return;

    _presetSlots[slot]->setOccupied(occupied);
    if (occupied) {
        _presetSlots[slot]->setPresetInfo(effectId, paletteId, brightness);
    }
}

void DisplayUI::setActivePresetSlot(uint8_t slot) {
    // Clear previous active
    if (_activePresetSlot < 8 && _presetSlots[_activePresetSlot]) {
        _presetSlots[_activePresetSlot]->setActive(false);
    }

    // Set new active
    _activePresetSlot = slot;
    if (slot < 8 && _presetSlots[slot]) {
        _presetSlots[slot]->setActive(true);
    }
}

#ifndef SIMULATOR_BUILD
void DisplayUI::refreshAllPresetSlots(PresetManager* pm) {
    if (!pm) return;

    uint8_t occupancy = pm->getOccupancyMask();

    for (uint8_t i = 0; i < 8; i++) {
        if (!_presetSlots[i]) continue;

        bool occupied = (occupancy & (1 << i)) != 0;
        _presetSlots[i]->setOccupied(occupied);

        if (occupied) {
            PresetData preset;
            if (pm->getPreset(i, preset)) {
                _presetSlots[i]->setPresetInfo(preset.effectId, preset.paletteId, preset.brightness);
            }
        }
    }
}
#endif  // !SIMULATOR_BUILD
