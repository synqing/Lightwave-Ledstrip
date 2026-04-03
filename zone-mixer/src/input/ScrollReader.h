/**
 * ScrollReader — Wraps M5UnitScroll library for one Unit-Scroll encoder.
 *
 * Uses the official M5UnitScroll API (getIncEncoderValue, getButtonStatus,
 * setLEDColor) instead of raw register access. The M5UnitScroll instance
 * is shared across all 4 scroll channels — PaHub channel selection happens
 * in InputManager before each poll() call.
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <M5UnitScroll.h>

class ScrollReader {
public:
    using ChangeCallback = void(*)(uint8_t inputId, int32_t delta, bool button);

    explicit ScrollReader(M5UnitScroll& scroll) : _scroll(scroll) {}

    void setCallback(ChangeCallback cb) { _callback = cb; }

    /// Poll encoder delta and button. inputId = this scroll's input ID.
    void poll(uint8_t inputId, uint32_t now) {
        // Read incremental delta (auto-resets after read)
        int16_t raw = _scroll.getIncEncoderValue();
        if (raw != 0 && _callback) {
            _callback(inputId, static_cast<int32_t>(raw), false);
        }

        // Read button (true = pressed)
        bool pressed = _scroll.getButtonStatus();
        if (pressed != _prevButton) {
            _prevButton = pressed;
            if (pressed && _callback) {
                _callback(inputId, 0, true);
            }
        }
    }

    /// Set LED colour (RGB packed as 0xRRGGBB).
    void setColour(uint32_t rgb) {
        _scroll.setLEDColor(rgb);
    }

    /// Set LED colour from individual R, G, B components.
    void setLED(uint8_t r, uint8_t g, uint8_t b) {
        uint32_t rgb = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        _scroll.setLEDColor(rgb);
    }

private:
    M5UnitScroll& _scroll;
    ChangeCallback _callback = nullptr;
    bool _prevButton = false;
};
