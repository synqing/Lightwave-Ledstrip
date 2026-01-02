#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <M5ROTATE8.h>

/**
 * Controller for M5 8ROTATE unit managing 8 LED strip parameters
 *
 * Hardware: M5 8ROTATE unit at I2C address 0x41
 * I2C Pins: SDA=2, SCL=1
 *
 * Features:
 * - Efficient polling using encoder change mask
 * - Hardware quirk compensation (divides encoder delta by 2)
 * - Per-encoder parameter tracking with min/max clamping
 * - Button press detection with default value reset
 * - Callback notification system for parameter changes
 * - External value setting for WebSocket synchronization
 */

class EncoderController {
public:
    // Parameter indices matching encoder positions
    enum Parameter : uint8_t {
        EFFECT = 0,      // Effect selection (0-95)
        BRIGHTNESS = 1,  // LED brightness (0-255)
        PALETTE = 2,     // Color palette (0-63)
        SPEED = 3,       // Animation speed (1-100)
        INTENSITY = 4,   // Effect intensity (0-255)
        SATURATION = 5,  // Color saturation (0-255)
        COMPLEXITY = 6,  // Effect complexity (0-255)
        VARIATION = 7    // Effect variation (0-255)
    };

    // Callback type: void callback(uint8_t encoder_index, uint16_t new_value, bool was_button_reset)
    using ChangeCallback = void (*)(uint8_t encoder_index, uint16_t new_value, bool was_button_reset);

    /**
     * Constructor
     * @param sda_pin I2C SDA pin (default: 2)
     * @param scl_pin I2C SCL pin (default: 1)
     * @param i2c_address M5ROTATE8 I2C address (default: 0x41)
     */
    EncoderController(int sda_pin = 2, int scl_pin = 1, uint8_t i2c_address = 0x41);

    /**
     * Update the I2C pins used for recovery re-initialisation.
     * Note: This does NOT call Wire.begin(). main.cpp owns bus init; this just stores pins for recovery.
     */
    void setI2CPins(int sda_pin, int scl_pin);

    /**
     * Initialize the encoder controller
     * @return true if initialization successful, false otherwise
     */
    bool begin();

    /**
     * Poll encoders for changes and update parameter values
     * Call this in the main loop (e.g., every 10-50ms)
     * Uses change mask for efficiency - only reads changed encoders
     */
    void update();

    /**
     * Get current value for a parameter
     * @param param Parameter to read
     * @return Current parameter value
     */
    uint16_t getValue(Parameter param) const;

    /**
     * Set parameter value externally (e.g., from WebSocket)
     * @param param Parameter to set
     * @param value New value (will be clamped to valid range)
     * @param trigger_callback If true, triggers change callback
     */
    void setValue(Parameter param, uint16_t value, bool trigger_callback = false);

    /**
     * Register callback for parameter changes
     * @param callback Function to call when any parameter changes
     */
    void setChangeCallback(ChangeCallback callback);

    /**
     * Check if encoder hardware is connected
     * @return true if M5ROTATE8 responds on I2C bus
     */
    bool isConnected();

    /**
     * Get reference to underlying M5ROTATE8 for LED control
     * @return Reference to M5ROTATE8 instance
     */
    M5ROTATE8& getEncoder() { return _encoder; }

    /**
     * Reset all encoders to their default values
     */
    void resetToDefaults();

private:
    // ------------------------------------------------------------------------
    // Detent-aware debouncing (ported from Lightwave-Ledstrip v1 EncoderManager)
    //
    // M5ROTATE8 often reports step sizes of 2 per detent, but can also emit 1s
    // (half detent) depending on polling timing. This debouncer normalises to
    // clean ±1 detent steps and applies a minimum emit interval per encoder.
    // ------------------------------------------------------------------------
    struct DetentDebounce {
        int32_t pending_count = 0;
        uint32_t last_emit_time = 0;
        bool expecting_pair = false;

        bool processRawDelta(int32_t raw_delta, uint32_t now_ms);
        int32_t consumeNormalisedDelta();
    };

    // Parameter configuration
    struct ParamConfig {
        uint16_t min_value;
        uint16_t max_value;
        uint16_t default_value;
    };

    static const ParamConfig PARAM_CONFIGS[8];

    // Hardware
    M5ROTATE8 _encoder;
    int _sda_pin;
    int _scl_pin;

    // State tracking
    uint16_t _values[8];           // Current parameter values
    bool _last_button_pressed[8];  // For edge-detect without per-channel I2C reads (V2 mask)

    // LightwaveOS-style reliability state
    bool _available = false;
    bool _error_state = false;
    uint8_t _error_count = 0;
    uint32_t _next_recovery_ms = 0;

    // Non-blocking recovery state machine (keeps main loop responsive so WS stays serviced)
    enum class RecoveryStage : uint8_t {
        Idle = 0,
        HwWireEnd1,
        HwWireEnd2,
        HwPinsRelease,
        HwDeinit,
        HwPeriphReset,
        HwWaitAfterReset,
        PrepWireEnd1,
        PrepWireEnd2,
        PrepPinsRelease,
        PrepBusClear,
        PrepWaitAfterBusClear,
        PrepWireBegin,
        PrepWaitAfterWireBegin,
        WakeGeneralCall,
        WakeDeviceCall,
        Flush,
        TryBegin,
    };

    RecoveryStage _recoveryStage = RecoveryStage::Idle;
    uint32_t _recovery_next_step_ms = 0;
    uint8_t _recovery_freq_idx = 0;     // 0=100kHz, 1=50kHz
    uint8_t _recovery_wake_iter = 0;    // 0..2
    bool _recovery_hw_reset = false;

    int32_t _accumulated_values[8] = {0};
    uint32_t _last_encoder_change_time[8] = {0};
    uint8_t _last_active_encoder_id = 255;

    // Detent-normalisation: residual accumulator for step-size 2/1 quirk
    int32_t _raw_residual[8] = {0};  // Accumulates fractional steps

    // Per-channel detent debounce state (v1-style)
    DetentDebounce _detentDebounce[8];
    static const uint32_t DETENT_DEBOUNCE_INTERVAL_MS = 60;  // Minimum between emitted detents

    // Direction-flip damping (balanced profile)
    int8_t _last_direction[8] = {0};  // -1, 0, or +1
    uint32_t _last_direction_change_time[8] = {0};
    static const uint32_t DIRECTION_FLIP_WINDOW_MS = 40;  // Balanced: 40ms

    // Per-parameter callback throttle (reduce WS spam)
    uint32_t _last_callback_time[8] = {0};
    static const uint32_t CALLBACK_THROTTLE_MS = 35;  // Balanced: 35ms per parameter

    // Button debounce state (balanced profile)
    bool _button_stable_state[8] = {false};  // Debounced button state
    uint32_t _button_state_change_time[8] = {0};
    static const uint32_t BUTTON_DEBOUNCE_MS = 40;  // Balanced: 40ms stable press required

    // Fine/coarse mode (switch-based)
    bool _coarse_mode = false;  // false = fine, true = coarse
    static const int32_t COARSE_MULTIPLIER = 3;  // Coarse steps are 3x fine steps
    static const bool ENABLE_COARSE_SWITCH = false;  // Default off (prevents erratic 3x stepping if switch floats)

    // Callback
    ChangeCallback _callback;

    /**
     * Clamp value to parameter's valid range
     * @param param Parameter index
     * @param value Value to clamp
     * @return Clamped value
     */
    uint16_t clampValue(uint8_t param, int32_t value) const;

    void enterErrorState(uint32_t now_ms, const char* reason);
    bool attemptRecovery(uint32_t now_ms);
    int32_t safeGetRelDelta(uint8_t channel, uint32_t now_ms);
    void processEncoderDelta(uint8_t channel, int32_t delta);

    /**
     * Process button press for a specific channel
     * @param channel Encoder channel (0-7)
     */
    void processButtonPress(uint8_t channel);
};
