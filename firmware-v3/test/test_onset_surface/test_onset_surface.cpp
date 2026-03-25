#include <unity.h>

#include <fstream>
#include <string>

#include "../../src/audio/contracts/OnsetSemantics.h"
#include "../../src/effects/ieffect/AudioReactivePolicy.h"
#include "../../src/plugins/api/EffectContext.h"

using lightwaveos::audio::ControlBusFrame;
using lightwaveos::audio::MusicalGridSnapshot;
using lightwaveos::audio::OnsetSemanticInputs;
using lightwaveos::audio::OnsetSemanticTrackerState;
using lightwaveos::audio::updateOnsetContext;
using lightwaveos::effects::ieffect::AudioReactivePolicy::TriggerMode;
using lightwaveos::effects::ieffect::AudioReactivePolicy::audioTrigger;
using lightwaveos::plugins::EffectContext;

namespace {

std::string readTextFile(const char* path) {
    std::ifstream in(path);
    if (!in.good()) {
        TEST_FAIL_MESSAGE(path);
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

} // namespace

void test_onset_semantics_maps_transport_into_effect_surface() {
    ControlBusFrame bus{};
    MusicalGridSnapshot grid{};
    OnsetSemanticTrackerState tracker{};
    lightwaveos::plugins::OnsetContext onset{};

    bus.onsetFlux = 0.91f;
    bus.onsetEnv = 0.62f;
    bus.onsetEvent = 0.57f;
    bus.onsetBassFlux = 0.88f;
    bus.onsetMidFlux = 0.44f;
    bus.onsetHighFlux = 0.33f;
    bus.kickTrigger = true;
    bus.snareTrigger = true;
    bus.hihatTrigger = true;
    bus.snareEnergy = 0.42f;
    bus.hihatEnergy = 0.38f;

    grid.beat_phase01 = 0.25f;
    grid.bpm_smoothed = 134.0f;
    grid.tempo_confidence = 0.81f;
    grid.beat_tick = true;
    grid.downbeat_tick = true;
    grid.beat_strength = 0.72f;

    updateOnsetContext(OnsetSemanticInputs{bus, grid, true, false, 1000u, 1.0f / 120.0f}, tracker, onset);

    TEST_ASSERT_TRUE(onset.beat.fired);
    TEST_ASSERT_TRUE(onset.downbeat.fired);
    TEST_ASSERT_TRUE(onset.transient.fired);
    TEST_ASSERT_TRUE(onset.kick.fired);
    TEST_ASSERT_TRUE(onset.snare.fired);
    TEST_ASSERT_TRUE(onset.hihat.fired);
    TEST_ASSERT_TRUE(onset.timingReliable);
    TEST_ASSERT_TRUE(onset.kick.reliable);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.25f, onset.phase01);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 134.0f, onset.bpm);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.81f, onset.tempoConfidence);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.57f, onset.transient.strength01);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.62f, onset.transient.level01);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.88f, onset.kick.level01);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.44f, onset.snare.level01);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.38f, onset.hihat.level01);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.91f, onset.raw.flux);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.62f, onset.raw.env);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.57f, onset.raw.event);
}

void test_onset_semantics_tracks_sequence_age_and_interval() {
    ControlBusFrame bus{};
    MusicalGridSnapshot grid{};
    OnsetSemanticTrackerState tracker{};
    lightwaveos::plugins::OnsetContext onset{};

    bus.onsetEvent = 0.75f;
    bus.onsetEnv = 0.50f;

    updateOnsetContext(OnsetSemanticInputs{bus, grid, true, false, 1000u, 1.0f / 120.0f}, tracker, onset);
    TEST_ASSERT_EQUAL_UINT32(1u, onset.transient.sequence);
    TEST_ASSERT_EQUAL_UINT32(0u, onset.transient.intervalMs);
    TEST_ASSERT_EQUAL_UINT32(0u, onset.transient.ageMs);

    bus.onsetEvent = 0.0f;
    bus.onsetEnv = 0.20f;
    updateOnsetContext(OnsetSemanticInputs{bus, grid, true, false, 1120u, 1.0f / 120.0f}, tracker, onset);
    TEST_ASSERT_EQUAL_UINT32(1u, onset.transient.sequence);
    TEST_ASSERT_EQUAL_UINT32(120u, onset.transient.ageMs);

    bus.onsetEvent = 0.90f;
    bus.onsetEnv = 0.70f;
    updateOnsetContext(OnsetSemanticInputs{bus, grid, true, false, 1280u, 1.0f / 120.0f}, tracker, onset);
    TEST_ASSERT_EQUAL_UINT32(2u, onset.transient.sequence);
    TEST_ASSERT_EQUAL_UINT32(280u, onset.transient.intervalMs);
    TEST_ASSERT_EQUAL_UINT32(0u, onset.transient.ageMs);
}

void test_onset_semantics_marks_detector_channels_unreliable_under_trinity() {
    ControlBusFrame bus{};
    MusicalGridSnapshot grid{};
    OnsetSemanticTrackerState tracker{};
    lightwaveos::plugins::OnsetContext onset{};

    bus.onsetFlux = 0.80f;
    bus.onsetEnv = 0.50f;
    bus.onsetEvent = 0.40f;
    bus.kickTrigger = true;
    bus.snareTrigger = true;
    bus.hihatTrigger = true;
    bus.snareEnergy = 0.35f;
    bus.hihatEnergy = 0.25f;
    grid.beat_tick = true;
    grid.beat_strength = 0.60f;
    grid.tempo_confidence = 0.80f;

    updateOnsetContext(OnsetSemanticInputs{bus, grid, true, true, 1000u, 1.0f / 120.0f}, tracker, onset);

    TEST_ASSERT_TRUE(onset.beat.reliable);
    TEST_ASSERT_FALSE(onset.transient.reliable);
    TEST_ASSERT_FALSE(onset.kick.reliable);
    TEST_ASSERT_FALSE(onset.snare.reliable);
    TEST_ASSERT_FALSE(onset.hihat.reliable);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, onset.raw.flux);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, onset.raw.env);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, onset.raw.event);
}

void test_onset_wrappers_and_trigger_policy_use_first_class_surface() {
    EffectContext ctx{};
    uint32_t lastBeatMs = 0;

    ctx.audio.available = true;
    ctx.audio.onset.transient.fired = true;
    ctx.audio.onset.transient.level01 = 0.48f;
    ctx.audio.onset.raw.env = 0.48f;
    ctx.audio.onset.raw.event = 0.57f;
    ctx.audio.onset.kick.fired = true;
    ctx.audio.onset.kick.level01 = 0.66f;
    ctx.audio.onset.raw.bassFlux = 0.88f;

    TEST_ASSERT_TRUE(ctx.audio.hasOnsetEvent());
    TEST_ASSERT_TRUE(ctx.audio.isKickHit());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.48f, ctx.audio.onsetEnv());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.57f, ctx.audio.onsetEvent());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.88f, ctx.audio.kickFlux());

    const auto transient = audioTrigger(ctx, TriggerMode::Transient, 128.0f, lastBeatMs);
    TEST_ASSERT_TRUE(transient.fired);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.57f, transient.strength01);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.48f, transient.level01);

    const auto kick = audioTrigger(ctx, TriggerMode::Kick, 128.0f, lastBeatMs);
    TEST_ASSERT_TRUE(kick.fired);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.88f, kick.strength01);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.66f, kick.level01);
}

void test_pilot_effects_no_longer_reach_through_control_bus() {
    const char* bloomPath = "src/effects/ieffect/BeatPulseBloomEffect.cpp";
    const char* shockwavePath = "src/effects/ieffect/BeatPulseShockwaveEffect.cpp";
    const char* breathePath = "src/effects/ieffect/BeatPulseBreatheEffect.cpp";
    const char* ripplePath = "src/effects/ieffect/RippleEsTunedEffect.cpp";

    TEST_ASSERT_EQUAL_UINT64(static_cast<uint64_t>(std::string::npos),
                             static_cast<uint64_t>(readTextFile(bloomPath).find("ctx.audio.controlBus")));
    TEST_ASSERT_EQUAL_UINT64(static_cast<uint64_t>(std::string::npos),
                             static_cast<uint64_t>(readTextFile(shockwavePath).find("ctx.audio.controlBus")));
    TEST_ASSERT_EQUAL_UINT64(static_cast<uint64_t>(std::string::npos),
                             static_cast<uint64_t>(readTextFile(breathePath).find("ctx.audio.controlBus")));
    TEST_ASSERT_EQUAL_UINT64(static_cast<uint64_t>(std::string::npos),
                             static_cast<uint64_t>(readTextFile(ripplePath).find("ctx.audio.controlBus")));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_onset_semantics_maps_transport_into_effect_surface);
    RUN_TEST(test_onset_semantics_tracks_sequence_age_and_interval);
    RUN_TEST(test_onset_semantics_marks_detector_channels_unreliable_under_trinity);
    RUN_TEST(test_onset_wrappers_and_trigger_policy_use_first_class_surface);
    RUN_TEST(test_pilot_effects_no_longer_reach_through_control_bus);
    return UNITY_END();
}
