/**
 * @file NoveltyFlux.cpp
 * @brief Novelty flux implementation
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#include "NoveltyFlux.h"
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

NoveltyFlux::NoveltyFlux()
    : m_baseline(0.001f)
    , m_baselineAlpha(0.99f)
    , m_initialized(false)
{
    memset(m_prevBins, 0, sizeof(m_prevBins));
}

void NoveltyFlux::init() {
    memset(m_prevBins, 0, sizeof(m_prevBins));
    m_baseline = 0.001f;
    m_initialized = true;
}

float NoveltyFlux::update(const float* rhythm_bins) {
    // #region agent log
    static uint32_t log_counter = 0;
    if ((log_counter++ % 125) == 0) {  // Log every ~1 second
        float input_max = 0.0f, input_sum = 0.0f;
        if (rhythm_bins != nullptr) {
            for (size_t i = 0; i < RHYTHM_BINS; i++) {
                if (rhythm_bins[i] > input_max) input_max = rhythm_bins[i];
                input_sum += rhythm_bins[i];
            }
        }
        char input_data[256];
        snprintf(input_data, sizeof(input_data),
            "{\"initialized\":%d,\"input_max\":%.6f,\"input_sum\":%.6f,\"hypothesisId\":\"H\"}",
            m_initialized ? 1 : 0, input_max, input_sum);
        debug_log(3, "NoveltyFlux.cpp:update", "novelty_input", input_data, 0);
    }
    // #endregion

    if (!m_initialized || rhythm_bins == nullptr) {
        return 0.0f;
    }

    // Compute half-wave rectified spectral flux
    float flux = 0.0f;
    for (size_t i = 0; i < RHYTHM_BINS; i++) {
        float diff = rhythm_bins[i] - m_prevBins[i];
        if (diff > 0.0f) {
            flux += diff;  // Half-wave rectification (only positive changes)
        }
    }

    // Update baseline (EMA)
    m_baseline = m_baselineAlpha * m_baseline + (1.0f - m_baselineAlpha) * flux;

    // Normalize by baseline (scale-invariant)
    float normalized_flux = (m_baseline > 0.001f) ? (flux / m_baseline) : 0.0f;

    // #region agent log
    if ((log_counter % 125) == 1) {  // Log after computation
        char output_data[256];
        snprintf(output_data, sizeof(output_data),
            "{\"flux\":%.6f,\"baseline\":%.6f,\"normalized_flux\":%.6f,\"hypothesisId\":\"H\"}",
            flux, m_baseline, normalized_flux);
        debug_log(3, "NoveltyFlux.cpp:update", "novelty_output", output_data, 0);
    }
    // #endregion

    // Update previous bins
    memcpy(m_prevBins, rhythm_bins, RHYTHM_BINS * sizeof(float));

    return normalized_flux;
}

void NoveltyFlux::reset() {
    memset(m_prevBins, 0, sizeof(m_prevBins));
    m_baseline = 0.001f;
}

} // namespace k1
} // namespace audio
} // namespace lightwaveos

