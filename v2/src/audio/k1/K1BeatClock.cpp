/**
 * @file K1BeatClock.cpp
 * @brief K1-Lightwave Stage 4: Beat Clock (PLL) Implementation
 *
 * Ported from Tab5.DSP for ESP32-S3 Lightwave v2 integration.
 */

#include "K1BeatClock.h"
#include <cmath>

namespace lightwaveos {
namespace audio {
namespace k1 {

void K1BeatClock::begin(uint32_t now_ms) {
    phase_rad_ = 0.0f;
    phase01_ = 0.0f;
    bpm_ = 120.0f;
    ref_phase_rad_ = 0.0f;
    phase_error_ = 0.0f;
    freq_error_ = 0.0f;
    last_beat_ms_ = now_ms;
    last_tick_ = false;
    last_update_ms_ = now_ms;
    locked_ = false;
    confidence_ = 0.0f;
}

float K1BeatClock::wrapPhase(float p) const {
    if (!std::isfinite(p)) return 0.0f;
    float wrapped = fmodf(p + (float)M_PI, 2.0f * (float)M_PI);
    if (wrapped < 0.0f) wrapped += 2.0f * (float)M_PI;
    return wrapped - (float)M_PI;
}

float K1BeatClock::resonatorPhaseToPhase01(float rp) const {
    float p01 = (rp + (float)M_PI) / (2.0f * (float)M_PI);
    if (p01 < 0.0f) p01 += 1.0f;
    if (p01 >= 1.0f) p01 -= 1.0f;
    return p01;
}

void K1BeatClock::updateFromTactus(const K1TactusFrame& in,
                                    uint32_t now_ms,
                                    K1BeatClockState& out) {
    last_update_ms_ = now_ms;
    locked_ = in.locked;
    confidence_ = in.confidence;

    if (!in.locked || in.bpm < (float)ST2_BPM_MIN) {
        out.t_ms = now_ms;
        out.phase01 = phase01_;
        out.beat_tick = false;
        out.bpm = bpm_;
        out.confidence = 0.0f;
        out.locked = false;
        out.phase_error = 0.0f;
        out.freq_error = 0.0f;
        return;
    }

    ref_phase_rad_ = in.phase_hint;

    float bpm_diff = fabsf(in.bpm - bpm_);
    bool big_change = bpm_diff > 5.0f;

    if (big_change) {
        phase_rad_ = ref_phase_rad_;
        bpm_ = in.bpm;
        phase_error_ = 0.0f;
        freq_error_ = 0.0f;
    } else {
        bpm_ = 0.95f * bpm_ + 0.05f * in.bpm;

        phase_error_ = wrapPhase(ref_phase_rad_ - phase_rad_);

        float phase_correction = phase_error_ * ST4_PHASE_GAIN;
        if (phase_correction > ST4_MAX_PHASE_CORR * 2.0f * (float)M_PI) {
            phase_correction = ST4_MAX_PHASE_CORR * 2.0f * (float)M_PI;
        }
        if (phase_correction < -ST4_MAX_PHASE_CORR * 2.0f * (float)M_PI) {
            phase_correction = -ST4_MAX_PHASE_CORR * 2.0f * (float)M_PI;
        }
        phase_rad_ += phase_correction;

        freq_error_ = 0.9f * freq_error_ + 0.1f * phase_error_;
        float freq_correction = freq_error_ * ST4_FREQ_GAIN * 60.0f;
        if (freq_correction > ST4_MAX_FREQ_CORR) freq_correction = ST4_MAX_FREQ_CORR;
        if (freq_correction < -ST4_MAX_FREQ_CORR) freq_correction = -ST4_MAX_FREQ_CORR;
        bpm_ += freq_correction;
    }

    if (bpm_ < (float)ST2_BPM_MIN) bpm_ = (float)ST2_BPM_MIN;
    if (bpm_ > (float)ST2_BPM_MAX) bpm_ = (float)ST2_BPM_MAX;

    out.t_ms = now_ms;
    out.phase01 = k1_wrap01(phase01_);
    out.beat_tick = last_tick_;
    out.bpm = bpm_;
    out.confidence = k1_clamp01(confidence_);
    out.locked = locked_;
    out.phase_error = phase_error_;
    out.freq_error = freq_error_;

    last_tick_ = false;
}

void K1BeatClock::tick(uint32_t now_ms, float delta_sec, K1BeatClockState& out) {
    float beats_per_sec = bpm_ / 60.0f;
    float rad_per_sec = beats_per_sec * 2.0f * (float)M_PI;

    phase_rad_ += rad_per_sec * delta_sec;

    bool tick = false;
    while (phase_rad_ >= 2.0f * (float)M_PI) {
        phase_rad_ -= 2.0f * (float)M_PI;
        tick = true;
    }
    while (phase_rad_ < 0.0f) {
        phase_rad_ += 2.0f * (float)M_PI;
    }

    // Debounce
    float beat_period_ms = 60000.0f / bpm_;
    uint32_t debounce_ms = (uint32_t)(beat_period_ms * ST4_BEAT_DEBOUNCE_RATIO);
    if (tick && (now_ms - last_beat_ms_) < debounce_ms) {
        tick = false;
    }
    if (tick) {
        last_beat_ms_ = now_ms;
    }

    phase01_ = phase_rad_ / (2.0f * (float)M_PI);
    if (phase01_ < 0.0f) phase01_ += 1.0f;
    if (phase01_ >= 1.0f) phase01_ -= 1.0f;

    out.t_ms = now_ms;
    out.phase01 = k1_wrap01(phase01_);
    out.beat_tick = tick;
    out.bpm = bpm_;
    out.confidence = k1_clamp01(confidence_);
    out.locked = locked_;
    out.phase_error = phase_error_;
    out.freq_error = freq_error_;

    last_tick_ = tick;
}

} // namespace k1
} // namespace audio
} // namespace lightwaveos
