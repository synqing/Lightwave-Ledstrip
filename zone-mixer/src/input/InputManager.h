/**
 * InputManager — Orchestrates polling of all 6 devices through PaHub v2.1.
 *
 * Owns 1x shared M5UnitScroll instance (reused across 4 PaHub channels),
 * 2x M5ROTATE8 instances (EncA at 0x42, EncB at 0x41), and their reader
 * wrappers. Manages PaHub channel switching and 100Hz poll loop.
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
#include <M5UnitScroll.h>
#include <m5rotate8.h>
#include "I2CBusClear.h"
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

    InputManager()
        : _encADev(hw::ENCODER_A_ADDR, &Wire)
        , _encBDev(hw::ENCODER_B_ADDR, &Wire)
        , _encA(_encADev)
        , _encB(_encBDev)
        , _scroll{ScrollReader(_scrollDev), ScrollReader(_scrollDev),
                  ScrollReader(_scrollDev), ScrollReader(_scrollDev)}
    {}

    void setCallback(ChangeCallback cb) {
        _callback = cb;
        _encA.setCallback(cb);
        _encB.setCallback(cb);
        for (auto& s : _scroll) s.setCallback(cb);
    }

    bool begin() {
        Serial.println("[Input] Initialising devices...");

        // Step 0: Clear bus if PCA9548A is holding SDA low after ESP32 reset.
        i2cBusClear(hw::I2C_SDA, hw::I2C_SCL);

        // Step 1: Initialise I2C bus — matches March 31 working pattern.
        // NO Wire.end() — calling end() on a never-started bus corrupts i2c-ng.
        // NO PaHub probe — zero-byte address probe can timeout on i2c-ng.
        // Go straight to selectChannel + device probes (old working pattern).
        Wire.begin(hw::I2C_SDA, hw::I2C_SCL);
        Wire.setClock(hw::I2C_FREQ);
        Wire.setTimeOut(10);  // 10ms — proven value from March 31 build

        // Step 2: Store M5UnitScroll parameters (no bus probe — see lib patch).
        _scrollDev.begin(&Wire, hw::SCROLL_I2C_ADDR,
                         hw::I2C_SDA, hw::I2C_SCL, hw::I2C_FREQ);

        // Step 3: Probe devices via selectChannel + isConnected/getDevStatus.
        // PaHub health is inferred: if ANY selectChannel succeeds, PaHub is alive.

        // EncA on CH0
        bool chOk = pahub::selectChannel(Wire, hw::CH_ENC_A);
        bool aOk = chOk && _encADev.begin();
        if (aOk) {
            Serial.printf("[Input] EncA OK: 0x%02X\n", hw::ENCODER_A_ADDR);
            _encAAvail = true;
        } else {
            Serial.printf("[Input] EncA FAIL: 0x%02X (ch=%s)\n",
                          hw::ENCODER_A_ADDR, chOk ? "ok" : "fail");
        }

        // EncB on CH1
        chOk = pahub::selectChannel(Wire, hw::CH_ENC_B);
        bool bOk = chOk && _encBDev.begin();
        if (bOk) {
            Serial.printf("[Input] EncB OK: 0x%02X\n", hw::ENCODER_B_ADDR);
            _encBAvail = true;
        } else {
            Serial.printf("[Input] EncB FAIL: 0x%02X (ch=%s)\n",
                          hw::ENCODER_B_ADDR, chOk ? "ok" : "fail");
        }

        // Scrolls on CH2-CH5
        static constexpr uint8_t scrollChannels[4] = {
            hw::CH_SCROLL_1, hw::CH_SCROLL_2, hw::CH_SCROLL_3, hw::CH_SCROLL_M
        };
        uint8_t scrollOk = 0;
        for (uint8_t i = 0; i < 4; i++) {
            chOk = pahub::selectChannel(Wire, scrollChannels[i]);
            if (!chOk) {
                Serial.printf("[Input] Scroll CH%d FAIL (channel select)\n",
                              scrollChannels[i]);
                continue;
            }
            if (_scrollDev.getDevStatus()) {
                _scrollAvail[i] = true;
                scrollOk++;
                Serial.printf("[Input] Scroll CH%d OK\n", scrollChannels[i]);
            } else {
                Serial.printf("[Input] Scroll CH%d FAIL\n", scrollChannels[i]);
            }
        }

        Serial.printf("[Input] Ready: EncA=%s EncB=%s Scrolls=%d/4\n",
                      aOk ? "OK" : "FAIL", bOk ? "OK" : "FAIL", scrollOk);

        // Step 5: Set initial LED colours per zone colour scheme
        setScrollLEDs();

        // Step 6: Disable all channels after init
        pahub::disableAll(Wire);

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
        if (_encAAvail && pahub::selectChannel(Wire, hw::CH_ENC_A)) {
            _encA.poll(ID_ENC_A_BASE, now);
            _encAErrors = 0;
        } else if (_encAAvail) {
            handleDeviceError(0);
        }

        // --- 8Encoder B (CH1) ---
        if (_encBAvail && pahub::selectChannel(Wire, hw::CH_ENC_B)) {
            _encB.poll(ID_ENC_B_BASE, now);
            _encBErrors = 0;
        } else if (_encBAvail) {
            handleDeviceError(1);
        }

        // --- 4 Scroll units (CH2-CH5) ---
        static constexpr uint8_t scrollChannels[4] = {
            hw::CH_SCROLL_1, hw::CH_SCROLL_2, hw::CH_SCROLL_3, hw::CH_SCROLL_M
        };
        static constexpr uint8_t scrollIds[4] = {
            ID_SCROLL_ZONE1, ID_SCROLL_ZONE2, ID_SCROLL_ZONE3, ID_SCROLL_MASTER
        };

        for (uint8_t i = 0; i < 4; i++) {
            if (_scrollAvail[i] && pahub::selectChannel(Wire, scrollChannels[i])) {
                _scroll[i].poll(scrollIds[i], now);
                _scrollErrors[i] = 0;
            } else if (_scrollAvail[i]) {
                handleDeviceError(2 + i);
            }
        }

        uint32_t elapsed = micros() - t0;
        _lastPollUs = elapsed;

        // Warn if poll cycle exceeds 50ms (only log once per second)
        if (elapsed > 50000 && (now - _lastWarnMs > 1000)) {
            _lastWarnMs = now;
            Serial.printf("[Input] WARN: poll took %lu us\n", elapsed);
        }
    }

    /// Set an LED on any input device. Guarded against uninitialised devices.
    void setInputLED(uint8_t inputId, uint8_t r, uint8_t g, uint8_t b) {
        if (inputId < 8) {
            if (!_encAAvail) return;
            pahub::selectChannel(Wire, hw::CH_ENC_A);
            _encA.setLED(inputId, r, g, b);
        } else if (inputId < 16) {
            if (!_encBAvail) return;
            pahub::selectChannel(Wire, hw::CH_ENC_B);
            _encB.setLED(inputId - 8, r, g, b);
        } else if (inputId >= 16 && inputId <= 19) {
            uint8_t idx = inputId - 16;
            if (!_scrollAvail[idx]) return;
            static constexpr uint8_t ch[4] = {
                hw::CH_SCROLL_1, hw::CH_SCROLL_2, hw::CH_SCROLL_3, hw::CH_SCROLL_M
            };
            pahub::selectChannel(Wire, ch[idx]);
            _scroll[idx].setLED(r, g, b);
        }
    }

    /// Set status LED (LED 8) on an 8Encoder unit. unit: 0=EncA, 1=EncB.
    void setStatusLED(uint8_t unit, uint8_t r, uint8_t g, uint8_t b) {
        if (unit == 0) {
            if (!_encAAvail) return;
            pahub::selectChannel(Wire, hw::CH_ENC_A);
            _encA.setLED(8, r, g, b);
        } else if (unit == 1) {
            if (!_encBAvail) return;
            pahub::selectChannel(Wire, hw::CH_ENC_B);
            _encB.setLED(8, r, g, b);
        }
    }

    uint32_t lastPollUs() const { return _lastPollUs; }

private:
    static constexpr uint8_t MAX_CONSECUTIVE_ERRORS = 10;

    void handleDeviceError(uint8_t deviceIdx) {
        // deviceIdx: 0=EncA, 1=EncB, 2-5=Scrolls
        uint8_t& errCount = (deviceIdx == 0) ? _encAErrors :
                            (deviceIdx == 1) ? _encBErrors :
                            _scrollErrors[deviceIdx - 2];

        errCount++;
        if (errCount >= MAX_CONSECUTIVE_ERRORS) {
            Serial.printf("[Input] Device %d: %d consecutive errors — attempting re-init\n",
                          deviceIdx, errCount);
            errCount = 0;
            reinitDevice(deviceIdx);
        }
    }

    void reinitDevice(uint8_t deviceIdx) {
        if (deviceIdx == 0) {
            pahub::selectChannel(Wire, hw::CH_ENC_A);
            _encAAvail = _encADev.begin();
            Serial.printf("[Input] EncA re-init: %s\n", _encAAvail ? "OK" : "FAIL");
        } else if (deviceIdx == 1) {
            pahub::selectChannel(Wire, hw::CH_ENC_B);
            _encBAvail = _encBDev.begin();
            Serial.printf("[Input] EncB re-init: %s\n", _encBAvail ? "OK" : "FAIL");
        } else {
            uint8_t scrollIdx = deviceIdx - 2;
            static constexpr uint8_t scrollChannels[4] = {
                hw::CH_SCROLL_1, hw::CH_SCROLL_2, hw::CH_SCROLL_3, hw::CH_SCROLL_M
            };
            pahub::selectChannel(Wire, scrollChannels[scrollIdx]);
            _scrollAvail[scrollIdx] = _scrollDev.getDevStatus();
            Serial.printf("[Input] Scroll %d re-init: %s\n",
                          scrollIdx, _scrollAvail[scrollIdx] ? "OK" : "FAIL");
        }
    }

    void setScrollLEDs() {
        static constexpr uint8_t scrollChannels[4] = {
            hw::CH_SCROLL_1, hw::CH_SCROLL_2, hw::CH_SCROLL_3, hw::CH_SCROLL_M
        };

        // Set zone colour LEDs on each Scroll unit (50% brightness)
        for (uint8_t i = 0; i < 3; i++) {
            pahub::selectChannel(Wire, scrollChannels[i]);
            uint8_t r = (hw::ZONE_COLOUR[i] >> 16) & 0xFF;
            uint8_t g = (hw::ZONE_COLOUR[i] >> 8) & 0xFF;
            uint8_t b = hw::ZONE_COLOUR[i] & 0xFF;
            _scroll[i].setLED(r >> 1, g >> 1, b >> 1);
        }
        // Master = white dim
        pahub::selectChannel(Wire, hw::CH_SCROLL_M);
        _scroll[3].setLED(64, 64, 64);
    }

    // Device instances (owned)
    M5UnitScroll _scrollDev;                          // Shared across 4 scroll channels
    M5ROTATE8    _encADev;                            // EncA at 0x42
    M5ROTATE8    _encBDev;                            // EncB at 0x41

    // Reader wrappers
    Rotate8Reader _encA;
    Rotate8Reader _encB;
    ScrollReader  _scroll[4];

    // Availability tracking
    bool _encAAvail = false;
    bool _encBAvail = false;
    bool _scrollAvail[4] = {};

    // Error counting for re-init
    uint8_t _encAErrors = 0;
    uint8_t _encBErrors = 0;
    uint8_t _scrollErrors[4] = {};

    // State
    ChangeCallback _callback = nullptr;
    uint32_t _lastPoll = 0;
    uint32_t _lastPollUs = 0;
    uint32_t _lastWarnMs = 0;
};
