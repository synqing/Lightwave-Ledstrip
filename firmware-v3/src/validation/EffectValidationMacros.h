// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file EffectValidationMacros.h
 * @brief Zero-overhead instrumentation macros for audio-reactive effect validation
 *
 * When FEATURE_EFFECT_VALIDATION is disabled, all macros compile to nothing
 * for zero runtime cost. When enabled, they provide per-frame validation
 * sampling with microsecond-precision timestamps.
 *
 * Target: 120 FPS render loop with minimal overhead (<0.1% CPU impact)
 *
 * Usage pattern in effect render():
 * @code
 *   void MyEffect::render(EffectContext& ctx) {
 *       VALIDATION_INIT(EFFECT_ID_WAVE_COLLISION);
 *
 *       // ... compute phase delta ...
 *       VALIDATION_PHASE(m_phase, phaseDelta);
 *
 *       // ... compute speed before/after slew limiting ...
 *       VALIDATION_SPEED(rawSpeed, m_speedScaleSmooth);
 *
 *       // ... audio metrics ...
 *       VALIDATION_AUDIO(m_dominantBin, m_energyAvg, m_energyDelta);
 *
 *       // ... check for jog-dial reversal ...
 *       VALIDATION_REVERSAL_CHECK(prevDelta, phaseDelta);
 *
 *       // ... at end of render ...
 *       VALIDATION_SUBMIT(g_validationRing);
 *   }
 * @endcode
 */

#pragma once

#include "../config/features.h"

// Define FEATURE_EFFECT_VALIDATION if not already defined
#ifndef FEATURE_EFFECT_VALIDATION
#define FEATURE_EFFECT_VALIDATION 0
#endif

#if FEATURE_EFFECT_VALIDATION

//-----------------------------------------------------------------------------
// ENABLED: Full instrumentation with validation sample collection
//-----------------------------------------------------------------------------

#include "EffectValidationMetrics.h"

// Global validation ring buffer is declared in EffectValidationMetrics.h

// ESP32 high-resolution timer for microsecond precision
#ifdef ESP_PLATFORM
#include <esp_timer.h>
#define VALIDATION_GET_TIME_US() static_cast<uint32_t>(esp_timer_get_time())
#else
// Native test fallback using std::chrono
#include <chrono>
inline uint32_t _validation_get_time_us() {
    using namespace std::chrono;
    return static_cast<uint32_t>(
        duration_cast<microseconds>(
            steady_clock::now().time_since_epoch()
        ).count()
    );
}
#define VALIDATION_GET_TIME_US() _validation_get_time_us()
#endif

/**
 * @brief Initialize validation sample for this frame
 *
 * Must be called at the start of render(). Creates a thread-local static
 * sample and sets the effect ID and timestamp.
 *
 * @param effect_id The effect's numeric ID (e.g., 16, 17)
 */
#define VALIDATION_INIT(_eff_id) \
    static thread_local ::lightwaveos::validation::EffectValidationSample _val_sample; \
    _val_sample = ::lightwaveos::validation::EffectValidationSample(); \
    _val_sample.timestamp_us = VALIDATION_GET_TIME_US(); \
    _val_sample.effect_id = static_cast<uint8_t>(_eff_id)

/**
 * @brief Record phase accumulator state
 *
 * @param _phase_val Current phase accumulator value
 * @param _delta_val Phase change this frame
 */
#define VALIDATION_PHASE(_phase_val, _delta_val) \
    do { \
        _val_sample.phase = (_phase_val); \
        _val_sample.phase_delta = (_delta_val); \
    } while(0)

/**
 * @brief Record speed values before and after slew limiting
 *
 * @param raw Raw speed value before slew limiting
 * @param smooth Speed value after slew limiting
 */
#define VALIDATION_SPEED(raw, smooth) \
    do { \
        _val_sample.speed_scale_raw = (raw); \
        _val_sample.speed_scale_smooth = (smooth); \
    } while(0)

/**
 * @brief Record audio metrics
 *
 * @param _freq_bin Dominant frequency bin (0-11 for chroma, 0-7 for bands)
 * @param _energy_val Average energy level (0.0-1.0)
 * @param _delta_val Energy delta for transient detection
 */
#define VALIDATION_AUDIO(_freq_bin, _energy_val, _delta_val) \
    do { \
        _val_sample.dominant_freq_bin = static_cast<float>(_freq_bin); \
        _val_sample.energy_avg = (_energy_val); \
        _val_sample.energy_delta = (_delta_val); \
    } while(0)

/**
 * @brief Record scroll phase (AudioBloom specific)
 *
 * @param _scroll_val Current scroll position (0.0-1.0)
 */
#define VALIDATION_SCROLL(_scroll_val) \
    _val_sample.scroll_phase = (_scroll_val)

/**
 * @brief Check for and count direction reversals (jog-dial detection)
 *
 * Increments the reversal counter if a sign change is detected between
 * the previous and current phase deltas. This helps identify jerky
 * motion caused by noisy audio input or inadequate slew limiting.
 *
 * @param prev_delta Previous frame's phase delta
 * @param curr_delta Current frame's phase delta
 */
#define VALIDATION_REVERSAL_CHECK(prev_delta, curr_delta) \
    do { \
        if (::lightwaveos::validation::detectReversal((prev_delta), (curr_delta))) { \
            _val_sample.reversal_count++; \
        } \
    } while(0)

/**
 * @brief Submit the completed sample to the ring buffer
 *
 * Call at the end of render() to push the accumulated validation data
 * to the ring buffer for later analysis.
 *
 * @param ring_buffer_ptr Pointer to the EffectValidationRing
 */
#define VALIDATION_SUBMIT(ring_buffer_ptr) \
    do { \
        if ((ring_buffer_ptr) != nullptr) { \
            (ring_buffer_ptr)->push(_val_sample); \
        } \
    } while(0)

/**
 * @brief Get the current sample for inspection (useful in tests)
 *
 * @return Reference to the in-progress sample
 */
#define VALIDATION_GET_SAMPLE() _val_sample

/**
 * @brief Set hop sequence number for audio correlation
 *
 * @param hop_seq The current audio hop sequence number
 */
#define VALIDATION_SET_HOP_SEQ(hop_seq) \
    _val_sample.hop_seq = (hop_seq)

#else // !FEATURE_EFFECT_VALIDATION

//-----------------------------------------------------------------------------
// DISABLED: No-op stubs for zero runtime overhead
// These compile to nothing, ensuring zero impact on 120 FPS render loop
//-----------------------------------------------------------------------------

#define VALIDATION_INIT(...)              do {} while(0)
#define VALIDATION_PHASE(...)             do {} while(0)
#define VALIDATION_SPEED(...)             do {} while(0)
#define VALIDATION_AUDIO(...)             do {} while(0)
#define VALIDATION_SCROLL(...)            do {} while(0)
#define VALIDATION_REVERSAL_CHECK(...)    do {} while(0)
#define VALIDATION_SUBMIT(...)            do {} while(0)
#define VALIDATION_SET_HOP_SEQ(...)       do {} while(0)

// Include stdint.h for type definitions when disabled
#include <stdint.h>
#include <stddef.h>

// Forward declare an empty struct for VALIDATION_GET_SAMPLE when disabled
namespace lightwaveos {
namespace validation {
    struct EffectValidationSample {
        uint32_t timestamp_us = 0;
        uint32_t hop_seq = 0;
        uint8_t effect_id = 0;
        uint8_t reversal_count = 0;
        uint16_t frame_seq = 0;
        float phase = 0.0f;
        float phase_delta = 0.0f;
        float speed_scale_raw = 0.0f;
        float speed_scale_smooth = 0.0f;
        float dominant_freq_bin = 0.0f;
        float energy_avg = 0.0f;
        float energy_delta = 0.0f;
        float scroll_phase = 0.0f;
    };

    // Empty ring buffer stub when validation is disabled
    template<size_t N = 128>
    class EffectValidationRing {
    public:
        bool push(const EffectValidationSample&) { return true; }
        size_t drain(EffectValidationSample*, size_t) { return 0; }
        size_t available() const { return 0; }
        bool empty() const { return true; }
        static constexpr size_t capacity() { return N - 1; }
        void clear() {}
    };

    template<size_t N = 128>
    using ValidationRingBuffer = EffectValidationRing<N>;

    // Stub global pointer declaration (matches enabled path)
    extern EffectValidationRing<32>* g_validationRing;
    inline void initValidationRing() {}
    inline bool isValidationRingReady() { return false; }
} // namespace validation
} // namespace lightwaveos

#define VALIDATION_GET_SAMPLE()           (::lightwaveos::validation::EffectValidationSample{})

#endif // FEATURE_EFFECT_VALIDATION
