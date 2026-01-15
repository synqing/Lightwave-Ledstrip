/**
 * @file K1AudioFrontEnd.cpp
 * @brief K1 front-end orchestrator implementation
 *
 * @author LightwaveOS Team
 * @version 1.0.0 - K1 Migration
 */

#include "K1AudioFrontEnd.h"
#include <cstring>
#include <algorithm>
#include <cmath>
#include <set>
#include "../AudioDebugConfig.h"
#include <cstdio>

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

K1AudioFrontEnd::K1AudioFrontEnd()
    : m_hopIndex(0)
    , m_initialized(false)
{
    memset(m_rhythmMags, 0, sizeof(m_rhythmMags));
    memset(m_harmonyMags, 0, sizeof(m_harmonyMags));
    memset(m_rhythmMagsRaw, 0, sizeof(m_rhythmMagsRaw));
    memset(m_harmonyMagsRaw, 0, sizeof(m_harmonyMagsRaw));
    memset(m_chroma12, 0, sizeof(m_chroma12));
    memset(&m_currentFrame, 0, sizeof(m_currentFrame));
}

K1AudioFrontEnd::~K1AudioFrontEnd() {
    // Destructors handle cleanup
}

void K1AudioFrontEnd::extractUniqueN(const GoertzelBinSpec* specs, size_t num_bins, uint16_t* out_N, size_t* out_count) {
    if (specs == nullptr || out_N == nullptr || out_count == nullptr) {
        *out_count = 0;
        return;
    }

    std::set<uint16_t> unique_N_set;
    for (size_t i = 0; i < num_bins; i++) {
        unique_N_set.insert(specs[i].N);
    }

    size_t idx = 0;
    for (uint16_t N : unique_N_set) {
        if (idx < 10) {  // Safety limit
            out_N[idx++] = N;
        }
    }
    *out_count = idx;
}

bool K1AudioFrontEnd::init() {
    if (m_initialized) {
        return true;
    }

    // Initialize ring buffer (capacity >= N_MAX + HOP_SAMPLES + margin)
    const size_t ringCapacity = 4096;
    if (!m_ringBuffer.init(ringCapacity)) {
        return false;
    }

    // Extract unique N values from both banks
    uint16_t unique_N[20];
    size_t unique_count = 0;

    // Collect from rhythm bank
    std::set<uint16_t> all_N;
    for (size_t i = 0; i < 24; i++) {
        all_N.insert(kRhythmBins_16k_24[i].N);
    }
    for (size_t i = 0; i < 64; i++) {
        all_N.insert(kHarmonyBins_16k_64[i].N);
    }

    size_t idx = 0;
    for (uint16_t N : all_N) {
        if (idx < 20) {
            unique_N[idx++] = N;
        }
    }
    unique_count = idx;

    // Initialize window bank
    if (!m_windowBank.init(unique_N, unique_count)) {
        return false;
    }

    // Initialize rhythm bank
    if (!m_rhythmBank.init(kRhythmBins_16k_24, 24, &m_windowBank)) {
        return false;
    }

    // Initialize harmony bank
    if (!m_harmonyBank.init(kHarmonyBins_16k_64, 64, &m_windowBank)) {
        return false;
    }

    // Initialize noise floors
    m_rhythmNoiseFloor.init(RHYTHM_BINS, 1.5f, 0.999f);
    m_harmonyNoiseFloor.init(HARMONY_BINS, 1.5f, 0.999f);

    // Initialize AGC
    m_rhythmAGC.init(RHYTHM_BINS, AGCMode::Rhythm);
    m_harmonyAGC.init(HARMONY_BINS, AGCMode::Harmony);

    // Initialize novelty flux
    m_noveltyFlux.init();

    // Initialize chroma extractor
    m_chromaExtractor.init();

    // Initialize chroma stability
    m_chromaStability.init(8);

    m_hopIndex = 0;
    m_initialized = true;
    return true;
}

AudioFeatureFrame K1AudioFrontEnd::processHop(const AudioChunk& chunk, bool is_clipping) {
    AudioFeatureFrame frame;
    memset(&frame, 0, sizeof(frame));

    // #region agent log
    static uint32_t log_counter = 0;
    if ((log_counter++ % 125) == 0) {  // Log every ~1 second
        int16_t sample_min = 32767, sample_max = -32768;
        int64_t sample_sum = 0;
        for (size_t i = 0; i < chunk.n; i++) {
            if (chunk.samples[i] < sample_min) sample_min = chunk.samples[i];
            if (chunk.samples[i] > sample_max) sample_max = chunk.samples[i];
            sample_sum += chunk.samples[i];
        }
        float sample_mean = static_cast<float>(sample_sum) / static_cast<float>(chunk.n);
        char chunk_data[256];
        snprintf(chunk_data, sizeof(chunk_data),
            "{\"initialized\":%d,\"chunk_n\":%zu,\"sample_min\":%d,\"sample_max\":%d,\"sample_mean\":%.1f,\"sample_counter_end\":%llu,\"hypothesisId\":\"F\"}",
            m_initialized ? 1 : 0, chunk.n, sample_min, sample_max, sample_mean, (unsigned long long)chunk.sample_counter_end);
        debug_log(3, "K1AudioFrontEnd.cpp:processHop", "k1_entry", chunk_data, chunk.sample_counter_end);
    }
    // #endregion

    if (!m_initialized) {
        return frame;
    }

    // Update hop index
    m_hopIndex++;
    frame.hop_index = m_hopIndex;

    // Push chunk into ring buffer
    m_ringBuffer.push(chunk.samples, chunk.n, chunk.sample_counter_end);

    // Derive timestamps from sample counter
    frame.t_samples = chunk.sample_counter_end;
    frame.t_us = static_cast<float>(chunk.sample_counter_end * 1000000ULL) / static_cast<float>(FS_HZ);

    // Set clipping flag
    frame.is_clipping = is_clipping;

    // Process RhythmBank every hop
    m_rhythmBank.processAll(m_ringBuffer, m_rhythmMagsRaw);

    // #region agent log
    if ((log_counter % 125) == 1) {  // Log right after processing
        float raw_max = 0.0f, raw_sum = 0.0f;
        for (size_t i = 0; i < RHYTHM_BINS; i++) {
            if (m_rhythmMagsRaw[i] > raw_max) raw_max = m_rhythmMagsRaw[i];
            raw_sum += m_rhythmMagsRaw[i];
        }
        char raw_data[256];
        snprintf(raw_data, sizeof(raw_data),
            "{\"raw_max\":%.6f,\"raw_sum\":%.6f,\"hypothesisId\":\"G\"}",
            raw_max, raw_sum);
        debug_log(3, "K1AudioFrontEnd.cpp:processHop", "rhythm_raw", raw_data, chunk.sample_counter_end);
    }
    // #endregion

    // Apply noise floor subtraction
    m_rhythmNoiseFloor.update(m_rhythmMagsRaw, is_clipping);
    m_rhythmNoiseFloor.subtract(m_rhythmMagsRaw, m_rhythmMags, RHYTHM_BINS);

    // Apply AGC
    m_rhythmAGC.process(m_rhythmMags, m_rhythmMags, RHYTHM_BINS);

    // #region agent log
    if ((log_counter % 125) == 2) {  // Log after noise floor and AGC
        float processed_max = 0.0f, processed_sum = 0.0f;
        for (size_t i = 0; i < RHYTHM_BINS; i++) {
            if (m_rhythmMags[i] > processed_max) processed_max = m_rhythmMags[i];
            processed_sum += m_rhythmMags[i];
        }
        char processed_data[256];
        snprintf(processed_data, sizeof(processed_data),
            "{\"processed_max\":%.6f,\"processed_sum\":%.6f,\"hypothesisId\":\"G\"}",
            processed_max, processed_sum);
        debug_log(3, "K1AudioFrontEnd.cpp:processHop", "rhythm_processed", processed_data, chunk.sample_counter_end);
    }
    // #endregion

    // Copy to output frame
    memcpy(frame.rhythm_bins, m_rhythmMags, RHYTHM_BINS * sizeof(float));

    // Compute rhythm energy (RMS)
    float rhythm_energy_sum = 0.0f;
    for (size_t i = 0; i < RHYTHM_BINS; i++) {
        rhythm_energy_sum += m_rhythmMags[i] * m_rhythmMags[i];
    }
    frame.rhythm_energy = std::sqrt(rhythm_energy_sum / static_cast<float>(RHYTHM_BINS));

    // Compute novelty flux
    frame.rhythm_novelty = m_noveltyFlux.update(m_rhythmMags);
    
    // #region agent log
    if ((log_counter % 125) == 3) {  // Log after novelty
        char novelty_data[256];
        snprintf(novelty_data, sizeof(novelty_data),
            "{\"rhythm_novelty\":%.6f,\"rhythm_energy\":%.6f,\"hypothesisId\":\"H\"}",
            frame.rhythm_novelty, frame.rhythm_energy);
        debug_log(3, "K1AudioFrontEnd.cpp:processHop", "novelty_output", novelty_data, chunk.sample_counter_end);
    }
    // #endregion

    // Process HarmonyBank every 2 hops
    bool harmony_tick = (m_hopIndex % HARMONY_TICK_DIV == 0);
    frame.harmony_valid = harmony_tick;

    if (harmony_tick) {
        m_harmonyBank.processAll(m_ringBuffer, m_harmonyMagsRaw);

        // Apply noise floor subtraction
        m_harmonyNoiseFloor.update(m_harmonyMagsRaw, is_clipping);
        m_harmonyNoiseFloor.subtract(m_harmonyMagsRaw, m_harmonyMags, HARMONY_BINS);

        // Apply AGC
        m_harmonyAGC.process(m_harmonyMags, m_harmonyMags, HARMONY_BINS);

        // Copy to output frame
        memcpy(frame.harmony_bins, m_harmonyMags, HARMONY_BINS * sizeof(float));

        // Extract chroma
        m_chromaExtractor.extract(m_harmonyMags, m_chroma12);
        memcpy(frame.chroma12, m_chroma12, 12 * sizeof(float));

        // Compute key clarity
        frame.key_clarity = m_chromaExtractor.keyClarity(m_chroma12);

        // Update chroma stability
        frame.chroma_stability = m_chromaStability.update(m_chroma12);
    } else {
        // Harmony not valid - zero out fields
        memset(frame.harmony_bins, 0, HARMONY_BINS * sizeof(float));
        memset(frame.chroma12, 0, 12 * sizeof(float));
        frame.key_clarity = 0.0f;
        frame.chroma_stability = 0.0f;
    }

    // Detect silence (low RMS)
    frame.is_silence = (frame.rhythm_energy < 0.01f);

    // Overload policy: drop harmony if over budget (not implemented yet, always false)
    frame.overload = false;

    // Store current frame
    m_currentFrame = frame;

    return frame;
}

} // namespace k1
} // namespace audio
} // namespace lightwaveos

