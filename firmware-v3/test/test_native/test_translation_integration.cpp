/**
 * Translation integration tests across transport and helper seams.
 */

#include <unity.h>

#include "../../src/plugins/api/EffectContext.h"
#include "../../src/effects/ieffect/AudioReactiveLowRiskPackHelpers.h"

using lightwaveos::audio::ControlBusFrame;
using lightwaveos::audio::MotionPrimitive;
using lightwaveos::audio::kDefaultSceneParameters;
using lightwaveos::effects::ieffect::lowrisk_ar::Ar16Controls;
using lightwaveos::effects::ieffect::lowrisk_ar::ArRuntimeState;
using lightwaveos::effects::ieffect::lowrisk_ar::updateSignals;
using lightwaveos::plugins::AudioContext;
using lightwaveos::plugins::EffectContext;

void test_scene_survives_controlbus_to_effect_context_copy_path() {
    ControlBusFrame published{};
    published.scene = kDefaultSceneParameters;
    published.scene.motion_type = MotionPrimitive::FLOW;
    published.scene.phrase_progress = 0.42f;
    published.scene.tension = 0.63f;
    published.scene.beat_pulse = 0.55f;
    published.scene.timing_reliable = true;

    AudioContext shared{};
    shared.controlBus = published;  // RendererActor shared copy stage
    shared.available = true;

    EffectContext ctx{};
    ctx.audio = shared;             // EffectContext by-value copy stage

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MotionPrimitive::FLOW),
                            static_cast<uint8_t>(ctx.audio.motionType()));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.42f, ctx.audio.phraseProgress());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.63f, ctx.audio.tension());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.55f, ctx.audio.beatPulse());
    TEST_ASSERT_TRUE(ctx.audio.timingReliable());
}

void test_helper_path_modulates_motion_rate_and_beat_accent_from_scene() {
    EffectContext ctx{};
    ctx.audio.available = true;
    ctx.rawTotalTimeMs = 2000u;
    ctx.rawDeltaTimeSeconds = 0.016f;
    ctx.deltaTimeSeconds = 0.016f;
    ctx.gHue = 24u;

    // Stable baseline with no natural beat pulse.
    ctx.audio.musicalGrid.beat_tick = false;
    ctx.audio.musicalGrid.beat_strength = 0.0f;
    ctx.audio.musicalGrid.tempo_confidence = 0.0f;
    for (uint8_t i = 0; i < lightwaveos::audio::CONTROLBUS_NUM_BANDS; ++i) {
        ctx.audio.controlBus.bands[i] = 0.25f;
        ctx.audio.controlBus.heavy_bands[i] = 0.25f;
    }
    for (uint8_t i = 0; i < lightwaveos::audio::CONTROLBUS_NUM_CHROMA; ++i) {
        ctx.audio.controlBus.heavy_chroma[i] = 0.05f;
    }

    ArRuntimeState state{};
    Ar16Controls controls;
    controls.resetDefaults();

    ctx.audio.controlBus.scene = kDefaultSceneParameters;
    ctx.audio.controlBus.scene.motion_rate = 0.60f;
    ctx.audio.controlBus.scene.beat_pulse = 0.10f;
    auto lowScene = updateSignals(state, ctx, controls, 0.016f);
    const float lowRate = controls.motionRate();

    ctx.rawTotalTimeMs += 16u;
    ctx.audio.controlBus.scene.motion_rate = 1.75f;
    ctx.audio.controlBus.scene.beat_pulse = 0.72f;
    auto highScene = updateSignals(state, ctx, controls, 0.016f);
    const float highRate = controls.motionRate();

    TEST_ASSERT_TRUE(highRate > lowRate);
    TEST_ASSERT_TRUE(highScene.beat >= 0.72f);
    TEST_ASSERT_TRUE(highScene.beat >= lowScene.beat);
}

void run_translation_integration_tests() {
    RUN_TEST(test_scene_survives_controlbus_to_effect_context_copy_path);
    RUN_TEST(test_helper_path_modulates_motion_rate_and_beat_accent_from_scene);
}
