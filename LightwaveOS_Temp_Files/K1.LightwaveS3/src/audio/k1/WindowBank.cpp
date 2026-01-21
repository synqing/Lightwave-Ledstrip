/**
 * @file WindowBank.cpp
 * @brief Window bank implementation
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#include "WindowBank.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cstdio>
#include "../AudioDebugConfig.h"

namespace lightwaveos {
namespace audio {
namespace k1 {

// #region agent log
// Native-safe debug logging using sample counter (not system timers)
// Rate-limited to prevent serial monitor spam (1-4 seconds depending on verbosity)
static void debug_log(uint8_t minVerbosity, const char* location, const char* message, const char* data_json, uint64_t t_samples) {
    auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
    if (dbgCfg.verbosity < minVerbosity) {
        return;  // Suppress if verbosity too low
    }
    // Rate limiting: only print if enough time has passed
    if (!dbgCfg.shouldPrint(minVerbosity)) {
        return;  // Suppress if too soon since last print
    }
    // Output JSON to serial with special prefix for parsing
    // Convert t_samples to microseconds for logging: t_us = (t_samples * 1000000ULL) / 16000
    uint64_t t_us = (t_samples * 1000000ULL) / 16000;
    printf("DEBUG_JSON:{\"location\":\"%s\",\"message\":\"%s\",\"data\":%s,\"timestamp\":%llu}\n",
           location, message, data_json, (unsigned long long)t_us);
}
// #endregion

WindowBank::WindowBank()
    : m_entries(nullptr)
    , m_count(0)
    , m_initialized(false)
{
}

WindowBank::~WindowBank() {
    if (m_entries != nullptr) {
        for (size_t i = 0; i < m_count; i++) {
            if (m_entries[i].lut != nullptr) {
                free(m_entries[i].lut);
            }
        }
        free(m_entries);
    }
}

bool WindowBank::init(const uint16_t* unique_N, size_t count) {
    if (m_initialized || unique_N == nullptr || count == 0) {
        return false;
    }

    // Allocate entries array
    m_entries = static_cast<WindowEntry*>(malloc(count * sizeof(WindowEntry)));
    if (m_entries == nullptr) {
        return false;
    }

    m_count = count;

    // Build window LUT for each unique N
    for (size_t i = 0; i < count; i++) {
        uint16_t N = unique_N[i];
        
        // Allocate LUT
        m_entries[i].lut = static_cast<int16_t*>(malloc(N * sizeof(int16_t)));
        if (m_entries[i].lut == nullptr) {
            // Cleanup on failure
            for (size_t j = 0; j < i; j++) {
                free(m_entries[j].lut);
            }
            free(m_entries);
            m_entries = nullptr;
            return false;
        }

        m_entries[i].N = N;
        
        // Build Hann window
        buildHannQ15(N, m_entries[i].lut);
        
        // Compute normalization factor
        m_entries[i].normFactor = computeNormFactor(m_entries[i].lut, N);
    }

    m_initialized = true;
    return true;
}

void WindowBank::buildHannQ15(uint16_t N, int16_t* out_lut) const {
    if (N == 0 || out_lut == nullptr) {
        return;
    }

    const float two_pi = 6.28318530717958647692f;
    const float denom = static_cast<float>(N - 1);

    for (uint16_t n = 0; n < N; n++) {
        const float x = static_cast<float>(n) / denom;
        const float w = 0.5f * (1.0f - std::cos(two_pi * x));  // 0..1
        
        // Convert to Q15 [0..32767]
        int32_t q = static_cast<int32_t>(std::lround(w * 32767.0f));
        if (q < 0) q = 0;
        if (q > 32767) q = 32767;
        out_lut[n] = static_cast<int16_t>(q);
    }
}

float WindowBank::computeNormFactor(const int16_t* lut, uint16_t N) const {
    if (lut == nullptr || N == 0) {
        return 0.0f;
    }

    // Compute window energy Ew = Σ(w[n]^2) in Q15 space
    // Convert Q15 to float, square, sum
    uint64_t energy_q30 = 0;  // Q30 accumulator (Q15 * Q15 = Q30)
    
    for (uint16_t n = 0; n < N; n++) {
        int32_t w_q15 = static_cast<int32_t>(lut[n]);
        int64_t w_sq = static_cast<int64_t>(w_q15) * static_cast<int64_t>(w_q15);
        energy_q30 += static_cast<uint64_t>(w_sq);
    }
    
    // Convert Q30 to float: Ew = energy_q30 / (16384^2)
    float Ew = static_cast<float>(energy_q30) / (16384.0f * 16384.0f);
    
    // Normalization factor: 1.0f / (Ew * N)
    // This makes Pnorm = Praw * normFactor comparable across different N
    float normFactor = 1.0f / (Ew * static_cast<float>(N));
    
    // #region agent log
    static uint32_t norm_log_counter = 0;
    if ((norm_log_counter++ % 125) == 0) {
        char norm_data[256];
        snprintf(norm_data, sizeof(norm_data),
            "{\"N\":%u,\"Ew\":%.6f,\"normFactor\":%.6f,\"hypothesisId\":\"I\"}",
            N, Ew, normFactor);
        debug_log(3, "WindowBank.cpp:computeNormFactor", "normalization_factor", norm_data, 0);
    }
    // #endregion
    
    return normFactor;
    
    return normFactor;
}

const int16_t* WindowBank::getHannQ15(uint16_t N) const {
    if (!m_initialized) {
        return nullptr;
    }

    for (size_t i = 0; i < m_count; i++) {
        if (m_entries[i].N == N) {
            return m_entries[i].lut;
        }
    }

    return nullptr;
}

float WindowBank::getNormFactor(uint16_t N) const {
    if (!m_initialized) {
        return 0.0f;
    }

    for (size_t i = 0; i < m_count; i++) {
        if (m_entries[i].N == N) {
            return m_entries[i].normFactor;
        }
    }

    return 0.0f;
}

} // namespace k1
} // namespace audio
} // namespace lightwaveos

