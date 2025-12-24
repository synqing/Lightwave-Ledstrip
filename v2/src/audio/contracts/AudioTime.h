#pragma once
#include "config/features.h"
#if FEATURE_AUDIO_SYNC

#include <cstdint>

namespace lightwaveos {
namespace audio {

/**
 * @brief Sample-index monotonic clock
 * sample_index is the single source of truth for audio time
 */
struct AudioTime {
    uint64_t sample_index = 0;      // Monotonic sample counter (never wraps in practice)
    uint32_t sample_rate_hz = 16000; // Sample rate for conversion
    uint64_t monotonic_us = 0;      // Wall clock at sample_index (for extrapolation)

    AudioTime() = default;
    AudioTime(uint64_t samples, uint32_t rate, uint64_t us)
        : sample_index(samples), sample_rate_hz(rate), monotonic_us(us) {}
};

// Utility functions
inline float AudioTime_SamplesToSeconds(int64_t sample_delta, uint32_t sample_rate_hz) {
    return static_cast<float>(sample_delta) / static_cast<float>(sample_rate_hz);
}

inline float AudioTime_SecondsBetween(const AudioTime& a, const AudioTime& b) {
    int64_t delta = static_cast<int64_t>(b.sample_index) - static_cast<int64_t>(a.sample_index);
    return AudioTime_SamplesToSeconds(delta, a.sample_rate_hz);
}

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
