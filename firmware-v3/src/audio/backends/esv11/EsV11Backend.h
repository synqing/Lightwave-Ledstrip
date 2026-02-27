/**
 * @file EsV11Backend.h
 * @brief Emotiscope v1.1_320 end-to-end audio backend (capture + DSP + tempo).
 *
 * This backend is designed to preserve ES v1.1 behaviour as closely as possible.
 * It owns the ES vendor state and exposes a minimal interface for AudioActor.
 *
 * British English: centre, colour, initialise.
 */

#pragma once

#include "config/features.h"

#if FEATURE_AUDIO_SYNC && FEATURE_AUDIO_BACKEND_ESV11

#include <cstdint>

namespace lightwaveos::audio::esv11 {

struct EsV11Outputs {
    // Timebase
    uint64_t now_us = 0;
    uint32_t now_ms = 0;
    uint64_t sample_index = 0;   // Monotonic sample counter (12.8kHz)

    // Spectrum/chroma/VU/novelty
    float spectrogram_smooth[64] = {0};     // 0..1
    float chromagram[12] = {0};             // 0..1
    float vu_level = 0.0f;                  // 0..1
    float novelty_norm_last = 0.0f;         // 0..1 (latest novelty_curve_normalized sample)

    // Tempo/beat (derived from ES tempi bank)
    float top_bpm = 120.0f;
    float tempo_confidence = 0.0f;          // 0..1
    float phase_radians = 0.0f;             // [-pi, pi]
    bool beat_tick = false;                 // True on wrap event of selected tempo
    float beat_strength = 0.0f;             // 0..1

    // Waveform: last 128 time-domain samples, int16 range
    int16_t waveform[128] = {0};
};

class EsV11Backend {
public:
    EsV11Backend() = default;

    bool init();

    /**
     * @brief Read a 64-sample chunk from I2S and update ES pipeline state.
     * @param now_us Current micros() timestamp.
     * @return true if chunk was processed.
     */
    bool readAndProcessChunk(uint64_t now_us);

    /**
     * @brief Get the latest derived outputs for contract publishing.
     * @param out Output struct filled by value.
     */
    void getLatestOutputs(EsV11Outputs& out) const;

private:
    uint64_t m_sampleIndex = 0;
    uint64_t m_lastGpuTickUs = 0;

    // Beat tracking for selected tempo (wrap detection)
    uint16_t m_lastTopTempoIndex = 0;
    bool m_lastTopPhaseInverted = false;
    uint8_t m_beatInBar = 0;

    // Cached outputs
    EsV11Outputs m_latest{};

    void tickEsGpu(float delta);
    void refreshOutputs(uint64_t now_us);
};

} // namespace lightwaveos::audio::esv11

#endif // FEATURE_AUDIO_SYNC && FEATURE_AUDIO_BACKEND_ESV11
