#pragma once
// ============================================================================
// EncoderProcessing - Clean Encoder Processing Logic for Tab5
// ============================================================================
// Ported from K1.8encoderS3 EncoderController, keeping ONLY the safe
// processing logic (debounce, clamp, wrap). NO I2C recovery code!
//
// This module provides:
// - DetentDebounce: Normalizes M5ROTATE8's step-size quirk (2 per detent,
//   occasionally 1) into consistent ±1 detent steps
// - Clamp/Wrap utilities: Parameter range enforcement
// - Button debouncing: Time-based stable press detection
// - Callback throttling: Reduces WebSocket message spam
//
// CRITICAL SAFETY NOTE:
// This file does NOT contain any I2C recovery logic. The Tab5's internal
// I2C bus is shared with display/touch/audio, so aggressive recovery
// patterns (periph_module_reset, i2cDeinit, bus clear) are forbidden.
// ============================================================================

#include <Arduino.h>
#include <climits>  // For INT32_MAX
#include "../config/Config.h"

// ============================================================================
// DetentDebounce - Normalizes M5ROTATE8 step-size quirk
// ============================================================================
// The M5ROTATE8 often reports step sizes of 2 per detent, but can also emit 1s
// (half detent) depending on polling timing. This debouncer normalises to
// clean ±1 detent steps and applies a minimum emit interval per encoder.
//
// Ported from K1.8encoderS3 EncoderController.cpp lines 108-166

struct DetentDebounce {
    int32_t pending_count = 0;
    uint32_t last_emit_time = 0;
    bool expecting_pair = false;

    // Minimum time between detent emissions (60ms prevents rapid-fire)
    static const uint32_t INTERVAL_MS = 60;

    /**
     * Process raw encoder delta and determine if a normalized step should emit
     * @param raw_delta Raw delta from M5ROTATE8 (-2, -1, 0, +1, +2, or larger)
     * @param now_ms Current time in milliseconds
     * @return true if a normalized step is ready to emit
     */
    bool processRawDelta(int32_t raw_delta, uint32_t now_ms) {
        if (raw_delta == 0) return false;

        // Full detent in one read (common): raw of ±2
        if (abs(raw_delta) == 2) {
            pending_count = (raw_delta > 0) ? 1 : -1;
            expecting_pair = false;
            if (now_ms - last_emit_time >= INTERVAL_MS) {
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
                if (now_ms - last_emit_time >= INTERVAL_MS) {
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
        if (now_ms - last_emit_time >= INTERVAL_MS) {
            last_emit_time = now_ms;
            return true;
        }
        pending_count = 0;
        return false;
    }

    /**
     * Consume the normalized delta after processRawDelta returned true
     * @return Normalized delta (-1, 0, or +1)
     */
    int32_t consumeNormalisedDelta() {
        const int32_t out = pending_count;
        pending_count = 0;
        expecting_pair = false;
        return out;
    }

    /**
     * Reset the debounce state (e.g., after button press reset)
     */
    void reset() {
        pending_count = 0;
        last_emit_time = 0;
        expecting_pair = false;
    }
};

// ============================================================================
// ButtonDebounce - Time-based button press detection
// ============================================================================
// Requires button to be stable for DEBOUNCE_MS before registering press.
// Prevents false triggers from mechanical bounce.

struct ButtonDebounce {
    bool stable_state = false;
    uint32_t state_change_time = 0;

    static const uint32_t DEBOUNCE_MS = 40;  // 40ms stable press required

    /**
     * Process button state and determine if press should trigger
     * @param is_pressed Current physical button state
     * @param now_ms Current time in milliseconds
     * @return true if a debounced press event should trigger
     */
    bool processState(bool is_pressed, uint32_t now_ms) {
        if (is_pressed != stable_state) {
            // State changed - start debounce timer
            if (state_change_time == 0) {
                state_change_time = now_ms;
            } else if (now_ms - state_change_time >= DEBOUNCE_MS) {
                // State has been stable for debounce period
                bool was_pressed = stable_state;
                stable_state = is_pressed;
                state_change_time = 0;

                // Return true only on rising edge (button pressed down)
                return is_pressed && !was_pressed;
            }
        } else {
            // State unchanged - reset timer
            state_change_time = 0;
        }
        return false;
    }

    /**
     * Reset debounce state
     */
    void reset() {
        stable_state = false;
        state_change_time = 0;
    }
};

// ============================================================================
// CallbackThrottle - Per-parameter callback rate limiting
// ============================================================================
// Limits how often a callback fires for a given parameter to reduce
// WebSocket message spam during rapid encoder turning.

struct CallbackThrottle {
    uint32_t last_callback_time = 0;

    static const uint32_t THROTTLE_MS = 50;  // 50ms minimum between callbacks (matches PARAM_THROTTLE_MS)

    /**
     * Check if callback should fire (respects throttle)
     * @param now_ms Current time in milliseconds
     * @return true if callback is allowed, false if throttled
     */
    bool shouldFire(uint32_t now_ms) {
        if (now_ms - last_callback_time >= THROTTLE_MS) {
            last_callback_time = now_ms;
            return true;
        }
        return false;
    }

    /**
     * Force callback (resets timer without checking)
     * Use for button resets which should always trigger
     */
    void force(uint32_t now_ms) {
        last_callback_time = now_ms;
    }

    void reset() {
        last_callback_time = 0;
    }
};

// ============================================================================
// Value Processing Utilities
// ============================================================================

namespace EncoderProcessing {

/**
 * Clamp value to parameter's valid range
 * @param param Parameter index (0-7)
 * @param value Value to clamp
 * @return Clamped value within parameter's min/max range
 */
inline uint16_t clampValue(uint8_t param, int32_t value) {
    if (param >= 8) return 0;

    // Direct cast - Parameter enum order matches encoder indices:
    // 0=Effect, 1=Palette, 2=Speed, 3=Mood, 4=FadeAmount, 5=Complexity, 6=Variation, 7=Brightness
    Parameter p = static_cast<Parameter>(param);
    uint8_t minVal = getParameterMin(p);
    uint8_t maxVal = getParameterMax(p);

    if (value < minVal) return minVal;
    if (value > maxVal) return maxVal;
    return static_cast<uint16_t>(value);
}

/**
 * Wrap value around parameter's range (for Effect/Palette)
 * @param param Parameter index (0-7)
 * @param value Value to wrap
 * @return Wrapped value within parameter's min/max range
 */
inline uint16_t wrapValue(uint8_t param, int32_t value) {
    if (param >= 8) return 0;

    // Direct cast - Parameter enum order matches encoder indices:
    // 0=Effect, 1=Palette, 2=Speed, 3=Mood, 4=FadeAmount, 5=Complexity, 6=Variation, 7=Brightness
    Parameter p = static_cast<Parameter>(param);
    uint8_t minVal = getParameterMin(p);
    uint8_t maxVal = getParameterMax(p);
    int32_t range = (maxVal - minVal) + 1;

    // Normalize value to range using modulo for efficiency and correctness
    // Handle negative values correctly with overflow protection
    if (value < minVal) {
        int32_t diff = minVal - value;
        // CRITICAL FIX: Prevent integer overflow in wrap calculation
        // Use modulo arithmetic instead of multiplication to avoid overflow
        int32_t wraps = (diff + range - 1) / range;  // Ceiling division
        // Limit wraps to prevent overflow: max wraps that won't overflow
        int32_t maxSafeWraps = (INT32_MAX - minVal) / range;
        if (wraps > maxSafeWraps) {
            // For extreme values, use modulo directly
            value = minVal + ((diff % range + range) % range);
        } else {
            value += wraps * range;
        }
    } else if (value > maxVal) {
        int32_t diff = value - maxVal;
        // CRITICAL FIX: Prevent integer overflow in wrap calculation
        int32_t wraps = (diff + range - 1) / range;  // Ceiling division
        // Limit wraps to prevent overflow
        int32_t maxSafeWraps = (INT32_MAX - maxVal) / range;
        if (wraps > maxSafeWraps) {
            // For extreme values, use modulo directly
            value = maxVal - ((diff % range + range) % range);
        } else {
            value -= wraps * range;
        }
    }

    return static_cast<uint16_t>(value);
}

/**
 * Determine if parameter should wrap (Effect, Palette) or clamp (others)
 * @param param Parameter index (0-7)
 * @return true if parameter wraps, false if it clamps
 */
inline bool shouldWrap(uint8_t param) {
    // Parameter enum order (matches PARAMETER_TABLE):
    // 0=Effect, 1=Palette, 2=Speed, 3=Mood, 4=FadeAmount, 5=Complexity, 6=Variation, 7=Brightness
    // Effect (0) and Palette (1) wrap; others clamp
    return (param == 0 || param == 1);
}

/**
 * Apply delta to value with appropriate wrap/clamp behavior
 * @param param Parameter index (0-7)
 * @param current_value Current value
 * @param delta Delta to apply (-1, 0, +1 typically)
 * @return New value with wrap/clamp applied
 */
inline uint16_t applyDelta(uint8_t param, uint16_t current_value, int32_t delta) {
    int32_t new_value = static_cast<int32_t>(current_value) + delta;

    if (shouldWrap(param)) {
        return wrapValue(param, new_value);
    } else {
        return clampValue(param, new_value);
    }
}

}  // namespace EncoderProcessing
