#pragma once
#include <stdint.h>
#include <math.h>
#include "AudioTime.h"

namespace lightwaveos::audio {

static constexpr uint8_t CONTROLBUS_NUM_BANDS  = 8;
static constexpr uint8_t CONTROLBUS_NUM_CHROMA = 12;
static constexpr uint8_t CONTROLBUS_WAVEFORM_N = 128;  // Sensory Bridge NATIVE_RESOLUTION waveform points

/**
 * @brief Raw (unsmoothed) per-hop measurements produced by the audio thread.
 */
struct ControlBusRawInput {
    float rms = 0.0f;   // 0..1
    float flux = 0.0f;  // 0..1 (novelty proxy)

    float bands[CONTROLBUS_NUM_BANDS] = {0};     // 0..1
    float chroma[CONTROLBUS_NUM_CHROMA] = {0};   // 0..1 (optional in Phase 2)
    int16_t waveform[CONTROLBUS_WAVEFORM_N] = {0};  // Time-domain samples (int16_t range: -32768 to 32767)
};

/**
 * @brief FAST LANE control frame (125 Hz / 8ms updates).
 *
 * Published via SnapshotBuffer<ControlBusFrame> for texture-reactive effects.
 * Contains smoothed DSP signals updated every HOP_FAST (128 samples).
 *
 * Thread Safety: Consumed BY VALUE in render domain.
 */
struct ControlBusFrame {
    AudioTime t;              ///< Timestamp at end of hop (sample_index is king)
    uint32_t hop_seq = 0;     ///< Monotonic hop counter for staleness detection

    float rms = 0.0f;         ///< RMS energy [0,1]
    float flux = 0.0f;        ///< Spectral flux / novelty [0,1]

    float bands[CONTROLBUS_NUM_BANDS] = {0};      ///< 8-band energy [0,1]
    float chroma[CONTROLBUS_NUM_CHROMA] = {0};    ///< 12-class chromagram [0,1]
    int16_t waveform[CONTROLBUS_WAVEFORM_N] = {0}; ///< Time-domain waveform samples
};

/**
 * @brief BEAT LANE observation frame (62.5 Hz / 16ms updates).
 *
 * Published via SnapshotBuffer<BeatObsFrame> for beat-locked effects.
 * Contains beat/tempo tracking results from the BeatTracker.
 *
 * Thread Safety: Consumed BY VALUE in render domain.
 * The RendererActor applies these observations to its owned MusicalGrid.
 */
struct BeatObsFrame {
    AudioTime t_obs;           ///< Timestamp when observation was made

    // Beat detection
    bool beat_pulse = false;   ///< True for one frame when beat detected
    float beat_strength = 0.0f; ///< Beat confidence/strength [0,1]
    bool downbeat_pulse = false; ///< True when beat is also a downbeat (beat 1)

    // Tempo tracking
    bool tempo_valid = false;  ///< True when tempo estimate is reliable
    float bpm_est = 120.0f;    ///< Current BPM estimate
    float tempo_conf = 0.0f;   ///< Tempo tracking confidence [0,1]

    // Band-weighted spectral flux (for debugging/visualization)
    float weighted_flux = 0.0f; ///< Bass-weighted spectral flux used for onset detection
};

/**
 * @brief Smoothed DSP control signals at hop cadence.
 *
 * Audio thread owns a ControlBus instance; each hop:
 *  - UpdateFromHop(now, raw)
 *  - Publish(GetFrame()) to SnapshotBuffer<ControlBusFrame>
 */
class ControlBus {
public:
    ControlBus();

    void Reset();

    void UpdateFromHop(const AudioTime& now, const ControlBusRawInput& raw);

    ControlBusFrame GetFrame() const { return m_frame; }

private:
    ControlBusFrame m_frame{};

    // One-pole smoothing state
    float m_rms_s = 0.0f;
    float m_flux_s = 0.0f;
    float m_bands_s[CONTROLBUS_NUM_BANDS] = {0};
    float m_chroma_s[CONTROLBUS_NUM_CHROMA] = {0};

    // Tunables (Phase-2 defaults; keep them stable until you port Tab5 constants)
    float m_alpha_fast = 0.35f;  // fast response
    float m_alpha_slow = 0.12f;  // slower response
};

} // namespace lightwaveos::audio
