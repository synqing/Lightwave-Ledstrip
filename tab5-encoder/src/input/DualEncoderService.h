// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once
// ============================================================================
// DualEncoderService - Unified 16-Encoder Interface for Tab5
// ============================================================================
// Manages TWO M5ROTATE8 encoder units on the SAME I2C bus with different
// addresses, providing a clean 16-parameter API (indices 0-15).
//
// Hardware Layout:
//   Unit A (0x42): Encoders 0-7,  LEDs 0-7,  Status LED 8
//   Unit B (0x41): Encoders 8-15, LEDs 8-15, Status LED 8 (on Unit B)
//
// I2C Address Configuration:
//   Unit A must be reprogrammed to 0x42 using register 0xFF
//   Unit B uses factory default 0x41
//   Both units connect to Grove Port.A (GPIO 53/54)
//
// Usage:
//   Wire.begin(53, 54, 100000);  // Grove Port.A
//   DualEncoderService encoders(&Wire, 0x42, 0x41);  // Different addresses
//   encoders.setChangeCallback([](uint8_t idx, uint16_t val, bool reset) { ... });
//   encoders.begin();
//   // In loop:
//   encoders.update();
//
// CRITICAL SAFETY NOTE:
// This service contains NO I2C recovery logic. Tab5's shared I2C bus
// architecture means aggressive recovery patterns are forbidden.
// ============================================================================

#include <Arduino.h>
#include <Wire.h>
#include "Rotate8Transport.h"
#include "EncoderProcessing.h"
#include "../config/Config.h"

// Forward declaration (for class members)
class ButtonHandler;
class CoarseModeManager;

class DualEncoderService {
public:
    // Callback signature: (encoder_index 0-15, new_value, was_button_reset)
    using ChangeCallback = void (*)(uint8_t index, uint16_t value, bool wasReset);

    // Number of encoders per unit and total
    static constexpr uint8_t ENCODERS_PER_UNIT = 8;
    static constexpr uint8_t TOTAL_ENCODERS = 16;

    /**
     * Constructor for same-bus dual-address operation
     * @param wire Pointer to TwoWire instance (both units share same bus)
     * @param addressA I2C address for Unit A (0-7), default 0x42 (reprogrammed)
     * @param addressB I2C address for Unit B (8-15), default 0x41 (factory)
     */
    DualEncoderService(TwoWire* wire, uint8_t addressA = 0x42, uint8_t addressB = 0x41);

    /**
     * Initialize both encoder units
     * @return true if at least one unit initialized successfully
     */
    bool begin();

    /**
     * Poll all 16 encoders and process changes
     * Call this in the main loop (recommended: every 5-20ms)
     */
    void update();

    // ========================================================================
    // Value Access (indices 0-15)
    // ========================================================================

    /**
     * Get current value for a parameter
     * @param param Parameter index (0-15)
     * @return Current parameter value
     */
    uint16_t getValue(uint8_t param) const;

    /**
     * Set parameter value externally (e.g., from WebSocket sync)
     * @param param Parameter index (0-15)
     * @param value New value (will be clamped/wrapped to valid range)
     * @param triggerCallback If true, invokes the change callback
     */
    void setValue(uint8_t param, uint16_t value, bool triggerCallback = false);

    /**
     * Get all current values
     * @param values Array to fill (must have 16 elements)
     */
    void getAllValues(uint16_t values[TOTAL_ENCODERS]) const;

    /**
     * Reset all parameters to their default values
     * @param triggerCallbacks If true, invokes callback for each parameter
     */
    void resetToDefaults(bool triggerCallbacks = true);

    // ========================================================================
    // Callback Registration
    // ========================================================================

    /**
     * Register callback for parameter changes
     * Single unified callback for all 16 encoders
     * @param callback Function to call when any parameter changes
     */
    void setChangeCallback(ChangeCallback callback);

    /**
     * Set button handler for special button behaviors
     * @param handler ButtonHandler instance (can be nullptr to disable)
     */
    void setButtonHandler(ButtonHandler* handler) { _buttonHandler = handler; }

    /**
     * Set coarse mode manager for ENC-A acceleration
     * @param manager CoarseModeManager instance (can be nullptr to disable)
     */
    void setCoarseModeManager(CoarseModeManager* manager) { _coarseModeManager = manager; }

    // ========================================================================
    // Status
    // ========================================================================

    /**
     * Check if Unit A (encoders 0-7) is available
     * @return true if Unit A was successfully initialized
     */
    bool isUnitAAvailable() const;

    /**
     * Check if Unit B (encoders 8-15) is available
     * @return true if Unit B was successfully initialized
     */
    bool isUnitBAvailable() const;

    /**
     * Check if any unit is available
     * @return true if at least one unit is available
     */
    bool isAnyAvailable() const;

    /**
     * Check if both units are available
     * @return true if both units are available
     */
    bool areBothAvailable() const;

    // ========================================================================
    // LED Control
    // ========================================================================

    /**
     * Flash an encoder LED briefly
     * @param index Encoder index (0-15): 0-7 = Unit A, 8-15 = Unit B
     * @param r Red component (0-255)
     * @param g Green component (0-255)
     * @param b Blue component (0-255)
     */
    void flashLed(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

    /**
     * Set status LED color for a unit
     * Each unit has LED 8 as its status LED
     * @param unit Unit index (0 = Unit A, 1 = Unit B)
     * @param r Red component (0-255)
     * @param g Green component (0-255)
     * @param b Blue component (0-255)
     */
    void setStatusLed(uint8_t unit, uint8_t r, uint8_t g, uint8_t b);

    /**
     * Turn off all LEDs on both units
     */
    void allLedsOff();

    // ========================================================================
    // Direct Access (for advanced use)
    // ========================================================================

    /**
     * Get reference to Unit A transport layer
     * Use with caution - bypasses service-level state management
     * @return Reference to Rotate8Transport for Unit A
     */
    Rotate8Transport& transportA() { return _transportA; }

    /**
     * Get reference to Unit B transport layer
     * Use with caution - bypasses service-level state management
     * @return Reference to Rotate8Transport for Unit B
     */
    Rotate8Transport& transportB() { return _transportB; }

private:
    // Transport layers for both units
    Rotate8Transport _transportA;
    Rotate8Transport _transportB;

    // Unified value storage (0-15)
    uint16_t _values[TOTAL_ENCODERS];

    // Processing state for all 16 encoders
    DetentDebounce _detentDebounce[TOTAL_ENCODERS];
    ButtonDebounce _buttonDebounce[TOTAL_ENCODERS];
    CallbackThrottle _callbackThrottle[TOTAL_ENCODERS];

    // LED flash state
    struct LedFlash {
        uint32_t start_time = 0;
        bool active = false;
        static const uint32_t DURATION_MS = 100;
    };
    LedFlash _ledFlash[TOTAL_ENCODERS];

    // Unified callback
    ChangeCallback _callback = nullptr;

    // Button handler for special behaviors (zone mode, speed/palette toggle)
    ButtonHandler* _buttonHandler = nullptr;

    // Coarse mode manager for ENC-A acceleration (encoders 0-7)
    CoarseModeManager* _coarseModeManager = nullptr;

    // ========================================================================
    // Internal Methods
    // ========================================================================

    /**
     * Process encoder delta for a global index
     * @param globalIdx Global encoder index (0-15)
     * @param rawDelta Raw delta from M5ROTATE8
     * @param now Current time in milliseconds
     */
    void processEncoderDelta(uint8_t globalIdx, int32_t rawDelta, uint32_t now);

    /**
     * Process button state for a global index
     * @param globalIdx Global encoder index (0-15)
     * @param isPressed Current button state
     * @param now Current time in milliseconds
     */
    void processButton(uint8_t globalIdx, bool isPressed, uint32_t now);

    /**
     * Update LED flash states (turn off LEDs after flash duration)
     * @param now Current time in milliseconds
     */
    void updateLedFlash(uint32_t now);

    /**
     * Invoke the change callback if registered
     * @param globalIdx Global encoder index (0-15)
     * @param value Current value
     * @param wasReset true if this was a button reset
     */
    void invokeCallback(uint8_t globalIdx, uint16_t value, bool wasReset);

    // ========================================================================
    // Helper Methods
    // ========================================================================

    /**
     * Get the transport layer for a global index
     * @param globalIdx Global encoder index (0-15)
     * @return Reference to appropriate Rotate8Transport (A for 0-7, B for 8-15)
     */
    Rotate8Transport& getTransport(uint8_t globalIdx);

    /**
     * Convert global index to local index within a unit
     * @param globalIdx Global encoder index (0-15)
     * @return Local index (0-7), maps 8-15 to 0-7
     */
    uint8_t toLocalIdx(uint8_t globalIdx) const;

    /**
     * Check if global index belongs to Unit A
     * @param globalIdx Global encoder index (0-15)
     * @return true if index is 0-7 (Unit A)
     */
    bool isUnitA(uint8_t globalIdx) const;

    /**
     * Get default value for a global parameter index
     * @param globalIdx Global encoder index (0-15)
     * @return Default value for the parameter
     */
    uint16_t getDefaultValue(uint8_t globalIdx) const;

    /**
     * Apply wrap/clamp behavior for a global index
     * @param globalIdx Global encoder index (0-15)
     * @param value Value to process
     * @return Processed value with wrap/clamp applied
     */
    uint16_t applyRangeConstraint(uint8_t globalIdx, int32_t value) const;

    /**
     * Check if a global index should wrap (vs clamp)
     * @param globalIdx Global encoder index (0-15)
     * @return true if parameter should wrap
     */
    bool shouldWrapGlobal(uint8_t globalIdx) const;
};

// ============================================================================
// Inline Implementation
// ============================================================================

inline DualEncoderService::DualEncoderService(TwoWire* wire, uint8_t addressA, uint8_t addressB)
    : _transportA(wire, addressA)  // Same bus, address 0x42 (reprogrammed)
    , _transportB(wire, addressB)  // Same bus, address 0x41 (factory)
{
    // Initialize values to defaults
    // Unit A (indices 0-7): Global parameters
    // CRITICAL: Order must match Parameter enum and PARAMETER_TABLE!
    // Index 0=Effect, 1=Palette, 2=Speed, 3=Mood, 4=FadeAmount, 5=Complexity, 6=Variation, 7=Brightness
    _values[0] = ParamDefault::EFFECT;       // Index 0 = Effect (0)
    _values[1] = ParamDefault::PALETTE;      // Index 1 = Palette (0, wraps 0-74)
    _values[2] = ParamDefault::SPEED;        // Index 2 = Speed (25)
    _values[3] = ParamDefault::MOOD;         // Index 3 = Mood (0)
    _values[4] = ParamDefault::FADEAMOUNT;   // Index 4 = FadeAmount (0)
    _values[5] = ParamDefault::COMPLEXITY;   // Index 5 = Complexity (128)
    _values[6] = ParamDefault::VARIATION;    // Index 6 = Variation (0)
    _values[7] = ParamDefault::BRIGHTNESS;   // Index 7 = Brightness (128)

    // Unit B (indices 8-15): Zone parameters
    _values[8] = ParamDefault::ZONE0_EFFECT;
    _values[9] = ParamDefault::ZONE0_SPEED;
    _values[10] = ParamDefault::ZONE1_EFFECT;
    _values[11] = ParamDefault::ZONE1_SPEED;
    _values[12] = ParamDefault::ZONE2_EFFECT;
    _values[13] = ParamDefault::ZONE2_SPEED;
    _values[14] = ParamDefault::ZONE3_EFFECT;
    _values[15] = ParamDefault::ZONE3_SPEED;
}

inline bool DualEncoderService::begin() {
    bool unitAOk = _transportA.begin();
    bool unitBOk = _transportB.begin();

    // Set status LEDs to indicate unit availability
    if (unitAOk) {
        // Unit A status LED: dim green
        // #region agent log (DISABLED)
        // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"boot\",\"hypothesisId\":\"H1\",\"location\":\"DualEncoderService.h:begin\",\"message\":\"statusLed.unitA.set\",\"data\":{\"channel\":8,\"r\":0,\"g\":32,\"b\":0},\"timestamp\":%lu}\n",
                      // static_cast<unsigned long>(millis()));
                // #endregion
        _transportA.setLED(8, 0, 32, 0);
    }
    if (unitBOk) {
        // Unit B status LED: dim blue (to differentiate from Unit A)
        // #region agent log (DISABLED)
        // Serial.printf("{\"sessionId\":\"debug-session\",\"runId\":\"boot\",\"hypothesisId\":\"H1\",\"location\":\"DualEncoderService.h:begin\",\"message\":\"statusLed.unitB.set\",\"data\":{\"channel\":8,\"r\":0,\"g\":0,\"b\":32},\"timestamp\":%lu}\n",
                      // static_cast<unsigned long>(millis()));
                // #endregion
        _transportB.setLED(8, 0, 0, 32);
    }

    // Return true if at least one unit is available (graceful degradation)
    return unitAOk || unitBOk;
}

inline void DualEncoderService::update() {
    uint32_t now = millis();

    // Poll Unit A encoders (indices 0-7)
    if (_transportA.isAvailable()) {
        const uint32_t _dbg_t0 = millis();
        for (uint8_t localIdx = 0; localIdx < ENCODERS_PER_UNIT; localIdx++) {
            uint8_t globalIdx = localIdx;  // 0-7

            // Read raw encoder delta
            int32_t rawDelta = _transportA.getRelCounter(localIdx);
            processEncoderDelta(globalIdx, rawDelta, now);

            // Check button state
            bool isPressed = _transportA.getKeyPressed(localIdx);
            processButton(globalIdx, isPressed, now);
        }
        const uint32_t _dbg_dt = millis() - _dbg_t0;
        // #region agent log (DISABLED)
        static uint32_t s_lastI2CSlowMs = 0;
        const uint32_t _dbg_now = millis();
        if (false) {
            if (_dbg_dt > 250 && (_dbg_now - s_lastI2CSlowMs) > 500) {
                s_lastI2CSlowMs = _dbg_now;
                Serial.printf(
                    "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1\",\"location\":\"Tab5.encoder/src/input/DualEncoderService.h:update\",\"message\":\"i2c.poll.slow\",\"data\":{\"unit\":\"A\",\"dtMs\":%lu,\"timeoutMs\":%u},\"timestamp\":%lu}\n",
                    (unsigned long)_dbg_dt,
                    (unsigned)I2C::TIMEOUT_MS,
                    (unsigned long)_dbg_now
                );
            }
        }
        // #endregion
    }

    // Poll Unit B encoders (indices 8-15)
    if (_transportB.isAvailable()) {
        const uint32_t _dbg_t0 = millis();
        for (uint8_t localIdx = 0; localIdx < ENCODERS_PER_UNIT; localIdx++) {
            uint8_t globalIdx = localIdx + ENCODERS_PER_UNIT;  // 8-15

            // Read raw encoder delta
            int32_t rawDelta = _transportB.getRelCounter(localIdx);
            processEncoderDelta(globalIdx, rawDelta, now);

            // Check button state
            bool isPressed = _transportB.getKeyPressed(localIdx);
            processButton(globalIdx, isPressed, now);
        }
        const uint32_t _dbg_dt = millis() - _dbg_t0;
        // #region agent log (DISABLED)
        static uint32_t s_lastI2CSlowMs = 0;
        const uint32_t _dbg_now = millis();
        if (false) {
            if (_dbg_dt > 250 && (_dbg_now - s_lastI2CSlowMs) > 500) {
                s_lastI2CSlowMs = _dbg_now;
                Serial.printf(
                    "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"H1\",\"location\":\"Tab5.encoder/src/input/DualEncoderService.h:update\",\"message\":\"i2c.poll.slow\",\"data\":{\"unit\":\"B\",\"dtMs\":%lu,\"timeoutMs\":%u},\"timestamp\":%lu}\n",
                    (unsigned long)_dbg_dt,
                    (unsigned)I2C::TIMEOUT_MS,
                    (unsigned long)_dbg_now
                );
            }
        }
        // #endregion
    }

    // Update LED flash states for all 16 encoders
    updateLedFlash(now);
}

inline uint16_t DualEncoderService::getValue(uint8_t param) const {
    if (param >= TOTAL_ENCODERS) return 0;
    return _values[param];
}

inline void DualEncoderService::setValue(uint8_t param, uint16_t value, bool triggerCallback) {
    if (param >= TOTAL_ENCODERS) return;

    // Apply range constraint
    _values[param] = applyRangeConstraint(param, static_cast<int32_t>(value));

    if (triggerCallback) {
        invokeCallback(param, _values[param], false);
    }
}

inline void DualEncoderService::getAllValues(uint16_t values[TOTAL_ENCODERS]) const {
    for (uint8_t i = 0; i < TOTAL_ENCODERS; i++) {
        values[i] = _values[i];
    }
}

inline void DualEncoderService::resetToDefaults(bool triggerCallbacks) {
    // Reset Unit A parameters (0-7)
    // CRITICAL: Order must match Parameter enum and PARAMETER_TABLE!
    // Index 0=Effect, 1=Palette, 2=Speed, 3=Mood, 4=FadeAmount, 5=Complexity, 6=Variation, 7=Brightness
    _values[0] = ParamDefault::EFFECT;       // Index 0 = Effect (0)
    _values[1] = ParamDefault::PALETTE;      // Index 1 = Palette (0, wraps 0-74)
    _values[2] = ParamDefault::SPEED;        // Index 2 = Speed (25)
    _values[3] = ParamDefault::MOOD;         // Index 3 = Mood (0)
    _values[4] = ParamDefault::FADEAMOUNT;   // Index 4 = FadeAmount (0)
    _values[5] = ParamDefault::COMPLEXITY;   // Index 5 = Complexity (128)
    _values[6] = ParamDefault::VARIATION;    // Index 6 = Variation (0)
    _values[7] = ParamDefault::BRIGHTNESS;   // Index 7 = Brightness (128)

    // Reset Unit B parameters (8-15) to zone defaults
    _values[8] = ParamDefault::ZONE0_EFFECT;
    _values[9] = ParamDefault::ZONE0_SPEED;
    _values[10] = ParamDefault::ZONE1_EFFECT;
    _values[11] = ParamDefault::ZONE1_SPEED;
    _values[12] = ParamDefault::ZONE2_EFFECT;
    _values[13] = ParamDefault::ZONE2_SPEED;
    _values[14] = ParamDefault::ZONE3_EFFECT;
    _values[15] = ParamDefault::ZONE3_SPEED;

    // Reset all processing states
    for (uint8_t i = 0; i < TOTAL_ENCODERS; i++) {
        _detentDebounce[i].reset();
        _buttonDebounce[i].reset();
        _callbackThrottle[i].reset();
    }

    if (triggerCallbacks && _callback) {
        for (uint8_t i = 0; i < TOTAL_ENCODERS; i++) {
            _callback(i, _values[i], true);
        }
    }
}

inline void DualEncoderService::setChangeCallback(ChangeCallback callback) {
    _callback = callback;
}

inline bool DualEncoderService::isUnitAAvailable() const {
    return _transportA.isAvailable();
}

inline bool DualEncoderService::isUnitBAvailable() const {
    return _transportB.isAvailable();
}

inline bool DualEncoderService::isAnyAvailable() const {
    return _transportA.isAvailable() || _transportB.isAvailable();
}

inline bool DualEncoderService::areBothAvailable() const {
    return _transportA.isAvailable() && _transportB.isAvailable();
}

inline void DualEncoderService::flashLed(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index >= TOTAL_ENCODERS) return;

    _ledFlash[index].start_time = millis();
    _ledFlash[index].active = true;

    uint8_t localIdx = toLocalIdx(index);
    if (isUnitA(index)) {
        _transportA.setLED(localIdx, r, g, b);
    } else {
        _transportB.setLED(localIdx, r, g, b);
    }
}

inline void DualEncoderService::setStatusLed(uint8_t unit, uint8_t r, uint8_t g, uint8_t b) {
    if (unit == 0) {
        _transportA.setLED(8, r, g, b);
    } else if (unit == 1) {
        _transportB.setLED(8, r, g, b);
    }
}

inline void DualEncoderService::allLedsOff() {
    _transportA.allLEDsOff();
    _transportB.allLEDsOff();

    // Clear all flash states
    for (uint8_t i = 0; i < TOTAL_ENCODERS; i++) {
        _ledFlash[i].active = false;
    }
}

// Include CoarseModeManager before inline method that uses it
#include "CoarseModeManager.h"

inline void DualEncoderService::processEncoderDelta(uint8_t globalIdx, int32_t rawDelta, uint32_t now) {
    if (globalIdx >= TOTAL_ENCODERS) return;

    // Process through DetentDebounce
    if (_detentDebounce[globalIdx].processRawDelta(rawDelta, now)) {
        int32_t normalizedDelta = _detentDebounce[globalIdx].consumeNormalisedDelta();

        if (normalizedDelta != 0) {
            // Apply coarse mode multiplier if enabled (ENC-A only, indices 0-7)
            if (_coarseModeManager && globalIdx < 8) {
                normalizedDelta = _coarseModeManager->applyCoarseMode(globalIdx, normalizedDelta, now);
            }

            // Apply delta with wrap/clamp
            uint16_t oldValue = _values[globalIdx];
            int32_t newValue = static_cast<int32_t>(_values[globalIdx]) + normalizedDelta;
            _values[globalIdx] = applyRangeConstraint(globalIdx, newValue);

            // #region agent log (DISABLED)
            // Serial.printf("[DEBUG] {\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"WRAP2\",\"location\":\"DualEncoderService.h:503\",\"message\":\"processEncoderDelta\",\"data\":{\"globalIdx\":%d,\"oldValue\":%d,\"normalizedDelta\":%ld,\"newValueBeforeConstraint\":%ld,\"newValueAfterConstraint\":%d,\"shouldWrap\":%d},\"timestamp\":%lu}\n", globalIdx, oldValue, (long)normalizedDelta, (long)newValue, _values[globalIdx], shouldWrapGlobal(globalIdx) ? 1 : 0, (unsigned long)now);
                        // #endregion

            // Flash LED for activity feedback (bright green)
            flashLed(globalIdx, 0, 255, 0);

            // Invoke callback (with throttling)
            if (_callbackThrottle[globalIdx].shouldFire(now)) {
                invokeCallback(globalIdx, _values[globalIdx], false);
            }
        }
    }
}

// Include ButtonHandler before inline method that uses it
#include "ButtonHandler.h"

inline void DualEncoderService::processButton(uint8_t globalIdx, bool isPressed, uint32_t now) {
    if (globalIdx >= TOTAL_ENCODERS) return;

    if (_buttonDebounce[globalIdx].processState(isPressed, now)) {
        // Check if ButtonHandler wants to handle this button
        bool handled = false;
        if (_buttonHandler) {
            handled = _buttonHandler->handleButtonPress(globalIdx);
        }

        if (!handled) {
            // Default behavior: reset to default
            uint16_t defaultVal = getDefaultValue(globalIdx);
            _values[globalIdx] = defaultVal;

            // Reset debounce state
            _detentDebounce[globalIdx].reset();

            // Force callback (resets always propagate)
            _callbackThrottle[globalIdx].force(now);

            // Flash LED cyan for reset
            flashLed(globalIdx, 0, 128, 255);

            // Invoke callback
            invokeCallback(globalIdx, _values[globalIdx], true);
        } else {
            // Button was handled by ButtonHandler - flash LED green to indicate special action
            flashLed(globalIdx, 0, 255, 0);
        }
    }
}

inline void DualEncoderService::updateLedFlash(uint32_t now) {
    for (uint8_t i = 0; i < TOTAL_ENCODERS; i++) {
        if (_ledFlash[i].active) {
            if (now - _ledFlash[i].start_time >= LedFlash::DURATION_MS) {
                // Turn off the LED
                uint8_t localIdx = toLocalIdx(i);
                if (isUnitA(i)) {
                    _transportA.setLED(localIdx, 0, 0, 0);
                } else {
                    _transportB.setLED(localIdx, 0, 0, 0);
                }
                _ledFlash[i].active = false;
            }
        }
    }
}

inline void DualEncoderService::invokeCallback(uint8_t globalIdx, uint16_t value, bool wasReset) {
    if (_callback) {
        _callback(globalIdx, value, wasReset);
    }
}

inline Rotate8Transport& DualEncoderService::getTransport(uint8_t globalIdx) {
    if (globalIdx < ENCODERS_PER_UNIT) {
        return _transportA;
    }
    return _transportB;
}

inline uint8_t DualEncoderService::toLocalIdx(uint8_t globalIdx) const {
    if (globalIdx < ENCODERS_PER_UNIT) {
        return globalIdx;
    }
    return globalIdx - ENCODERS_PER_UNIT;  // Maps 8-15 to 0-7
}

inline bool DualEncoderService::isUnitA(uint8_t globalIdx) const {
    return globalIdx < ENCODERS_PER_UNIT;
}

inline uint16_t DualEncoderService::getDefaultValue(uint8_t globalIdx) const {
    if (globalIdx < ENCODERS_PER_UNIT) {
        // Unit A: Use defined parameter defaults
        Parameter param = static_cast<Parameter>(globalIdx);
        return getParameterDefault(param);
    }
    // Unit B: Placeholder default
    return 128;
}

inline uint16_t DualEncoderService::applyRangeConstraint(uint8_t globalIdx, int32_t value) const {
    if (globalIdx < ENCODERS_PER_UNIT) {
        // Unit A: Use EncoderProcessing utilities
        if (shouldWrapGlobal(globalIdx)) {
            return EncoderProcessing::wrapValue(globalIdx, value);
        } else {
            return EncoderProcessing::clampValue(globalIdx, value);
        }
    }

    // Unit B: Simple clamp to 0-255 (placeholder range)
    if (value < 0) return 0;
    if (value > 255) return 255;
    return static_cast<uint16_t>(value);
}

inline bool DualEncoderService::shouldWrapGlobal(uint8_t globalIdx) const {
    if (globalIdx < ENCODERS_PER_UNIT) {
        // Unit A: Check wrap behavior via EncoderProcessing
        return EncoderProcessing::shouldWrap(globalIdx);
    }
    // Unit B: No wrap (clamp only)
    return false;
}
