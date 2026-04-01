/**
 * Rotate8Reader — Polls one M5Rotate8 (8-encoder) unit via I2C.
 * Uses the M5ROTATE8 library for register access.
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <M5ROTATE8.h>
#include <Wire.h>
#include "DetentDebounce.h"
#include "../config.h"

class Rotate8Reader {
public:
    using ChangeCallback = void(*)(uint8_t inputId, int32_t delta, bool button);

    bool begin(TwoWire* wire, uint8_t addr) {
        _addr = addr;
        _wire = wire;
        // M5ROTATE8 takes addr+wire in constructor, then begin() with no args
        _dev = M5ROTATE8(addr, wire);
        if (!_dev.begin()) {
            Serial.printf("[Rot8] FAIL: 0x%02X not responding\n", addr);
            return false;
        }
        Serial.printf("[Rot8] OK: 0x%02X (firmware v%d)\n", addr, _dev.getVersion());
        _available = true;
        return true;
    }

    void setCallback(ChangeCallback cb) { _callback = cb; }

    /// Poll all 8 encoders + buttons. baseId = first input ID for this unit.
    void poll(uint8_t baseId, uint32_t now) {
        if (!_available) return;

        for (uint8_t i = 0; i < 8; i++) {
            delayMicroseconds(hw::INTER_TXN_DELAY_US);

            // Read relative counter (auto-resets)
            int32_t raw = _dev.getRelCounter(i);

            // Discard wild values (I2C corruption)
            if (raw > 100 || raw < -100) {
                raw = 0;
            }

            // Apply DetentDebounce
            if (raw != 0 && _debounce[i].process(raw, now)) {
                int32_t delta = _debounce[i].consume();
                if (delta != 0 && _callback) {
                    _callback(baseId + i, delta, false);
                }
            }

            delayMicroseconds(hw::INTER_TXN_DELAY_US);

            // Read button
            bool pressed = _dev.getKeyPressed(i);
            if (pressed != _prevButton[i]) {
                _prevButton[i] = pressed;
                if (pressed && _callback) {
                    _callback(baseId + i, 0, true);
                }
            }
        }
    }

    void setLED(uint8_t ch, uint8_t r, uint8_t g, uint8_t b) {
        if (!_available || ch > 8) return;
        _dev.writeRGB(ch, r, g, b);
    }

    bool isAvailable() const { return _available; }

private:
    M5ROTATE8 _dev;
    TwoWire* _wire = nullptr;
    uint8_t _addr = 0;
    bool _available = false;
    ChangeCallback _callback = nullptr;
    DetentDebounce _debounce[8] = {};
    bool _prevButton[8] = {};
};
