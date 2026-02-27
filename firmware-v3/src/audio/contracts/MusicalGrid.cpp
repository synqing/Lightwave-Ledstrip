#include "MusicalGrid.h"
#include <math.h>

namespace lightwaveos::audio {

static inline double fract(double x) {
    double i;
    return modf(x, &i);
}

float MusicalGrid::clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

float MusicalGrid::wrapHalf(float phase01) {
    // phase01 assumed in [0,1)
    if (phase01 > 0.5f) phase01 -= 1.0f; // -> (-0.5, 0.5]
    return phase01;
}

MusicalGrid::MusicalGrid() { Reset(); }

void MusicalGrid::Reset() {
    m_has_tick = false;
    m_last_tick_t = AudioTime{};
    m_bpm_target = 120.0f;
    m_bpm_smoothed = 120.0f;
    m_conf = 0.0f;
    m_beat_float = 0.0;
    m_prev_beat_index = 0;

    m_beats_per_bar = 4;
    m_beat_unit = 4;

    m_pending_beat = false;
    m_pending_strength = 0.0f;
    m_pending_is_downbeat = false;
    m_tuning = MusicalGridTuning{};

    MusicalGridSnapshot s;
    s.bpm_smoothed = m_bpm_smoothed;
    s.tempo_confidence = m_conf;
    s.beats_per_bar = m_beats_per_bar;
    s.beat_unit = m_beat_unit;
    m_snap.Publish(s);
}

void MusicalGrid::SetTimeSignature(uint8_t beats_per_bar, uint8_t beat_unit) {
    if (beats_per_bar == 0) beats_per_bar = 4;
    if (beat_unit == 0) beat_unit = 4;
    m_beats_per_bar = beats_per_bar;
    m_beat_unit = beat_unit;
}

void MusicalGrid::setTuning(const MusicalGridTuning& tuning) {
    m_tuning = tuning;
}

void MusicalGrid::OnTempoEstimate(const AudioTime& /*t*/, float bpm, float confidence01) {
    // Clamp BPM to a sane musical range.
    if (bpm < m_tuning.bpmMin) bpm = m_tuning.bpmMin;
    if (bpm > m_tuning.bpmMax) bpm = m_tuning.bpmMax;

    m_bpm_target = bpm;

    // Confidence is "availability of tempo lock", not a feature flag.
    confidence01 = clamp01(confidence01);
    if (confidence01 > m_conf) m_conf = confidence01;
}

void MusicalGrid::OnBeatObservation(const AudioTime& t, float strength01, bool is_downbeat) {
    m_pending_beat = true;
    m_pending_beat_t = t;
    m_pending_strength = clamp01(strength01);
    m_pending_is_downbeat = is_downbeat;

    // Store beat strength for effects (instant update, decays in Tick)
    m_lastBeatStrength = m_pending_strength;

    // Seeing a beat is strong evidence we're alive.
    if (m_pending_strength > m_conf) m_conf = m_pending_strength;
}

// ============================================================================
// K1-Lightwave Integration (Phase 3)
// ============================================================================

void MusicalGrid::updateFromK1(float bpm, float confidence, bool is_locked) {
    // K1 provides tempo estimates from its Goertzel resonator bank.
    // Feed directly into target BPM for PLL tracking.

    // Clamp BPM to configured range
    if (bpm < m_tuning.bpmMin) bpm = m_tuning.bpmMin;
    if (bpm > m_tuning.bpmMax) bpm = m_tuning.bpmMax;

    m_bpm_target = bpm;

    // K1 confidence drives how strongly we trust the tempo
    confidence = clamp01(confidence);

    // When K1 is locked, boost confidence slightly for stability
    if (is_locked && confidence > 0.5f) {
        confidence = confidence * 0.9f + 0.1f; // Slight boost toward 1.0
    }

    // Only update confidence upward (decay handled in Tick)
    if (confidence > m_conf) {
        m_conf = confidence;
    }
}

void MusicalGrid::onK1Beat(int beat_in_bar, bool is_downbeat, float strength) {
    // K1 beat events are already time-aligned by the PLL.
    // Create a synthetic AudioTime for "now" and trigger observation.

    // Use current state to estimate AudioTime
    // Note: In production, K1 would provide precise timestamp
    AudioTime now;
    now.sample_index = m_last_tick_t.sample_index;
    now.sample_rate_hz = m_last_tick_t.sample_rate_hz;
    now.monotonic_us = m_last_tick_t.monotonic_us;

    m_pending_beat = true;
    m_pending_beat_t = now;
    m_pending_strength = clamp01(strength);
    m_pending_is_downbeat = is_downbeat;

    // Store beat strength for effects (instant update, decays in Tick)
    m_lastBeatStrength = m_pending_strength;

    // Beat observation bumps confidence
    if (strength > m_conf) {
        m_conf = strength;
    }

    (void)beat_in_bar; // Reserved for future bar-level phase correction
}

// ============================================================================
// Trinity External Sync (Offline ML Analysis)
// ============================================================================

void MusicalGrid::injectExternalBeat(float bpm, float phase01, bool isTick, bool isDownbeat, int beatInBar)
{
    // Clamp BPM to configured range
    if (bpm < m_tuning.bpmMin) bpm = m_tuning.bpmMin;
    if (bpm > m_tuning.bpmMax) bpm = m_tuning.bpmMax;

    m_externalBpm = bpm;
    m_externalPhase01 = clamp01(phase01);
    m_externalBeatTick = isTick;
    m_externalDownbeatTick = isDownbeat;
    m_externalBeatInBar = (beatInBar >= 0 && beatInBar < m_beats_per_bar) ? beatInBar : 0;

    // Update internal state directly (bypass PLL)
    m_bpm_smoothed = bpm;
    m_bpm_target = bpm;
    m_conf = 1.0f;  // High confidence for pre-computed analysis

    // Update beat counter to match phase
    const double beat_float = (double)m_externalPhase01 + (double)(beatInBar + m_beats_per_bar * 0);
    m_beat_float = beat_float;
}

void MusicalGrid::setExternalSyncMode(bool enabled)
{
    m_externalSyncMode = enabled;
    if (!enabled) {
        // Reset to normal PLL mode
        m_externalBpm = 120.0f;
        m_externalPhase01 = 0.0f;
        m_externalBeatTick = false;
        m_externalDownbeatTick = false;
    }
}

void MusicalGrid::Tick(const AudioTime& render_now) {
    MusicalGridSnapshot s;
    s.t = render_now;
    s.beats_per_bar = m_beats_per_bar;
    s.beat_unit = m_beat_unit;

    s.beat_tick = false;
    s.downbeat_tick = false;

    // External sync mode: bypass PLL, use injected state directly
    if (m_externalSyncMode) {
        s.bpm_smoothed = m_externalBpm;
        s.tempo_confidence = 1.0f;
        s.beat_phase01 = m_externalPhase01;
        s.bar_phase01 = m_externalPhase01 / (float)m_beats_per_bar;
        s.beat_tick = m_externalBeatTick;
        s.downbeat_tick = m_externalDownbeatTick;
        s.beat_in_bar = (uint8_t)m_externalBeatInBar;
        s.beat_strength = m_externalBeatTick ? 1.0f : 0.0f;
        
        // Estimate beat/bar indices from phase
        const double beat_float = (double)m_externalPhase01 + (double)(m_externalBeatInBar + m_beats_per_bar * 0);
        s.beat_index = (uint64_t)floor(beat_float);
        s.bar_index = (uint64_t)floor(beat_float / (double)m_beats_per_bar);
        
        // Clear tick flags after reading (one-shot)
        m_externalBeatTick = false;
        m_externalDownbeatTick = false;
        
        m_snap.Publish(s);
        return;
    }

    if (!m_has_tick) {
        // First tick seeds timing without inventing history.
        m_has_tick = true;
        m_last_tick_t = render_now;
        m_prev_beat_index = 0;

        s.bpm_smoothed = m_bpm_smoothed;
        s.tempo_confidence = m_conf;
        s.beat_phase01 = 0.0f;
        s.bar_phase01 = 0.0f;
        s.beat_index = 0;
        s.bar_index = 0;
        s.beat_in_bar = 0;

        m_snap.Publish(s);
        return;
    }

    // dt in samples (sample-index is king)
    const int64_t ds = AudioTime_SamplesBetween(m_last_tick_t, render_now);
    const float dt_s = (render_now.sample_rate_hz > 0)
        ? (float)ds / (float)render_now.sample_rate_hz
        : 0.0f;

    if (dt_s < 0.0f) {
        // Time went backwards: ignore update to avoid phase explosions.
        m_snap.Publish(s);
        return;
    }

    // Smooth BPM toward target (render-domain time constant ~0.5s)
    const float tau = m_tuning.bpmTau;
    const float a = (tau > 0.0f) ? (1.0f - expf(-dt_s / tau)) : 1.0f;
    m_bpm_smoothed = m_bpm_smoothed + (m_bpm_target - m_bpm_smoothed) * a;

    // Confidence decays during silence/stalls (graceful degradation).
    const float conf_tau = m_tuning.confidenceTau;
    const float conf_decay = (conf_tau > 0.0f) ? expf(-dt_s / conf_tau) : 0.0f;
    m_conf *= conf_decay;

    // Beat strength decays faster (~150ms) for visual punch
    // tau = 0.15s means 63% decay in 150ms, full fade in ~500ms
    const float beat_tau = 0.15f;
    const float beat_decay = expf(-dt_s / beat_tau);
    m_lastBeatStrength *= beat_decay;

    // Integrate continuous beat counter (PLL freewheel)
    m_beat_float += (double)dt_s * ((double)m_bpm_smoothed / 60.0);

    // If a beat observation has become "current", apply phase correction.
    // Observation is time-stamped; we correct *now* based on predicted phase at obs time.
    if (m_pending_beat && (render_now.sample_index >= m_pending_beat_t.sample_index)) {
        const double sr = (double)render_now.sample_rate_hz;
        const double spb = (sr > 0.0) ? (sr * 60.0 / (double)m_bpm_smoothed) : 1.0; // samples per beat

        const double samples_back = (double)(render_now.sample_index - m_pending_beat_t.sample_index);
        const double beats_back = samples_back / spb;

        const double beat_at_obs = m_beat_float - beats_back;
        float phase_err01 = (float)fract(beat_at_obs);       // [0,1)
        float phase_err = wrapHalf(phase_err01);             // [-0.5,0.5)

        // Phase correction gain: strong beats pull harder, but never hard-jump.
        m_beat_float -= (double)(phase_err * m_tuning.phaseCorrectionGain * m_pending_strength);

        // Optional downbeat assist: align bar phase more aggressively when explicitly tagged.
        if (m_pending_is_downbeat && m_beats_per_bar > 0) {
            const double bar_at_obs = beat_at_obs / (double)m_beats_per_bar;
            float bar_err01 = (float)fract(bar_at_obs);
            float bar_err = wrapHalf(bar_err01);
            m_beat_float -= (double)(bar_err * (float)m_beats_per_bar * m_tuning.barCorrectionGain * m_pending_strength);
        }

        // Consuming an observation bumps confidence (it's live signal).
        if (m_pending_strength > m_conf) m_conf = m_pending_strength;

        m_pending_beat = false;
    }

    // Derive indices + phases
    const uint64_t beat_index = (uint64_t)floor(m_beat_float);
    const float beat_phase01 = (float)fract(m_beat_float); // [0,1)

    const double bar_float = (m_beats_per_bar > 0) ? (m_beat_float / (double)m_beats_per_bar) : 0.0;
    const uint64_t bar_index = (uint64_t)floor(bar_float);
    const float bar_phase01 = (float)fract(bar_float);

    // Generate ticks when crossing boundaries
    if (beat_index != m_prev_beat_index) {
        s.beat_tick = true;
        // If renderer stutters and we skipped multiple beats, still emit a single tick.
        m_prev_beat_index = beat_index;
    }

    const uint8_t beat_in_bar = (m_beats_per_bar > 0)
        ? (uint8_t)(beat_index % (uint64_t)m_beats_per_bar)
        : 0;

    if (s.beat_tick && beat_in_bar == 0) {
        s.downbeat_tick = true;
    }

    // Populate snapshot
    s.bpm_smoothed = m_bpm_smoothed;
    s.tempo_confidence = clamp01(m_conf);

    s.beat_index = beat_index;
    s.bar_index = bar_index;
    s.beat_in_bar = beat_in_bar;

    s.beat_phase01 = beat_phase01;
    s.bar_phase01 = bar_phase01;
    s.beat_strength = m_lastBeatStrength;

    // Commit timebase
    m_last_tick_t = render_now;

    m_snap.Publish(s);
}

} // namespace lightwaveos::audio
