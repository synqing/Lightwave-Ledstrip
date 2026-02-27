// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once
#include <stdint.h>

namespace lightwaveos::audio {

/**
 * @brief Monotonic audio clock where sample_index is the source of truth.
 *
 * sample_index: Monotonic count of ADC samples since boot/start (64-bit).
 * sample_rate_hz: Sample rate that sample_index is expressed in.
 * monotonic_us: Monotonic microseconds timestamp (best from esp_timer_get_time()).
 */
struct AudioTime {
    uint64_t sample_index = 0;
    uint32_t sample_rate_hz = 12800;  // Match I2S sample rate (Emotiscope)
    uint64_t monotonic_us = 0;

    AudioTime() = default;
    AudioTime(uint64_t idx, uint32_t sr, uint64_t us)
        : sample_index(idx), sample_rate_hz(sr), monotonic_us(us) {}
};

/**
 * @brief Signed sample difference (b - a). Positive if b is later.
 */
int64_t AudioTime_SamplesBetween(const AudioTime& a, const AudioTime& b);

/**
 * @brief Signed seconds difference (b - a) based on sample_index/sample_rate_hz.
 * Returns 0.0f if sample_rate_hz is 0.
 */
float AudioTime_SecondsBetween(const AudioTime& a, const AudioTime& b);

} // namespace lightwaveos::audio
