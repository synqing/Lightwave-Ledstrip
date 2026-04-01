/**
 * ScrollReader — Polls one M5Stack Unit-Scroll via direct I2C registers.
 * Bypasses the M5Unit-Scroll library (immature, known bugs).
 *
 * Register Map (addr 0x40):
 *   0x10: Absolute encoder position (4 bytes, int32_t)
 *   0x20: Button state (1 byte, 0x00 = pressed)
 *   0x30: RGB LED (3 bytes)
 *   0x50: Incremental encoder delta (4 bytes, int32_t, auto-resets)
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <Wire.h>
#include "../config.h"

class ScrollReader {
public:
    using ChangeCallback = void(*)(uint8_t inputId, int32_t delta, bool button);

    bool begin(TwoWire* wire) {
        _wire = wire;
        // Probe device at default address
        _wire->beginTransmission(hw::SCROLL_ADDR);
        if (_wire->endTransmission() != 0) {
            Serial.println("[Scroll] FAIL: 0x40 not responding");
            return false;
        }
        Serial.println("[Scroll] OK: 0x40");
        _available = true;
        return true;
    }

    void setCallback(ChangeCallback cb) { _callback = cb; }

    /// Poll encoder + button. inputId = this scroll's input ID.
    void poll(uint8_t inputId, uint32_t now) {
        if (!_available) return;

        // Read incremental delta (register 0x50, 4 bytes, auto-resets)
        delayMicroseconds(hw::INTER_TXN_DELAY_US);
        int32_t delta = readInt32(hw::SCROLL_REG_INC);

        if (delta != 0 && _callback) {
            _callback(inputId, delta, false);
        }

        // Read button (register 0x20, 1 byte, 0x00 = pressed)
        delayMicroseconds(hw::INTER_TXN_DELAY_US);
        bool pressed = (readByte(hw::SCROLL_REG_BUTTON) == 0x00);

        if (pressed != _prevButton) {
            _prevButton = pressed;
            if (pressed && _callback) {
                _callback(inputId, 0, true);
            }
        }
    }

    void setLED(uint8_t r, uint8_t g, uint8_t b) {
        if (!_available) return;
        _wire->beginTransmission(hw::SCROLL_ADDR);
        _wire->write(hw::SCROLL_REG_LED);
        _wire->write(r);
        _wire->write(g);
        _wire->write(b);
        _wire->endTransmission();
    }

    bool isAvailable() const { return _available; }

private:
    int32_t readInt32(uint8_t reg) {
        _wire->beginTransmission(hw::SCROLL_ADDR);
        _wire->write(reg);
        _wire->endTransmission(false);  // Repeated start
        _wire->requestFrom(hw::SCROLL_ADDR, (uint8_t)4);
        if (_wire->available() < 4) return 0;

        // Little-endian
        int32_t val = 0;
        val |= _wire->read();
        val |= _wire->read() << 8;
        val |= _wire->read() << 16;
        val |= _wire->read() << 24;
        return val;
    }

    uint8_t readByte(uint8_t reg) {
        _wire->beginTransmission(hw::SCROLL_ADDR);
        _wire->write(reg);
        _wire->endTransmission(false);
        _wire->requestFrom(hw::SCROLL_ADDR, (uint8_t)1);
        return _wire->available() ? _wire->read() : 0xFF;
    }

    TwoWire* _wire = nullptr;
    bool _available = false;
    ChangeCallback _callback = nullptr;
    bool _prevButton = false;
};
