#include <unity.h>

#include "../../src/plugins/api/EffectContext.h"

using lightwaveos::plugins::EffectContext;

void test_onset_surface_exposes_semantic_channels() {
    EffectContext ctx{};
    ctx.audio.available = true;
    ctx.audio.onset.phase01 = 0.25f;
    ctx.audio.onset.bpm = 134.0f;
    ctx.audio.onset.tempoConfidence = 0.81f;
    ctx.audio.onset.timingReliable = true;
    ctx.audio.onset.beat.fired = true;
    ctx.audio.onset.beat.level01 = 0.72f;
    ctx.audio.onset.downbeat.fired = true;
    ctx.audio.onset.transient.fired = true;
    ctx.audio.onset.transient.level01 = 0.48f;
    ctx.audio.onset.kick.fired = true;
    ctx.audio.onset.kick.level01 = 0.66f;
    ctx.audio.onset.snare.fired = true;
    ctx.audio.onset.snare.level01 = 0.42f;
    ctx.audio.onset.hihat.fired = true;
    ctx.audio.onset.hihat.level01 = 0.38f;
    ctx.audio.onset.raw.flux = 0.91f;
    ctx.audio.onset.raw.env = 0.62f;
    ctx.audio.onset.raw.event = 0.57f;
    ctx.audio.onset.raw.bassFlux = 0.88f;
    ctx.audio.onset.raw.midFlux = 0.44f;
    ctx.audio.onset.raw.highFlux = 0.33f;

    TEST_ASSERT_TRUE(ctx.audio.isOnBeat());
    TEST_ASSERT_TRUE(ctx.audio.isOnDownbeat());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.72f, ctx.audio.beatStrength());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 134.0f, ctx.audio.bpm());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.81f, ctx.audio.tempoConfidence());
    TEST_ASSERT_TRUE(ctx.audio.hasOnsetEvent());
    TEST_ASSERT_TRUE(ctx.audio.isKickHit());
    TEST_ASSERT_TRUE(ctx.audio.isSnareHit());
    TEST_ASSERT_TRUE(ctx.audio.isHihatHit());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.62f, ctx.audio.onsetEnv());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.57f, ctx.audio.onsetEvent());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.91f, ctx.audio.onsetFlux());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.88f, ctx.audio.kickFlux());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.44f, ctx.audio.snareFlux());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.33f, ctx.audio.hihatFlux());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.66f, ctx.audio.kickLevel());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.42f, ctx.audio.snare());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.38f, ctx.audio.hihat());
}

void test_onset_wrappers_fall_back_to_legacy_fields_when_surface_is_unset() {
    EffectContext ctx{};
    ctx.audio.available = true;
    ctx.audio.musicalGrid.beat_tick = true;
    ctx.audio.musicalGrid.downbeat_tick = true;
    ctx.audio.musicalGrid.beat_strength = 0.54f;
    ctx.audio.musicalGrid.bpm_smoothed = 128.0f;
    ctx.audio.musicalGrid.tempo_confidence = 0.64f;
    ctx.audio.controlBus.onsetFlux = 0.83f;
    ctx.audio.controlBus.onsetEnv = 0.51f;
    ctx.audio.controlBus.onsetEvent = 0.47f;
    ctx.audio.controlBus.onsetBassFlux = 0.92f;
    ctx.audio.controlBus.onsetMidFlux = 0.31f;
    ctx.audio.controlBus.onsetHighFlux = 0.28f;
    ctx.audio.controlBus.kickTrigger = true;
    ctx.audio.controlBus.snareTrigger = true;
    ctx.audio.controlBus.hihatTrigger = true;
    ctx.audio.controlBus.snareEnergy = 0.36f;
    ctx.audio.controlBus.hihatEnergy = 0.22f;

    TEST_ASSERT_TRUE(ctx.audio.isOnBeat());
    TEST_ASSERT_TRUE(ctx.audio.isOnDownbeat());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.54f, ctx.audio.beatStrength());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 128.0f, ctx.audio.bpm());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.64f, ctx.audio.tempoConfidence());
    TEST_ASSERT_TRUE(ctx.audio.hasOnsetEvent());
    TEST_ASSERT_TRUE(ctx.audio.isKickHit());
    TEST_ASSERT_TRUE(ctx.audio.isSnareHit());
    TEST_ASSERT_TRUE(ctx.audio.isHihatHit());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.51f, ctx.audio.onsetEnv());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.47f, ctx.audio.onsetEvent());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.83f, ctx.audio.onsetFlux());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.92f, ctx.audio.kickFlux());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.31f, ctx.audio.snareFlux());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.28f, ctx.audio.hihatFlux());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.36f, ctx.audio.snare());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.22f, ctx.audio.hihat());
}

void run_onset_effect_context_tests() {
    RUN_TEST(test_onset_surface_exposes_semantic_channels);
    RUN_TEST(test_onset_wrappers_fall_back_to_legacy_fields_when_surface_is_unset);
}
