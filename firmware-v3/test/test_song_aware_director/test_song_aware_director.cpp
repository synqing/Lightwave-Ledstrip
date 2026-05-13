#ifdef NATIVE_BUILD

#include <cstring>

#include <unity.h>

#include "../../src/core/songaware/SongAwareDirector.h"

using lightwaveos::audio::ControlBusFrame;
using lightwaveos::audio::MusicalGridSnapshot;
using lightwaveos::INVALID_EFFECT_ID;
using lightwaveos::songaware::SongAwareClassificationReason;
using lightwaveos::songaware::SongAwareConfig;
using lightwaveos::songaware::SongAwareDirector;
using lightwaveos::songaware::SongAwareDirectorContext;
using lightwaveos::songaware::SongAwareLastAction;
using lightwaveos::songaware::SongAwareMode;
using lightwaveos::songaware::SongAwareProfile;
using lightwaveos::songaware::SongAwareOwner;
using lightwaveos::songaware::SongAwareParams;
using lightwaveos::songaware::SongAwarePolicySnapshot;
using lightwaveos::songaware::SongAwareRuntimeState;
using lightwaveos::songaware::SongAwareState;
using lightwaveos::songaware::SongAwareSuppressedReason;
using lightwaveos::songaware::SongAwareSwitchRequest;
using lightwaveos::songaware::SongAwareActionPlan;
using lightwaveos::songaware::SongAwareBoundaryGate;
using lightwaveos::songaware::SongAwareIntent;
using lightwaveos::songaware::kSongAwareMaxPolicySnapshotCount;
using lightwaveos::songaware::parseSongAwareMode;
using lightwaveos::songaware::parseSongAwareProfile;
using lightwaveos::songaware::songAwareClassificationReasonName;
using lightwaveos::songaware::songAwareLastActionName;
using lightwaveos::songaware::songAwareModeName;
using lightwaveos::songaware::songAwareProfileName;
using lightwaveos::songaware::songAwareOwnerName;
using lightwaveos::songaware::songAwareStateName;
using lightwaveos::songaware::songAwareSuppressedReasonName;

namespace {

SongAwareConfig makeConfig(SongAwareMode mode = SongAwareMode::Director,
                           bool switchingEnabled = true,
                           SongAwareProfile profile = SongAwareProfile::Balanced) {
    SongAwareConfig cfg;
    cfg.enabled = true;
    cfg.mode = mode;
    cfg.profile = profile;
    cfg.familyMorphing = false;
    cfg.constrainedSwitching = switchingEnabled;
    cfg.switchingEnabled = switchingEnabled;
    cfg.sensitivity = 1.0f;
    cfg.intensityScalar = 1.0f;
    cfg.motionScalar = 1.0f;
    cfg.confidenceFloor = 0.20f;
    return cfg;
}

void restoreReadyDirector(SongAwareDirector& director,
                          const SongAwareConfig& cfg = makeConfig(),
                          SongAwareState stableState = SongAwareState::Unknown) {
    director.reset();

    SongAwareRuntimeState runtime = director.exportRuntimeState();
    runtime.config = cfg;
    runtime.status.effectiveMode = cfg.enabled ? cfg.mode : SongAwareMode::Off;
    runtime.status.profile = cfg.profile;
    runtime.status.suppressedReason = cfg.enabled ? SongAwareSuppressedReason::None
                                                  : SongAwareSuppressedReason::Disabled;
    runtime.status.currentSongState = stableState;
    runtime.status.rawSongState = stableState;
    runtime.status.candidateSongState = stableState;
    runtime.bootGraceUntilMs = 0;
    runtime.enableGraceUntilMs = 0;
    runtime.antiThrashUntilMs = 0;
    runtime.healthCleanSinceMs = 0;
    runtime.healthLastDegradedAtMs = 0;
    director.restoreRuntimeState(runtime);
}

MusicalGridSnapshot readyBoundaryGrid() {
    MusicalGridSnapshot grid;
    grid.bpm_smoothed = 124.0f;
    grid.tempo_confidence = 0.82f;
    grid.beat_phase01 = 0.0f;
    grid.bar_phase01 = 0.0f;
    grid.beat_tick = true;
    grid.downbeat_tick = true;
    grid.beat_strength = 0.50f;
    return grid;
}

MusicalGridSnapshot waitingBoundaryGrid() {
    MusicalGridSnapshot grid;
    grid.bpm_smoothed = 124.0f;
    grid.tempo_confidence = 0.82f;
    grid.beat_phase01 = 0.42f;
    grid.bar_phase01 = 0.42f;
    grid.beat_tick = false;
    grid.downbeat_tick = false;
    grid.beat_strength = 0.50f;
    return grid;
}

ControlBusFrame baseFrame(float confidence = 0.85f) {
    ControlBusFrame frame;
    frame.audioConfidence = confidence;
    frame.silentScale = 1.0f;
    frame.isSilent = false;
    frame.rms = 0.32f;
    frame.fast_rms = frame.rms;
    frame.flux = 0.18f;
    frame.fast_flux = frame.flux;
    frame.liveliness = 0.32f;
    frame.saliency.overallSaliency = 0.20f;
    frame.tempoConfidence = 0.40f;
    frame.tempoBpm = 124.0f;
    frame.es_tempo_confidence = 0.40f;
    frame.es_bpm = 124.0f;
    return frame;
}

ControlBusFrame frameForState(SongAwareState state) {
    ControlBusFrame frame = baseFrame();
    switch (state) {
        case SongAwareState::Silence:
            frame.isSilent = true;
            frame.silentScale = 0.0f;
            frame.rms = 0.0f;
            frame.fast_rms = 0.0f;
            frame.flux = 0.0f;
            frame.fast_flux = 0.0f;
            frame.liveliness = 0.0f;
            break;
        case SongAwareState::Ambient:
            frame.rms = 0.18f;
            frame.fast_rms = 0.18f;
            frame.flux = 0.10f;
            frame.fast_flux = 0.10f;
            frame.liveliness = 0.30f;
            frame.tempoConfidence = 0.0f;
            frame.es_tempo_confidence = 0.0f;
            break;
        case SongAwareState::Steady:
            break;
        case SongAwareState::Build:
            frame.rms = 0.40f;
            frame.fast_rms = 0.40f;
            frame.flux = 0.46f;
            frame.fast_flux = 0.46f;
            frame.liveliness = 0.50f;
            frame.saliency.overallSaliency = 0.52f;
            break;
        case SongAwareState::Drop:
            frame.rms = 0.54f;
            frame.fast_rms = 0.54f;
            frame.liveliness = 0.66f;
            frame.saliency.overallSaliency = 0.68f;
            frame.onsetEvent = 0.75f;
            break;
        case SongAwareState::Breakdown:
            frame.rms = 0.18f;
            frame.fast_rms = 0.18f;
            frame.flux = 0.08f;
            frame.fast_flux = 0.08f;
            frame.liveliness = 0.20f;
            frame.tempoConfidence = 0.50f;
            frame.es_tempo_confidence = 0.50f;
            break;
        case SongAwareState::Dense:
            frame.rms = 0.72f;
            frame.fast_rms = 0.72f;
            frame.flux = 0.34f;
            frame.fast_flux = 0.34f;
            frame.liveliness = 0.76f;
            break;
        case SongAwareState::Transition:
            frame.rms = 0.34f;
            frame.fast_rms = 0.34f;
            frame.flux = 0.74f;
            frame.fast_flux = 0.74f;
            frame.liveliness = 0.44f;
            frame.saliency.overallSaliency = 0.76f;
            break;
        default:
            break;
    }
    return frame;
}

uint16_t policyEffectForState(SongAwareState state) {
    SongAwareDirector director;
    const auto selection = director.resolveSelection(state, 1.0f, INVALID_EFFECT_ID);
    return selection.policy.effectId;
}

SongAwareSwitchRequest evaluateForState(SongAwareDirector& director,
                                        SongAwareState state,
                                        uint32_t nowMs,
                                        uint16_t activeEffectId,
                                        SongAwareDirectorContext context = SongAwareDirectorContext{}) {
    SongAwareSwitchRequest request;
    MusicalGridSnapshot grid = readyBoundaryGrid();
    const ControlBusFrame frame = frameForState(state);
    director.evaluateDirector(frame, grid, true, nowMs, activeEffectId, context, request);
    return request;
}

SongAwareParams baseParams(uint16_t effectId = 0x1302) {
    SongAwareParams params;
    params.effectId = effectId;
    params.brightness = 160;
    params.speed = 27;
    params.intensity = 128;
    params.saturation = 128;
    params.complexity = 128;
    params.variation = 0;
    params.hue = 0;
    return params;
}

void assertClassification(SongAwareState expectedState,
                          SongAwareClassificationReason expectedReason,
                          uint32_t nowMs) {
    SongAwareDirector director;
    restoreReadyDirector(director);

    SongAwareSwitchRequest request;
    MusicalGridSnapshot grid = readyBoundaryGrid();
    if (expectedState == SongAwareState::Ambient) {
        grid.tempo_confidence = 0.0f;
        grid.beat_strength = 0.0f;
    }
    const ControlBusFrame frame = frameForState(expectedState);
    director.evaluateDirector(frame, grid, true, nowMs, INVALID_EFFECT_ID,
                              SongAwareDirectorContext{}, request);
    const auto status = director.getStatus();

    (void)request;
    TEST_ASSERT_EQUAL(expectedState, status.rawSongState);
    TEST_ASSERT_EQUAL(expectedState, status.currentSongState);
    TEST_ASSERT_EQUAL(expectedReason, status.classificationReason);
}

} // namespace

void test_song_aware_defaults_reset_and_off_disable_activity() {
    SongAwareDirector director;
    const auto defaults = director.getStatus();

    TEST_ASSERT_FALSE(defaults.enabled);
    TEST_ASSERT_EQUAL(SongAwareMode::Off, defaults.effectiveMode);
    TEST_ASSERT_EQUAL(SongAwareSuppressedReason::Disabled, defaults.suppressedReason);
    TEST_ASSERT_EQUAL_UINT32(0, defaults.parameterUpdates);
    TEST_ASSERT_EQUAL_UINT32(0, defaults.automaticEffectSwitches);

    restoreReadyDirector(director, makeConfig(SongAwareMode::Off, false));
    ControlBusFrame frame = frameForState(SongAwareState::Drop);
    MusicalGridSnapshot grid;
    SongAwareParams params = baseParams();

    TEST_ASSERT_FALSE(director.apply(frame, grid, true, 1.0f / 120.0f, 10000, params));
    const auto offStatus = director.getStatus();
    TEST_ASSERT_EQUAL(SongAwareMode::Off, offStatus.effectiveMode);
    TEST_ASSERT_EQUAL(SongAwareSuppressedReason::Disabled, offStatus.suppressedReason);
    TEST_ASSERT_EQUAL(SongAwareLastAction::None, offStatus.lastAction);

    director.reset();
    const auto resetStatus = director.getStatus();
    TEST_ASSERT_FALSE(resetStatus.enabled);
    TEST_ASSERT_EQUAL(SongAwareMode::Off, resetStatus.effectiveMode);
}

void test_song_aware_mode_parsing_and_names_cover_public_modes() {
    bool ok = false;
    TEST_ASSERT_EQUAL(SongAwareMode::Off, parseSongAwareMode("off", &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(SongAwareMode::Assist, parseSongAwareMode("assist", &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(SongAwareMode::Assist, parseSongAwareMode("parameter", &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(SongAwareMode::Assist, parseSongAwareMode("on", &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(SongAwareMode::Assist, parseSongAwareMode("subtle", &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(SongAwareMode::Assist, parseSongAwareMode("balanced", &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(SongAwareMode::Assist, parseSongAwareMode("high", &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(SongAwareMode::Director, parseSongAwareMode("director", &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(SongAwareMode::Off, parseSongAwareMode("not-a-mode", &ok));
    TEST_ASSERT_FALSE(ok);

    TEST_ASSERT_EQUAL_STRING("off", songAwareModeName(SongAwareMode::Off));
    TEST_ASSERT_EQUAL_STRING("assist", songAwareModeName(SongAwareMode::Assist));
    TEST_ASSERT_EQUAL_STRING("director", songAwareModeName(SongAwareMode::Director));

    TEST_ASSERT_EQUAL(SongAwareProfile::Subtle, parseSongAwareProfile("subtle", &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(SongAwareProfile::Balanced, parseSongAwareProfile("balanced", &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(SongAwareProfile::High, parseSongAwareProfile("high", &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(SongAwareProfile::Balanced, parseSongAwareProfile("not-a-profile", &ok));
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_STRING("subtle", songAwareProfileName(SongAwareProfile::Subtle));
    TEST_ASSERT_EQUAL_STRING("balanced", songAwareProfileName(SongAwareProfile::Balanced));
    TEST_ASSERT_EQUAL_STRING("high", songAwareProfileName(SongAwareProfile::High));
}

void test_song_aware_classifier_reports_each_director_state() {
    assertClassification(SongAwareState::Silence,
                         SongAwareClassificationReason::SilentFrame,
                         10000);
    assertClassification(SongAwareState::Ambient,
                         SongAwareClassificationReason::AmbientLowEnergy,
                         11000);
    assertClassification(SongAwareState::Steady,
                         SongAwareClassificationReason::SteadyDefault,
                         12000);
    assertClassification(SongAwareState::Build,
                         SongAwareClassificationReason::BuildEnergy,
                         13000);
    assertClassification(SongAwareState::Drop,
                         SongAwareClassificationReason::DropOnset,
                         14000);
    assertClassification(SongAwareState::Breakdown,
                         SongAwareClassificationReason::QuietBreakdown,
                         15000);
    assertClassification(SongAwareState::Dense,
                         SongAwareClassificationReason::DenseEnergy,
                         16000);
    assertClassification(SongAwareState::Transition,
                         SongAwareClassificationReason::SpectralTransition,
                         17000);
}

void test_song_aware_boot_and_enable_grace_suppress_switching() {
    SongAwareDirector director;
    director.reset();
    director.setConfig(makeConfig());

    const SongAwareSwitchRequest bootRequest =
        evaluateForState(director, SongAwareState::Drop, 1000, INVALID_EFFECT_ID);
    auto status = director.getStatus();
    TEST_ASSERT_FALSE(bootRequest.requested);
    TEST_ASSERT_EQUAL(SongAwareSuppressedReason::BootGrace, status.suppressedReason);
    TEST_ASSERT_TRUE(status.bootGraceRemainingMs > 0);

    SongAwareRuntimeState runtime = director.exportRuntimeState();
    runtime.config = SongAwareConfig{};
    runtime.bootGraceUntilMs = 0;
    runtime.enableGraceUntilMs = 0;
    runtime.status.currentSongState = SongAwareState::Drop;
    runtime.status.rawSongState = SongAwareState::Drop;
    runtime.status.candidateSongState = SongAwareState::Drop;
    director.restoreRuntimeState(runtime);
    director.setConfig(makeConfig());

    const SongAwareSwitchRequest enableRequest =
        evaluateForState(director, SongAwareState::Drop, 5000, INVALID_EFFECT_ID);
    status = director.getStatus();
    TEST_ASSERT_FALSE(enableRequest.requested);
    TEST_ASSERT_EQUAL(SongAwareSuppressedReason::EnableGrace, status.suppressedReason);
    TEST_ASSERT_TRUE(status.enableGraceRemainingMs > 0);
}

void test_song_aware_switching_disabled_for_assist_or_disabled_switching_modes() {
    SongAwareDirector director;

    restoreReadyDirector(director, makeConfig(SongAwareMode::Assist, true), SongAwareState::Drop);
    SongAwareSwitchRequest request =
        evaluateForState(director, SongAwareState::Drop, 10000, INVALID_EFFECT_ID);
    auto status = director.getStatus();
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL(SongAwareMode::Assist, status.effectiveMode);
    TEST_ASSERT_EQUAL(SongAwareSuppressedReason::SwitchingDisabled, status.suppressedReason);

    restoreReadyDirector(director, makeConfig(SongAwareMode::Director, false), SongAwareState::Drop);
    request = evaluateForState(director, SongAwareState::Drop, 11000, INVALID_EFFECT_ID);
    status = director.getStatus();
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL(SongAwareSuppressedReason::SwitchingDisabled, status.suppressedReason);
}

void test_song_aware_successful_switch_request_and_applied_counters() {
    SongAwareDirector director;
    restoreReadyDirector(director, makeConfig(), SongAwareState::Drop);

    const uint16_t targetEffect = policyEffectForState(SongAwareState::Drop);
    const SongAwareSwitchRequest request =
        evaluateForState(director, SongAwareState::Drop, 10000, INVALID_EFFECT_ID);

    TEST_ASSERT_TRUE(request.requested);
    TEST_ASSERT_EQUAL_UINT16(targetEffect, request.targetEffectId);
    TEST_ASSERT_EQUAL_STRING("advanced_optical", request.targetFamily);
    TEST_ASSERT_EQUAL_STRING("photonic_drop_texture", request.targetVisualLanguage);
    TEST_ASSERT_EQUAL_STRING("drop_impact", request.reason);

    director.notifySwitchApplied(0x2222, targetEffect, 10000, "Drop target");
    const auto status = director.getStatus();
    TEST_ASSERT_EQUAL_UINT32(1, status.automaticEffectSwitches);
    TEST_ASSERT_EQUAL(SongAwareLastAction::EffectSwitch, status.lastAction);
    TEST_ASSERT_EQUAL(SongAwareOwner::Director, status.owner);
    TEST_ASSERT_EQUAL(SongAwareSuppressedReason::None, status.suppressedReason);
    TEST_ASSERT_EQUAL_UINT16(0x2222, status.lastSwitchFromEffectId);
    TEST_ASSERT_EQUAL_UINT16(targetEffect, status.lastSwitchToEffectId);
    TEST_ASSERT_EQUAL_UINT16(targetEffect, status.activeEffectId);
    TEST_ASSERT_TRUE(status.cooldownRemainingMs > 0);
    TEST_ASSERT_TRUE(status.antiThrashRemainingMs > 0);
}

void test_song_aware_same_effect_suppresses_without_counting_switch() {
    SongAwareDirector director;
    restoreReadyDirector(director, makeConfig(), SongAwareState::Drop);

    const uint16_t targetEffect = policyEffectForState(SongAwareState::Drop);
    const SongAwareSwitchRequest request =
        evaluateForState(director, SongAwareState::Drop, 10000, targetEffect);
    const auto status = director.getStatus();

    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL(SongAwareSuppressedReason::SameEffect, status.suppressedReason);
    TEST_ASSERT_EQUAL(SongAwareLastAction::None, status.lastAction);
    TEST_ASSERT_EQUAL_UINT32(0, status.automaticEffectSwitches);
}

void test_song_aware_dwell_gate_blocks_newly_established_state() {
    SongAwareDirector director;
    restoreReadyDirector(director);

    const SongAwareSwitchRequest request =
        evaluateForState(director, SongAwareState::Drop, 10000, INVALID_EFFECT_ID);
    const auto status = director.getStatus();

    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL(SongAwareSuppressedReason::Dwell, status.suppressedReason);
    TEST_ASSERT_TRUE(status.dwellRemainingMs > 0);
}

void test_song_aware_cooldown_blocks_after_recent_switch() {
    SongAwareDirector director;
    restoreReadyDirector(director, makeConfig(), SongAwareState::Drop);

    const uint16_t dropEffect = policyEffectForState(SongAwareState::Drop);
    const uint16_t buildEffect = policyEffectForState(SongAwareState::Build);
    director.notifySwitchApplied(0x2222, dropEffect, 10000, "Drop target");

    SongAwareRuntimeState runtime = director.exportRuntimeState();
    runtime.status.currentSongState = SongAwareState::Build;
    runtime.status.rawSongState = SongAwareState::Build;
    runtime.status.candidateSongState = SongAwareState::Build;
    director.restoreRuntimeState(runtime);

    const SongAwareSwitchRequest request =
        evaluateForState(director, SongAwareState::Build, 15000, dropEffect);
    const auto status = director.getStatus();

    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL_UINT16(buildEffect, status.selectedEffectId);
    TEST_ASSERT_EQUAL(SongAwareSuppressedReason::Cooldown, status.suppressedReason);
    TEST_ASSERT_TRUE(status.cooldownRemainingMs > 0);
}

void test_song_aware_rate_limit_blocks_third_switch_inside_window() {
    SongAwareDirector director;
    restoreReadyDirector(director, makeConfig(), SongAwareState::Dense);

    const uint16_t buildEffect = policyEffectForState(SongAwareState::Build);
    const uint16_t dropEffect = policyEffectForState(SongAwareState::Drop);
    const uint16_t denseEffect = policyEffectForState(SongAwareState::Dense);
    director.notifySwitchApplied(0x1111, buildEffect, 10000, "Build target");
    director.notifySwitchApplied(buildEffect, dropEffect, 40000, "Drop target");

    const SongAwareSwitchRequest request =
        evaluateForState(director, SongAwareState::Dense, 61000, dropEffect);
    const auto status = director.getStatus();

    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL_UINT16(denseEffect, status.selectedEffectId);
    TEST_ASSERT_EQUAL_UINT8(2, status.switchesInWindow);
    TEST_ASSERT_TRUE(status.switchWindowRemainingMs > 0);
    TEST_ASSERT_EQUAL(SongAwareSuppressedReason::RateLimit, status.suppressedReason);
}

void test_song_aware_anti_thrash_blocks_immediate_aba_switch() {
    SongAwareDirector director;
    restoreReadyDirector(director, makeConfig(), SongAwareState::Build);

    const uint16_t buildEffect = policyEffectForState(SongAwareState::Build);
    const uint16_t dropEffect = policyEffectForState(SongAwareState::Drop);
    director.notifySwitchApplied(buildEffect, dropEffect, 10000, "Drop target");

    const SongAwareSwitchRequest request =
        evaluateForState(director, SongAwareState::Build, 11000, dropEffect);
    const auto status = director.getStatus();

    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL_UINT16(buildEffect, status.selectedEffectId);
    TEST_ASSERT_EQUAL(SongAwareSuppressedReason::AntiThrash, status.suppressedReason);
    TEST_ASSERT_TRUE(status.antiThrashRemainingMs > 0);
}

void test_song_aware_manual_and_show_owners_suppress_director_switches() {
    SongAwareDirector director;
    restoreReadyDirector(director, makeConfig(), SongAwareState::Drop);
    director.markShowControl(10000);

    SongAwareSwitchRequest request =
        evaluateForState(director, SongAwareState::Drop, 10500, INVALID_EFFECT_ID);
    auto status = director.getStatus();
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_TRUE(director.isShowOwnerActive(10500));
    TEST_ASSERT_EQUAL(SongAwareOwner::Show, status.owner);
    TEST_ASSERT_EQUAL(SongAwareSuppressedReason::ShowOwner, status.suppressedReason);

    restoreReadyDirector(director, makeConfig(), SongAwareState::Drop);
    director.markManualControl(20000);
    request = evaluateForState(director, SongAwareState::Drop, 20500, INVALID_EFFECT_ID);
    status = director.getStatus();
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL(SongAwareOwner::Manual, status.owner);
    TEST_ASSERT_EQUAL(SongAwareSuppressedReason::ManualOwner, status.suppressedReason);
}

void test_song_aware_health_gate_and_recovery_window_suppress_switches() {
    SongAwareDirector director;
    restoreReadyDirector(director, makeConfig(), SongAwareState::Drop);

    SongAwareDirectorContext context;
    context.health.showSkips = 1;
    SongAwareSwitchRequest request =
        evaluateForState(director, SongAwareState::Drop, 10000, INVALID_EFFECT_ID, context);
    auto status = director.getStatus();
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL(SongAwareSuppressedReason::Health, status.suppressedReason);
    TEST_ASSERT_TRUE(status.healthDegraded);
    TEST_ASSERT_EQUAL_UINT32(1, status.showSkips);

    context.health.showSkips = 0;
    request = evaluateForState(director, SongAwareState::Drop, 11000, INVALID_EFFECT_ID, context);
    status = director.getStatus();
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL(SongAwareSuppressedReason::HealthRecovering, status.suppressedReason);
    TEST_ASSERT_FALSE(status.healthDegraded);
    TEST_ASSERT_TRUE(status.healthCleanWindowRemainingMs > 0);
}

void test_song_aware_restore_runtime_state_and_reset_counters() {
    SongAwareDirector director;
    restoreReadyDirector(director, makeConfig(), SongAwareState::Drop);
    const uint16_t targetEffect = policyEffectForState(SongAwareState::Drop);
    director.notifySwitchApplied(0x2222, targetEffect, 10000, "Drop target");

    ControlBusFrame frame = frameForState(SongAwareState::Drop);
    MusicalGridSnapshot grid;
    SongAwareParams params = baseParams(targetEffect);
    TEST_ASSERT_TRUE(director.apply(frame, grid, true, 1.0f / 120.0f, 12000, params));

    const SongAwareRuntimeState saved = director.exportRuntimeState();
    SongAwareDirector restored;
    restored.restoreRuntimeState(saved);
    auto status = restored.getStatus();

    TEST_ASSERT_TRUE(restored.getConfig().enabled);
    TEST_ASSERT_EQUAL(SongAwareMode::Director, restored.getConfig().mode);
    TEST_ASSERT_EQUAL_UINT32(1, status.automaticEffectSwitches);
    TEST_ASSERT_EQUAL_UINT32(1, status.parameterUpdates);
    TEST_ASSERT_EQUAL_UINT16(targetEffect, status.activeEffectId);

    restored.resetCounters();
    status = restored.getStatus();
    TEST_ASSERT_EQUAL_UINT32(0, status.automaticEffectSwitches);
    TEST_ASSERT_EQUAL_UINT32(0, status.parameterUpdates);
    TEST_ASSERT_EQUAL_UINT8(0, status.switchesInWindow);
}

void test_song_aware_assist_mode_changes_controls_without_switching() {
    SongAwareDirector director;
    restoreReadyDirector(director, makeConfig(SongAwareMode::Assist, false), SongAwareState::Drop);

    ControlBusFrame frame = frameForState(SongAwareState::Drop);
    MusicalGridSnapshot grid = readyBoundaryGrid();
    SongAwareParams params = baseParams(0x1302);

    const bool changed = director.apply(frame, grid, true, 0.050f, 10000, params);
    const auto status = director.getStatus();

    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_TRUE(params.speed > 27);
    TEST_ASSERT_TRUE(params.intensity > 128);
    TEST_ASSERT_TRUE(params.complexity > 128);
    TEST_ASSERT_EQUAL_UINT8(160, params.brightness);
    TEST_ASSERT_TRUE(params.saturation >= 128);
    TEST_ASSERT_TRUE(params.variation > 0);
    TEST_ASSERT_TRUE(params.hue > 0);
    TEST_ASSERT_EQUAL_UINT16(0x1302, params.effectId);
    TEST_ASSERT_EQUAL(SongAwareState::Drop, status.currentSongState);
    TEST_ASSERT_EQUAL(SongAwareLastAction::ParameterUpdate, status.lastAction);
    TEST_ASSERT_EQUAL(SongAwareIntent::DropImpact, status.intent);
    TEST_ASSERT_EQUAL_UINT32(1, status.parameterUpdates);
    TEST_ASSERT_EQUAL_UINT32(0, status.automaticEffectSwitches);
}

void test_song_aware_director_apply_is_parameter_only_and_never_switches_effect() {
    SongAwareDirector director;
    restoreReadyDirector(director, makeConfig(), SongAwareState::Dense);

    ControlBusFrame frame = frameForState(SongAwareState::Dense);
    MusicalGridSnapshot grid = readyBoundaryGrid();
    SongAwareParams params = baseParams(0x1302);

    const bool changed = director.apply(frame, grid, true, 1.0f / 120.0f, 10000, params);
    const auto status = director.getStatus();

    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_EQUAL_UINT16(0x1302, params.effectId);
    TEST_ASSERT_EQUAL_UINT32(1, status.parameterUpdates);
    TEST_ASSERT_EQUAL_UINT32(0, status.automaticEffectSwitches);
    TEST_ASSERT_EQUAL(SongAwareLastAction::ParameterUpdate, status.lastAction);
}

void test_song_aware_low_confidence_suppresses_activity() {
    SongAwareDirector director;
    restoreReadyDirector(director, makeConfig(SongAwareMode::Assist, false), SongAwareState::Drop);

    ControlBusFrame frame = frameForState(SongAwareState::Drop);
    frame.audioConfidence = 0.30f;
    SongAwareConfig cfg = makeConfig(SongAwareMode::Assist, false);
    cfg.confidenceFloor = 0.70f;
    director.setConfig(cfg);

    SongAwareRuntimeState runtime = director.exportRuntimeState();
    runtime.bootGraceUntilMs = 0;
    runtime.enableGraceUntilMs = 0;
    director.restoreRuntimeState(runtime);

    MusicalGridSnapshot grid;
    SongAwareParams params = baseParams();

    const bool changed = director.apply(frame, grid, true, 1.0f / 120.0f, 10000, params);
    const auto status = director.getStatus();

    TEST_ASSERT_FALSE(changed);
    TEST_ASSERT_EQUAL_UINT8(27, params.speed);
    TEST_ASSERT_EQUAL_UINT8(128, params.intensity);
    TEST_ASSERT_EQUAL_UINT8(128, params.complexity);
    TEST_ASSERT_EQUAL(SongAwareSuppressedReason::LowConfidence, status.suppressedReason);
    TEST_ASSERT_EQUAL_UINT32(0, status.parameterUpdates);
    TEST_ASSERT_EQUAL_UINT32(0, status.automaticEffectSwitches);
}

void test_song_aware_policy_and_string_telemetry_helpers() {
    TEST_ASSERT_TRUE(SongAwareDirector::policyCount() > 1);

    SongAwarePolicySnapshot first;
    TEST_ASSERT_TRUE(SongAwareDirector::policySnapshot(0, first));
    TEST_ASSERT_EQUAL(SongAwareState::Unknown, first.state);
    TEST_ASSERT_FALSE(SongAwareDirector::policySnapshot(SongAwareDirector::policyCount(), first));

    SongAwarePolicySnapshot policies[kSongAwareMaxPolicySnapshotCount];
    const uint8_t copied = SongAwareDirector::copyPolicyTable(policies, kSongAwareMaxPolicySnapshotCount);
    SongAwareDirector director;
    const auto allowlist = director.getAllowlistSnapshot();
    TEST_ASSERT_EQUAL_UINT8(SongAwareDirector::policyCount(), copied);
    TEST_ASSERT_EQUAL_UINT8(copied, allowlist.count);

    const auto selection = director.resolveSelection(SongAwareState::Drop, 0.90f, INVALID_EFFECT_ID);
    TEST_ASSERT_TRUE(selection.valid);
    TEST_ASSERT_EQUAL(SongAwareState::Drop, selection.policy.state);
    TEST_ASSERT_TRUE(selection.score > 0.0f);

    const auto debug = director.getDebugSnapshot();
    TEST_ASSERT_TRUE(debug.bootGraceMs > 0);
    TEST_ASSERT_TRUE(debug.minimumDwellMs > 0);
    TEST_ASSERT_TRUE(debug.switchCooldownMs > 0);
    TEST_ASSERT_TRUE(debug.antiThrashWindowMs > 0);
    TEST_ASSERT_TRUE(debug.healthCleanWindowMs > 0);

    TEST_ASSERT_EQUAL_STRING("manual", songAwareOwnerName(SongAwareOwner::Manual));
    TEST_ASSERT_EQUAL_STRING("cooldown", songAwareSuppressedReasonName(SongAwareSuppressedReason::Cooldown));
    TEST_ASSERT_EQUAL_STRING("dense", songAwareStateName(SongAwareState::Dense));
    TEST_ASSERT_EQUAL_STRING("effect_switch", songAwareLastActionName(SongAwareLastAction::EffectSwitch));
    TEST_ASSERT_EQUAL_STRING("drop_onset", songAwareClassificationReasonName(SongAwareClassificationReason::DropOnset));
}

void test_song_aware_transition_telemetry_blocks_switch_until_complete() {
    SongAwareDirector director;
    restoreReadyDirector(director, makeConfig(), SongAwareState::Drop);

    const uint16_t dropEffect = policyEffectForState(SongAwareState::Drop);
    director.notifyTransitionStarted(0x2222, dropEffect, 10000, 1000);

    SongAwareSwitchRequest request =
        evaluateForState(director, SongAwareState::Drop, 10500, INVALID_EFFECT_ID);
    auto status = director.getStatus();
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL(SongAwareSuppressedReason::TransitionActive, status.suppressedReason);
    TEST_ASSERT_TRUE(status.transitionActive);
    TEST_ASSERT_EQUAL_UINT32(500, status.transitionRemainingMs);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.5f, status.transitionProgress);

    request = evaluateForState(director, SongAwareState::Drop, 11000, INVALID_EFFECT_ID);
    status = director.getStatus();
    (void)request;
    TEST_ASSERT_FALSE(status.transitionActive);
    TEST_ASSERT_EQUAL_UINT32(0, status.transitionRemainingMs);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, status.transitionProgress);
}

void test_song_aware_mode_taxonomy_collapsed() {
    SongAwareDirector director;

    restoreReadyDirector(director, makeConfig(SongAwareMode::Off, false), SongAwareState::Drop);
    ControlBusFrame frame = frameForState(SongAwareState::Drop);
    MusicalGridSnapshot grid = readyBoundaryGrid();
    SongAwareParams params = baseParams();
    TEST_ASSERT_FALSE(director.apply(frame, grid, true, 0.050f, 10000, params));
    TEST_ASSERT_EQUAL(SongAwareSuppressedReason::Disabled, director.getStatus().suppressedReason);

    restoreReadyDirector(director, makeConfig(SongAwareMode::Assist, true), SongAwareState::Drop);
    params = baseParams();
    TEST_ASSERT_TRUE(director.apply(frame, grid, true, 0.050f, 11000, params));
    SongAwareSwitchRequest request;
    director.evaluateDirector(frame, grid, true, 11000, INVALID_EFFECT_ID, SongAwareDirectorContext{}, request);
    auto status = director.getStatus();
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL(SongAwareMode::Assist, status.effectiveMode);
    TEST_ASSERT_EQUAL_UINT32(0, status.automaticEffectSwitches);

    restoreReadyDirector(director, makeConfig(SongAwareMode::Director, true), SongAwareState::Drop);
    director.evaluateDirector(frame, grid, true, 12000, INVALID_EFFECT_ID, SongAwareDirectorContext{}, request);
    status = director.getStatus();
    TEST_ASSERT_TRUE(request.requested);
    TEST_ASSERT_EQUAL(SongAwareMode::Director, status.effectiveMode);
    TEST_ASSERT_EQUAL(SongAwareActionPlan::EffectSwitch, status.actionPlan);
}

void test_song_aware_profile_scalars_are_real() {
    ControlBusFrame frame = frameForState(SongAwareState::Drop);
    MusicalGridSnapshot grid = readyBoundaryGrid();

    SongAwareDirector subtle;
    restoreReadyDirector(subtle,
                         makeConfig(SongAwareMode::Assist, false, SongAwareProfile::Subtle),
                         SongAwareState::Drop);
    SongAwareParams subtleParams = baseParams();
    TEST_ASSERT_TRUE(subtle.apply(frame, grid, true, 0.050f, 10000, subtleParams));

    SongAwareDirector balanced;
    restoreReadyDirector(balanced,
                         makeConfig(SongAwareMode::Assist, false, SongAwareProfile::Balanced),
                         SongAwareState::Drop);
    SongAwareParams balancedParams = baseParams();
    TEST_ASSERT_TRUE(balanced.apply(frame, grid, true, 0.050f, 10000, balancedParams));

    SongAwareDirector high;
    restoreReadyDirector(high,
                         makeConfig(SongAwareMode::Assist, false, SongAwareProfile::High),
                         SongAwareState::Drop);
    SongAwareParams highParams = baseParams();
    TEST_ASSERT_TRUE(high.apply(frame, grid, true, 0.050f, 10000, highParams));

    TEST_ASSERT_TRUE(subtleParams.intensity < balancedParams.intensity);
    TEST_ASSERT_TRUE(balancedParams.intensity < highParams.intensity);
    TEST_ASSERT_TRUE(subtleParams.speed < balancedParams.speed);
    TEST_ASSERT_TRUE(balancedParams.speed < highParams.speed);
    TEST_ASSERT_EQUAL(SongAwareProfile::Subtle, subtle.getStatus().profile);
    TEST_ASSERT_EQUAL(SongAwareProfile::High, high.getStatus().profile);
}

void test_song_aware_grid_boundary_gates_switching() {
    SongAwareDirector director;
    restoreReadyDirector(director, makeConfig(), SongAwareState::Drop);
    ControlBusFrame frame = frameForState(SongAwareState::Drop);
    SongAwareSwitchRequest request;
    director.evaluateDirector(frame,
                              waitingBoundaryGrid(),
                              true,
                              10000,
                              INVALID_EFFECT_ID,
                              SongAwareDirectorContext{},
                              request);
    auto status = director.getStatus();
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL(SongAwareSuppressedReason::BoundaryDeferred, status.suppressedReason);
    TEST_ASSERT_EQUAL(SongAwareBoundaryGate::WaitingForBoundary, status.boundaryGate);
    TEST_ASSERT_TRUE(status.waitingForBoundary);

    restoreReadyDirector(director, makeConfig(), SongAwareState::Drop);
    director.evaluateDirector(frame,
                              readyBoundaryGrid(),
                              true,
                              10000,
                              INVALID_EFFECT_ID,
                              SongAwareDirectorContext{},
                              request);
    status = director.getStatus();
    TEST_ASSERT_TRUE(request.requested);
    TEST_ASSERT_EQUAL(SongAwareSuppressedReason::None, status.suppressedReason);
    TEST_ASSERT_TRUE(status.boundaryReady);
    TEST_ASSERT_EQUAL(SongAwareBoundaryGate::DownbeatBoundary, status.boundaryGate);
}

void test_song_aware_assist_handles_build_drop_without_effect_switch() {
    SongAwareDirector director;
    restoreReadyDirector(director, makeConfig(SongAwareMode::Assist, true, SongAwareProfile::High),
                         SongAwareState::Build);
    ControlBusFrame frame = frameForState(SongAwareState::Build);
    MusicalGridSnapshot grid = readyBoundaryGrid();
    SongAwareParams params = baseParams();
    TEST_ASSERT_TRUE(director.apply(frame, grid, true, 0.050f, 10000, params));
    SongAwareSwitchRequest request;
    director.evaluateDirector(frame, grid, true, 10000, INVALID_EFFECT_ID, SongAwareDirectorContext{}, request);
    auto status = director.getStatus();
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_TRUE(params.intensity > 128);
    TEST_ASSERT_EQUAL(SongAwareIntent::BuildPressure, status.intent);
    TEST_ASSERT_EQUAL_UINT32(0, status.automaticEffectSwitches);
}

void test_song_aware_health_gate_uses_current_health_not_stale_counters() {
    SongAwareDirector director;
    restoreReadyDirector(director, makeConfig(), SongAwareState::Drop);
    ControlBusFrame frame = frameForState(SongAwareState::Drop);
    MusicalGridSnapshot grid = readyBoundaryGrid();
    SongAwareSwitchRequest request;
    SongAwareDirectorContext context;
    context.health.showSkips = 1;
    director.evaluateDirector(frame, grid, true, 10000, INVALID_EFFECT_ID, context, request);
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL(SongAwareSuppressedReason::Health, director.getStatus().suppressedReason);

    context.health.showSkips = 0;
    director.evaluateDirector(frame, grid, true, 11000, INVALID_EFFECT_ID, context, request);
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL(SongAwareSuppressedReason::HealthRecovering, director.getStatus().suppressedReason);

    director.evaluateDirector(frame, grid, true, 14050, INVALID_EFFECT_ID, context, request);
    TEST_ASSERT_TRUE(request.requested);
    TEST_ASSERT_EQUAL(SongAwareSuppressedReason::None, director.getStatus().suppressedReason);
}

void test_song_aware_mutable_allowlist_disables_state_policy() {
    SongAwareDirector director;
    restoreReadyDirector(director, makeConfig(), SongAwareState::Drop);
    TEST_ASSERT_TRUE(director.setPolicyAllowed(SongAwareState::Drop, false));
    TEST_ASSERT_FALSE(director.isPolicyAllowed(SongAwareState::Drop));

    SongAwareSwitchRequest request;
    director.evaluateDirector(frameForState(SongAwareState::Drop),
                              readyBoundaryGrid(),
                              true,
                              10000,
                              INVALID_EFFECT_ID,
                              SongAwareDirectorContext{},
                              request);
    const auto status = director.getStatus();
    const auto allowlist = director.getAllowlistSnapshot();
    bool sawDisabledDrop = false;
    for (uint8_t i = 0; i < allowlist.count; ++i) {
        if (allowlist.policies[i].state == SongAwareState::Drop) {
            sawDisabledDrop = !allowlist.policies[i].enabled;
        }
    }
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_TRUE(sawDisabledDrop);
    TEST_ASSERT_EQUAL(SongAwareSuppressedReason::AllowlistDisabled, status.suppressedReason);

    director.resetPolicyAllowlist();
    TEST_ASSERT_TRUE(director.isPolicyAllowed(SongAwareState::Drop));
}

void setUp(void) {}
void tearDown(void) {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_song_aware_defaults_reset_and_off_disable_activity);
    RUN_TEST(test_song_aware_mode_parsing_and_names_cover_public_modes);
    RUN_TEST(test_song_aware_classifier_reports_each_director_state);
    RUN_TEST(test_song_aware_boot_and_enable_grace_suppress_switching);
    RUN_TEST(test_song_aware_switching_disabled_for_assist_or_disabled_switching_modes);
    RUN_TEST(test_song_aware_successful_switch_request_and_applied_counters);
    RUN_TEST(test_song_aware_same_effect_suppresses_without_counting_switch);
    RUN_TEST(test_song_aware_dwell_gate_blocks_newly_established_state);
    RUN_TEST(test_song_aware_cooldown_blocks_after_recent_switch);
    RUN_TEST(test_song_aware_rate_limit_blocks_third_switch_inside_window);
    RUN_TEST(test_song_aware_anti_thrash_blocks_immediate_aba_switch);
    RUN_TEST(test_song_aware_manual_and_show_owners_suppress_director_switches);
    RUN_TEST(test_song_aware_health_gate_and_recovery_window_suppress_switches);
    RUN_TEST(test_song_aware_restore_runtime_state_and_reset_counters);
    RUN_TEST(test_song_aware_assist_mode_changes_controls_without_switching);
    RUN_TEST(test_song_aware_director_apply_is_parameter_only_and_never_switches_effect);
    RUN_TEST(test_song_aware_low_confidence_suppresses_activity);
    RUN_TEST(test_song_aware_policy_and_string_telemetry_helpers);
    RUN_TEST(test_song_aware_transition_telemetry_blocks_switch_until_complete);
    RUN_TEST(test_song_aware_mode_taxonomy_collapsed);
    RUN_TEST(test_song_aware_profile_scalars_are_real);
    RUN_TEST(test_song_aware_grid_boundary_gates_switching);
    RUN_TEST(test_song_aware_assist_handles_build_drop_without_effect_switch);
    RUN_TEST(test_song_aware_health_gate_uses_current_health_not_stale_counters);
    RUN_TEST(test_song_aware_mutable_allowlist_disables_state_policy);
    return UNITY_END();
}

#endif
