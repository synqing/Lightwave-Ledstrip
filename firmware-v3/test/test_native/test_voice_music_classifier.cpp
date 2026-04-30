// Phase 4 Move 4.1 — AUD-21 VoiceMusicClassifier heuristic test
//
// Per Topology_Reconciliation §5/§6: this is the heuristic-only voice-vs-music
// substrate. ML-class classifiers (PS-10 mood-driven, AUD-25 mood-classifier)
// are KILLED for v3 — they require Phase 6 INF-11 ML infrastructure that does
// not exist. Move 4.1 ships a render-trivial scalar producer built on signals
// already present on ControlBusFrame: chroma peakiness (inverse contribution),
// STM 4-8 Hz syllabic-rate energy, harmonic novelty, tempo lock.
//
// Test contract:
//   - Default state: voiceProb starts at 0.0, reset() returns there.
//   - Music-like input: strong chroma peak + steady tempo lock → voiceProb
//     converges low (< 0.25) over a few seconds.
//   - Voice-like input: flat chroma + no tempo lock + STM temporal energy in
//     the syllabic band → voiceProb converges high (> 0.6) over a few seconds.
//   - Smoothing: per-frame change is bounded (no flicker).
//   - reset(): returns to default state, prior accumulators cleared.
//
// All helpers are header-only inline; no heap, dt-correct EMA (tau = 2.0 s).

#include <unity.h>
#include <cmath>
#include <cstring>

#include "../../src/audio/contracts/ControlBus.h"
#include "../../src/effects/audio/VoiceMusicClassifier.h"

using lightwaveos::audio::ControlBusFrame;
using lightwaveos::audio::CONTROLBUS_NUM_CHROMA;
using lightwaveos::effects::audio::VoiceMusicClassifier;

namespace {

constexpr float kEps = 1e-5f;
constexpr float kFrameDt = 1.0f / 125.0f;  // ESV11 32 kHz hop cadence (8 ms)

// ─── Helpers ───────────────────────────────────────────────────────────────

// Seed a frame with strong music characteristics: one chroma bin dominant,
// tempo locked with high confidence, low STM temporal energy, low novelty.
ControlBusFrame makeMusicFrame() {
    ControlBusFrame f{};
    // Single peaked chroma class — characteristic of held chord notes.
    f.chroma[0] = 1.0f;  // root note dominant
    for (uint8_t i = 1; i < CONTROLBUS_NUM_CHROMA; ++i) {
        f.chroma[i] = 0.05f;  // small leakage in other classes
    }
    f.tempoLocked = true;
    f.tempoConfidence = 0.9f;
    f.tempoBpm = 120.0f;
    // STM: low temporal energy (no syllabic-rate modulation in steady chord).
    f.stmTemporalEnergy = 0.05f;
    f.stmReady = true;
    // Saliency: low harmonic/timbral novelty (steady chord, no key change).
    f.saliency.harmonicNoveltySmooth = 0.05f;
    f.saliency.timbralNoveltySmooth = 0.05f;
    f.audioConfidence = 0.95f;
    return f;
}

// Seed a frame with strong voice characteristics: flat chroma (vocal pitch
// roams across pitch classes over a window), no tempo lock, STM temporal
// energy in the 4-8 Hz syllabic band.
ControlBusFrame makeVoiceFrame() {
    ControlBusFrame f{};
    // Flat chroma — vocal energy spread across pitch classes when integrated.
    for (uint8_t i = 0; i < CONTROLBUS_NUM_CHROMA; ++i) {
        f.chroma[i] = 0.4f;
    }
    f.tempoLocked = false;
    f.tempoConfidence = 0.0f;
    f.tempoBpm = 0.0f;
    // STM: high temporal energy in the syllabic-rate band.
    f.stmTemporalEnergy = 0.7f;
    f.stmReady = true;
    // Saliency: high timbral novelty (formants shift), moderate harmonic.
    f.saliency.harmonicNoveltySmooth = 0.4f;
    f.saliency.timbralNoveltySmooth = 0.6f;
    f.audioConfidence = 0.95f;
    return f;
}

// ─── Tests ─────────────────────────────────────────────────────────────────

// 1 — Default-constructed classifier reports voiceProb == 0.0.
void test_default_state_is_zero() {
    VoiceMusicClassifier vmc;
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, vmc.voiceProb());
}

// 2 — Steady music input drives voiceProb low (well under 0.5) after warmup.
//     5 seconds at 125 Hz frame rate = 625 frames; tau = 2.0 s → ~2.5 tau.
void test_music_input_drives_voice_prob_low() {
    VoiceMusicClassifier vmc;
    const ControlBusFrame music = makeMusicFrame();
    for (int i = 0; i < 625; ++i) {
        vmc.update(music, kFrameDt);
    }
    // Music-like signature should keep voiceProb below 0.3.
    TEST_ASSERT_LESS_THAN_FLOAT(0.3f, vmc.voiceProb());
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(0.0f, vmc.voiceProb());
}

// 3 — Voice-like input drives voiceProb high after warmup.
void test_voice_input_drives_voice_prob_high() {
    VoiceMusicClassifier vmc;
    const ControlBusFrame voice = makeVoiceFrame();
    for (int i = 0; i < 625; ++i) {
        vmc.update(voice, kFrameDt);
    }
    // Voice-like signature should drive voiceProb above 0.6.
    TEST_ASSERT_GREATER_THAN_FLOAT(0.6f, vmc.voiceProb());
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(1.0f, vmc.voiceProb());
}

// 4 — Smoothing: per-frame change is bounded. Step input from voice to music
//     must not jump more than ~1/60 of full range in a single 1/60 s update
//     (the spec's flicker bound). With tau = 2.0 s, alpha ≈ 0.0083 per 60 fps
//     frame, so a worst-case 1.0 → 0.0 step changes by ≤ 0.01 per frame.
void test_smoothing_bounds_per_frame_change() {
    VoiceMusicClassifier vmc;
    const ControlBusFrame voice = makeVoiceFrame();
    // Warm up to a high voiceProb under voice input.
    for (int i = 0; i < 800; ++i) {
        vmc.update(voice, kFrameDt);
    }
    const float before = vmc.voiceProb();

    // One frame of music input — change must be bounded.
    const ControlBusFrame music = makeMusicFrame();
    vmc.update(music, 1.0f / 60.0f);
    const float after = vmc.voiceProb();

    const float delta = fabsf(after - before);
    // Per-frame change at 60 fps with tau=2.0 s is bounded by ≈ 1/120
    // (alpha ≈ 0.0083). Allow a generous 0.02 ceiling for safety.
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(0.02f, delta);
}

// 5 — reset() returns to default state.
void test_reset_returns_to_default() {
    VoiceMusicClassifier vmc;
    const ControlBusFrame voice = makeVoiceFrame();
    for (int i = 0; i < 200; ++i) {
        vmc.update(voice, kFrameDt);
    }
    TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, vmc.voiceProb());

    vmc.reset();
    TEST_ASSERT_FLOAT_WITHIN(kEps, 0.0f, vmc.voiceProb());
}

// 6 — Output stays clamped to [0, 1] under degenerate input (NaN-free, no
//     out-of-range escape). Feed an all-ones flat chroma with stmTemporal
//     saturated, no tempo, high novelty — the strongest voice signature —
//     and verify the result remains in range.
void test_output_clamped_to_unit_interval() {
    VoiceMusicClassifier vmc;
    ControlBusFrame extreme{};
    for (uint8_t i = 0; i < CONTROLBUS_NUM_CHROMA; ++i) {
        extreme.chroma[i] = 1.0f;  // perfectly flat
    }
    extreme.tempoLocked = false;
    extreme.stmTemporalEnergy = 1.0f;
    extreme.stmReady = true;
    extreme.saliency.harmonicNoveltySmooth = 1.0f;
    extreme.saliency.timbralNoveltySmooth = 1.0f;
    extreme.audioConfidence = 1.0f;
    for (int i = 0; i < 1000; ++i) {
        vmc.update(extreme, kFrameDt);
    }
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(0.0f, vmc.voiceProb());
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(1.0f, vmc.voiceProb());
}

// 7 — Silence path: when audioConfidence is zero (no music present), the
//     classifier holds its prior estimate rather than being driven by stale
//     chroma/STM noise. Document behaviour: silence acts as "freeze".
void test_silence_freezes_estimate() {
    VoiceMusicClassifier vmc;
    const ControlBusFrame voice = makeVoiceFrame();
    for (int i = 0; i < 500; ++i) {
        vmc.update(voice, kFrameDt);
    }
    const float beforeSilence = vmc.voiceProb();

    // Now feed silence (audioConfidence = 0) for a long period.
    ControlBusFrame silence{};
    silence.audioConfidence = 0.0f;
    silence.stmReady = false;
    silence.tempoLocked = false;
    for (int i = 0; i < 1000; ++i) {
        vmc.update(silence, kFrameDt);
    }
    const float afterSilence = vmc.voiceProb();

    // Silence freeze should hold prior estimate within ~10% drift.
    TEST_ASSERT_FLOAT_WITHIN(0.1f, beforeSilence, afterSilence);
}

}  // namespace

void run_voice_music_classifier_tests() {
    RUN_TEST(test_default_state_is_zero);
    RUN_TEST(test_music_input_drives_voice_prob_low);
    RUN_TEST(test_voice_input_drives_voice_prob_high);
    RUN_TEST(test_smoothing_bounds_per_frame_change);
    RUN_TEST(test_reset_returns_to_default);
    RUN_TEST(test_output_clamped_to_unit_interval);
    RUN_TEST(test_silence_freezes_estimate);
}
