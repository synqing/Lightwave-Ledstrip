#include "MusicalGrid.h"

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

void MusicalGrid::OnTempoEstimate(const AudioTime& /*t*/, float bpm, float confidence01) {
    // Clamp BPM to a sane musical range.
    if (bpm < 30.0f) bpm = 30.0f;
    if (bpm > 300.0f) bpm = 300.0f;

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

    // Seeing a beat is strong evidence we're alive.
    if (m_pending_strength > m_conf) m_conf = m_pending_strength;
}

void MusicalGrid::Tick(const AudioTime& render_now) {
    MusicalGridSnapshot s;
    s.t = render_now;
    s.beats_per_bar = m_beats_per_bar;
    s.beat_unit = m_beat_unit;

    s.beat_tick = false;
    s.downbeat_tick = false;

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
    const float tau = 0.50f;
    const float a = (tau > 0.0f) ? (1.0f - expf(-dt_s / tau)) : 1.0f;
    m_bpm_smoothed = m_bpm_smoothed + (m_bpm_target - m_bpm_smoothed) * a;

    // Confidence decays during silence/stalls (graceful degradation).
    const float conf_tau = 1.00f;
    const float conf_decay = (conf_tau > 0.0f) ? expf(-dt_s / conf_tau) : 0.0f;
    m_conf *= conf_decay;

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
        const float k_phase = 0.35f;
        m_beat_float -= (double)(phase_err * k_phase * m_pending_strength);

        // Optional downbeat assist: align bar phase more aggressively when explicitly tagged.
        if (m_pending_is_downbeat && m_beats_per_bar > 0) {
            const double bar_at_obs = beat_at_obs / (double)m_beats_per_bar;
            float bar_err01 = (float)fract(bar_at_obs);
            float bar_err = wrapHalf(bar_err01);
            const float k_bar = 0.20f;
            m_beat_float -= (double)(bar_err * (float)m_beats_per_bar * k_bar * m_pending_strength);
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

    // Commit timebase
    m_last_tick_t = render_now;

    m_snap.Publish(s);
}

} // namespace lightwaveos::audio
