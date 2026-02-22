/**
 * Audio reactive policy tests
 *
 * Verifies raw-vs-scaled timing contract and fallback beat tick behaviour.
 */

#include <unity.h>

#include "../../src/effects/ieffect/AudioReactivePolicy.h"

using lightwaveos::plugins::EffectContext;
using lightwaveos::effects::ieffect::AudioReactivePolicy::audioBeatTick;
using lightwaveos::effects::ieffect::AudioReactivePolicy::signalDt;
using lightwaveos::effects::ieffect::AudioReactivePolicy::visualDt;

void test_audio_policy_uses_raw_dt_for_signal_math() {
    EffectContext ctx;
    ctx.deltaTimeSeconds = 0.030f;
    ctx.rawDeltaTimeSeconds = 0.009f;
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.009f, signalDt(ctx));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.030f, visualDt(ctx));
}

void test_audio_policy_clamps_signal_dt() {
    EffectContext ctx;
    ctx.rawDeltaTimeSeconds = 0.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0001f, signalDt(ctx));

    ctx.rawDeltaTimeSeconds = 1.2f;
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.05f, signalDt(ctx));
}

void test_audio_policy_fallback_beat_uses_raw_time() {
    EffectContext ctx;
    ctx.audio.available = false;
    uint32_t lastBeatMs = 0;

    ctx.rawTotalTimeMs = 1000;
    TEST_ASSERT_TRUE(audioBeatTick(ctx, 128.0f, lastBeatMs));
    TEST_ASSERT_EQUAL_UINT32(1000u, lastBeatMs);

    ctx.rawTotalTimeMs = 1200;
    TEST_ASSERT_FALSE(audioBeatTick(ctx, 128.0f, lastBeatMs));
    TEST_ASSERT_EQUAL_UINT32(1000u, lastBeatMs);

    ctx.rawTotalTimeMs = 1500;
    TEST_ASSERT_TRUE(audioBeatTick(ctx, 128.0f, lastBeatMs));
    TEST_ASSERT_EQUAL_UINT32(1500u, lastBeatMs);
}

void test_audio_policy_ignores_audio_tick_when_tempo_conf_low() {
    EffectContext ctx;
    ctx.audio.available = true;
    ctx.audio.musicalGrid.beat_tick = true;
    ctx.audio.musicalGrid.tempo_confidence = 0.10f;  // below 0.25 gate
    uint32_t lastBeatMs = 1000;

    ctx.rawTotalTimeMs = 1100;
    TEST_ASSERT_FALSE(audioBeatTick(ctx, 128.0f, lastBeatMs));
    TEST_ASSERT_EQUAL_UINT32(1000u, lastBeatMs);
}

void run_audio_reactive_policy_tests() {
    RUN_TEST(test_audio_policy_uses_raw_dt_for_signal_math);
    RUN_TEST(test_audio_policy_clamps_signal_dt);
    RUN_TEST(test_audio_policy_fallback_beat_uses_raw_time);
    RUN_TEST(test_audio_policy_ignores_audio_tick_when_tempo_conf_low);
}
