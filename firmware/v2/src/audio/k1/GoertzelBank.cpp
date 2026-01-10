/**
 * @file GoertzelBank.cpp
 * @brief Goertzel bank implementation with group processing
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#include "GoertzelBank.h"
#include "GoertzelKernel.h"
#include <cstdlib>
#include <cstring>
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

GoertzelBank::GoertzelBank()
    : m_specs(nullptr)
    , m_num_bins(0)
    , m_windowBank(nullptr)
    , m_scratch(nullptr)
    , m_initialized(false)
{
}

GoertzelBank::~GoertzelBank() {
    if (m_scratch != nullptr) {
        free(m_scratch);
    }
}

bool GoertzelBank::init(const GoertzelBinSpec* specs, size_t num_bins, const WindowBank* windowBank) {
    if (m_initialized || specs == nullptr || num_bins == 0 || windowBank == nullptr) {
        return false;
    }

    // Allocate scratch buffer (size N_MAX)
    m_scratch = static_cast<int16_t*>(malloc(N_MAX * sizeof(int16_t)));
    if (m_scratch == nullptr) {
        return false;
    }

    m_specs = specs;
    m_num_bins = num_bins;
    m_windowBank = windowBank;

    // Build groups
    if (!m_groups.buildGroups(specs, num_bins)) {
        free(m_scratch);
        m_scratch = nullptr;
        return false;
    }

    m_initialized = true;
    return true;
}

void GoertzelBank::processGroup(const BinGroup& group, const AudioRingBuffer& ring, float* out_mags) {
    if (!m_initialized || group.N == 0 || group.indices.empty()) {
        return;
    }

    // CRITICAL: CopyLast + window once per N
    // 1. Copy last N samples from ring buffer
    // #region agent log
    static uint32_t copy_log_counter = 0;
    if ((copy_log_counter++ % 125) == 0) {
        char copy_data[256];
        snprintf(copy_data, sizeof(copy_data),
            "{\"N\":%u,\"copy_success\":%d,\"hypothesisId\":\"K\"}",
            group.N, ring.copyLast(group.N, m_scratch) ? 1 : 0);
        debug_log(3, "GoertzelBank.cpp:processGroup", "copyLast_check", copy_data, 0);
    }
    // #endregion
    
    if (!ring.copyLast(group.N, m_scratch)) {
        return;
    }
    
    // #region agent log
    static uint32_t sample_log_counter = 0;
    if ((sample_log_counter++ % 125) == 0) {
        int16_t sample_min = 32767, sample_max = -32768;
        int32_t sample_sum = 0;
        for (uint16_t n = 0; n < group.N; n++) {
            if (m_scratch[n] < sample_min) sample_min = m_scratch[n];
            if (m_scratch[n] > sample_max) sample_max = m_scratch[n];
            sample_sum += m_scratch[n];
        }
        float sample_mean = static_cast<float>(sample_sum) / static_cast<float>(group.N);
        char sample_data[256];
        snprintf(sample_data, sizeof(sample_data),
            "{\"N\":%u,\"sample_min\":%d,\"sample_max\":%d,\"sample_mean\":%.1f,\"hypothesisId\":\"K\"}",
            group.N, sample_min, sample_max, sample_mean);
        debug_log(3, "GoertzelBank.cpp:processGroup", "windowed_samples", sample_data, 0);
    }
    // #endregion

    // 2. Apply window (get LUT for this N)
    const int16_t* window_lut = m_windowBank->getHannQ15(group.N);
    if (window_lut == nullptr) {
        return;
    }

    // Window the samples (Q15 multiply, convert to int16)
    for (uint16_t n = 0; n < group.N; n++) {
        int32_t sample = static_cast<int32_t>(m_scratch[n]);
        int32_t window = static_cast<int32_t>(window_lut[n]);
        // Multiply Q15 * int16, result in Q15, then shift to int16
        int32_t windowed = (sample * window) >> 15;
        // Clamp to int16 range
        if (windowed > 32767) windowed = 32767;
        if (windowed < -32768) windowed = -32768;
        m_scratch[n] = static_cast<int16_t>(windowed);
    }

    // 3. Get normalization factor for this N
    float normFactor = m_windowBank->getNormFactor(group.N);

    // 4. Run kernel for all bins with this N
    // #region agent log
    static uint32_t goertzel_log_counter = 0;
    float first_raw_mag = 0.0f, first_norm_mag = 0.0f;
    bool logged_first = false;
    // #endregion
    
    for (uint8_t bin_idx : group.indices) {
        const GoertzelBinSpec& spec = m_specs[bin_idx];
        
        // Process through Goertzel kernel
        float raw_mag = GoertzelKernel::process(m_scratch, group.N, spec.coeff_q14);
        
        // Apply normalization
        float norm_mag = raw_mag * normFactor;
        
        // #region agent log
        if (!logged_first && (goertzel_log_counter++ % 125) == 0) {
            first_raw_mag = raw_mag;
            first_norm_mag = norm_mag;
            logged_first = true;
            char goertzel_data[256];
            snprintf(goertzel_data, sizeof(goertzel_data),
                "{\"N\":%u,\"raw_mag\":%.6f,\"normFactor\":%.6f,\"norm_mag\":%.6f,\"coeff_q14\":%d,\"hypothesisId\":\"I\"}",
                group.N, raw_mag, normFactor, norm_mag, spec.coeff_q14);
            debug_log(3, "GoertzelBank.cpp:processGroup", "goertzel_output", goertzel_data, 0);
        }
        // #endregion
        
        // Store in output array
        out_mags[bin_idx] = norm_mag;
    }
}

void GoertzelBank::processAll(const AudioRingBuffer& ring, float* out_mags) {
    if (!m_initialized || out_mags == nullptr) {
        return;
    }

    // Process each group (CopyLast + window once per group)
    size_t num_groups = m_groups.getGroupCount();
    for (size_t g = 0; g < num_groups; g++) {
        const BinGroup* group = m_groups.getGroup(g);
        if (group != nullptr) {
            processGroup(*group, ring, out_mags);
        }
    }
}

} // namespace k1
} // namespace audio
} // namespace lightwaveos

