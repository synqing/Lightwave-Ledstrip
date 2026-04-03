/**
 * Rotate8Reader — Wraps M5ROTATE8 library for one 8-encoder unit.
 *
 * Polls all 8 encoders via getRelCounter() + resetCounter(), applies
 * DetentDebounce to normalise M5Rotate8's +/-2 detent steps into
 * clean +/-1 output per physical click.
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <m5rotate8.h>
#include "DetentDebounce.h"

class Rotate8Reader {
public:
    using ChangeCallback = void(*)(uint8_t inputId, int32_t delta, bool button);

    explicit Rotate8Reader(M5ROTATE8& dev) : _dev(dev) {}

    void setCallback(ChangeCallback cb) { _callback = cb; }

    /// Poll all 8 encoders + buttons. baseId = first input ID for this unit.
    void poll(uint8_t baseId, uint32_t now) {
        for (uint8_t i = 0; i < 8; i++) {
            // Read cumulative relative counter
            int32_t raw = _dev.getRelCounter(i);

            // Reset counter after read (not auto-resetting like Unit-Scroll)
            if (raw != 0) {
                _dev.resetCounter(i);
            }

            // Discard wild values (I2C corruption artefact)
            if (raw > 100 || raw < -100) {
                raw = 0;
            }

            // Apply DetentDebounce normalisation
            if (raw != 0 && _debounce[i].process(raw, now)) {
                int32_t delta = _debounce[i].consume();
                if (delta != 0 && _callback) {
                    _callback(baseId + i, delta, false);
                }
            }

            // Read button state
            bool pressed = _dev.getKeyPressed(i);
            if (pressed != _prevButton[i]) {
                _prevButton[i] = pressed;
                if (pressed && _callback) {
                    _callback(baseId + i, 0, true);
                }
            }
        }
    }

    /// Set LED colour on one encoder (ch 0-7) or status LED (ch 8).
    void setLED(uint8_t ch, uint8_t r, uint8_t g, uint8_t b) {
        _dev.writeRGB(ch, r, g, b);
    }

private:
    M5ROTATE8& _dev;
    ChangeCallback _callback = nullptr;
    DetentDebounce _debounce[8] = {};
    bool _prevButton[8] = {};
};
