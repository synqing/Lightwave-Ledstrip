/**
 * Translation engine unit tests.
 *
 * Native tests compile only test sources, so include implementation directly.
 */

#include <unity.h>

#include "../../src/audio/TranslationEngine.h"
#include "../../src/audio/TranslationEngine.cpp"

using lightwaveos::audio::AudioFeatures;
using lightwaveos::audio::MotionPrimitive;
using lightwaveos::audio::SceneParameters;
using lightwaveos::audio::TranslationState;
using lightwaveos::audio::translation_get_parameters;
using lightwaveos::audio::translation_init;
using lightwaveos::audio::translation_update;

namespace {

AudioFeatures baseFeatures() {
    AudioFeatures f{};
    f.hop_sequence = 1u;
    f.rms = 0.30f;
    f.fast_rms = 0.30f;
    f.flux = 0.20f;
    f.fast_flux = 0.20f;
    f.bass = 0.30f;
    f.mid = 0.30f;
    f.treble = 0.30f;
    f.harmonic_saliency = 0.30f;
    f.rhythmic_saliency = 0.30f;
    f.timbral_saliency = 0.20f;
    f.dynamic_saliency = 0.20f;
    f.liveliness = 0.30f;
    f.tempo_bpm = 120.0f;
    f.tempo_confidence = 0.80f;
    f.beat_strength = 0.50f;
    f.beat_tick = false;
    f.downbeat_tick = false;
    f.snare_trigger = false;
    f.hihat_trigger = false;
    f.is_silent = false;
    f.silent_scale = 1.0f;
    f.style = lightwaveos::audio::MusicStyle::UNKNOWN;
    f.style_confidence = 0.0f;
    f.chord_root = 0u;
    f.chord_type = static_cast<lightwaveos::audio::ChordType>(0u);
    f.chord_confidence = 0.0f;
    return f;
}

}  // namespace

void test_translation_init_defaults() {
    TranslationState state{};
    translation_init(&state);

    SceneParameters scene{};
    translation_get_parameters(&state, &scene);

    const uint8_t motion = static_cast<uint8_t>(scene.motion_type);
    TEST_ASSERT_TRUE(
        motion == static_cast<uint8_t>(MotionPrimitive::DRIFT) ||
        motion == static_cast<uint8_t>(MotionPrimitive::LOCK)
    );
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, scene.phrase_progress);
    TEST_ASSERT_FALSE(scene.timing_reliable);
}

void test_translation_low_energy_unreliable_falls_back() {
    TranslationState state{};
    translation_init(&state);

    AudioFeatures f = baseFeatures();
    f.rms = 0.02f;
    f.fast_rms = 0.02f;
    f.flux = 0.01f;
    f.fast_flux = 0.01f;
    f.tempo_bpm = 0.0f;
    f.tempo_confidence = 0.0f;
    f.is_silent = true;
    f.silent_scale = 0.0f;

    for (int i = 0; i < 80; ++i) {
        translation_update(&f, 0.016f, &state);
    }

    SceneParameters scene{};
    translation_get_parameters(&state, &scene);
    const uint8_t motion = static_cast<uint8_t>(scene.motion_type);
    TEST_ASSERT_TRUE(
        motion == static_cast<uint8_t>(MotionPrimitive::LOCK) ||
        motion == static_cast<uint8_t>(MotionPrimitive::DRIFT)
    );
    TEST_ASSERT_TRUE(scene.brightness_scale <= 0.70f);
    TEST_ASSERT_FALSE(scene.timing_reliable);
}

void test_translation_strong_bass_onset_drives_bloom() {
    TranslationState state{};
    translation_init(&state);

    AudioFeatures f = baseFeatures();
    f.rms = 0.75f;
    f.fast_rms = 0.82f;
    f.flux = 0.65f;
    f.fast_flux = 0.85f;
    f.bass = 0.90f;
    f.rhythmic_saliency = 0.85f;
    f.harmonic_saliency = 0.20f;
    f.tempo_bpm = 128.0f;
    f.tempo_confidence = 0.90f;
    f.beat_tick = true;
    f.downbeat_tick = true;
    f.beat_strength = 0.90f;
    f.snare_trigger = true;

    translation_update(&f, 0.016f, &state);
    translation_update(&f, 0.016f, &state);

    SceneParameters scene{};
    translation_get_parameters(&state, &scene);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MotionPrimitive::BLOOM),
                            static_cast<uint8_t>(scene.motion_type));
    TEST_ASSERT_TRUE(scene.brightness_scale >= 0.85f);
    TEST_ASSERT_TRUE(scene.beat_pulse > 0.0f);
}

void test_translation_onset_refractory_suppresses_retrigger() {
    TranslationState state{};
    translation_init(&state);

    AudioFeatures f = baseFeatures();
    f.rms = 0.65f;
    f.fast_rms = 0.72f;
    f.fast_flux = 0.70f;
    f.bass = 0.70f;
    f.tempo_bpm = 120.0f;
    f.tempo_confidence = 0.90f;
    f.snare_trigger = true;
    f.beat_tick = false;
    f.downbeat_tick = false;

    translation_update(&f, 0.016f, &state);
    const float firstTimer = state.time_since_last_onset_s;
    TEST_ASSERT_TRUE(firstTimer < 0.001f);

    translation_update(&f, 0.010f, &state);
    const float secondTimer = state.time_since_last_onset_s;
    TEST_ASSERT_TRUE(secondTimer > 0.0f);
    TEST_ASSERT_TRUE(secondTimer < 0.03f);
}

void test_translation_beat_ticks_advance_phrase_and_emit_boundary() {
    TranslationState state{};
    translation_init(&state);

    AudioFeatures f = baseFeatures();
    f.style = lightwaveos::audio::MusicStyle::RHYTHMIC_DRIVEN;
    f.style_confidence = 0.90f;
    f.tempo_bpm = 128.0f;
    f.tempo_confidence = 0.90f;
    f.beat_tick = true;
    f.downbeat_tick = false;
    f.rhythmic_saliency = 0.90f;
    f.harmonic_saliency = 0.20f;
    f.chord_root = 0u;
    f.chord_confidence = 0.0f;

    for (int i = 0; i < 15; ++i) {
        f.downbeat_tick = ((i % 4) == 0);
        translation_update(&f, 0.02f, &state);
    }

    SceneParameters beforeBoundary{};
    translation_get_parameters(&state, &beforeBoundary);
    TEST_ASSERT_TRUE(beforeBoundary.phrase_progress > 0.0f);
    TEST_ASSERT_FALSE(beforeBoundary.phrase_boundary);

    // Force hard boundary on wrap via chord-root change with confidence.
    f.downbeat_tick = true;
    f.chord_root = 7u;
    f.chord_confidence = 0.80f;
    translation_update(&f, 0.02f, &state);

    SceneParameters boundary{};
    translation_get_parameters(&state, &boundary);
    TEST_ASSERT_TRUE(boundary.phrase_boundary);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MotionPrimitive::RECOIL),
                            static_cast<uint8_t>(boundary.motion_type));
}

void test_translation_holds_bpm_then_degrades_after_two_seconds() {
    TranslationState state{};
    translation_init(&state);

    AudioFeatures f = baseFeatures();
    f.tempo_bpm = 128.0f;
    f.tempo_confidence = 0.95f;
    f.beat_tick = true;
    f.downbeat_tick = true;

    translation_update(&f, 0.02f, &state);
    TEST_ASSERT_TRUE(state.timing_reliable);

    f.tempo_confidence = 0.10f;
    for (int i = 0; i < 39; ++i) {  // 1.95s at 50ms clamp
        translation_update(&f, 0.05f, &state);
    }
    TEST_ASSERT_TRUE(state.timing_reliable);

    translation_update(&f, 0.05f, &state);  // crosses 2.0s hold window
    TEST_ASSERT_FALSE(state.timing_reliable);
}

void test_translation_outputs_are_clamped() {
    TranslationState state{};
    translation_init(&state);

    AudioFeatures f{};
    f.hop_sequence = 99u;
    f.rms = 4.0f;
    f.fast_rms = 4.0f;
    f.flux = 4.0f;
    f.fast_flux = 4.0f;
    f.bass = 4.0f;
    f.mid = 4.0f;
    f.treble = 4.0f;
    f.harmonic_saliency = 4.0f;
    f.rhythmic_saliency = 4.0f;
    f.timbral_saliency = 4.0f;
    f.dynamic_saliency = 4.0f;
    f.liveliness = 4.0f;
    f.tempo_bpm = 400.0f;
    f.tempo_confidence = 4.0f;
    f.beat_strength = 4.0f;
    f.beat_tick = true;
    f.downbeat_tick = true;
    f.snare_trigger = true;
    f.hihat_trigger = true;
    f.is_silent = false;
    f.silent_scale = 4.0f;
    f.style = lightwaveos::audio::MusicStyle::RHYTHMIC_DRIVEN;
    f.style_confidence = 4.0f;
    f.chord_root = 11u;
    f.chord_type = static_cast<lightwaveos::audio::ChordType>(0u);
    f.chord_confidence = 4.0f;

    translation_update(&f, 0.016f, &state);

    SceneParameters scene{};
    translation_get_parameters(&state, &scene);

    TEST_ASSERT_TRUE(scene.brightness_scale >= 0.15f && scene.brightness_scale <= 1.25f);
    TEST_ASSERT_TRUE(scene.motion_rate >= 0.60f && scene.motion_rate <= 1.80f);
    TEST_ASSERT_TRUE(scene.motion_depth >= 0.0f && scene.motion_depth <= 1.0f);
    TEST_ASSERT_TRUE(scene.hue_shift_speed >= 0.0f && scene.hue_shift_speed <= 1.0f);
    TEST_ASSERT_TRUE(scene.diffusion >= 0.0f && scene.diffusion <= 1.0f);
    TEST_ASSERT_TRUE(scene.beat_pulse >= 0.0f && scene.beat_pulse <= 1.0f);
    TEST_ASSERT_TRUE(scene.tension >= 0.0f && scene.tension <= 1.0f);
    TEST_ASSERT_TRUE(scene.phrase_progress >= 0.0f && scene.phrase_progress <= 1.0f);
    TEST_ASSERT_TRUE(scene.transition_progress >= 0.0f && scene.transition_progress <= 1.0f);
}

void run_translation_engine_tests() {
    RUN_TEST(test_translation_init_defaults);
    RUN_TEST(test_translation_low_energy_unreliable_falls_back);
    RUN_TEST(test_translation_strong_bass_onset_drives_bloom);
    RUN_TEST(test_translation_onset_refractory_suppresses_retrigger);
    RUN_TEST(test_translation_beat_ticks_advance_phrase_and_emit_boundary);
    RUN_TEST(test_translation_holds_bpm_then_degrades_after_two_seconds);
    RUN_TEST(test_translation_outputs_are_clamped);
}

