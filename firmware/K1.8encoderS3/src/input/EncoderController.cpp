#include "EncoderController.h"
#include "config/Config.h"
#include "debug/AgentDebugLog.h"
#include "esp32-hal-i2c.h"
#include "driver/periph_ctrl.h"
#include "soc/periph_defs.h"

// Attempt to clear a potentially-stuck I2C bus (SDA held low, etc.)
// Uses SCL pulsing + STOP condition as per common I2C recovery practice.
// Enhanced with multiple cycles and more aggressive recovery (matches main.cpp).
// @param cycles Number of bus clear cycles to perform (default: 2, for post-reset recovery use 3)
static void i2cBusClear(uint8_t sda_pin, uint8_t scl_pin, int cycles = 2) {
    for (int cycle = 0; cycle < cycles; cycle++) {
        // Ensure Wire isn't driving the pins
        Wire.end();
        delay(5);

        pinMode(sda_pin, INPUT_PULLUP);
        pinMode(scl_pin, INPUT_PULLUP);
        delay(2);

        // If SDA is stuck low, try to clock it free (more aggressive: 18 pulses instead of 9)
        if (digitalRead(sda_pin) == LOW) {
            pinMode(scl_pin, OUTPUT_OPEN_DRAIN);
            digitalWrite(scl_pin, HIGH);
            delayMicroseconds(5);

            // More aggressive: 18 pulses (was 9)
            for (int i = 0; i < 18; i++) {
                digitalWrite(scl_pin, LOW);
                delayMicroseconds(5);
                digitalWrite(scl_pin, HIGH);
                delayMicroseconds(5);
            }

            pinMode(scl_pin, INPUT_PULLUP);
            delay(2);
        }

        // Send STOP sequence multiple times (2-3 times per cycle)
        for (int stop_count = 0; stop_count < 2; stop_count++) {
            pinMode(sda_pin, OUTPUT_OPEN_DRAIN);
            pinMode(scl_pin, OUTPUT_OPEN_DRAIN);
            digitalWrite(sda_pin, LOW);
            digitalWrite(scl_pin, HIGH);
            delayMicroseconds(5);
            digitalWrite(sda_pin, HIGH);
            delayMicroseconds(5);

            pinMode(sda_pin, INPUT_PULLUP);
            pinMode(scl_pin, INPUT_PULLUP);
            delay(2);
        }

        // Verify SDA release after each cycle
        pinMode(sda_pin, INPUT_PULLUP);
        pinMode(scl_pin, INPUT_PULLUP);
        delay(5);
        
        // If SDA is still stuck low and we have more cycles, try again
        if (cycle < cycles - 1 && digitalRead(sda_pin) == LOW) {
            delay(10);  // Extra delay before next cycle
        }
    }
    
    // Final verification: ensure SDA is released
    pinMode(sda_pin, INPUT_PULLUP);
    pinMode(scl_pin, INPUT_PULLUP);
    delay(5);
}

// Parameter configurations: min, max, default
const EncoderController::ParamConfig EncoderController::PARAM_CONFIGS[8] = {
    {0, 95, 0},       // EFFECT
    {0, 255, 128},    // BRIGHTNESS
    {0, 63, 0},       // PALETTE
    {1, 100, 25},     // SPEED
    {0, 255, 128},    // INTENSITY
    {0, 255, 255},    // SATURATION
    {0, 255, 128},    // COMPLEXITY
    {0, 255, 0}       // VARIATION
};

EncoderController::EncoderController(int sda_pin, int scl_pin, uint8_t i2c_address)
    : _encoder(i2c_address, &Wire)
    , _sda_pin(sda_pin)
    , _scl_pin(scl_pin)
    , _callback(nullptr)
{
    // Initialize values to defaults
    for (uint8_t i = 0; i < 8; i++) {
        _values[i] = PARAM_CONFIGS[i].default_value;
        _last_button_pressed[i] = false;
        _accumulated_values[i] = 0;
        _last_encoder_change_time[i] = 0;
        _raw_residual[i] = 0;
        _last_direction[i] = 0;
        _last_direction_change_time[i] = 0;
        _last_callback_time[i] = 0;
        _button_stable_state[i] = false;
        _button_state_change_time[i] = 0;
        _detentDebounce[i].pending_count = 0;
        _detentDebounce[i].last_emit_time = 0;
        _detentDebounce[i].expecting_pair = false;
    }
}

bool EncoderController::DetentDebounce::processRawDelta(int32_t raw_delta, uint32_t now_ms) {
    if (raw_delta == 0) return false;

    // Full detent in one read (common): raw of ±2
    if (abs(raw_delta) == 2) {
        pending_count = (raw_delta > 0) ? 1 : -1;
        expecting_pair = false;
        if (now_ms - last_emit_time >= DETENT_DEBOUNCE_INTERVAL_MS) {
            last_emit_time = now_ms;
            return true;
        }
        pending_count = 0;
        return false;
    }

    // Half detent / timing artefacts: raw of ±1
    if (abs(raw_delta) == 1) {
        if (!expecting_pair) {
            pending_count = raw_delta;   // store sign
            expecting_pair = true;
            return false;                // wait for second half
        }

        // Second half arrived
        if ((pending_count > 0 && raw_delta > 0) || (pending_count < 0 && raw_delta < 0)) {
            // Same direction -> treat as full detent
            pending_count = (pending_count > 0) ? 1 : -1;
            expecting_pair = false;
            if (now_ms - last_emit_time >= DETENT_DEBOUNCE_INTERVAL_MS) {
                last_emit_time = now_ms;
                return true;
            }
            pending_count = 0;
            return false;
        }

        // Direction changed -> restart pairing
        pending_count = raw_delta;
        expecting_pair = true;
        return false;
    }

    // Unusual count (>2): normalise to ±1
    pending_count = (raw_delta > 0) ? 1 : -1;
    expecting_pair = false;
    if (now_ms - last_emit_time >= DETENT_DEBOUNCE_INTERVAL_MS) {
        last_emit_time = now_ms;
        return true;
    }
    pending_count = 0;
    return false;
}

int32_t EncoderController::DetentDebounce::consumeNormalisedDelta() {
    const int32_t out = pending_count;
    pending_count = 0;
    expecting_pair = false;
    return out;
}

void EncoderController::setI2CPins(int sda_pin, int scl_pin) {
    _sda_pin = sda_pin;
    _scl_pin = scl_pin;
}

bool EncoderController::begin() {
    // I2C must be initialized before calling this (Wire.begin with auto-detected pins in main.cpp)
    // This function assumes Wire is already configured to the correct SDA/SCL pins
    // Critical: Longer delay for I2C bus stabilization after Wire.begin()
    delay(100);

    {
        char data[128];
        // Note: Wire is configured by main.cpp auto-detection; this just uses it
        snprintf(data, sizeof(data),
                 "{\"sda_pin\":%d,\"scl_pin\":%d,\"addr\":%u,\"wire_used\":\"Wire\"}",
                 _sda_pin, _scl_pin, (unsigned)I2C::ROTATE8_ADDRESS);
        agent_dbg_log("H1", "src/input/EncoderController.cpp:begin", "EncoderController begin() starting", data);
    }

    // Initialize encoder hardware
    if (!_encoder.begin()) {
        agent_dbg_log("H1", "src/input/EncoderController.cpp:begin", "M5ROTATE8 begin() failed", "{\"ok\":false}");
        _available = false;
        return false;
    }
    agent_dbg_log("H1", "src/input/EncoderController.cpp:begin", "M5ROTATE8 begin() succeeded", "{\"ok\":true}");
    _available = true;
    _error_state = false;
    _error_count = 0;
    _next_recovery_ms = 0;
    _last_active_encoder_id = 255;

    // LIGHTWAVEOS PATTERN: Clear all LEDs (9 total, including LED 8) - NO resetAll() call
    // LightwaveOS does NOT call resetAll() after begin(), just clears LEDs
    for (uint8_t i = 0; i < 9; i++) {
        _encoder.writeRGB(i, 0, 0, 0);
    }

    // Reset runtime tracking
    for (uint8_t i = 0; i < 8; i++) {
        _accumulated_values[i] = 0;
        _last_encoder_change_time[i] = 0;
        _last_button_pressed[i] = false;
        _raw_residual[i] = 0;
        _last_direction[i] = 0;
        _last_direction_change_time[i] = 0;
        _last_callback_time[i] = 0;
        _button_stable_state[i] = false;
        _button_state_change_time[i] = 0;
    }

    return true;
}

void EncoderController::enterErrorState(uint32_t now_ms, const char* reason) {
    _error_state = true;
    _available = false;
    _next_recovery_ms = now_ms + 5000;
    _recoveryStage = RecoveryStage::Idle;
    _recovery_next_step_ms = 0;
    _recovery_freq_idx = 0;
    _recovery_wake_iter = 0;
    _recovery_hw_reset = false;
    _last_active_encoder_id = 255;
    for (uint8_t i = 0; i < 8; i++) {
        _accumulated_values[i] = 0;
        _last_encoder_change_time[i] = 0;
        _last_button_pressed[i] = false;
    }

    // #region agent log
    {
        char data[196];
        snprintf(data, sizeof(data),
                 "{\"error_state\":true,\"next_recovery_ms\":%lu,\"reason\":\"%s\",\"sda\":%d,\"scl\":%d}",
                 (unsigned long)_next_recovery_ms,
                 reason ? reason : "",
                 _sda_pin, _scl_pin);
        agent_dbg_log("H3", "src/input/EncoderController.cpp:enterErrorState", "Entering encoder recovery mode", data);
    }
    // #endregion
}

bool EncoderController::attemptRecovery(uint32_t now_ms) {
    if (!_error_state) return true;
    if (now_ms < _next_recovery_ms) return false;

    // Non-blocking recovery: execute one step per call, using millis() scheduling instead of delay().
    // This keeps the main loop responsive so the WebSocket client can keep servicing _ws.loop().

    // Initialise recovery attempt if we're idle.
    if (_recoveryStage == RecoveryStage::Idle) {
        _recovery_hw_reset = (_error_count >= 3);
        _recovery_freq_idx = 0;
        _recovery_wake_iter = 0;
        _recovery_next_step_ms = now_ms;
        _recoveryStage = _recovery_hw_reset ? RecoveryStage::HwWireEnd1 : RecoveryStage::PrepWireEnd1;
        if (_recovery_hw_reset) {
            Serial.println("[ENCODER RECOVERY] Escalating to hardware-level I2C0 reset...");
        }
    }

    if (now_ms < _recovery_next_step_ms) {
        return false;
    }

    static const uint32_t frequencies[] = {100000, 50000};
    static const char* freq_names[] = {"100kHz", "50kHz"};

    const uint32_t freq = frequencies[_recovery_freq_idx];
    const char* freq_name = freq_names[_recovery_freq_idx];

    switch (_recoveryStage) {
        case RecoveryStage::HwWireEnd1:
            Wire.end();
            _recovery_next_step_ms = now_ms + 50;
            _recoveryStage = RecoveryStage::HwWireEnd2;
            return false;

        case RecoveryStage::HwWireEnd2:
            Wire.end();
            _recovery_next_step_ms = now_ms + 50;
            _recoveryStage = RecoveryStage::HwPinsRelease;
            return false;

        case RecoveryStage::HwPinsRelease:
            pinMode(_sda_pin, INPUT_PULLUP);
            pinMode(_scl_pin, INPUT_PULLUP);
            _recovery_next_step_ms = now_ms + 10;
            _recoveryStage = RecoveryStage::HwDeinit;
            return false;

        case RecoveryStage::HwDeinit: {
            esp_err_t err = i2cDeinit(0);
            if (err != ESP_OK) {
                Serial.printf("[ENCODER RECOVERY] i2cDeinit(0) returned: %d\n", err);
            }
            _recovery_next_step_ms = now_ms + 50;
            _recoveryStage = RecoveryStage::HwPeriphReset;
            return false;
        }

        case RecoveryStage::HwPeriphReset:
            periph_module_reset(PERIPH_I2C0_MODULE);
            _recovery_next_step_ms = now_ms + 200;
            _recoveryStage = RecoveryStage::HwWaitAfterReset;
            return false;

        case RecoveryStage::HwWaitAfterReset:
            Serial.println("[ENCODER RECOVERY] Hardware reset complete, attempting recovery...");
            _recoveryStage = RecoveryStage::PrepWireEnd1;
            return false;

        case RecoveryStage::PrepWireEnd1:
            // #region agent log
            {
                char data[160];
                snprintf(data, sizeof(data),
                         "{\"attempt\":true,\"sda\":%d,\"scl\":%d,\"freq\":%lu,\"freq_name\":\"%s\",\"timeout\":%u,\"hw_reset\":%s}",
                         _sda_pin, _scl_pin,
                         (unsigned long)freq,
                         freq_name,
                         (unsigned)I2C::TIMEOUT_MS,
                         _recovery_hw_reset ? "true" : "false");
                agent_dbg_log("H3", "src/input/EncoderController.cpp:attemptRecovery", "Attempting encoder recovery (non-blocking)", data);
            }
            // #endregion
            Wire.end();
            _recovery_next_step_ms = now_ms + 50;
            _recoveryStage = RecoveryStage::PrepWireEnd2;
            return false;

        case RecoveryStage::PrepWireEnd2:
            Wire.end();
            _recovery_next_step_ms = now_ms + 50;
            _recoveryStage = RecoveryStage::PrepPinsRelease;
            return false;

        case RecoveryStage::PrepPinsRelease:
            pinMode(_sda_pin, INPUT_PULLUP);
            pinMode(_scl_pin, INPUT_PULLUP);
            _recovery_next_step_ms = now_ms + 2;
            _recoveryStage = RecoveryStage::PrepBusClear;
            return false;

        case RecoveryStage::PrepBusClear:
            i2cBusClear(_sda_pin, _scl_pin, 3);
            _recovery_next_step_ms = now_ms + 200;
            _recoveryStage = RecoveryStage::PrepWaitAfterBusClear;
            return false;

        case RecoveryStage::PrepWaitAfterBusClear:
            Wire.end();
            _recovery_next_step_ms = now_ms + 50;
            _recoveryStage = RecoveryStage::PrepWireBegin;
            return false;

        case RecoveryStage::PrepWireBegin:
            Wire.begin(_sda_pin, _scl_pin, freq);
            Wire.setTimeOut(I2C::TIMEOUT_MS);
            _recovery_next_step_ms = now_ms + 200;
            _recovery_wake_iter = 0;
            _recoveryStage = RecoveryStage::PrepWaitAfterWireBegin;
            return false;

        case RecoveryStage::PrepWaitAfterWireBegin:
            _recoveryStage = RecoveryStage::WakeGeneralCall;
            return false;

        case RecoveryStage::WakeGeneralCall:
            Wire.beginTransmission(0x00);
            Wire.endTransmission();
            _recovery_wake_iter++;
            if (_recovery_wake_iter < 3) {
                _recovery_next_step_ms = now_ms + 2;
                return false;
            }
            _recovery_wake_iter = 0;
            _recoveryStage = RecoveryStage::WakeDeviceCall;
            return false;

        case RecoveryStage::WakeDeviceCall:
            Wire.beginTransmission(I2C::ROTATE8_ADDRESS);
            Wire.endTransmission();
            _recovery_wake_iter++;
            if (_recovery_wake_iter < 3) {
                _recovery_next_step_ms = now_ms + 2;
                return false;
            }
            _recovery_next_step_ms = now_ms + 50;
            _recoveryStage = RecoveryStage::Flush;
            return false;

        case RecoveryStage::Flush:
            Wire.beginTransmission(I2C::ROTATE8_ADDRESS);
            Wire.endTransmission();
            _recovery_next_step_ms = now_ms + 50;
            _recoveryStage = RecoveryStage::TryBegin;
            return false;

        case RecoveryStage::TryBegin: {
            const bool ok = _encoder.begin();
            if (ok) {
                // LIGHTWAVEOS EXACT PATTERN: Clear LEDs (9 total) - NO resetAll() call
                for (uint8_t i = 0; i < 9; i++) {
                    _encoder.writeRGB(i, 0, 0, 0);
                }
                _available = true;
                _error_state = false;
                _error_count = 0;
                _next_recovery_ms = 0;
                _recoveryStage = RecoveryStage::Idle;
                _recovery_next_step_ms = 0;
                _last_active_encoder_id = 255;
                for (uint8_t i = 0; i < 8; i++) {
                    _accumulated_values[i] = 0;
                    _last_encoder_change_time[i] = 0;
                    _last_button_pressed[i] = false;
                    _raw_residual[i] = 0;
                    _last_direction[i] = 0;
                    _last_direction_change_time[i] = 0;
                    _last_callback_time[i] = 0;
                    _button_stable_state[i] = false;
                    _button_state_change_time[i] = 0;
                }
                _coarse_mode = false;
                {
                    char data[128];
                    snprintf(data, sizeof(data), "{\"ok\":true,\"freq\":\"%s\",\"hw_reset\":%s}",
                             freq_name, _recovery_hw_reset ? "true" : "false");
                    agent_dbg_log("H3", "src/input/EncoderController.cpp:attemptRecovery", "Encoder recovered successfully", data);
                }
                return true;
            }

            // If 100kHz failed, try 50kHz next. Otherwise, back off and try later.
            if (_recovery_freq_idx == 0) {
                _recovery_freq_idx = 1;
                _recoveryStage = RecoveryStage::PrepWireEnd1;
                _recovery_next_step_ms = now_ms;
                agent_dbg_log("H3", "src/input/EncoderController.cpp:attemptRecovery", "100kHz recovery failed, will try 50kHz", "{\"ok\":false,\"will_retry\":true}");
                return false;
            }

            agent_dbg_log("H3", "src/input/EncoderController.cpp:attemptRecovery", "Encoder recovery failed at all frequencies; will retry later", "{\"ok\":false}");
            _next_recovery_ms = now_ms + 10000;
            _recoveryStage = RecoveryStage::Idle;
            _recovery_next_step_ms = 0;
            return false;
        }

        case RecoveryStage::Idle:
        default:
            _recoveryStage = RecoveryStage::Idle;
            _recovery_next_step_ms = 0;
            return false;
    }
}

int32_t EncoderController::safeGetRelDelta(uint8_t channel, uint32_t now_ms) {
    if (channel > 7) return 0;

    // LIGHTWAVEOS EXACT PATTERN: Lockout to avoid rapid cross-channel reads
    static const uint32_t encoder_lockout_ms = 50;
    if (_last_active_encoder_id != 255 &&
        _last_active_encoder_id != channel &&
        (now_ms - _last_encoder_change_time[_last_active_encoder_id] < encoder_lockout_ms)) {
        return 0;
    }

    // Read relative counter. On many ROTATE8 firmwares this is cumulative until reset.
    // v1 EncoderManager explicitly reset the counter after reading to keep per-poll deltas stable.
    int32_t raw_value = _encoder.getRelCounter(channel);
    if (raw_value != 0) {
        _encoder.resetCounter(channel);
    }
    bool read_successful = true;  // LightwaveOS sets this to true after read

    if (read_successful) {
        // LIGHTWAVEOS EXACT PATTERN: Sanity filter for wild spikes
        if (raw_value > 40 || raw_value < -40) {
            _error_count++;
            raw_value = 0;
        } else if (raw_value != 0) {
            // v1-style detent debounce: convert raw deltas to clean ±1 steps.
            if (_detentDebounce[channel].processRawDelta(raw_value, now_ms)) {
                raw_value = _detentDebounce[channel].consumeNormalisedDelta();
                _last_encoder_change_time[channel] = now_ms;
                _last_active_encoder_id = channel;
                _error_count = 0;
            } else {
                raw_value = 0;
            }
        } else {
            raw_value = 0;
        }
    }

    // LIGHTWAVEOS EXACT PATTERN: Check error count threshold
    if (_error_count > 5) {
        enterErrorState(now_ms, "encoder_read_error_threshold");
        for (int i = 0; i < 8; i++) {
            _accumulated_values[i] = 0;
            _raw_residual[i] = 0;  // Reset residuals on error
            _last_direction[i] = 0;
        }
        return 0;
    }

    return raw_value;
}

void EncoderController::processEncoderDelta(uint8_t channel, int32_t delta) {
    if (channel >= 8) return;
    if (delta == 0) return;

    // Apply fine/coarse scaling based on switch state
    int32_t scaled_delta = _coarse_mode ? (delta * COARSE_MULTIPLIER) : delta;

    int32_t new_value = static_cast<int32_t>(_values[channel]) + scaled_delta;

    // Wrap for discrete selectors (prevents “sticking” at edges during continuous rotation)
    // - Effect: 0..95
    // - Palette: 0..63
    uint16_t next_value = 0;
    if (channel == EFFECT || channel == PALETTE) {
        const ParamConfig& cfg = PARAM_CONFIGS[channel];
        const int32_t range = static_cast<int32_t>(cfg.max_value) - static_cast<int32_t>(cfg.min_value) + 1;
        if (range > 0) {
            int32_t wrapped = new_value - static_cast<int32_t>(cfg.min_value);
            // Proper modulo for negatives
            wrapped = (wrapped % range + range) % range;
            next_value = static_cast<uint16_t>(wrapped + static_cast<int32_t>(cfg.min_value));
        } else {
            next_value = cfg.min_value;
        }
    } else {
        next_value = clampValue(channel, new_value);
    }

    if (_values[channel] != next_value) {
        _values[channel] = next_value;
        
        // PER-PARAMETER CALLBACK THROTTLE: Only emit callback if enough time has passed
        // This reduces WebSocket spam while still updating internal value immediately
        uint32_t now_ms = millis();
        if (_callback && (now_ms - _last_callback_time[channel] >= CALLBACK_THROTTLE_MS)) {
            _callback(channel, next_value, false);
            _last_callback_time[channel] = now_ms;
        }
        // If throttled, value is still updated internally but callback deferred
    }
}

void EncoderController::update() {
    // Rate-limit encoder polling to reduce I2C load (prevents i2cRead timeouts)
    static uint32_t last_poll_ms = 0;
    uint32_t now_ms = millis();
    if (now_ms - last_poll_ms < 20) {
        return;
    }
    last_poll_ms = now_ms;

    // LIGHTWAVEOS EXACT PATTERN: Check error state first, wait 1000ms before recovery
    if (_error_state) {
        if (now_ms - _next_recovery_ms < 1000) {
            return;  // Wait 1000ms before attempting recovery (LightwaveOS pattern)
        }
        _available = false;  // LightwaveOS sets this before recovery attempt
        attemptRecovery(now_ms);
        if (!_available) {
            // Recovery failed, set next attempt time (LightwaveOS pattern)
            _next_recovery_ms = now_ms + 10000;
            return;
        }
        // Recovery succeeded - reset error state (LightwaveOS pattern)
        _error_state = false;
        _error_count = 0;
        for (int i = 0; i < 8; i++) {
            _accumulated_values[i] = 0;
            _last_encoder_change_time[i] = 0;
            _raw_residual[i] = 0;
            _last_direction[i] = 0;
            _last_direction_change_time[i] = 0;
        }
        _last_active_encoder_id = 255;
    }

    // LIGHTWAVEOS EXACT PATTERN: Return early if not available
    if (!_available) {
        return;
    }

    // Fine/coarse mode: disabled by default for K1.8encoderS3 because the switch line can float
    // and cause erratic 3x stepping. Enable explicitly if you have a stable switch wired.
    if (ENABLE_COARSE_SWITCH) {
        uint8_t sw = _encoder.inputSwitch();
        _coarse_mode = (sw == 1);  // Switch position 1 = coarse mode
    } else {
        _coarse_mode = false;
    }

    // LIGHTWAVEOS PATTERN: Poll all encoders directly using getRelCounter() (not mask-based)
    // This matches the exact pattern from LightwaveOS encoders.h check_encoders()
    for (uint8_t channel = 0; channel < 8; channel++) {
        int32_t delta = safeGetRelDelta(channel, now_ms);
        if (delta != 0) {
            processEncoderDelta(channel, delta);
            if (_error_count > 5) {
                enterErrorState(now_ms, "rel_delta_sanity_error");
                return;
            }
        }
    }

    // BUTTON DEBOUNCE (balanced profile): Time-based debounce instead of naive edge-detect
    for (uint8_t channel = 0; channel < 8; channel++) {
        bool raw_pressed = _encoder.getKeyPressed(channel);
        bool last_stable = _button_stable_state[channel];
        
        // Update debounced state: require stable press/release for BUTTON_DEBOUNCE_MS
        if (raw_pressed != last_stable) {
            // State changed from last known stable state
            if (_button_state_change_time[channel] == 0) {
                // Start debounce timer
                _button_state_change_time[channel] = now_ms;
            } else if ((now_ms - _button_state_change_time[channel]) >= BUTTON_DEBOUNCE_MS) {
                // Debounce period elapsed - state is now stable
                _button_stable_state[channel] = raw_pressed;
                _button_state_change_time[channel] = 0;
                
                // Detect rising edge (press) on stable state
                if (raw_pressed && !_last_button_pressed[channel]) {
                    processButtonPress(channel);
                }
                
                _last_button_pressed[channel] = raw_pressed;
            }
        } else {
            // State matches last stable state - reset debounce timer
            _button_state_change_time[channel] = 0;
            // Still update last_button_pressed for edge detection consistency
            _last_button_pressed[channel] = raw_pressed;
        }
    }
}

uint16_t EncoderController::getValue(Parameter param) const {
    if (param >= 8) {
        return 0;
    }
    return _values[param];
}

void EncoderController::setValue(Parameter param, uint16_t value, bool trigger_callback) {
    if (param >= 8) {
        return;
    }

    // Clamp to valid range
    uint16_t clamped_value = clampValue(param, value);

    // Update value if changed
    if (_values[param] != clamped_value) {
        _values[param] = clamped_value;

        // Trigger callback if requested
        if (trigger_callback && _callback) {
            _callback(param, clamped_value, false);
        }
    }
}

void EncoderController::setChangeCallback(ChangeCallback callback) {
    _callback = callback;
}

bool EncoderController::isConnected() {
    return _encoder.isConnected();
}

void EncoderController::resetToDefaults() {
    for (uint8_t i = 0; i < 8; i++) {
        _values[i] = PARAM_CONFIGS[i].default_value;

        // Notify callback
        if (_callback) {
            _callback(i, _values[i], true);
        }
    }

    // Reset encoder hardware positions
    _encoder.resetAll();
    _available = _encoder.isConnected();
    _error_state = !_available;
    _error_count = 0;
    _next_recovery_ms = 0;
    _last_active_encoder_id = 255;
    for (uint8_t i = 0; i < 8; i++) {
        _accumulated_values[i] = 0;
        _last_encoder_change_time[i] = 0;
        _last_button_pressed[i] = false;
        _raw_residual[i] = 0;
        _last_direction[i] = 0;
        _last_direction_change_time[i] = 0;
        _last_callback_time[i] = 0;
        _button_stable_state[i] = false;
        _button_state_change_time[i] = 0;
    }
    _coarse_mode = false;
}

uint16_t EncoderController::clampValue(uint8_t param, int32_t value) const {
    if (param >= 8) {
        return 0;
    }

    const ParamConfig& config = PARAM_CONFIGS[param];

    if (value < config.min_value) {
        return config.min_value;
    }
    if (value > config.max_value) {
        return config.max_value;
    }

    return static_cast<uint16_t>(value);
}

void EncoderController::processButtonPress(uint8_t channel) {
    if (channel >= 8) {
        return;
    }

    // Reset to default value
    uint16_t default_value = PARAM_CONFIGS[channel].default_value;

    // Update value
    _values[channel] = default_value;

    // Reset encoder position to match default value
    _encoder.resetCounter(channel);
    _accumulated_values[channel] = 0;
    _last_encoder_change_time[channel] = millis();

    // Notify callback with button reset flag
    if (_callback) {
        _callback(channel, default_value, true);
    }
}
