/**
 * @file TranslationEngine.cpp
 * @brief Per-hop perceptual translation from analyzed audio to scene semantics.
 */

#include "TranslationEngine.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#ifndef NATIVE_BUILD
#include <esp_timer.h>
#endif

namespace lightwaveos::audio {
namespace {

constexpr float kDtMin = 0.0005f;
constexpr float kDtMax = 0.05f;

constexpr float kTempoConfidenceReliable = 0.35f;
constexpr uint8_t kTempoConsecutiveHopsRequired = 3;
constexpr float kTempoHoldSeconds = 2.0f;
constexpr float kTempoMinBpm = 60.0f;
constexpr float kTempoMaxBpm = 180.0f;
constexpr float kFallbackBpm = 120.0f;

constexpr float kBeatPulseTau = 0.150f;
constexpr float kMotionTau = 0.250f;
constexpr float kDriftTau = 0.350f;

constexpr float kHardBoundaryScore = 0.18f;
constexpr float kChordBoundaryConfidence = 0.40f;
constexpr float kStyleRhythmicConfidence = 0.55f;

constexpr float kSceneBrightnessMin = 0.15f;
constexpr float kSceneBrightnessMax = 1.25f;
constexpr float kSceneRateMin = 0.60f;
constexpr float kSceneRateMax = 1.80f;
constexpr float kSceneDepthMin = 0.00f;
constexpr float kSceneDepthMax = 1.00f;

constexpr size_t kScratchCapacity = 8;

inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

inline float clamp01(float v) {
    return clampf(v, 0.0f, 1.0f);
}

inline float lerpf(float a, float b, float t) {
    return a + (b - a) * clamp01(t);
}

inline float expAlpha(float dt, float tau) {
    if (tau <= 0.0f) return 1.0f;
    return 1.0f - expf(-dt / tau);
}

inline float smoothExp(float current, float target, float dt, float tau) {
    return current + (target - current) * expAlpha(dt, tau);
}

inline float max3(float a, float b, float c) {
    return std::max(a, std::max(b, c));
}

struct TranslationScratch {
    const TranslationState* key = nullptr;
    MotionPrimitive pending_winner = MotionPrimitive::DRIFT;
    uint8_t pending_count = 0;
    uint8_t low_rms_hops = 0;
    uint16_t phrase_hops = 0;
    uint8_t phrase_chord_root = 0;
    float prev_rms = 0.0f;
    float negative_slope_s = 0.0f;
    float high_energy_age_s = 999.0f;
    uint8_t consecutive_reliable_hops = 0;
    float recoil_remaining_s = 0.0f;
    float nominal_hop_s = 0.008f;
    float last_change_score = 0.0f;
    bool skip_phrase_delta_next = false;
};

TranslationScratch g_scratch[kScratchCapacity];

TranslationScratch& scratchFor(const TranslationState* state) {
    for (size_t i = 0; i < kScratchCapacity; ++i) {
        if (g_scratch[i].key == state) {
            return g_scratch[i];
        }
    }
    for (size_t i = 0; i < kScratchCapacity; ++i) {
        if (g_scratch[i].key == nullptr) {
            g_scratch[i] = TranslationScratch{};
            g_scratch[i].key = state;
            return g_scratch[i];
        }
    }
    g_scratch[0] = TranslationScratch{};
    g_scratch[0].key = state;
    return g_scratch[0];
}

void resetScratch(const TranslationState* state) {
    TranslationScratch& scratch = scratchFor(state);
    scratch = TranslationScratch{};
    scratch.key = state;
}

float hueShiftTarget(float effectiveBpm, float harmonicSaliency, float chordConfidence) {
    const float tempoNorm = clamp01((effectiveBpm - 60.0f) / 120.0f);
    const float tempoMapped = 0.20f + 0.65f * tempoNorm;
    const float harmonicWeight = clamp01(0.60f * harmonicSaliency + 0.40f * chordConfidence);
    return clamp01(tempoMapped * harmonicWeight);
}

float computeTensionRaw(float rms, float flux, float rhythmic, float harmonic) {
    return clamp01(0.35f * rms + 0.25f * flux + 0.20f * rhythmic + 0.20f * harmonic);
}

void clampStateOutputs(TranslationState* state) {
    if (state == nullptr) return;
    state->brightness_scale = clampf(state->brightness_scale, kSceneBrightnessMin, kSceneBrightnessMax);
    state->motion_rate = clampf(state->motion_rate, kSceneRateMin, kSceneRateMax);
    state->motion_depth = clampf(state->motion_depth, kSceneDepthMin, kSceneDepthMax);
    state->hue_shift_speed = clamp01(state->hue_shift_speed);
    state->diffusion = clamp01(state->diffusion);
    state->beat_pulse = clamp01(state->beat_pulse);
    state->tension = clamp01(state->tension);
    state->phrase_progress = clamp01(state->phrase_progress);
    state->transition_progress = clamp01(state->transition_progress);
}

}  // namespace

void translation_init(TranslationState* state) {
    if (state == nullptr) {
        return;
    }

    state->current_motion = MotionPrimitive::DRIFT;
    state->target_motion = MotionPrimitive::DRIFT;
    state->transition_progress = 1.0f;
    state->brightness_scale = kDefaultSceneParameters.brightness_scale;
    state->motion_rate = kDefaultSceneParameters.motion_rate;
    state->motion_depth = kDefaultSceneParameters.motion_depth;
    state->hue_shift_speed = kDefaultSceneParameters.hue_shift_speed;
    state->diffusion = kDefaultSceneParameters.diffusion;
    state->beat_pulse = 0.0f;
    state->tension = 0.0f;
    state->phrase_progress = 0.0f;
    state->phrase_energy_accum = 0.0f;
    state->phrase_flux_accum = 0.0f;
    state->phrase_harmonic_accum = 0.0f;
    state->previous_phrase_energy = 0.0f;
    state->previous_phrase_flux = 0.0f;
    state->previous_phrase_harmonic = 0.0f;
    state->time_since_last_onset_s = 1.0f;
    state->time_since_last_beat_s = 999.0f;
    state->held_bpm = kFallbackBpm;
    state->held_bpm_age_s = kTempoHoldSeconds;
    state->beat_in_bar = 0u;
    state->bars_in_phrase = 8u;
    state->bar_in_phrase = 0u;
    state->phrase_boundary = false;
    state->timing_reliable = false;

    clampStateOutputs(state);
    resetScratch(state);
}

void translation_update(const AudioFeatures* features, float delta_time, TranslationState* state) {
    if (features == nullptr || state == nullptr) {
        return;
    }

#ifndef NATIVE_BUILD
    const uint64_t updateStartUs = esp_timer_get_time();
#endif
    const float dt = clampf(delta_time, kDtMin, kDtMax);
    TranslationScratch& scratch = scratchFor(state);
    scratch.nominal_hop_s = smoothExp(scratch.nominal_hop_s, dt, dt, 0.75f);
    const bool skip_phrase_delta = (dt > (2.0f * scratch.nominal_hop_s)) || scratch.skip_phrase_delta_next;
    scratch.skip_phrase_delta_next = false;

    const float rms = clamp01(features->rms);
    const float fast_rms = clamp01(features->fast_rms);
    const float flux = clamp01(features->flux);
    const float fast_flux = clamp01(features->fast_flux);
    const float bass = clamp01(features->bass);
    const float mid = clamp01(features->mid);
    const float treble = clamp01(features->treble);
    const float harmonic_saliency = clamp01(features->harmonic_saliency);
    const float rhythmic_saliency = clamp01(features->rhythmic_saliency);
    const float timbral_saliency = clamp01(features->timbral_saliency);
    const float dynamic_saliency = clamp01(features->dynamic_saliency);
    const float liveliness = clamp01(features->liveliness);
    const float tempo_confidence = clamp01(features->tempo_confidence);
    const float beat_strength = clamp01(features->beat_strength);
    const float silent_scale = clamp01(features->silent_scale);
    const float chord_confidence = clamp01(features->chord_confidence);

    const bool has_reliable_tempo_now =
        (tempo_confidence >= kTempoConfidenceReliable) &&
        (features->tempo_bpm >= kTempoMinBpm) &&
        (features->tempo_bpm <= kTempoMaxBpm);
    if (has_reliable_tempo_now) {
        if (scratch.consecutive_reliable_hops < 255u) {
            ++scratch.consecutive_reliable_hops;
        }
        // Require N consecutive hops above threshold before trusting.
        // Prevents single-hop confidence spikes from creating false latches.
        if (scratch.consecutive_reliable_hops >= kTempoConsecutiveHopsRequired) {
            state->held_bpm = features->tempo_bpm;
            state->held_bpm_age_s = 0.0f;
            state->timing_reliable = true;
        }
    } else {
        scratch.consecutive_reliable_hops = 0u;
        state->held_bpm_age_s += dt;
        state->timing_reliable = (state->held_bpm_age_s < (kTempoHoldSeconds - 1e-4f));
    }

    const float effective_bpm = clampf(
        (state->held_bpm > 1.0f) ? state->held_bpm : kFallbackBpm,
        kTempoMinBpm,
        kTempoMaxBpm
    );
    const float beat_period = 60.0f / effective_bpm;

    state->time_since_last_onset_s += dt;
    state->time_since_last_beat_s += dt;

    const bool use_four_bar_phrase =
        (features->style == audio::MusicStyle::RHYTHMIC_DRIVEN) ||
        (features->tempo_bpm >= 110.0f) ||
        ((rhythmic_saliency >= harmonic_saliency) && (tempo_confidence >= kStyleRhythmicConfidence));
    state->bars_in_phrase = use_four_bar_phrase ? 4u : 8u;

    const float onset_refractory_window = std::max(0.06f, 0.25f * beat_period);
    const bool onset_signal = features->snare_trigger || features->hihat_trigger || (fast_flux > 0.38f);
    const bool accepted_onset = onset_signal && (state->time_since_last_onset_s >= onset_refractory_window);
    if (accepted_onset) {
        state->time_since_last_onset_s = 0.0f;
    }

    if (rms < 0.05f) {
        if (scratch.low_rms_hops < 255u) {
            ++scratch.low_rms_hops;
        }
    } else {
        scratch.low_rms_hops = 0u;
    }

    bool phrase_wrap = false;
    state->phrase_boundary = false;

    const bool accepted_beat = features->beat_tick && (state->timing_reliable || has_reliable_tempo_now);
    if (accepted_beat) {
        state->time_since_last_beat_s = 0.0f;
        state->beat_in_bar = static_cast<uint8_t>((state->beat_in_bar + 1u) & 0x3u);
        if (state->beat_in_bar == 0u) {
            const uint8_t next_bar = static_cast<uint8_t>(state->bar_in_phrase + 1u);
            if (next_bar >= state->bars_in_phrase) {
                state->bar_in_phrase = 0u;
                phrase_wrap = true;
            } else {
                state->bar_in_phrase = next_bar;
            }
        }
    } else if (state->time_since_last_beat_s > (2.0f * beat_period)) {
        state->timing_reliable = false;
        state->phrase_boundary = false;
    }

    if (state->timing_reliable) {
        state->phrase_energy_accum += rms;
        state->phrase_flux_accum += flux;
        state->phrase_harmonic_accum += harmonic_saliency;
        if (scratch.phrase_hops < 65535u) {
            ++scratch.phrase_hops;
        }
    }

    if (phrase_wrap) {
        bool hard_boundary = false;
        if (!skip_phrase_delta && scratch.phrase_hops > 0u) {
            const float inv_hops = 1.0f / static_cast<float>(scratch.phrase_hops);
            const float mean_energy = state->phrase_energy_accum * inv_hops;
            const float mean_flux = state->phrase_flux_accum * inv_hops;
            const float mean_harmonic = state->phrase_harmonic_accum * inv_hops;

            const float mean_rms_delta = mean_energy - state->previous_phrase_energy;
            const float mean_flux_delta = mean_flux - state->previous_phrase_flux;
            const float harmonic_delta = mean_harmonic - state->previous_phrase_harmonic;
            const float change_score =
                0.45f * fabsf(mean_rms_delta) +
                0.30f * fabsf(mean_flux_delta) +
                0.25f * fabsf(harmonic_delta);
            scratch.last_change_score = change_score;

            const bool chord_hard_boundary =
                (chord_confidence >= kChordBoundaryConfidence) &&
                (features->chord_root != scratch.phrase_chord_root);
            hard_boundary = (change_score >= kHardBoundaryScore) || chord_hard_boundary;

            state->previous_phrase_energy = mean_energy;
            state->previous_phrase_flux = mean_flux;
            state->previous_phrase_harmonic = mean_harmonic;
            scratch.phrase_chord_root = features->chord_root;
        }

        state->phrase_energy_accum = 0.0f;
        state->phrase_flux_accum = 0.0f;
        state->phrase_harmonic_accum = 0.0f;
        scratch.phrase_hops = 0u;
        state->phrase_boundary = hard_boundary;

        if (!hard_boundary) {
            state->transition_progress = std::min(state->transition_progress, 0.35f);
        }
    }

    if (state->timing_reliable) {
        const float bars = static_cast<float>((state->bars_in_phrase == 0u) ? 1u : state->bars_in_phrase);
        state->phrase_progress = clamp01(
            (static_cast<float>(state->bar_in_phrase) +
             static_cast<float>(state->beat_in_bar) / 4.0f) / bars
        );
    }

    const float energy_now = clamp01(0.65f * rms + 0.35f * fast_rms);
    const float energy_slope = rms - scratch.prev_rms;
    scratch.prev_rms = rms;

    if (energy_slope < -0.01f) {
        scratch.negative_slope_s += dt;
    } else {
        scratch.negative_slope_s = 0.0f;
    }
    if (energy_now >= 0.72f) {
        scratch.high_energy_age_s = 0.0f;
    } else {
        scratch.high_energy_age_s += dt;
    }

    const bool decay_condition =
        (scratch.high_energy_age_s <= (2.0f * beat_period)) &&
        (scratch.negative_slope_s >= (0.5f * beat_period));
    const bool rhythmic_dominant =
        (rhythmic_saliency >= harmonic_saliency) &&
        (rhythmic_saliency >= timbral_saliency);
    const bool harmonic_or_timbral_dominant =
        (max3(harmonic_saliency, rhythmic_saliency, timbral_saliency) != rhythmic_saliency);
    const bool stable_energy = (fabsf(energy_slope) < 0.06f) && (fast_flux < 0.38f);
    const bool bloom_trigger =
        (features->beat_tick || accepted_onset) &&
        ((bass > 0.42f) || (fast_flux > 0.38f) || features->snare_trigger) &&
        (rms > 0.18f);
    const bool lock_condition = features->is_silent || (scratch.low_rms_hops >= 2u);
    const bool strong_phrase_opening_downbeat = state->phrase_boundary && features->downbeat_tick && (beat_strength >= 0.60f);

    MotionPrimitive winner = MotionPrimitive::DRIFT;
    if (lock_condition) {
        winner = MotionPrimitive::LOCK;
    } else if (state->phrase_boundary || strong_phrase_opening_downbeat) {
        winner = MotionPrimitive::RECOIL;
    } else if (bloom_trigger) {
        winner = MotionPrimitive::BLOOM;
    } else if (state->timing_reliable && rhythmic_dominant && stable_energy) {
        winner = MotionPrimitive::FLOW;
    } else if ((!state->timing_reliable) || harmonic_or_timbral_dominant) {
        winner = MotionPrimitive::DRIFT;
    } else if (decay_condition) {
        winner = MotionPrimitive::DECAY;
    }

    if (!state->timing_reliable) {
        if (state->time_since_last_beat_s <= beat_period) {
            winner = state->current_motion;
        } else {
            winner = MotionPrimitive::DRIFT;
        }
    }

    if (winner == MotionPrimitive::RECOIL && state->phrase_boundary) {
        state->target_motion = MotionPrimitive::RECOIL;
        state->current_motion = MotionPrimitive::RECOIL;
        state->transition_progress = 0.0f;
        scratch.recoil_remaining_s = lerpf(0.150f, 0.220f, beat_strength);
        scratch.pending_count = 0u;
    } else {
        if (winner != scratch.pending_winner) {
            scratch.pending_winner = winner;
            scratch.pending_count = 1u;
        } else if (scratch.pending_count < 255u) {
            ++scratch.pending_count;
        }

        state->target_motion = scratch.pending_winner;
        if (scratch.pending_count >= 2u && state->current_motion != state->target_motion) {
            state->current_motion = state->target_motion;
            state->transition_progress = 0.0f;
        }
    }

    if (state->current_motion == MotionPrimitive::RECOIL) {
        scratch.recoil_remaining_s -= dt;
        if (scratch.recoil_remaining_s <= 0.0f && state->target_motion == MotionPrimitive::RECOIL) {
            state->target_motion = MotionPrimitive::DRIFT;
        }
    }

    if (!(state->current_motion == MotionPrimitive::RECOIL && scratch.recoil_remaining_s > 0.0f)) {
        state->transition_progress = smoothExp(state->transition_progress, 1.0f, dt, kMotionTau);
    }

    const float harmonic_drive = clamp01(0.60f * harmonic_saliency + 0.40f * chord_confidence);
    const float rhythmic_drive = clamp01(0.50f * rhythmic_saliency + 0.30f * beat_strength + 0.20f * dynamic_saliency);
    const float timbral_drive = clamp01(0.55f * timbral_saliency + 0.45f * treble);

    float brightness_target = state->brightness_scale;
    float rate_target = state->motion_rate;
    float depth_target = state->motion_depth;
    float diffusion_target = state->diffusion;

    switch (state->current_motion) {
        case MotionPrimitive::LOCK:
            brightness_target = lerpf(0.20f, 0.35f, silent_scale);
            rate_target = lerpf(0.60f, 0.75f, 0.35f * energy_now);
            depth_target = lerpf(0.10f, 0.20f, 0.50f * energy_now);
            diffusion_target = lerpf(0.20f, 0.35f, 1.0f - silent_scale);
            break;
        case MotionPrimitive::DRIFT:
            brightness_target = lerpf(0.35f, 0.70f, harmonic_drive);
            rate_target = lerpf(0.75f, 1.05f, 0.50f * harmonic_drive + 0.50f * liveliness);
            depth_target = lerpf(0.25f, 0.55f, timbral_drive);
            diffusion_target = lerpf(0.45f, 0.70f, 0.50f * harmonic_drive + 0.50f * timbral_drive);
            break;
        case MotionPrimitive::FLOW:
            brightness_target = lerpf(0.55f, 0.95f, energy_now);
            rate_target = lerpf(1.00f, 1.30f, rhythmic_drive);
            depth_target = lerpf(0.55f, 0.85f, rhythmic_drive);
            diffusion_target = lerpf(0.35f, 0.55f, 1.0f - rhythmic_drive);
            break;
        case MotionPrimitive::BLOOM:
            brightness_target = lerpf(0.85f, 1.25f, energy_now);
            rate_target = lerpf(1.20f, 1.55f, std::max(rhythmic_drive, beat_strength));
            depth_target = lerpf(0.70f, 1.00f, energy_now);
            diffusion_target = lerpf(0.55f, 0.80f, std::max(energy_now, beat_strength));
            break;
        case MotionPrimitive::RECOIL:
            brightness_target = lerpf(0.90f, 1.15f, beat_strength);
            rate_target = lerpf(1.20f, 1.55f, beat_strength);
            depth_target = lerpf(0.70f, 1.00f, beat_strength);
            diffusion_target = clamp01(lerpf(0.55f, 0.80f, beat_strength) - 0.15f);
            state->transition_progress = 0.0f;
            break;
        case MotionPrimitive::DECAY:
            brightness_target = lerpf(0.40f, 0.75f, energy_now);
            rate_target = lerpf(0.80f, 1.00f, energy_now);
            depth_target = lerpf(0.20f, 0.45f, energy_now);
            diffusion_target = lerpf(0.30f, 0.50f, energy_now);
            break;
    }

    const float tension_raw = computeTensionRaw(rms, flux, rhythmic_saliency, harmonic_saliency);
    const float tension_target = (state->current_motion == MotionPrimitive::LOCK)
        ? std::min(tension_raw, 0.20f)
        : tension_raw;
    const float hue_target = hueShiftTarget(effective_bpm, harmonic_saliency, chord_confidence);

    state->brightness_scale = smoothExp(state->brightness_scale, brightness_target, dt, kMotionTau);
    state->motion_rate = smoothExp(state->motion_rate, rate_target, dt, kMotionTau);
    state->motion_depth = smoothExp(state->motion_depth, depth_target, dt, kMotionTau);
    state->diffusion = smoothExp(state->diffusion, diffusion_target, dt, kDriftTau);
    state->hue_shift_speed = smoothExp(state->hue_shift_speed, hue_target, dt, kDriftTau);
    state->tension = smoothExp(state->tension, tension_target, dt, kMotionTau);

    const bool pulse_trigger = bloom_trigger || (state->current_motion == MotionPrimitive::RECOIL);
    if (pulse_trigger) {
        state->beat_pulse = 1.0f;
    } else {
        state->beat_pulse = smoothExp(state->beat_pulse, 0.0f, dt, kBeatPulseTau);
    }

    // Enforce documented scene ranges for the active motion primitive.
    switch (state->current_motion) {
        case MotionPrimitive::LOCK:
            state->brightness_scale = clampf(state->brightness_scale, 0.20f, 0.35f);
            state->motion_rate = clampf(state->motion_rate, 0.60f, 0.75f);
            state->motion_depth = clampf(state->motion_depth, 0.10f, 0.20f);
            state->diffusion = clampf(state->diffusion, 0.20f, 0.35f);
            state->tension = std::min(state->tension, 0.20f);
            break;
        case MotionPrimitive::DRIFT:
            state->brightness_scale = clampf(state->brightness_scale, 0.35f, 0.70f);
            state->motion_rate = clampf(state->motion_rate, 0.75f, 1.05f);
            state->motion_depth = clampf(state->motion_depth, 0.25f, 0.55f);
            state->diffusion = clampf(state->diffusion, 0.45f, 0.70f);
            break;
        case MotionPrimitive::FLOW:
            state->brightness_scale = clampf(state->brightness_scale, 0.55f, 0.95f);
            state->motion_rate = clampf(state->motion_rate, 1.00f, 1.30f);
            state->motion_depth = clampf(state->motion_depth, 0.55f, 0.85f);
            state->diffusion = clampf(state->diffusion, 0.35f, 0.55f);
            break;
        case MotionPrimitive::BLOOM:
            state->brightness_scale = clampf(state->brightness_scale, 0.85f, 1.25f);
            state->motion_rate = clampf(state->motion_rate, 1.20f, 1.55f);
            state->motion_depth = clampf(state->motion_depth, 0.70f, 1.00f);
            state->diffusion = clampf(state->diffusion, 0.55f, 0.80f);
            break;
        case MotionPrimitive::DECAY:
            state->brightness_scale = clampf(state->brightness_scale, 0.40f, 0.75f);
            state->motion_rate = clampf(state->motion_rate, 0.80f, 1.00f);
            state->motion_depth = clampf(state->motion_depth, 0.20f, 0.45f);
            state->diffusion = clampf(state->diffusion, 0.30f, 0.50f);
            break;
        case MotionPrimitive::RECOIL:
            // RECOIL is impulse-shaped: diffusion and beat pulse are managed explicitly above.
            break;
    }

    clampStateOutputs(state);

#ifndef NATIVE_BUILD
    const uint64_t updateEndUs = esp_timer_get_time();
    if (updateEndUs > updateStartUs && (updateEndUs - updateStartUs) > 5000ULL) {
        scratch.skip_phrase_delta_next = true;
    }
#endif
}

void translation_get_parameters(const TranslationState* state, SceneParameters* params) {
    if (params == nullptr) {
        return;
    }
    if (state == nullptr) {
        *params = kDefaultSceneParameters;
        return;
    }

    params->motion_type = state->current_motion;
    params->brightness_scale = state->brightness_scale;
    params->motion_rate = state->motion_rate;
    params->motion_depth = state->motion_depth;
    params->hue_shift_speed = state->hue_shift_speed;
    params->diffusion = state->diffusion;
    params->beat_pulse = state->beat_pulse;
    params->tension = state->tension;
    params->phrase_progress = state->phrase_progress;
    params->transition_progress = state->transition_progress;
    params->phrase_boundary = state->phrase_boundary;
    params->timing_reliable = state->timing_reliable;

    params->brightness_scale = clampf(params->brightness_scale, kSceneBrightnessMin, kSceneBrightnessMax);
    params->motion_rate = clampf(params->motion_rate, kSceneRateMin, kSceneRateMax);
    params->motion_depth = clampf(params->motion_depth, kSceneDepthMin, kSceneDepthMax);
    params->hue_shift_speed = clamp01(params->hue_shift_speed);
    params->diffusion = clamp01(params->diffusion);
    params->beat_pulse = clamp01(params->beat_pulse);
    params->tension = clamp01(params->tension);
    params->phrase_progress = clamp01(params->phrase_progress);
    params->transition_progress = clamp01(params->transition_progress);
}

}  // namespace lightwaveos::audio
