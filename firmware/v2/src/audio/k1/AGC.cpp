/**
 * @file AGC.cpp
 * @brief AGC implementation
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#include "AGC.h"
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

AGC::AGC()
    : m_targetLevel(nullptr)
    , m_numBins(0)
    , m_mode(AGCMode::Rhythm)
    , m_gain(1.0f)
    , m_attackRate(0.95f)
    , m_releaseRate(0.999f)
    , m_maxGain(1.0f)
    , m_initialized(false)
{
}

void AGC::init(size_t num_bins, AGCMode mode) {
    if (m_initialized) {
        if (m_targetLevel != nullptr) {
            free(m_targetLevel);
        }
    }

    m_targetLevel = static_cast<float*>(malloc(num_bins * sizeof(float)));
    if (m_targetLevel == nullptr) {
        return;
    }

    m_numBins = num_bins;
    m_mode = mode;
    m_gain = 1.0f;

    // Set parameters based on mode
    if (mode == AGCMode::Rhythm) {
        m_attackRate = 0.95f;   // Fast attack
        m_releaseRate = 0.999f; // Slow release
        m_maxGain = 1.0f;       // Attenuation-only (never boosts)
    } else {  // Harmony
        m_attackRate = 0.98f;   // Slower attack
        m_releaseRate = 0.998f; // Moderate release
        m_maxGain = 2.0f;       // Mild boost allowed
    }

    // Initialize target levels
    for (size_t i = 0; i < num_bins; i++) {
        m_targetLevel[i] = 0.1f;  // Target level
    }

    m_initialized = true;
}

void AGC::updateGain(const float* mags, size_t num_bins) {
    if (!m_initialized || mags == nullptr || num_bins == 0) {
        return;
    }

    // Compute average magnitude
    float sum = 0.0f;
    for (size_t i = 0; i < num_bins && i < m_numBins; i++) {
        sum += mags[i];
    }
    float avg_mag = sum / static_cast<float>(num_bins);

    // Compute desired gain to reach target level
    float target_avg = 0.0f;
    for (size_t i = 0; i < m_numBins; i++) {
        target_avg += m_targetLevel[i];
    }
    target_avg /= static_cast<float>(m_numBins);

    float desired_gain = (avg_mag > 0.001f) ? (target_avg / avg_mag) : 1.0f;

    // Clamp desired gain
    if (m_mode == AGCMode::Rhythm) {
        // Attenuation-only: never exceed 1.0
        desired_gain = std::min(1.0f, desired_gain);
    } else {
        // Harmony: cap at maxGain
        desired_gain = std::min(m_maxGain, desired_gain);
    }

    // Update gain with attack/release
    if (desired_gain < m_gain) {
        // Attack (decrease gain)
        m_gain = m_attackRate * m_gain + (1.0f - m_attackRate) * desired_gain;
    } else {
        // Release (increase gain)
        m_gain = m_releaseRate * m_gain + (1.0f - m_releaseRate) * desired_gain;
    }

    // Clamp final gain
    m_gain = std::max(0.01f, std::min(m_maxGain, m_gain));
}

void AGC::process(const float* mags_in, float* mags_out, size_t num_bins) {
    if (!m_initialized || mags_in == nullptr || mags_out == nullptr) {
        return;
    }

    // Update gain
    updateGain(mags_in, num_bins);

    // #region agent log
    static uint32_t agc_log_counter = 0;
    float max_in = 0.0f, max_out = 0.0f;
    for (size_t i = 0; i < std::min(num_bins, m_numBins); i++) {
        if (mags_in[i] > max_in) max_in = mags_in[i];
    }
    // #endregion

    // Apply gain
    size_t n = std::min(num_bins, m_numBins);
    for (size_t i = 0; i < n; i++) {
        mags_out[i] = mags_in[i] * m_gain;
        // #region agent log
        if (mags_out[i] > max_out) max_out = mags_out[i];
        // #endregion
    }
    
    // #region agent log
    if ((agc_log_counter++ % 125) == 0) {
        char agc_data[256];
        snprintf(agc_data, sizeof(agc_data),
            "{\"max_in\":%.6f,\"gain\":%.6f,\"maxGain\":%.2f,\"max_out\":%.6f,\"mode\":%d,\"hypothesisId\":\"L\"}",
            max_in, m_gain, m_maxGain, max_out, static_cast<int>(m_mode));
        debug_log(3, "AGC.cpp:process", "agc_processing", agc_data, 0);
    }
    // #endregion
}

} // namespace k1
} // namespace audio
} // namespace lightwaveos

