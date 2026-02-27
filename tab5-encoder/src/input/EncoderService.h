#pragma once
// ============================================================================
// EncoderService - High-Level Encoder Interface for Tab5
// ============================================================================
// Encapsulates the complete encoder subsystem:
// - Transport layer (Rotate8Transport)
// - Debounce processing (DetentDebounce, ButtonDebounce)
// - Value management with wrap/clamp
// - Callback system for parameter changes
// - LED feedback for encoder activity
//
// This provides a clean API for main.cpp and future network integration.
//
// Usage:
//   EncoderService encoders(&Wire);
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

class EncoderService {
public:
    // Callback signature: (encoder_index, new_value, was_button_reset)
    using ChangeCallback = void (*)(uint8_t index, uint16_t value, bool wasReset);

    /**
     * Constructor
     * @param wire Pointer to TwoWire instance (e.g., &Wire after Wire.begin())
     * @param address M5ROTATE8 I2C address (default: 0x41)
     */
    EncoderService(TwoWire* wire, uint8_t address = 0x41);

    /**
     * Initialize the encoder service
     * @return true if M5ROTATE8 initialized successfully
     */
    bool begin();

    /**
     * Poll encoders and process changes
     * Call this in the main loop (recommended: every 5-20ms)
     */
    void update();

    // ========================================================================
    // Value Access
    // ========================================================================

    /**
     * Get current value for a parameter
     * @param param Parameter index (0-7)
     * @return Current parameter value
     */
    uint16_t getValue(uint8_t param) const;

    /**
     * Set parameter value externally (e.g., from WebSocket sync)
     * @param param Parameter index (0-7)
     * @param value New value (will be clamped/wrapped to valid range)
     * @param triggerCallback If true, invokes the change callback
     */
    void setValue(uint8_t param, uint16_t value, bool triggerCallback = false);

    /**
     * Get all current values
     * @param values Array to fill (must have 8 elements)
     */
    void getAllValues(uint16_t values[8]) const;

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
     * @param callback Function to call when any parameter changes
     */
    void setChangeCallback(ChangeCallback callback);

    // ========================================================================
    // Status
    // ========================================================================

    /**
     * Check if encoder hardware is available
     * @return true if M5ROTATE8 was successfully initialized
     */
    bool isAvailable() const;

    /**
     * Check if encoder hardware is currently responding
     * Performs an actual I2C transaction
     * @return true if device responds
     */
    bool isConnected();

    /**
     * Get firmware version from M5ROTATE8
     * @return Version number (typically 2 for V2 firmware)
     */
    uint8_t getVersion();

    // ========================================================================
    // LED Control
    // ========================================================================

    /**
     * Set status LED (LED 8) color
     * @param r Red component (0-255)
     * @param g Green component (0-255)
     * @param b Blue component (0-255)
     */
    void setStatusLed(uint8_t r, uint8_t g, uint8_t b);

    /**
     * Flash an encoder LED briefly
     * Used internally for feedback, but exposed for external use
     * @param channel Encoder channel (0-7)
     * @param r Red component (0-255)
     * @param g Green component (0-255)
     * @param b Blue component (0-255)
     */
    void flashLed(uint8_t channel, uint8_t r, uint8_t g, uint8_t b);

    /**
     * Turn off all LEDs
     */
    void allLedsOff();

    // ========================================================================
    // Direct Access (for advanced use)
    // ========================================================================

    /**
     * Get reference to underlying transport layer
     * Use with caution - bypasses service-level state management
     * @return Reference to Rotate8Transport
     */
    Rotate8Transport& transport() { return _transport; }

private:
    // Transport layer
    Rotate8Transport _transport;

    // Current parameter values
    uint16_t _values[8];

    // Processing state
    DetentDebounce _detentDebounce[8];
    ButtonDebounce _buttonDebounce[8];
    CallbackThrottle _callbackThrottle[8];

    // LED flash state
    struct LedFlash {
        uint32_t start_time = 0;
        bool active = false;
        static const uint32_t DURATION_MS = 100;
    };
    LedFlash _ledFlash[8];

    // Callback
    ChangeCallback _callback = nullptr;

    // Internal methods
    void processEncoderDelta(uint8_t channel, int32_t rawDelta, uint32_t now);
    void processButton(uint8_t channel, bool isPressed, uint32_t now);
    void updateLedFlash(uint32_t now);
    void invokeCallback(uint8_t channel, uint16_t value, bool wasReset);
};

// ============================================================================
// Inline Implementation
// ============================================================================

inline EncoderService::EncoderService(TwoWire* wire, uint8_t address)
    : _transport(wire, address)
{
    // Initialize values to defaults
    _values[0] = ParamDefault::EFFECT;
    _values[1] = ParamDefault::BRIGHTNESS;
    _values[2] = ParamDefault::PALETTE;
    _values[3] = ParamDefault::SPEED;
    _values[4] = ParamDefault::INTENSITY;
    _values[5] = ParamDefault::SATURATION;
    _values[6] = ParamDefault::COMPLEXITY;
    _values[7] = ParamDefault::VARIATION;
}

inline bool EncoderService::begin() {
    bool ok = _transport.begin();
    if (ok) {
        // Set status LED to dim green on successful init
        _transport.setLED(8, 0, 32, 0);
    }
    return ok;
}

inline void EncoderService::update() {
    if (!_transport.isAvailable()) return;

    uint32_t now = millis();

    // Poll all 8 encoders
    for (uint8_t i = 0; i < 8; i++) {
        // Read raw encoder delta
        int32_t rawDelta = _transport.getRelCounter(i);
        processEncoderDelta(i, rawDelta, now);

        // Check button state
        bool isPressed = _transport.getKeyPressed(i);
        processButton(i, isPressed, now);
    }

    // Update LED flash states
    updateLedFlash(now);
}

inline uint16_t EncoderService::getValue(uint8_t param) const {
    if (param >= 8) return 0;
    return _values[param];
}

inline void EncoderService::setValue(uint8_t param, uint16_t value, bool triggerCallback) {
    if (param >= 8) return;

    // Apply wrap/clamp to ensure valid range
    if (EncoderProcessing::shouldWrap(param)) {
        value = EncoderProcessing::wrapValue(param, value);
    } else {
        value = EncoderProcessing::clampValue(param, value);
    }

    _values[param] = value;

    if (triggerCallback) {
        invokeCallback(param, value, false);
    }
}

inline void EncoderService::getAllValues(uint16_t values[8]) const {
    for (uint8_t i = 0; i < 8; i++) {
        values[i] = _values[i];
    }
}

inline void EncoderService::resetToDefaults(bool triggerCallbacks) {
    _values[0] = ParamDefault::EFFECT;
    _values[1] = ParamDefault::BRIGHTNESS;
    _values[2] = ParamDefault::PALETTE;
    _values[3] = ParamDefault::SPEED;
    _values[4] = ParamDefault::INTENSITY;
    _values[5] = ParamDefault::SATURATION;
    _values[6] = ParamDefault::COMPLEXITY;
    _values[7] = ParamDefault::VARIATION;

    // Reset debounce states
    for (uint8_t i = 0; i < 8; i++) {
        _detentDebounce[i].reset();
        _buttonDebounce[i].reset();
        _callbackThrottle[i].reset();
    }

    if (triggerCallbacks && _callback) {
        for (uint8_t i = 0; i < 8; i++) {
            _callback(i, _values[i], true);
        }
    }
}

inline void EncoderService::setChangeCallback(ChangeCallback callback) {
    _callback = callback;
}

inline bool EncoderService::isAvailable() const {
    return _transport.isAvailable();
}

inline bool EncoderService::isConnected() {
    return _transport.isConnected();
}

inline uint8_t EncoderService::getVersion() {
    return _transport.getVersion();
}

inline void EncoderService::setStatusLed(uint8_t r, uint8_t g, uint8_t b) {
    _transport.setLED(8, r, g, b);
}

inline void EncoderService::flashLed(uint8_t channel, uint8_t r, uint8_t g, uint8_t b) {
    if (channel >= 8) return;
    _ledFlash[channel].start_time = millis();
    _ledFlash[channel].active = true;
    _transport.setLED(channel, r, g, b);
}

inline void EncoderService::allLedsOff() {
    _transport.allLEDsOff();
}

inline void EncoderService::processEncoderDelta(uint8_t channel, int32_t rawDelta, uint32_t now) {
    if (channel >= 8) return;

    // Process through DetentDebounce
    if (_detentDebounce[channel].processRawDelta(rawDelta, now)) {
        int32_t normalizedDelta = _detentDebounce[channel].consumeNormalisedDelta();

        if (normalizedDelta != 0) {
            // Apply delta with wrap/clamp
            uint16_t oldVal = _values[channel];
            _values[channel] = EncoderProcessing::applyDelta(channel, _values[channel], normalizedDelta);

            // Flash LED for activity feedback
            flashLed(channel, 0, 255, 0);  // Bright green

            // Invoke callback (with throttling)
            if (_callbackThrottle[channel].shouldFire(now)) {
                invokeCallback(channel, _values[channel], false);
            }
        }
    }
}

inline void EncoderService::processButton(uint8_t channel, bool isPressed, uint32_t now) {
    if (channel >= 8) return;

    if (_buttonDebounce[channel].processState(isPressed, now)) {
        // Debounced button press - reset to default
        Parameter param = static_cast<Parameter>(channel);
        uint16_t defaultVal = getParameterDefault(param);

        _values[channel] = defaultVal;

        // Reset debounce state
        _detentDebounce[channel].reset();

        // Force callback (resets always propagate)
        _callbackThrottle[channel].force(now);

        // Flash LED cyan for reset
        flashLed(channel, 0, 128, 255);

        // Invoke callback
        invokeCallback(channel, _values[channel], true);
    }
}

inline void EncoderService::updateLedFlash(uint32_t now) {
    for (uint8_t i = 0; i < 8; i++) {
        if (_ledFlash[i].active) {
            if (now - _ledFlash[i].start_time >= LedFlash::DURATION_MS) {
                _transport.setLED(i, 0, 0, 0);
                _ledFlash[i].active = false;
            }
        }
    }
}

inline void EncoderService::invokeCallback(uint8_t channel, uint16_t value, bool wasReset) {
    if (_callback) {
        _callback(channel, value, wasReset);
    }
}
