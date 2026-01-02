#pragma once

#include <Arduino.h>
#include "src/config/Config.h"

/**
 * @file EncoderProcessing.h
 * @brief Hardware-agnostic encoder processing logic
 * 
 * Processes raw encoder deltas and button states into parameter changes.
 * No I2C, GPIO, or hardware dependencies - pure processing logic.
 */

// Parameter configuration structure
struct ParamConfig {
    uint16_t min_value;
    uint16_t max_value;
    uint16_t default_value;
};

// Event callback type: void callback(uint8_t paramId, uint16_t value, bool isReset)
using EncoderEventCallback = void (*)(uint8_t paramId, uint16_t value, bool isReset);

class EncoderProcessing {
public:
    /**
     * @brief Constructor
     */
    EncoderProcessing();

    /**
     * @brief Initialise processing state
     */
    void begin();

    /**
     * @brief Process encoder delta for a channel
     * @param channel Encoder channel (0-7)
     * @param raw_delta Raw delta from transport layer
     * @param now_ms Current time in milliseconds
     * @return true if value changed and callback should be emitted
     */
    bool processDelta(uint8_t channel, int32_t raw_delta, uint32_t now_ms);

    /**
     * @brief Process button state change for a channel
     * @param channel Encoder channel (0-7)
     * @param raw_pressed Raw button state from transport layer
     * @param now_ms Current time in milliseconds
     * @return true if button press detected (reset event)
     */
    bool processButton(uint8_t channel, bool raw_pressed, uint32_t now_ms);

    /**
     * @brief Get current parameter value
     * @param param Parameter index (0-7)
     * @return Current value
     */
    uint16_t getValue(uint8_t param) const;

    /**
     * @brief Set parameter value externally (e.g., from WebSocket)
     * @param param Parameter index (0-7)
     * @param value New value (will be clamped)
     * @param trigger_callback If true, triggers callback
     */
    void setValue(uint8_t param, uint16_t value, bool trigger_callback = false);

    /**
     * @brief Register callback for parameter changes
     * @param callback Function to call when parameter changes
     */
    void setCallback(EncoderEventCallback callback);

    /**
     * @brief Reset all parameters to defaults
     */
    void resetToDefaults();

private:
    // Detent-aware debouncing
    struct DetentDebounce {
        int32_t pending_count = 0;
        uint32_t last_emit_time = 0;
        bool expecting_pair = false;

        bool processRawDelta(int32_t raw_delta, uint32_t now_ms);
        int32_t consumeNormalisedDelta();
    };

    // Parameter configurations
    static const ParamConfig PARAM_CONFIGS[8];

    // State tracking
    uint16_t _values[8];
    DetentDebounce _detentDebounce[8];
    
    // Button debounce state
    bool _button_stable_state[8];
    uint32_t _button_state_change_time[8];
    bool _last_button_pressed[8];
    
    // Callback throttling
    uint32_t _last_callback_time[8];
    
    // Direction-flip damping
    int8_t _last_direction[8];
    uint32_t _last_direction_change_time[8];
    
    // Timing constants
    static const uint32_t DETENT_DEBOUNCE_INTERVAL_MS = 60;
    static const uint32_t BUTTON_DEBOUNCE_MS = 40;
    static const uint32_t CALLBACK_THROTTLE_MS = 35;
    static const uint32_t DIRECTION_FLIP_WINDOW_MS = 40;
    
    // Callback
    EncoderEventCallback _callback;

    /**
     * @brief Clamp value to parameter's valid range
     * @param param Parameter index
     * @param value Value to clamp
     * @return Clamped value
     */
    uint16_t clampValue(uint8_t param, int32_t value) const;

    /**
     * @brief Process normalised delta into parameter change
     * @param channel Encoder channel (0-7)
     * @param delta Normalised delta (±1)
     */
    void applyDelta(uint8_t channel, int32_t delta);
};

