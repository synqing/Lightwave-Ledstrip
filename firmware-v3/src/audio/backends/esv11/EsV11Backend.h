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

    // Runtime-tuneable tempo stabilisation parameters.
    // Modify via serial commands (e.g. "tempo gate_base 1.3") without reflashing.
    struct TempoParams {
        float gateBase      = 1.2f;   // Minimum magnitude ratio for generic bin switch
        float gateScale     = 0.8f;   // Additional gate from stability (max gate = base+scale)
        float gateTau       = 4.0f;   // Seconds for gate to reach halfway to max
        float confFloor     = 0.10f;  // Below this raw conf, freeze bin switches
        float validationThr = 0.08f;  // Raw conf must exceed this to validate a bin
        float stabilityTau  = 4.0f;   // Seconds for stability confidence to reach 0.5
        uint32_t holdUs     = 200000; // Minimum hold time before allowing switch (us)
        uint8_t octaveRuns  = 4;      // Consecutive hops for octave promotion
        float decayFloor    = 0.005f; // Magnitude below this = "decayed" (escape hatch + freeze)
        float octRatioLo    = 1.9f;   // Lower bound for octave detection (candidate/stable)
        float octRatioHi    = 2.1f;   // Upper bound for octave detection
        float wsSepFloor    = 0.001f; // Winner separation denominator floor
        float confDecay     = 0.994f; // Confidence smoothing decay factor (per 1/60s)
        uint32_t genericPersistUs = 1000000; // Generic candidate must persist this long before promotion (us)
    };
    TempoParams m_tp;

public:
    TempoParams& tempoParams() { return m_tp; }
    const TempoParams& tempoParams() const { return m_tp; }

private:
    uint64_t m_sampleIndex = 0;
    uint64_t m_lastGpuTickUs = 0;

    // Beat tracking: free-running phase accumulator (pre-v3 EsBeatClock approach)
    uint16_t m_stableTopBin = 0;       // Hysteresis-stabilised bin selection
    uint64_t m_stableBinLockedUs = 0;  // Timestamp when stable bin was last changed
    uint16_t m_octaveCandBin = 0;      // Last octave-related candidate seen
    uint8_t  m_octaveCandRuns = 0;     // Consecutive windows octave candidate persisted
    uint16_t m_genericCandBin = 0;     // Last generic candidate seen (for persistence check)
    uint64_t m_genericCandFirstUs = 0; // Timestamp when generic candidate first appeared
    bool     m_stableBinValidated = false; // True once raw conf > threshold on current bin
    float m_beatPhase = 0.0f;          // 0..1, wraps at 1.0 → beat_tick
    uint64_t m_lastRefreshUs = 0;      // Delta timing for phase accumulator
    uint8_t m_beatInBar = 0;

    // Cached outputs
    EsV11Outputs m_latest{};

    void tickEsGpu(float delta);
    void refreshOutputs(uint64_t now_us);
};

} // namespace lightwaveos::audio::esv11

#endif // FEATURE_AUDIO_SYNC && FEATURE_AUDIO_BACKEND_ESV11
