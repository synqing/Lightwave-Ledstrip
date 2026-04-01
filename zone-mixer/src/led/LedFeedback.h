/**
 * LedFeedback — Visual feedback on all 22 LEDs across 6 devices.
 *
 * Dirty-flag per LED, flushed at 10Hz max. Provides:
 * - Value-proportional brightness on scroll/encoder LEDs
 * - Zone colour coding (cyan/green/orange)
 * - Connection state indication (green/blue/red breathing)
 * - Button flash (100ms white pulse)
 * - Speed/palette mode colour toggle
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <Arduino.h>
#include <cmath>
#include "../config.h"
#include "../input/InputManager.h"

class LedFeedback {
public:
    void begin(InputManager* mgr) { _mgr = mgr; }

    /// Call every loop iteration. Flushes dirty LEDs at 10Hz.
    void update(uint32_t now) {
        // Run animations
        updateBreathing(now);
        updateFlashes(now);

        // Rate-limited flush
        if (!_anyDirty || (now - _lastFlush < 100)) return;
        _lastFlush = now;
        _anyDirty = false;

        for (uint8_t i = 0; i < LED_COUNT; i++) {
            if (!_dirty[i]) continue;
            _dirty[i] = false;
            writeLED(i, _r[i], _g[i], _b[i]);
        }
    }

    // --- Value updates from ParameterMapper ---

    /// Set a parameter value for proportional LED brightness.
    void setParamValue(uint8_t inputId, uint8_t value) {
        if (inputId >= 20) return;
        _paramValue[inputId] = value;
        recomputeLED(inputId);
    }

    /// Set zone enabled state (dims LED when disabled).
    void setZoneEnabled(uint8_t zoneIdx, bool enabled) {
        if (zoneIdx >= 3) return;
        _zoneEnabled[zoneIdx] = enabled;
        recomputeLED(16 + zoneIdx);  // Scroll LEDs
    }

    /// Set speed/palette mode for zone encoders (changes colour).
    void setSpeedPalMode(uint8_t zoneIdx, bool isPalette) {
        if (zoneIdx >= 3) return;
        _isPalette[zoneIdx] = isPalette;
        recomputeLED(4 + zoneIdx);  // EncA E4-E6
    }

    /// Set EdgeMixer mode (changes EncB E0 colour).
    void setEdgeMixerMode(uint8_t mode) {
        _emMode = mode;
        recomputeLED(8);  // EncB E0 mapped to LED index 9
    }

    /// Set spatial/temporal state (changes EncB E3 colour).
    void setSpatialTemporal(uint8_t state) {
        _stState = state;
        recomputeLED(11);
    }

    /// Set camera mode state.
    void setCameraMode(bool on) {
        _cameraOn = on;
        recomputeLED(14);
    }

    // --- Connection state ---

    enum class ConnState : uint8_t { DISCONNECTED, CONNECTING, CONNECTED };
    void setConnectionState(ConnState s) {
        _connState = s;
        _connDirty = true;
    }

    void setZoneSystemOn(bool on) {
        _zoneSysOn = on;
        // EncA LED 8 = zone system indicator
        setRaw(LED_ENCA_STATUS, on ? 0 : 0, on ? 80 : 0, 0);
    }

    // --- Flash trigger ---
    void flashButton(uint8_t inputId) {
        if (inputId >= 20) return;
        uint8_t li = inputToLedIndex(inputId);
        if (li == 0xFF) return;
        _flashExpiry[li] = millis() + 100;
        setRaw(li, 128, 128, 128);  // White flash
    }

private:
    // LED index mapping:
    //  0-7:  EncA E0-E7
    //  8:    EncA LED 8 (status)
    //  9-16: EncB E0-E7
    //  17:   EncB LED 8 (status)
    //  18-21: Scroll Z1/Z2/Z3/Master
    static constexpr uint8_t LED_COUNT = 22;
    static constexpr uint8_t LED_ENCA_STATUS = 8;
    static constexpr uint8_t LED_ENCB_STATUS = 17;

    InputManager* _mgr = nullptr;

    uint8_t _r[LED_COUNT] = {};
    uint8_t _g[LED_COUNT] = {};
    uint8_t _b[LED_COUNT] = {};
    bool _dirty[LED_COUNT] = {};
    bool _anyDirty = false;
    uint32_t _lastFlush = 0;

    // Parameter values (for proportional brightness)
    uint8_t _paramValue[20] = {};
    bool _zoneEnabled[3] = {true, true, true};
    bool _isPalette[3] = {};
    uint8_t _emMode = 0;
    uint8_t _stState = 0;
    bool _cameraOn = false;
    bool _zoneSysOn = false;

    // Connection breathing
    ConnState _connState = ConnState::DISCONNECTED;
    bool _connDirty = true;

    // Flash timers
    uint32_t _flashExpiry[LED_COUNT] = {};

    // --- Map inputId to LED index ---
    uint8_t inputToLedIndex(uint8_t inputId) const {
        if (inputId < 8) return inputId;          // EncA E0-E7
        if (inputId < 16) return inputId + 1;     // EncB E0-E7 → LED 9-16
        if (inputId < 20) return inputId + 2;     // Scrolls → LED 18-21
        return 0xFF;
    }

    void setRaw(uint8_t ledIdx, uint8_t r, uint8_t g, uint8_t b) {
        if (ledIdx >= LED_COUNT) return;
        if (_r[ledIdx] == r && _g[ledIdx] == g && _b[ledIdx] == b) return;
        _r[ledIdx] = r; _g[ledIdx] = g; _b[ledIdx] = b;
        _dirty[ledIdx] = true;
        _anyDirty = true;
    }

    void writeLED(uint8_t ledIdx, uint8_t r, uint8_t g, uint8_t b) {
        if (!_mgr) return;
        if (ledIdx < 8) {
            _mgr->setInputLED(ledIdx, r, g, b);          // EncA E0-E7
        } else if (ledIdx == LED_ENCA_STATUS) {
            _mgr->setStatusLED(0, r, g, b);               // EncA LED 8
        } else if (ledIdx <= 16) {
            _mgr->setInputLED(ledIdx - 1, r, g, b);       // EncB E0-E7 → inputId 8-15
        } else if (ledIdx == LED_ENCB_STATUS) {
            _mgr->setStatusLED(1, r, g, b);               // EncB LED 8
        } else {
            _mgr->setInputLED(ledIdx - 2, r, g, b);       // Scrolls → inputId 16-19
        }
    }

    // --- Recompute LED colour from current state ---
    void recomputeLED(uint8_t inputId) {
        uint8_t li = inputToLedIndex(inputId);
        if (li == 0xFF) return;

        uint8_t r = 0, g = 0, b = 0;

        // Scroll zones (16-18): zone colour at proportional brightness
        if (inputId >= 16 && inputId <= 18) {
            uint8_t zi = inputId - 16;
            uint32_t c = hw::ZONE_COLOUR[zi];
            uint8_t br = _zoneEnabled[zi] ? _paramValue[inputId] : 25;
            r = ((c >> 16) & 0xFF) * br / 510;
            g = ((c >> 8) & 0xFF) * br / 510;
            b = (c & 0xFF) * br / 510;
        }
        // Master scroll (19): white proportional
        else if (inputId == 19) {
            uint8_t v = _paramValue[19] / 4;  // 0-63 range
            r = g = b = v;
        }
        // Zone effect encoders (0-2): zone colour fixed
        else if (inputId < 3) {
            uint32_t c = hw::ZONE_COLOUR[inputId];
            r = ((c >> 16) & 0xFF) >> 2;  // 25% brightness
            g = ((c >> 8) & 0xFF) >> 2;
            b = (c & 0xFF) >> 2;
        }
        // Master speed (3): blue
        else if (inputId == 3) {
            b = 64;
        }
        // Speed/palette toggle (4-6)
        else if (inputId >= 4 && inputId <= 6) {
            if (_isPalette[inputId - 4]) { g = 64; } else { b = 64; }
        }
        // EdgeMixer mode (8)
        else if (inputId == 8) {
            static constexpr uint8_t emColours[7][3] = {
                {32,32,32}, {64,32,0}, {0,0,64}, {0,48,48},
                {32,32,32}, {48,0,48}, {48,48,0}
            };
            uint8_t m = min(_emMode, (uint8_t)6);
            r = emColours[m][0]; g = emColours[m][1]; b = emColours[m][2];
        }
        // EdgeMixer spread (9): warm amber proportional
        else if (inputId == 9) {
            r = _paramValue[9] * 64 / 60;  // 0-60 → 0-64
            g = _paramValue[9] * 32 / 60;
        }
        // EdgeMixer strength (10): cool blue proportional
        else if (inputId == 10) {
            b = _paramValue[10] / 4;
        }
        // Spatial/temporal (11): 4-state colour
        else if (inputId == 11) {
            static constexpr uint8_t stColours[4][3] = {
                {0,0,0}, {0,0,64}, {0,64,0}, {64,64,64}
            };
            uint8_t s = min(_stState, (uint8_t)3);
            r = stColours[s][0]; g = stColours[s][1]; b = stColours[s][2];
        }
        // Zone count (12): green
        else if (inputId == 12) { g = 48; }
        // Transition (13): dim purple
        else if (inputId == 13) { r = 32; b = 32; }
        // Camera mode (14): orange when on
        else if (inputId == 14) {
            if (_cameraOn) { r = 64; g = 32; }
        }
        // Preset (15): white dim
        else if (inputId == 15) { r = g = b = 32; }

        setRaw(li, r, g, b);
    }

    void updateBreathing(uint32_t now) {
        if (!_connDirty && _connState == ConnState::CONNECTED) return;
        _connDirty = false;

        uint8_t r = 0, g = 0, b = 0;
        if (_connState == ConnState::CONNECTED) {
            r = 0; g = 80; b = 0;
        } else if (_connState == ConnState::CONNECTING) {
            // Sine breathing: 30-100% over 1500ms
            float phase = fmodf((float)now / 1500.0f, 1.0f);
            float factor = 0.3f + 0.7f * (0.5f + 0.5f * sinf(phase * 2.0f * M_PI));
            b = (uint8_t)(180.0f * factor);
            _connDirty = true;  // Keep animating
        } else {
            r = 180; g = 0; b = 0;
        }
        setRaw(LED_ENCB_STATUS, r, g, b);
    }

    void updateFlashes(uint32_t now) {
        for (uint8_t i = 0; i < LED_COUNT; i++) {
            if (_flashExpiry[i] != 0 && now >= _flashExpiry[i]) {
                _flashExpiry[i] = 0;
                // Restore by recomputing from state
                // Map LED index back to inputId
                uint8_t inputId;
                if (i < 8) inputId = i;
                else if (i == 8) continue;  // Status LED, no param
                else if (i <= 16) inputId = i - 1;
                else if (i == 17) continue;
                else inputId = i - 2;
                recomputeLED(inputId);
            }
        }
    }
};
