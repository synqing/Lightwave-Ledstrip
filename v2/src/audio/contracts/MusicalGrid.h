#pragma once
#include <stdint.h>
#include <math.h>
#include "AudioTime.h"
#include "SnapshotBuffer.h"

namespace lightwaveos::audio {

/**
 * @brief Snapshot consumed by effects (BY VALUE).
 */
struct MusicalGridSnapshot {
    AudioTime t;

    float bpm_smoothed = 120.0f;
    float tempo_confidence = 0.0f; // 0..1

    // Phases (0..1)
    float beat_phase01 = 0.0f;
    float bar_phase01  = 0.0f;

    // Ticks (true only on frames where boundary crossed / observation applied)
    bool beat_tick = false;
    bool downbeat_tick = false;

    uint64_t beat_index = 0;
    uint64_t bar_index  = 0;

    uint8_t beats_per_bar = 4; // e.g. 4/4
    uint8_t beat_unit     = 4;
    uint8_t beat_in_bar   = 0; // 0..beats_per_bar-1
};

/**
 * @brief Tunable musical grid parameters (render-domain PLL)
 */
struct MusicalGridTuning {
    float bpmMin = 30.0f;
    float bpmMax = 300.0f;
    float bpmTau = 0.50f;
    float confidenceTau = 1.00f;
    float phaseCorrectionGain = 0.35f;
    float barCorrectionGain = 0.20f;
};

/**
 * @brief Render-domain PLL-style musical grid.
 *
 * Non-negotiable: Tick() is called ONLY by the renderer at ~120 FPS.
 * Audio thread ONLY calls OnTempoEstimate() / OnBeatObservation().
 *
 * Internally publishes snapshots to a SnapshotBuffer so you can ReadLatest(mg) BY VALUE.
 */
class MusicalGrid {
public:
    MusicalGrid();

    void Reset();

    void SetTimeSignature(uint8_t beats_per_bar, uint8_t beat_unit);

    void setTuning(const MusicalGridTuning& tuning);
    MusicalGridTuning getTuning() const { return m_tuning; }

    // Audio-domain observations (do NOT call Tick from audio thread)
    void OnTempoEstimate(const AudioTime& t, float bpm, float confidence01);
    void OnBeatObservation(const AudioTime& t, float strength01, bool is_downbeat);

    // Render-domain update (120 FPS)
    void Tick(const AudioTime& render_now);

    // BY-VALUE snapshot read (thread-safe pattern, even if you later share it cross-core)
    uint32_t ReadLatest(MusicalGridSnapshot& out) const { return m_snap.ReadLatest(out); }

private:
    SnapshotBuffer<MusicalGridSnapshot> m_snap;

    // Core state
    bool m_has_tick = false;
    AudioTime m_last_tick_t{};

    float m_bpm_target = 120.0f;
    float m_bpm_smoothed = 120.0f;
    float m_conf = 0.0f;

    double m_beat_float = 0.0;      // continuous beat counter
    uint64_t m_prev_beat_index = 0; // for beat_tick generation

    uint8_t m_beats_per_bar = 4;
    uint8_t m_beat_unit = 4;

    // Pending beat observation (time-stamped)
    bool m_pending_beat = false;
    AudioTime m_pending_beat_t{};
    float m_pending_strength = 0.0f;
    bool m_pending_is_downbeat = false;

    MusicalGridTuning m_tuning;

private:
    static float clamp01(float x);
    static float wrapHalf(float phase01); // maps [0,1) to [-0.5,0.5)
};

} // namespace lightwaveos::audio
