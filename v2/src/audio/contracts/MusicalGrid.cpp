#include "MusicalGrid.h"
#if FEATURE_AUDIO_SYNC

#include <cmath>

namespace lightwaveos {
namespace audio {

MusicalGrid::MusicalGrid() {
    updatePhaseIncrement();
}

void MusicalGrid::updatePhaseIncrement() {
    // Phase increment per sample = (BPM / 60) / sample_rate
    // For 120 BPM at 16kHz: (120/60) / 16000 = 0.000125
    m_phaseIncrement = (m_targetBpm / 60.0f) / static_cast<float>(m_snapshot.t.sample_rate_hz);
}

void MusicalGrid::Tick(const AudioTime& now) {
    // Compute elapsed samples since last tick
    int64_t sampleDelta = 0;
    if (m_lastTickTime.sample_index > 0) {
        sampleDelta = static_cast<int64_t>(now.sample_index - m_lastTickTime.sample_index);
    }
    m_lastTickTime = now;

    // Advance phase
    bool wasBeat = false;
    uint8_t prevBeat = m_snapshot.beat_in_bar;

    for (int64_t i = 0; i < sampleDelta; ++i) {
        m_phase += m_phaseIncrement;

        // Handle beat transition
        if (m_phase >= 1.0f) {
            m_phase -= 1.0f;
            m_snapshot.beat_index++;
            wasBeat = true;

            // Update bar tracking
            m_snapshot.beat_in_bar = (m_snapshot.beat_in_bar + 1) % m_snapshot.beats_per_bar;
            if (m_snapshot.beat_in_bar == 0) {
                m_snapshot.bar_index++;
                m_snapshot.downbeat_tick = true;
            }
        }
    }

    m_snapshot.t = now;
    m_snapshot.beat_phase01 = m_phase;
    m_snapshot.beat_tick = wasBeat;

    // Compute bar phase (linear interpolation across beats)
    float beatsInBar = static_cast<float>(m_snapshot.beats_per_bar);
    m_snapshot.bar_phase01 = (static_cast<float>(m_snapshot.beat_in_bar) + m_phase) / beatsInBar;

    // Clear downbeat tick after one frame
    if (!wasBeat || m_snapshot.beat_in_bar != 0) {
        m_snapshot.downbeat_tick = false;
    }

    // Update smoothed BPM
    m_snapshot.bpm_smoothed = m_targetBpm;
    m_snapshot.tempo_confidence = m_bpmConfidence;
}

void MusicalGrid::OnTempoEstimate(float bpm, float confidence) {
    // Simple IIR filter for BPM smoothing
    const float alpha = 0.1f; // Smoothing factor
    m_targetBpm = m_targetBpm * (1.0f - alpha) + bpm * alpha;
    m_bpmConfidence = confidence;
    updatePhaseIncrement();
}

void MusicalGrid::OnBeatObservation(const AudioTime& now, float strength, bool is_downbeat) {
    // PLL phase correction
    // If we detect a beat and our phase is not near 0, nudge it toward 0
    const float tolerance = 0.1f; // Within 10% of beat
    if (m_phase > tolerance && m_phase < (1.0f - tolerance)) {
        // Apply phase correction proportional to beat strength
        float correction = (m_phase < 0.5f) ? -m_phase : (1.0f - m_phase);
        m_phase += correction * strength * 0.2f; // Gentle nudge

        // Clamp phase to [0, 1)
        if (m_phase < 0.0f) m_phase += 1.0f;
        if (m_phase >= 1.0f) m_phase -= 1.0f;
    }

    // If downbeat detected, align bar phase
    if (is_downbeat && m_snapshot.beat_in_bar != 0) {
        m_snapshot.beat_in_bar = 0;
        m_snapshot.bar_index++;
    }
}

void MusicalGrid::ReadLatest(MusicalGridSnapshot& out) const {
    out = m_snapshot;
}

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
