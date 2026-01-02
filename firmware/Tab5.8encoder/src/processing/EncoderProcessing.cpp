#include "EncoderProcessing.h"

// Parameter configurations: min, max, default
const ParamConfig EncoderProcessing::PARAM_CONFIGS[8] = {
    {0, 95, 0},       // EFFECT
    {0, 255, 128},    // BRIGHTNESS
    {0, 63, 0},       // PALETTE
    {1, 100, 25},     // SPEED
    {0, 255, 128},    // INTENSITY
    {0, 255, 255},    // SATURATION
    {0, 255, 128},    // COMPLEXITY
    {0, 255, 0}       // VARIATION
};

EncoderProcessing::EncoderProcessing()
    : _callback(nullptr)
{
    begin();
}

void EncoderProcessing::begin() {
    // Initialise values to defaults
    for (uint8_t i = 0; i < 8; i++) {
        _values[i] = PARAM_CONFIGS[i].default_value;
        _last_button_pressed[i] = false;
        _button_stable_state[i] = false;
        _button_state_change_time[i] = 0;
        _last_callback_time[i] = 0;
        _last_direction[i] = 0;
        _last_direction_change_time[i] = 0;
        _detentDebounce[i].pending_count = 0;
        _detentDebounce[i].last_emit_time = 0;
        _detentDebounce[i].expecting_pair = false;
    }
}

bool EncoderProcessing::DetentDebounce::processRawDelta(int32_t raw_delta, uint32_t now_ms) {
    if (raw_delta == 0) return false;

    // Full detent in one read (common): raw of ±2
    if (abs(raw_delta) == 2) {
        pending_count = (raw_delta > 0) ? 1 : -1;
        expecting_pair = false;
        if (now_ms - last_emit_time >= 60) {  // DETENT_DEBOUNCE_INTERVAL_MS
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
            if (now_ms - last_emit_time >= 60) {  // DETENT_DEBOUNCE_INTERVAL_MS
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
    if (now_ms - last_emit_time >= 60) {  // DETENT_DEBOUNCE_INTERVAL_MS
        last_emit_time = now_ms;
        return true;
    }
    pending_count = 0;
    return false;
}

int32_t EncoderProcessing::DetentDebounce::consumeNormalisedDelta() {
    const int32_t out = pending_count;
    pending_count = 0;
    expecting_pair = false;
    return out;
}

bool EncoderProcessing::processDelta(uint8_t channel, int32_t raw_delta, uint32_t now_ms) {
    if (channel >= 8) return false;
    if (raw_delta == 0) return false;

    // Sanity filter for wild spikes
    if (raw_delta > 40 || raw_delta < -40) {
        return false;
    }

    // Detent debounce: convert raw deltas to clean ±1 steps
    if (_detentDebounce[channel].processRawDelta(raw_delta, now_ms)) {
        int32_t normalised_delta = _detentDebounce[channel].consumeNormalisedDelta();
        uint16_t old_value = _values[channel];
        applyDelta(channel, normalised_delta);
        
        // Emit callback if value changed and throttling allows
        if (_values[channel] != old_value && _callback && (now_ms - _last_callback_time[channel] >= CALLBACK_THROTTLE_MS)) {
            _last_callback_time[channel] = now_ms;
            _callback(channel, _values[channel], false);
            return true;
        }
    }
    
    return false;
}

void EncoderProcessing::applyDelta(uint8_t channel, int32_t delta) {
    if (channel >= 8) return;
    if (delta == 0) return;

    int32_t new_value = static_cast<int32_t>(_values[channel]) + delta;

    // Wrap for discrete selectors (Effect, Palette)
    uint16_t next_value = 0;
    if (channel == 0 || channel == 2) {  // EFFECT or PALETTE
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
        // Clamp for continuous params
        next_value = clampValue(channel, new_value);
    }

    if (_values[channel] != next_value) {
        _values[channel] = next_value;
    }
}

bool EncoderProcessing::processButton(uint8_t channel, bool raw_pressed, uint32_t now_ms) {
    if (channel >= 8) return false;

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
                // Reset to default value
                uint16_t default_value = PARAM_CONFIGS[channel].default_value;
                _values[channel] = default_value;
                _last_button_pressed[channel] = raw_pressed;
                
                // Emit callback with reset flag
                if (_callback) {
                    _callback(channel, default_value, true);
                }
                return true;
            }
            
            _last_button_pressed[channel] = raw_pressed;
        }
    } else {
        // State matches last stable state - reset debounce timer
        _button_state_change_time[channel] = 0;
        _last_button_pressed[channel] = raw_pressed;
    }
    
    return false;
}

uint16_t EncoderProcessing::getValue(uint8_t param) const {
    if (param >= 8) {
        return 0;
    }
    return _values[param];
}

void EncoderProcessing::setValue(uint8_t param, uint16_t value, bool trigger_callback) {
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

void EncoderProcessing::setCallback(EncoderEventCallback callback) {
    _callback = callback;
}

void EncoderProcessing::resetToDefaults() {
    for (uint8_t i = 0; i < 8; i++) {
        _values[i] = PARAM_CONFIGS[i].default_value;

        // Notify callback
        if (_callback) {
            _callback(i, _values[i], true);
        }
    }
}

uint16_t EncoderProcessing::clampValue(uint8_t param, int32_t value) const {
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

