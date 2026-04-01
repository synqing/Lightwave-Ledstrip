/**
 * InputManager — Orchestrates polling of all 6 devices through PaHub v2.1.
 *
 * Input ID Map:
 *   0-7:   8Encoder A (CH0, addr 0x42) — Zone selection / effects
 *   8-15:  8Encoder B (CH1, addr 0x41) — Mix & config
 *   16:    Scroll Zone 1 (CH2)
 *   17:    Scroll Zone 2 (CH3)
 *   18:    Scroll Zone 3 (CH4)
 *   19:    Scroll Master  (CH5)
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <Wire.h>
#include <esp_task_wdt.h>
#include "PaHub.h"
#include "Rotate8Reader.h"
#include "ScrollReader.h"
#include "../config.h"

class InputManager {
public:
    using ChangeCallback = void(*)(uint8_t inputId, int32_t delta, bool button);

    // Input ID constants
    static constexpr uint8_t ID_ENC_A_BASE   = 0;   // 0-7
    static constexpr uint8_t ID_ENC_B_BASE   = 8;   // 8-15
    static constexpr uint8_t ID_SCROLL_ZONE1 = 16;
    static constexpr uint8_t ID_SCROLL_ZONE2 = 17;
    static constexpr uint8_t ID_SCROLL_ZONE3 = 18;
    static constexpr uint8_t ID_SCROLL_MASTER = 19;
    static constexpr uint8_t INPUT_COUNT     = 20;

    void setCallback(ChangeCallback cb) {
        _callback = cb;
        _encA.setCallback(cb);
        _encB.setCallback(cb);
        for (auto& s : _scroll) s.setCallback(cb);
    }

    bool begin() {
        Serial.println("[Input] Initialising devices...");

        // Init 8Encoder A on CH0
        pahub::selectChannel(hw::CH_ENC_A);
        bool aOk = _encA.begin(&Wire, hw::ENCODER_A_ADDR);

        // Init 8Encoder B on CH1
        pahub::selectChannel(hw::CH_ENC_B);
        bool bOk = _encB.begin(&Wire, hw::ENCODER_B_ADDR);

        // Init 4 Scroll units on CH2-CH5
        static constexpr uint8_t scrollChannels[4] = {
            hw::CH_SCROLL_1, hw::CH_SCROLL_2, hw::CH_SCROLL_3, hw::CH_SCROLL_M
        };
        static constexpr uint8_t scrollIds[4] = {
            ID_SCROLL_ZONE1, ID_SCROLL_ZONE2, ID_SCROLL_ZONE3, ID_SCROLL_MASTER
        };

        uint8_t scrollOk = 0;
        for (uint8_t i = 0; i < 4; i++) {
            pahub::selectChannel(scrollChannels[i]);
            if (_scroll[i].begin(&Wire)) {
                scrollOk++;
            }
        }

        Serial.printf("[Input] Ready: EncA=%s EncB=%s Scrolls=%d/4\n",
                      aOk ? "OK" : "FAIL", bOk ? "OK" : "FAIL", scrollOk);

        // Set initial LED colours on Scroll units
        setScrollLEDs();

        return aOk || bOk || scrollOk > 0;
    }

    /// Poll all devices. Call from loop() at target rate.
    void poll() {
        uint32_t now = millis();

        // Rate limit: skip if called too fast
        if (now - _lastPoll < (hw::POLL_INTERVAL_US / 1000)) return;
        _lastPoll = now;

        uint32_t t0 = micros();

        // --- 8Encoder A (CH0) ---
        if (pahub::selectChannel(hw::CH_ENC_A)) {
            _encA.poll(ID_ENC_A_BASE, now);
        }
        esp_task_wdt_reset();

        // --- 8Encoder B (CH1) ---
        if (pahub::selectChannel(hw::CH_ENC_B)) {
            _encB.poll(ID_ENC_B_BASE, now);
        }
        esp_task_wdt_reset();

        // --- 4 Scroll units (CH2-CH5) ---
        static constexpr uint8_t scrollChannels[4] = {
            hw::CH_SCROLL_1, hw::CH_SCROLL_2, hw::CH_SCROLL_3, hw::CH_SCROLL_M
        };
        static constexpr uint8_t scrollIds[4] = {
            ID_SCROLL_ZONE1, ID_SCROLL_ZONE2, ID_SCROLL_ZONE3, ID_SCROLL_MASTER
        };

        for (uint8_t i = 0; i < 4; i++) {
            if (pahub::selectChannel(scrollChannels[i])) {
                _scroll[i].poll(scrollIds[i], now);
            }
        }
        esp_task_wdt_reset();

        uint32_t elapsed = micros() - t0;
        _lastPollUs = elapsed;

        // Warn if poll cycle exceeds budget
        if (elapsed > 8000) {  // >8ms = too slow
            Serial.printf("[Input] WARN: poll took %lu us\n", elapsed);
        }
    }

    /// Set an LED on any input device.
    void setInputLED(uint8_t inputId, uint8_t r, uint8_t g, uint8_t b) {
        if (inputId < 8) {
            pahub::selectChannel(hw::CH_ENC_A);
            _encA.setLED(inputId, r, g, b);
        } else if (inputId < 16) {
            pahub::selectChannel(hw::CH_ENC_B);
            _encB.setLED(inputId - 8, r, g, b);
        } else if (inputId >= 16 && inputId <= 19) {
            uint8_t idx = inputId - 16;
            static constexpr uint8_t ch[4] = {
                hw::CH_SCROLL_1, hw::CH_SCROLL_2, hw::CH_SCROLL_3, hw::CH_SCROLL_M
            };
            pahub::selectChannel(ch[idx]);
            _scroll[idx].setLED(r, g, b);
        }
    }

    /// Set status LED (LED 8) on an 8Encoder unit. unit: 0=EncA, 1=EncB.
    void setStatusLED(uint8_t unit, uint8_t r, uint8_t g, uint8_t b) {
        if (unit == 0) {
            pahub::selectChannel(hw::CH_ENC_A);
            _encA.setLED(8, r, g, b);
        } else if (unit == 1) {
            pahub::selectChannel(hw::CH_ENC_B);
            _encB.setLED(8, r, g, b);
        }
    }

    uint32_t lastPollUs() const { return _lastPollUs; }

private:
    void setScrollLEDs() {
        // Set zone colour LEDs on each Scroll unit
        for (uint8_t i = 0; i < 3; i++) {
            static constexpr uint8_t ch[4] = {
                hw::CH_SCROLL_1, hw::CH_SCROLL_2, hw::CH_SCROLL_3, hw::CH_SCROLL_M
            };
            pahub::selectChannel(ch[i]);
            uint8_t r = (hw::ZONE_COLOUR[i] >> 16) & 0xFF;
            uint8_t g = (hw::ZONE_COLOUR[i] >> 8) & 0xFF;
            uint8_t b = hw::ZONE_COLOUR[i] & 0xFF;
            // Cap brightness at 50%
            _scroll[i].setLED(r >> 1, g >> 1, b >> 1);
        }
        // Master = white dim
        pahub::selectChannel(hw::CH_SCROLL_M);
        _scroll[3].setLED(64, 64, 64);
    }

    Rotate8Reader _encA;
    Rotate8Reader _encB;
    ScrollReader _scroll[4];
    ChangeCallback _callback = nullptr;
    uint32_t _lastPoll = 0;
    uint32_t _lastPollUs = 0;
};
