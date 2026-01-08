/**
 * @file NoiseFloor.cpp
 * @brief Noise floor implementation
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#include "NoiseFloor.h"
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "../AudioDebugConfig.h"

namespace lightwaveos {
namespace audio {
namespace k1 {

// #region agent log
// Native-safe debug logging using sample counter (not system timers)
static void debug_log(uint8_t minVerbosity, const char* location, const char* message, const char* data_json, uint64_t t_samples) {
    auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
    if (dbgCfg.verbosity < minVerbosity) {
        return;  // Suppress if verbosity too low
    }
    // Output JSON to serial with special prefix for parsing
    // Convert t_samples to microseconds for logging: t_us = (t_samples * 1000000ULL) / 16000
    uint64_t t_us = (t_samples * 1000000ULL) / 16000;
    printf("DEBUG_JSON:{\"location\":\"%s\",\"message\":\"%s\",\"data\":%s,\"timestamp\":%llu}\n",
           location, message, data_json, (unsigned long long)t_us);
}
// #endregion

NoiseFloor::NoiseFloor()
    : m_noiseFloor(nullptr)
    , m_numBins(0)
    , m_k(1.5f)
    , m_leakRate(0.999f)
    , m_initialized(false)
{
}

void NoiseFloor::init(size_t num_bins, float k, float leak_rate) {
    if (m_initialized) {
        if (m_noiseFloor != nullptr) {
            free(m_noiseFloor);
        }
    }

    m_noiseFloor = static_cast<float*>(malloc(num_bins * sizeof(float)));
    if (m_noiseFloor == nullptr) {
        return;
    }

    m_numBins = num_bins;
    m_k = k;
    m_leakRate = leak_rate;

    // Initialize to match normalized Goertzel magnitude scale
    // Normalized magnitudes are typically 1e-6 to 1e-5 range after WindowBank normalization
    // Use 1e-6 as initial floor (will adapt upward via leaky-min if signal is present)
    const float INITIAL_NOISE_FLOOR = 1e-6f;
    for (size_t i = 0; i < num_bins; i++) {
        m_noiseFloor[i] = INITIAL_NOISE_FLOOR;
    }

    m_initialized = true;
}

void NoiseFloor::update(const float* mags, bool is_clipping) {
    if (!m_initialized || mags == nullptr) {
        return;
    }

    // Freeze updates when clipping
    if (is_clipping) {
        return;
    }

    // Leaky-min update: noiseFloor = leakRate * noiseFloor + (1 - leakRate) * min(noiseFloor, mag)
    for (size_t i = 0; i < m_numBins; i++) {
        float current_min = std::min(m_noiseFloor[i], mags[i]);
        m_noiseFloor[i] = m_leakRate * m_noiseFloor[i] + (1.0f - m_leakRate) * current_min;
    }
}

void NoiseFloor::subtract(const float* mags_in, float* mags_out, size_t num_bins) const {
    if (!m_initialized || mags_in == nullptr || mags_out == nullptr) {
        return;
    }

    // #region agent log
    static uint32_t noise_log_counter = 0;
    float max_in = 0.0f, max_floor = 0.0f, max_threshold = 0.0f, max_out = 0.0f;
    for (size_t i = 0; i < std::min(num_bins, m_numBins); i++) {
        if (mags_in[i] > max_in) max_in = mags_in[i];
        if (m_noiseFloor[i] > max_floor) max_floor = m_noiseFloor[i];
        float threshold = m_k * m_noiseFloor[i];
        if (threshold > max_threshold) max_threshold = threshold;
    }
    // #endregion

    size_t n = std::min(num_bins, m_numBins);
    for (size_t i = 0; i < n; i++) {
        float threshold = m_k * m_noiseFloor[i];
        mags_out[i] = std::max(0.0f, mags_in[i] - threshold);
        // #region agent log
        if (mags_out[i] > max_out) max_out = mags_out[i];
        // #endregion
    }
    
    // #region agent log
    if ((noise_log_counter++ % 125) == 0) {
        char noise_data[256];
        snprintf(noise_data, sizeof(noise_data),
            "{\"max_in\":%.6f,\"max_floor\":%.6f,\"k\":%.2f,\"max_threshold\":%.6f,\"max_out\":%.6f,\"hypothesisId\":\"J\"}",
            max_in, max_floor, m_k, max_threshold, max_out);
        debug_log(3, "NoiseFloor.cpp:subtract", "noise_subtraction", noise_data, 0);
    }
    // #endregion
}

float NoiseFloor::getNoiseFloor(size_t bin_idx) const {
    if (!m_initialized || bin_idx >= m_numBins) {
        return 0.0f;
    }
    return m_noiseFloor[bin_idx];
}

} // namespace k1
} // namespace audio
} // namespace lightwaveos

