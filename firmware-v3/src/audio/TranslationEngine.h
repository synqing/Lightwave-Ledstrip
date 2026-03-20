#pragma once

#include <cstdint>
#include "contracts/StyleDetector.h"

namespace lightwaveos::audio {

enum class ChordType : uint8_t;

enum class MotionPrimitive : uint8_t { DRIFT, FLOW, BLOOM, RECOIL, LOCK, DECAY };

struct AudioFeatures {
    uint32_t hop_sequence;
    float rms;
    float fast_rms;
    float flux;
    float fast_flux;
    float bass;
    float mid;
    float treble;
    float harmonic_saliency;
    float rhythmic_saliency;
    float timbral_saliency;
    float dynamic_saliency;
    float liveliness;
    float tempo_bpm;
    float tempo_confidence;
    float beat_strength;
    bool beat_tick;
    bool downbeat_tick;
    bool snare_trigger;
    bool hihat_trigger;
    bool is_silent;
    float silent_scale;
    audio::MusicStyle style;
    float style_confidence;
    uint8_t chord_root;
    audio::ChordType chord_type;
    float chord_confidence;
};

struct SceneParameters {
    MotionPrimitive motion_type;
    float brightness_scale;     // 0.15..1.25
    float motion_rate;          // 0.60..1.80
    float motion_depth;         // 0.00..1.00
    float hue_shift_speed;      // 0.00..1.00 normalized
    float diffusion;            // 0.00..1.00
    float beat_pulse;           // 0.00..1.00
    float tension;              // 0.00..1.00
    float phrase_progress;      // 0.00..1.00
    float transition_progress;  // 0.00..1.00
    bool phrase_boundary;
    bool timing_reliable;
};

struct TranslationState {
    MotionPrimitive current_motion;
    MotionPrimitive target_motion;
    float transition_progress;
    float brightness_scale;
    float motion_rate;
    float motion_depth;
    float hue_shift_speed;
    float diffusion;
    float beat_pulse;
    float tension;
    float phrase_progress;
    float phrase_energy_accum;
    float phrase_flux_accum;
    float phrase_harmonic_accum;
    float previous_phrase_energy;
    float previous_phrase_flux;
    float previous_phrase_harmonic;
    float time_since_last_onset_s;
    float time_since_last_beat_s;
    float held_bpm;
    float held_bpm_age_s;
    uint8_t beat_in_bar;
    uint8_t bars_in_phrase;
    uint8_t bar_in_phrase;
    bool phrase_boundary;
    bool timing_reliable;
};

inline constexpr SceneParameters kDefaultSceneParameters = {
    MotionPrimitive::DRIFT,
    0.45f,   // brightness_scale
    0.90f,   // motion_rate
    0.35f,   // motion_depth
    0.20f,   // hue_shift_speed
    0.55f,   // diffusion
    0.0f,    // beat_pulse
    0.0f,    // tension
    0.0f,    // phrase_progress
    1.0f,    // transition_progress
    false,   // phrase_boundary
    false    // timing_reliable
};

void translation_init(TranslationState* state);
void translation_update(const AudioFeatures* features, float delta_time, TranslationState* state);
void translation_get_parameters(const TranslationState* state, SceneParameters* params);

}  // namespace lightwaveos::audio
