#pragma once
#include "config/features.h"
#if FEATURE_AUDIO_SYNC

#include "AudioTime.h"
#include <cstdint>

namespace lightwaveos {
namespace audio {

/**
 * @brief Snapshot of musical grid state for effects
 */
struct MusicalGridSnapshot {
    AudioTime t;
    float bpm_smoothed = 120.0f;        // Smoothed BPM estimate
    float tempo_confidence = 0.0f;      // Confidence in BPM [0,1]
    uint64_t beat_index = 0;            // Total beats since start
    float beat_phase01 = 0.0f;          // Phase within beat [0,1)
    bool beat_tick = false;             // True on beat transition
    uint64_t bar_index = 0;             // Total bars since start
    float bar_phase01 = 0.0f;           // Phase within bar [0,1)
    bool downbeat_tick = false;         // True on downbeat (beat 1)
    uint8_t beat_in_bar = 0;            // Which beat in bar (0-3 for 4/4)
    uint8_t beats_per_bar = 4;          // Time signature numerator
};

/**
 * @brief Musical grid PLL - tracks beat phase and tempo
 *
 * IMPORTANT: Tick() must be called from the RENDER thread at 120 FPS.
 * Audio thread only calls OnTempoEstimate() and OnBeatObservation().
 */
class MusicalGrid {
public:
    MusicalGrid();

    /**
     * @brief Advance the grid phase (called from RENDER thread at 120 FPS)
     * @param now Current render time (extrapolated from audio)
     */
    void Tick(const AudioTime& now);

    /**
     * @brief Feed a new tempo estimate (called from AUDIO thread)
     * @param bpm Estimated tempo in BPM
     * @param confidence Confidence in estimate [0,1]
     */
    void OnTempoEstimate(float bpm, float confidence);

    /**
     * @brief Feed a beat observation (called from AUDIO thread)
     * @param now Time of the beat observation
     * @param strength Strength of the beat [0,1]
     * @param is_downbeat True if this is beat 1 of a bar
     */
    void OnBeatObservation(const AudioTime& now, float strength, bool is_downbeat);

    /**
     * @brief Get the latest snapshot for effects
     * @param out Destination for snapshot copy
     */
    void ReadLatest(MusicalGridSnapshot& out) const;

    /**
     * @brief Set beats per bar (time signature numerator)
     */
    void SetBeatsPerBar(uint8_t beats) { m_beatsPerBar = beats; }

private:
    MusicalGridSnapshot m_snapshot;
    AudioTime m_lastTickTime;
    float m_targetBpm = 120.0f;
    float m_bpmConfidence = 0.0f;
    uint8_t m_beatsPerBar = 4;

    // PLL state
    float m_phase = 0.0f;           // Current phase [0,1)
    float m_phaseIncrement = 0.0f;  // Phase increment per sample

    void updatePhaseIncrement();
};

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
