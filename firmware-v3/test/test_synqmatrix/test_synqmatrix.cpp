#ifdef NATIVE_BUILD

#include <cstring>

#include <unity.h>

#include "../../src/core/synqmatrix/SynqMatrix.h"

using lightwaveos::audio::ControlBusFrame;
using lightwaveos::audio::MusicalGridSnapshot;
using lightwaveos::INVALID_EFFECT_ID;
using lightwaveos::synqmatrix::SynqMatrixClassificationReason;
using lightwaveos::synqmatrix::SynqMatrixConfig;
using lightwaveos::synqmatrix::SynqMatrix;
using lightwaveos::synqmatrix::SynqMatrixContext;
using lightwaveos::synqmatrix::SynqMatrixLastAction;
using lightwaveos::synqmatrix::SynqMatrixMode;
using lightwaveos::synqmatrix::SynqMatrixProfile;
using lightwaveos::synqmatrix::SynqMatrixOwner;
using lightwaveos::synqmatrix::SynqMatrixParams;
using lightwaveos::synqmatrix::SynqMatrixPolicySnapshot;
using lightwaveos::synqmatrix::SynqMatrixRuntimeState;
using lightwaveos::synqmatrix::SynqMatrixState;
using lightwaveos::synqmatrix::SynqMatrixSuppressedReason;
using lightwaveos::synqmatrix::SynqMatrixSwitchRequest;
using lightwaveos::synqmatrix::SynqMatrixActionPlan;
using lightwaveos::synqmatrix::SynqMatrixBoundaryGate;
using lightwaveos::synqmatrix::SynqMatrixIntent;
using lightwaveos::synqmatrix::kSynqMatrixMaxPolicySnapshotCount;
using lightwaveos::synqmatrix::parseSynqMatrixMode;
using lightwaveos::synqmatrix::parseSynqMatrixProfile;
using lightwaveos::synqmatrix::synqMatrixClassificationReasonName;
using lightwaveos::synqmatrix::synqMatrixLastActionName;
using lightwaveos::synqmatrix::synqMatrixModeName;
using lightwaveos::synqmatrix::synqMatrixProfileName;
using lightwaveos::synqmatrix::synqMatrixOwnerName;
using lightwaveos::synqmatrix::synqMatrixStateName;
using lightwaveos::synqmatrix::synqMatrixSuppressedReasonName;

namespace {

SynqMatrixConfig makeConfig(SynqMatrixMode mode = SynqMatrixMode::Director,
                           bool switchingEnabled = true,
                           SynqMatrixProfile profile = SynqMatrixProfile::Balanced) {
    SynqMatrixConfig cfg;
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

void restoreReadyDirector(SynqMatrix& director,
                          const SynqMatrixConfig& cfg = makeConfig(),
                          SynqMatrixState stableState = SynqMatrixState::Unknown) {
    director.reset();

    SynqMatrixRuntimeState runtime = director.exportRuntimeState();
    runtime.config = cfg;
    runtime.status.effectiveMode = cfg.enabled ? cfg.mode : SynqMatrixMode::Off;
    runtime.status.profile = cfg.profile;
    runtime.status.suppressedReason = cfg.enabled ? SynqMatrixSuppressedReason::None
                                                  : SynqMatrixSuppressedReason::Disabled;
    runtime.status.currentState = stableState;
    runtime.status.rawState = stableState;
    runtime.status.candidateState = stableState;
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

ControlBusFrame frameForState(SynqMatrixState state) {
    ControlBusFrame frame = baseFrame();
    switch (state) {
        case SynqMatrixState::Silence:
            frame.isSilent = true;
            frame.silentScale = 0.0f;
            frame.rms = 0.0f;
            frame.fast_rms = 0.0f;
            frame.flux = 0.0f;
            frame.fast_flux = 0.0f;
            frame.liveliness = 0.0f;
            break;
        case SynqMatrixState::Ambient:
            frame.rms = 0.18f;
            frame.fast_rms = 0.18f;
            frame.flux = 0.10f;
            frame.fast_flux = 0.10f;
            frame.liveliness = 0.30f;
            frame.tempoConfidence = 0.0f;
            frame.es_tempo_confidence = 0.0f;
            break;
        case SynqMatrixState::Steady:
            break;
        case SynqMatrixState::Build:
            frame.rms = 0.40f;
            frame.fast_rms = 0.40f;
            frame.flux = 0.46f;
            frame.fast_flux = 0.46f;
            frame.liveliness = 0.50f;
            frame.saliency.overallSaliency = 0.52f;
            break;
        case SynqMatrixState::Drop:
            frame.rms = 0.54f;
            frame.fast_rms = 0.54f;
            frame.liveliness = 0.66f;
            frame.saliency.overallSaliency = 0.68f;
            frame.onsetEvent = 0.75f;
            break;
        case SynqMatrixState::Breakdown:
            frame.rms = 0.18f;
            frame.fast_rms = 0.18f;
            frame.flux = 0.08f;
            frame.fast_flux = 0.08f;
            frame.liveliness = 0.20f;
            frame.tempoConfidence = 0.50f;
            frame.es_tempo_confidence = 0.50f;
            break;
        case SynqMatrixState::Dense:
            frame.rms = 0.72f;
            frame.fast_rms = 0.72f;
            frame.flux = 0.34f;
            frame.fast_flux = 0.34f;
            frame.liveliness = 0.76f;
            break;
        case SynqMatrixState::Transition:
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

uint16_t policyEffectForState(SynqMatrixState state) {
    SynqMatrix director;
    const auto selection = director.resolveSelection(state, 1.0f, INVALID_EFFECT_ID);
    return selection.policy.effectId;
}

SynqMatrixSwitchRequest evaluateForState(SynqMatrix& director,
                                        SynqMatrixState state,
                                        uint32_t nowMs,
                                        uint16_t activeEffectId,
                                        SynqMatrixContext context = SynqMatrixContext{}) {
    SynqMatrixSwitchRequest request;
    MusicalGridSnapshot grid = readyBoundaryGrid();
    const ControlBusFrame frame = frameForState(state);
    director.tick(frame, grid, true, nowMs, activeEffectId, context, request);
    return request;
}

SynqMatrixParams baseParams(uint16_t effectId = 0x1302) {
    SynqMatrixParams params;
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

void assertClassification(SynqMatrixState expectedState,
                          SynqMatrixClassificationReason expectedReason,
                          uint32_t nowMs) {
    SynqMatrix director;
    restoreReadyDirector(director);

    SynqMatrixSwitchRequest request;
    MusicalGridSnapshot grid = readyBoundaryGrid();
    if (expectedState == SynqMatrixState::Ambient) {
        grid.tempo_confidence = 0.0f;
        grid.beat_strength = 0.0f;
    }
    const ControlBusFrame frame = frameForState(expectedState);
    director.tick(frame, grid, true, nowMs, INVALID_EFFECT_ID,
                              SynqMatrixContext{}, request);
    const auto status = director.getStatus();

    (void)request;
    TEST_ASSERT_EQUAL(expectedState, status.rawState);
    TEST_ASSERT_EQUAL(expectedState, status.currentState);
    TEST_ASSERT_EQUAL(expectedReason, status.classificationReason);
}

} // namespace

void test_synq_matrix_defaults_reset_and_off_disable_activity() {
    SynqMatrix director;
    const auto defaults = director.getStatus();

    TEST_ASSERT_FALSE(defaults.enabled);
    TEST_ASSERT_EQUAL(SynqMatrixMode::Off, defaults.effectiveMode);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::Disabled, defaults.suppressedReason);
    TEST_ASSERT_EQUAL_UINT32(0, defaults.parameterUpdates);
    TEST_ASSERT_EQUAL_UINT32(0, defaults.automaticEffectSwitches);

    restoreReadyDirector(director, makeConfig(SynqMatrixMode::Off, false));
    ControlBusFrame frame = frameForState(SynqMatrixState::Drop);
    MusicalGridSnapshot grid;
    SynqMatrixParams params = baseParams();

    TEST_ASSERT_FALSE(director.apply(frame, grid, true, 1.0f / 120.0f, 10000, params));
    const auto offStatus = director.getStatus();
    TEST_ASSERT_EQUAL(SynqMatrixMode::Off, offStatus.effectiveMode);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::Disabled, offStatus.suppressedReason);
    TEST_ASSERT_EQUAL(SynqMatrixLastAction::None, offStatus.lastAction);

    director.reset();
    const auto resetStatus = director.getStatus();
    TEST_ASSERT_FALSE(resetStatus.enabled);
    TEST_ASSERT_EQUAL(SynqMatrixMode::Off, resetStatus.effectiveMode);
}

void test_synq_matrix_mode_parsing_and_names_cover_public_modes() {
    bool ok = false;
    TEST_ASSERT_EQUAL(SynqMatrixMode::Off, parseSynqMatrixMode("off", &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(SynqMatrixMode::Assist, parseSynqMatrixMode("assist", &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(SynqMatrixMode::Assist, parseSynqMatrixMode("parameter", &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(SynqMatrixMode::Assist, parseSynqMatrixMode("on", &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(SynqMatrixMode::Assist, parseSynqMatrixMode("subtle", &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(SynqMatrixMode::Assist, parseSynqMatrixMode("balanced", &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(SynqMatrixMode::Assist, parseSynqMatrixMode("high", &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(SynqMatrixMode::Director, parseSynqMatrixMode("director", &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(SynqMatrixMode::Off, parseSynqMatrixMode("not-a-mode", &ok));
    TEST_ASSERT_FALSE(ok);

    TEST_ASSERT_EQUAL_STRING("off", synqMatrixModeName(SynqMatrixMode::Off));
    TEST_ASSERT_EQUAL_STRING("assist", synqMatrixModeName(SynqMatrixMode::Assist));
    TEST_ASSERT_EQUAL_STRING("director", synqMatrixModeName(SynqMatrixMode::Director));

    TEST_ASSERT_EQUAL(SynqMatrixProfile::Subtle, parseSynqMatrixProfile("subtle", &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(SynqMatrixProfile::Balanced, parseSynqMatrixProfile("balanced", &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(SynqMatrixProfile::High, parseSynqMatrixProfile("high", &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(SynqMatrixProfile::Balanced, parseSynqMatrixProfile("not-a-profile", &ok));
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_STRING("subtle", synqMatrixProfileName(SynqMatrixProfile::Subtle));
    TEST_ASSERT_EQUAL_STRING("balanced", synqMatrixProfileName(SynqMatrixProfile::Balanced));
    TEST_ASSERT_EQUAL_STRING("high", synqMatrixProfileName(SynqMatrixProfile::High));
}

void test_synq_matrix_classifier_reports_each_director_state() {
    assertClassification(SynqMatrixState::Silence,
                         SynqMatrixClassificationReason::SilentFrame,
                         10000);
    assertClassification(SynqMatrixState::Ambient,
                         SynqMatrixClassificationReason::AmbientLowEnergy,
                         11000);
    assertClassification(SynqMatrixState::Steady,
                         SynqMatrixClassificationReason::SteadyDefault,
                         12000);
    assertClassification(SynqMatrixState::Build,
                         SynqMatrixClassificationReason::BuildEnergy,
                         13000);
    assertClassification(SynqMatrixState::Drop,
                         SynqMatrixClassificationReason::DropOnset,
                         14000);
    assertClassification(SynqMatrixState::Breakdown,
                         SynqMatrixClassificationReason::QuietBreakdown,
                         15000);
    assertClassification(SynqMatrixState::Dense,
                         SynqMatrixClassificationReason::DenseEnergy,
                         16000);
    assertClassification(SynqMatrixState::Transition,
                         SynqMatrixClassificationReason::SpectralTransition,
                         17000);
}

void test_synq_matrix_boot_and_enable_grace_suppress_switching() {
    SynqMatrix director;
    director.reset();
    director.setConfig(makeConfig());

    const SynqMatrixSwitchRequest bootRequest =
        evaluateForState(director, SynqMatrixState::Drop, 1000, INVALID_EFFECT_ID);
    auto status = director.getStatus();
    TEST_ASSERT_FALSE(bootRequest.requested);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::BootGrace, status.suppressedReason);
    TEST_ASSERT_TRUE(status.bootGraceRemainingMs > 0);

    SynqMatrixRuntimeState runtime = director.exportRuntimeState();
    runtime.config = SynqMatrixConfig{};
    runtime.bootGraceUntilMs = 0;
    runtime.enableGraceUntilMs = 0;
    runtime.status.currentState = SynqMatrixState::Drop;
    runtime.status.rawState = SynqMatrixState::Drop;
    runtime.status.candidateState = SynqMatrixState::Drop;
    director.restoreRuntimeState(runtime);
    director.setConfig(makeConfig());

    const SynqMatrixSwitchRequest enableRequest =
        evaluateForState(director, SynqMatrixState::Drop, 5000, INVALID_EFFECT_ID);
    status = director.getStatus();
    TEST_ASSERT_FALSE(enableRequest.requested);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::EnableGrace, status.suppressedReason);
    TEST_ASSERT_TRUE(status.enableGraceRemainingMs > 0);
}

void test_synq_matrix_switching_disabled_for_assist_or_disabled_switching_modes() {
    SynqMatrix director;

    restoreReadyDirector(director, makeConfig(SynqMatrixMode::Assist, true), SynqMatrixState::Drop);
    SynqMatrixSwitchRequest request =
        evaluateForState(director, SynqMatrixState::Drop, 10000, INVALID_EFFECT_ID);
    auto status = director.getStatus();
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL(SynqMatrixMode::Assist, status.effectiveMode);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::SwitchingDisabled, status.suppressedReason);

    restoreReadyDirector(director, makeConfig(SynqMatrixMode::Director, false), SynqMatrixState::Drop);
    request = evaluateForState(director, SynqMatrixState::Drop, 11000, INVALID_EFFECT_ID);
    status = director.getStatus();
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::SwitchingDisabled, status.suppressedReason);
}

void test_synq_matrix_successful_switch_request_and_applied_counters() {
    SynqMatrix director;
    restoreReadyDirector(director, makeConfig(), SynqMatrixState::Drop);

    const uint16_t targetEffect = policyEffectForState(SynqMatrixState::Drop);
    const SynqMatrixSwitchRequest request =
        evaluateForState(director, SynqMatrixState::Drop, 10000, INVALID_EFFECT_ID);

    TEST_ASSERT_TRUE(request.requested);
    TEST_ASSERT_EQUAL_UINT16(targetEffect, request.targetEffectId);
    TEST_ASSERT_EQUAL_STRING("advanced_optical", request.targetFamily);
    TEST_ASSERT_EQUAL_STRING("photonic_drop_texture", request.targetVisualLanguage);
    TEST_ASSERT_EQUAL_STRING("drop_impact", request.reason);

    director.notifySwitchApplied(0x2222, targetEffect, 10000, "Drop target");
    const auto status = director.getStatus();
    TEST_ASSERT_EQUAL_UINT32(1, status.automaticEffectSwitches);
    TEST_ASSERT_EQUAL(SynqMatrixLastAction::EffectSwitch, status.lastAction);
    TEST_ASSERT_EQUAL(SynqMatrixOwner::Director, status.owner);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::None, status.suppressedReason);
    TEST_ASSERT_EQUAL_UINT16(0x2222, status.lastSwitchFromEffectId);
    TEST_ASSERT_EQUAL_UINT16(targetEffect, status.lastSwitchToEffectId);
    TEST_ASSERT_EQUAL_UINT16(targetEffect, status.activeEffectId);
    TEST_ASSERT_TRUE(status.cooldownRemainingMs > 0);
    TEST_ASSERT_TRUE(status.antiThrashRemainingMs > 0);
}

void test_synq_matrix_same_effect_suppresses_without_counting_switch() {
    SynqMatrix director;
    restoreReadyDirector(director, makeConfig(), SynqMatrixState::Drop);

    const uint16_t targetEffect = policyEffectForState(SynqMatrixState::Drop);
    const SynqMatrixSwitchRequest request =
        evaluateForState(director, SynqMatrixState::Drop, 10000, targetEffect);
    const auto status = director.getStatus();

    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::SameEffect, status.suppressedReason);
    TEST_ASSERT_EQUAL(SynqMatrixLastAction::None, status.lastAction);
    TEST_ASSERT_EQUAL_UINT32(0, status.automaticEffectSwitches);
}

void test_synq_matrix_dwell_gate_blocks_newly_established_state() {
    SynqMatrix director;
    restoreReadyDirector(director);

    const SynqMatrixSwitchRequest request =
        evaluateForState(director, SynqMatrixState::Drop, 10000, INVALID_EFFECT_ID);
    const auto status = director.getStatus();

    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::Dwell, status.suppressedReason);
    TEST_ASSERT_TRUE(status.dwellRemainingMs > 0);
}

void test_synq_matrix_cooldown_blocks_after_recent_switch() {
    SynqMatrix director;
    restoreReadyDirector(director, makeConfig(), SynqMatrixState::Drop);

    const uint16_t dropEffect = policyEffectForState(SynqMatrixState::Drop);
    const uint16_t buildEffect = policyEffectForState(SynqMatrixState::Build);
    director.notifySwitchApplied(0x2222, dropEffect, 10000, "Drop target");

    SynqMatrixRuntimeState runtime = director.exportRuntimeState();
    runtime.status.currentState = SynqMatrixState::Build;
    runtime.status.rawState = SynqMatrixState::Build;
    runtime.status.candidateState = SynqMatrixState::Build;
    director.restoreRuntimeState(runtime);

    const SynqMatrixSwitchRequest request =
        evaluateForState(director, SynqMatrixState::Build, 15000, dropEffect);
    const auto status = director.getStatus();

    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL_UINT16(buildEffect, status.selectedEffectId);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::Cooldown, status.suppressedReason);
    TEST_ASSERT_TRUE(status.cooldownRemainingMs > 0);
}

void test_synq_matrix_rate_limit_blocks_third_switch_inside_window() {
    SynqMatrix director;
    restoreReadyDirector(director, makeConfig(), SynqMatrixState::Dense);

    const uint16_t buildEffect = policyEffectForState(SynqMatrixState::Build);
    const uint16_t dropEffect = policyEffectForState(SynqMatrixState::Drop);
    const uint16_t denseEffect = policyEffectForState(SynqMatrixState::Dense);
    director.notifySwitchApplied(0x1111, buildEffect, 10000, "Build target");
    director.notifySwitchApplied(buildEffect, dropEffect, 40000, "Drop target");

    const SynqMatrixSwitchRequest request =
        evaluateForState(director, SynqMatrixState::Dense, 61000, dropEffect);
    const auto status = director.getStatus();

    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL_UINT16(denseEffect, status.selectedEffectId);
    TEST_ASSERT_EQUAL_UINT8(2, status.switchesInWindow);
    TEST_ASSERT_TRUE(status.switchWindowRemainingMs > 0);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::RateLimit, status.suppressedReason);
}

void test_synq_matrix_anti_thrash_blocks_immediate_aba_switch() {
    SynqMatrix director;
    restoreReadyDirector(director, makeConfig(), SynqMatrixState::Build);

    const uint16_t buildEffect = policyEffectForState(SynqMatrixState::Build);
    const uint16_t dropEffect = policyEffectForState(SynqMatrixState::Drop);
    director.notifySwitchApplied(buildEffect, dropEffect, 10000, "Drop target");

    const SynqMatrixSwitchRequest request =
        evaluateForState(director, SynqMatrixState::Build, 11000, dropEffect);
    const auto status = director.getStatus();

    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL_UINT16(buildEffect, status.selectedEffectId);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::AntiThrash, status.suppressedReason);
    TEST_ASSERT_TRUE(status.antiThrashRemainingMs > 0);
}

void test_synq_matrix_manual_and_show_owners_suppress_director_switches() {
    SynqMatrix director;
    restoreReadyDirector(director, makeConfig(), SynqMatrixState::Drop);
    director.markShowControl(10000);

    SynqMatrixSwitchRequest request =
        evaluateForState(director, SynqMatrixState::Drop, 10500, INVALID_EFFECT_ID);
    auto status = director.getStatus();
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_TRUE(director.isShowOwnerActive(10500));
    TEST_ASSERT_EQUAL(SynqMatrixOwner::Show, status.owner);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::ShowOwner, status.suppressedReason);

    restoreReadyDirector(director, makeConfig(), SynqMatrixState::Drop);
    director.markManualControl(20000);
    request = evaluateForState(director, SynqMatrixState::Drop, 20500, INVALID_EFFECT_ID);
    status = director.getStatus();
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL(SynqMatrixOwner::Manual, status.owner);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::ManualOwner, status.suppressedReason);
}

void test_synq_matrix_health_gate_and_recovery_window_suppress_switches() {
    SynqMatrix director;
    restoreReadyDirector(director, makeConfig(), SynqMatrixState::Drop);

    SynqMatrixContext context;
    context.health.showSkips = 1;
    SynqMatrixSwitchRequest request =
        evaluateForState(director, SynqMatrixState::Drop, 10000, INVALID_EFFECT_ID, context);
    auto status = director.getStatus();
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::Health, status.suppressedReason);
    TEST_ASSERT_TRUE(status.healthDegraded);
    TEST_ASSERT_EQUAL_UINT32(1, status.showSkips);

    context.health.showSkips = 0;
    request = evaluateForState(director, SynqMatrixState::Drop, 11000, INVALID_EFFECT_ID, context);
    status = director.getStatus();
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::HealthRecovering, status.suppressedReason);
    TEST_ASSERT_FALSE(status.healthDegraded);
    TEST_ASSERT_TRUE(status.healthCleanWindowRemainingMs > 0);
}

void test_synq_matrix_restore_runtime_state_and_reset_counters() {
    SynqMatrix director;
    restoreReadyDirector(director, makeConfig(), SynqMatrixState::Drop);
    const uint16_t targetEffect = policyEffectForState(SynqMatrixState::Drop);
    director.notifySwitchApplied(0x2222, targetEffect, 10000, "Drop target");

    ControlBusFrame frame = frameForState(SynqMatrixState::Drop);
    MusicalGridSnapshot grid;
    SynqMatrixParams params = baseParams(targetEffect);
    TEST_ASSERT_TRUE(director.apply(frame, grid, true, 1.0f / 120.0f, 12000, params));

    const SynqMatrixRuntimeState saved = director.exportRuntimeState();
    SynqMatrix restored;
    restored.restoreRuntimeState(saved);
    auto status = restored.getStatus();

    TEST_ASSERT_TRUE(restored.getConfig().enabled);
    TEST_ASSERT_EQUAL(SynqMatrixMode::Director, restored.getConfig().mode);
    TEST_ASSERT_EQUAL_UINT32(1, status.automaticEffectSwitches);
    TEST_ASSERT_EQUAL_UINT32(1, status.parameterUpdates);
    TEST_ASSERT_EQUAL_UINT16(targetEffect, status.activeEffectId);

    restored.resetCounters();
    status = restored.getStatus();
    TEST_ASSERT_EQUAL_UINT32(0, status.automaticEffectSwitches);
    TEST_ASSERT_EQUAL_UINT32(0, status.parameterUpdates);
    TEST_ASSERT_EQUAL_UINT8(0, status.switchesInWindow);
}

void test_synq_matrix_assist_mode_changes_controls_without_switching() {
    SynqMatrix director;
    restoreReadyDirector(director, makeConfig(SynqMatrixMode::Assist, false), SynqMatrixState::Drop);

    ControlBusFrame frame = frameForState(SynqMatrixState::Drop);
    MusicalGridSnapshot grid = readyBoundaryGrid();
    SynqMatrixParams params = baseParams(0x1302);

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
    TEST_ASSERT_EQUAL(SynqMatrixState::Drop, status.currentState);
    TEST_ASSERT_EQUAL(SynqMatrixLastAction::ParameterUpdate, status.lastAction);
    TEST_ASSERT_EQUAL(SynqMatrixIntent::DropImpact, status.intent);
    TEST_ASSERT_EQUAL_UINT32(1, status.parameterUpdates);
    TEST_ASSERT_EQUAL_UINT32(0, status.automaticEffectSwitches);
}

void test_synq_matrix_director_apply_is_parameter_only_and_never_switches_effect() {
    SynqMatrix director;
    restoreReadyDirector(director, makeConfig(), SynqMatrixState::Dense);

    ControlBusFrame frame = frameForState(SynqMatrixState::Dense);
    MusicalGridSnapshot grid = readyBoundaryGrid();
    SynqMatrixParams params = baseParams(0x1302);

    const bool changed = director.apply(frame, grid, true, 1.0f / 120.0f, 10000, params);
    const auto status = director.getStatus();

    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_EQUAL_UINT16(0x1302, params.effectId);
    TEST_ASSERT_EQUAL_UINT32(1, status.parameterUpdates);
    TEST_ASSERT_EQUAL_UINT32(0, status.automaticEffectSwitches);
    TEST_ASSERT_EQUAL(SynqMatrixLastAction::ParameterUpdate, status.lastAction);
}

void test_synq_matrix_low_confidence_suppresses_activity() {
    SynqMatrix director;
    restoreReadyDirector(director, makeConfig(SynqMatrixMode::Assist, false), SynqMatrixState::Drop);

    ControlBusFrame frame = frameForState(SynqMatrixState::Drop);
    frame.audioConfidence = 0.30f;
    SynqMatrixConfig cfg = makeConfig(SynqMatrixMode::Assist, false);
    cfg.confidenceFloor = 0.70f;
    director.setConfig(cfg);

    SynqMatrixRuntimeState runtime = director.exportRuntimeState();
    runtime.bootGraceUntilMs = 0;
    runtime.enableGraceUntilMs = 0;
    director.restoreRuntimeState(runtime);

    MusicalGridSnapshot grid;
    SynqMatrixParams params = baseParams();

    const bool changed = director.apply(frame, grid, true, 1.0f / 120.0f, 10000, params);
    const auto status = director.getStatus();

    TEST_ASSERT_FALSE(changed);
    TEST_ASSERT_EQUAL_UINT8(27, params.speed);
    TEST_ASSERT_EQUAL_UINT8(128, params.intensity);
    TEST_ASSERT_EQUAL_UINT8(128, params.complexity);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::LowConfidence, status.suppressedReason);
    TEST_ASSERT_EQUAL_UINT32(0, status.parameterUpdates);
    TEST_ASSERT_EQUAL_UINT32(0, status.automaticEffectSwitches);
}

void test_synq_matrix_confidence_floor_remains_single_config_control() {
    SynqMatrix director;
    SynqMatrixConfig cfg = makeConfig(SynqMatrixMode::Assist, false);
    cfg.confidenceFloor = 0.70f;
    restoreReadyDirector(director, cfg, SynqMatrixState::Drop);

    const auto readback = director.getConfig();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.70f, readback.confidenceFloor);

    ControlBusFrame frame = frameForState(SynqMatrixState::Drop);
    frame.audioConfidence = 0.30f;
    SynqMatrixParams params = baseParams();
    const SynqMatrixParams before = params;

    TEST_ASSERT_FALSE(director.apply(frame, readyBoundaryGrid(), true, 1.0f / 120.0f, 10000, params));
    const auto status = director.getStatus();

    TEST_ASSERT_EQUAL_UINT8(before.speed, params.speed);
    TEST_ASSERT_EQUAL_UINT8(before.intensity, params.intensity);
    TEST_ASSERT_EQUAL_UINT8(before.complexity, params.complexity);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::LowConfidence, status.suppressedReason);
    TEST_ASSERT_FALSE(status.coasting);
    TEST_ASSERT_EQUAL_UINT32(0, status.audioConfidenceBelowFloorMs);
}

void test_synq_matrix_enters_and_exits_coast_on_audio_confidence_duration() {
    SynqMatrix director;
    SynqMatrixConfig cfg = makeConfig(SynqMatrixMode::Assist, false);
    cfg.confidenceFloor = 0.70f;
    restoreReadyDirector(director, cfg, SynqMatrixState::Drop);

    ControlBusFrame low = frameForState(SynqMatrixState::Drop);
    low.audioConfidence = 0.30f;
    SynqMatrixParams params = baseParams();

    TEST_ASSERT_FALSE(director.apply(low, readyBoundaryGrid(), true, 1.0f / 120.0f, 10000, params));
    TEST_ASSERT_FALSE(director.getStatus().coasting);
    TEST_ASSERT_FALSE(director.apply(low, readyBoundaryGrid(), true, 1.0f / 120.0f, 10500, params));
    TEST_ASSERT_FALSE(director.getStatus().coasting);
    TEST_ASSERT_FALSE(director.apply(low, readyBoundaryGrid(), true, 1.0f / 120.0f, 11000, params));

    auto status = director.getStatus();
    TEST_ASSERT_TRUE(status.coasting);
    TEST_ASSERT_EQUAL_UINT32(1000, status.audioConfidenceBelowFloorMs);
    TEST_ASSERT_EQUAL_UINT32(0, status.missedPredictionCount);
    TEST_ASSERT_EQUAL_UINT32(0, status.tempoWinnerChanges);

    ControlBusFrame recovered = frameForState(SynqMatrixState::Drop);
    recovered.audioConfidence = 0.90f;
    TEST_ASSERT_FALSE(director.apply(recovered, readyBoundaryGrid(), true, 1.0f / 120.0f, 11200, params));
    TEST_ASSERT_TRUE(director.getStatus().coasting);

    TEST_ASSERT_TRUE(director.apply(recovered, readyBoundaryGrid(), true, 0.050f, 11700, params));
    status = director.getStatus();
    TEST_ASSERT_FALSE(status.coasting);
    TEST_ASSERT_EQUAL_UINT32(0, status.audioConfidenceBelowFloorMs);
    TEST_ASSERT_EQUAL(SynqMatrixLastAction::ParameterUpdate, status.lastAction);
}

void test_synq_matrix_coast_leaves_incoming_params_unchanged_and_suppresses_switches() {
    SynqMatrix director;
    SynqMatrixConfig cfg = makeConfig(SynqMatrixMode::Director, true);
    cfg.confidenceFloor = 0.70f;
    restoreReadyDirector(director, cfg, SynqMatrixState::Drop);

    ControlBusFrame low = frameForState(SynqMatrixState::Drop);
    low.audioConfidence = 0.30f;
    SynqMatrixParams params = baseParams();
    const SynqMatrixParams before = params;
    SynqMatrixSwitchRequest request;

    director.tick(low, readyBoundaryGrid(), true, 10000, INVALID_EFFECT_ID, SynqMatrixContext{}, request);
    director.tick(low, readyBoundaryGrid(), true, 10500, INVALID_EFFECT_ID, SynqMatrixContext{}, request);
    director.tick(low, readyBoundaryGrid(), true, 11000, INVALID_EFFECT_ID, SynqMatrixContext{}, request);
    TEST_ASSERT_TRUE(director.getStatus().coasting);

    TEST_ASSERT_FALSE(director.apply(low, readyBoundaryGrid(), true, 0.050f, 11100, params));
    TEST_ASSERT_EQUAL_UINT8(before.speed, params.speed);
    TEST_ASSERT_EQUAL_UINT8(before.intensity, params.intensity);
    TEST_ASSERT_EQUAL_UINT8(before.complexity, params.complexity);
    TEST_ASSERT_EQUAL_UINT8(before.saturation, params.saturation);
    TEST_ASSERT_EQUAL_UINT8(before.variation, params.variation);
    TEST_ASSERT_EQUAL_UINT8(before.hue, params.hue);

    request = SynqMatrixSwitchRequest{};
    TEST_ASSERT_FALSE(director.tick(low, readyBoundaryGrid(), true, 11500, INVALID_EFFECT_ID,
                                    SynqMatrixContext{}, request));
    TEST_ASSERT_FALSE(request.requested);
    const auto status = director.getStatus();
    TEST_ASSERT_TRUE(status.coasting);
    TEST_ASSERT_EQUAL_UINT32(0, status.parameterUpdates);
    TEST_ASSERT_EQUAL_UINT32(0, status.automaticEffectSwitches);
}

void test_synq_matrix_low_confidence_pre_coast_blocks_state_promotion() {
    SynqMatrix director;
    SynqMatrixConfig cfg = makeConfig(SynqMatrixMode::Director, true);
    cfg.confidenceFloor = 0.70f;
    restoreReadyDirector(director, cfg, SynqMatrixState::Ambient);

    ControlBusFrame lowDrop = frameForState(SynqMatrixState::Drop);
    lowDrop.audioConfidence = 0.30f;
    SynqMatrixSwitchRequest request;

    TEST_ASSERT_FALSE(director.tick(lowDrop,
                                    readyBoundaryGrid(),
                                    true,
                                    10000,
                                    INVALID_EFFECT_ID,
                                    SynqMatrixContext{},
                                    request));
    TEST_ASSERT_FALSE(director.tick(lowDrop,
                                    readyBoundaryGrid(),
                                    true,
                                    10999,
                                    INVALID_EFFECT_ID,
                                    SynqMatrixContext{},
                                    request));

    const auto status = director.getStatus();
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_FALSE(status.coasting);
    TEST_ASSERT_EQUAL(SynqMatrixState::Ambient, status.currentState);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::LowConfidence, status.suppressedReason);
    TEST_ASSERT_EQUAL_UINT32(0, status.automaticEffectSwitches);
}

void test_synq_matrix_policy_and_string_telemetry_helpers() {
    TEST_ASSERT_TRUE(SynqMatrix::policyCount() > 1);

    SynqMatrixPolicySnapshot first;
    TEST_ASSERT_TRUE(SynqMatrix::policySnapshot(0, first));
    TEST_ASSERT_EQUAL(SynqMatrixState::Unknown, first.state);
    TEST_ASSERT_FALSE(SynqMatrix::policySnapshot(SynqMatrix::policyCount(), first));

    SynqMatrixPolicySnapshot policies[kSynqMatrixMaxPolicySnapshotCount];
    const uint8_t copied = SynqMatrix::copyPolicyTable(policies, kSynqMatrixMaxPolicySnapshotCount);
    SynqMatrix director;
    const auto allowlist = director.getAllowlistSnapshot();
    TEST_ASSERT_EQUAL_UINT8(SynqMatrix::policyCount(), copied);
    TEST_ASSERT_EQUAL_UINT8(copied, allowlist.count);

    const auto selection = director.resolveSelection(SynqMatrixState::Drop, 0.90f, INVALID_EFFECT_ID);
    TEST_ASSERT_TRUE(selection.valid);
    TEST_ASSERT_EQUAL(SynqMatrixState::Drop, selection.policy.state);
    TEST_ASSERT_TRUE(selection.score > 0.0f);

    const auto debug = director.getDebugSnapshot();
    TEST_ASSERT_TRUE(debug.bootGraceMs > 0);
    TEST_ASSERT_TRUE(debug.minimumDwellMs > 0);
    TEST_ASSERT_TRUE(debug.switchCooldownMs > 0);
    TEST_ASSERT_TRUE(debug.antiThrashWindowMs > 0);
    TEST_ASSERT_TRUE(debug.healthCleanWindowMs > 0);

    TEST_ASSERT_EQUAL_STRING("manual", synqMatrixOwnerName(SynqMatrixOwner::Manual));
    TEST_ASSERT_EQUAL_STRING("cooldown", synqMatrixSuppressedReasonName(SynqMatrixSuppressedReason::Cooldown));
    TEST_ASSERT_EQUAL_STRING("dense", synqMatrixStateName(SynqMatrixState::Dense));
    TEST_ASSERT_EQUAL_STRING("effect_switch", synqMatrixLastActionName(SynqMatrixLastAction::EffectSwitch));
    TEST_ASSERT_EQUAL_STRING("drop_onset", synqMatrixClassificationReasonName(SynqMatrixClassificationReason::DropOnset));
}

void test_synq_matrix_transition_telemetry_blocks_switch_until_complete() {
    SynqMatrix director;
    restoreReadyDirector(director, makeConfig(), SynqMatrixState::Drop);

    const uint16_t dropEffect = policyEffectForState(SynqMatrixState::Drop);
    director.notifyTransitionStarted(0x2222, dropEffect, 10000, 1000);

    SynqMatrixSwitchRequest request =
        evaluateForState(director, SynqMatrixState::Drop, 10500, INVALID_EFFECT_ID);
    auto status = director.getStatus();
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::TransitionActive, status.suppressedReason);
    TEST_ASSERT_TRUE(status.transitionActive);
    TEST_ASSERT_EQUAL_UINT32(500, status.transitionRemainingMs);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.5f, status.transitionProgress);

    request = evaluateForState(director, SynqMatrixState::Drop, 11000, INVALID_EFFECT_ID);
    status = director.getStatus();
    (void)request;
    TEST_ASSERT_FALSE(status.transitionActive);
    TEST_ASSERT_EQUAL_UINT32(0, status.transitionRemainingMs);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, status.transitionProgress);
}

void test_synq_matrix_mode_taxonomy_collapsed() {
    SynqMatrix director;

    restoreReadyDirector(director, makeConfig(SynqMatrixMode::Off, false), SynqMatrixState::Drop);
    ControlBusFrame frame = frameForState(SynqMatrixState::Drop);
    MusicalGridSnapshot grid = readyBoundaryGrid();
    SynqMatrixParams params = baseParams();
    TEST_ASSERT_FALSE(director.apply(frame, grid, true, 0.050f, 10000, params));
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::Disabled, director.getStatus().suppressedReason);

    restoreReadyDirector(director, makeConfig(SynqMatrixMode::Assist, true), SynqMatrixState::Drop);
    params = baseParams();
    TEST_ASSERT_TRUE(director.apply(frame, grid, true, 0.050f, 11000, params));
    SynqMatrixSwitchRequest request;
    director.tick(frame, grid, true, 11000, INVALID_EFFECT_ID, SynqMatrixContext{}, request);
    auto status = director.getStatus();
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL(SynqMatrixMode::Assist, status.effectiveMode);
    TEST_ASSERT_EQUAL_UINT32(0, status.automaticEffectSwitches);

    restoreReadyDirector(director, makeConfig(SynqMatrixMode::Director, true), SynqMatrixState::Drop);
    director.tick(frame, grid, true, 12000, INVALID_EFFECT_ID, SynqMatrixContext{}, request);
    status = director.getStatus();
    TEST_ASSERT_TRUE(request.requested);
    TEST_ASSERT_EQUAL(SynqMatrixMode::Director, status.effectiveMode);
    TEST_ASSERT_EQUAL(SynqMatrixActionPlan::EffectSwitch, status.actionPlan);
}

void test_synq_matrix_profile_scalars_are_real() {
    ControlBusFrame frame = frameForState(SynqMatrixState::Drop);
    MusicalGridSnapshot grid = readyBoundaryGrid();

    SynqMatrix subtle;
    restoreReadyDirector(subtle,
                         makeConfig(SynqMatrixMode::Assist, false, SynqMatrixProfile::Subtle),
                         SynqMatrixState::Drop);
    SynqMatrixParams subtleParams = baseParams();
    TEST_ASSERT_TRUE(subtle.apply(frame, grid, true, 0.050f, 10000, subtleParams));

    SynqMatrix balanced;
    restoreReadyDirector(balanced,
                         makeConfig(SynqMatrixMode::Assist, false, SynqMatrixProfile::Balanced),
                         SynqMatrixState::Drop);
    SynqMatrixParams balancedParams = baseParams();
    TEST_ASSERT_TRUE(balanced.apply(frame, grid, true, 0.050f, 10000, balancedParams));

    SynqMatrix high;
    restoreReadyDirector(high,
                         makeConfig(SynqMatrixMode::Assist, false, SynqMatrixProfile::High),
                         SynqMatrixState::Drop);
    SynqMatrixParams highParams = baseParams();
    TEST_ASSERT_TRUE(high.apply(frame, grid, true, 0.050f, 10000, highParams));

    TEST_ASSERT_TRUE(subtleParams.intensity < balancedParams.intensity);
    TEST_ASSERT_TRUE(balancedParams.intensity < highParams.intensity);
    TEST_ASSERT_TRUE(subtleParams.speed < balancedParams.speed);
    TEST_ASSERT_TRUE(balancedParams.speed < highParams.speed);
    TEST_ASSERT_EQUAL(SynqMatrixProfile::Subtle, subtle.getStatus().profile);
    TEST_ASSERT_EQUAL(SynqMatrixProfile::High, high.getStatus().profile);
}

void test_synq_matrix_grid_boundary_gates_switching() {
    SynqMatrix director;
    restoreReadyDirector(director, makeConfig(), SynqMatrixState::Drop);
    ControlBusFrame frame = frameForState(SynqMatrixState::Drop);
    SynqMatrixSwitchRequest request;
    director.tick(frame,
                              waitingBoundaryGrid(),
                              true,
                              10000,
                              INVALID_EFFECT_ID,
                              SynqMatrixContext{},
                              request);
    auto status = director.getStatus();
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::BoundaryDeferred, status.suppressedReason);
    TEST_ASSERT_EQUAL(SynqMatrixBoundaryGate::WaitingForBoundary, status.boundaryGate);
    TEST_ASSERT_TRUE(status.waitingForBoundary);

    restoreReadyDirector(director, makeConfig(), SynqMatrixState::Drop);
    director.tick(frame,
                              readyBoundaryGrid(),
                              true,
                              10000,
                              INVALID_EFFECT_ID,
                              SynqMatrixContext{},
                              request);
    status = director.getStatus();
    TEST_ASSERT_TRUE(request.requested);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::None, status.suppressedReason);
    TEST_ASSERT_TRUE(status.boundaryReady);
    TEST_ASSERT_EQUAL(SynqMatrixBoundaryGate::DownbeatBoundary, status.boundaryGate);
}

void test_synq_matrix_assist_handles_build_drop_without_effect_switch() {
    SynqMatrix director;
    restoreReadyDirector(director, makeConfig(SynqMatrixMode::Assist, true, SynqMatrixProfile::High),
                         SynqMatrixState::Build);
    ControlBusFrame frame = frameForState(SynqMatrixState::Build);
    MusicalGridSnapshot grid = readyBoundaryGrid();
    SynqMatrixParams params = baseParams();
    TEST_ASSERT_TRUE(director.apply(frame, grid, true, 0.050f, 10000, params));
    SynqMatrixSwitchRequest request;
    director.tick(frame, grid, true, 10000, INVALID_EFFECT_ID, SynqMatrixContext{}, request);
    auto status = director.getStatus();
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_TRUE(params.intensity > 128);
    TEST_ASSERT_EQUAL(SynqMatrixIntent::BuildPressure, status.intent);
    TEST_ASSERT_EQUAL_UINT32(0, status.automaticEffectSwitches);
}

void test_synq_matrix_health_gate_uses_current_health_not_stale_counters() {
    SynqMatrix director;
    restoreReadyDirector(director, makeConfig(), SynqMatrixState::Drop);
    ControlBusFrame frame = frameForState(SynqMatrixState::Drop);
    MusicalGridSnapshot grid = readyBoundaryGrid();
    SynqMatrixSwitchRequest request;
    SynqMatrixContext context;
    context.health.showSkips = 1;
    director.tick(frame, grid, true, 10000, INVALID_EFFECT_ID, context, request);
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::Health, director.getStatus().suppressedReason);

    context.health.showSkips = 0;
    director.tick(frame, grid, true, 11000, INVALID_EFFECT_ID, context, request);
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::HealthRecovering, director.getStatus().suppressedReason);

    director.tick(frame, grid, true, 14050, INVALID_EFFECT_ID, context, request);
    TEST_ASSERT_TRUE(request.requested);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::None, director.getStatus().suppressedReason);
}

void test_synq_matrix_mutable_allowlist_disables_state_policy() {
    SynqMatrix director;
    restoreReadyDirector(director, makeConfig(), SynqMatrixState::Drop);
    TEST_ASSERT_TRUE(director.setPolicyAllowed(SynqMatrixState::Drop, false));
    TEST_ASSERT_FALSE(director.isPolicyAllowed(SynqMatrixState::Drop));

    SynqMatrixSwitchRequest request;
    director.tick(frameForState(SynqMatrixState::Drop),
                              readyBoundaryGrid(),
                              true,
                              10000,
                              INVALID_EFFECT_ID,
                              SynqMatrixContext{},
                              request);
    const auto status = director.getStatus();
    const auto allowlist = director.getAllowlistSnapshot();
    bool sawDisabledDrop = false;
    for (uint8_t i = 0; i < allowlist.count; ++i) {
        if (allowlist.policies[i].state == SynqMatrixState::Drop) {
            sawDisabledDrop = !allowlist.policies[i].enabled;
        }
    }
    TEST_ASSERT_FALSE(request.requested);
    TEST_ASSERT_TRUE(sawDisabledDrop);
    TEST_ASSERT_EQUAL(SynqMatrixSuppressedReason::AllowlistDisabled, status.suppressedReason);

    director.resetPolicyAllowlist();
    TEST_ASSERT_TRUE(director.isPolicyAllowed(SynqMatrixState::Drop));
}

void setUp(void) {}
void tearDown(void) {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_synq_matrix_defaults_reset_and_off_disable_activity);
    RUN_TEST(test_synq_matrix_mode_parsing_and_names_cover_public_modes);
    RUN_TEST(test_synq_matrix_classifier_reports_each_director_state);
    RUN_TEST(test_synq_matrix_boot_and_enable_grace_suppress_switching);
    RUN_TEST(test_synq_matrix_switching_disabled_for_assist_or_disabled_switching_modes);
    RUN_TEST(test_synq_matrix_successful_switch_request_and_applied_counters);
    RUN_TEST(test_synq_matrix_same_effect_suppresses_without_counting_switch);
    RUN_TEST(test_synq_matrix_dwell_gate_blocks_newly_established_state);
    RUN_TEST(test_synq_matrix_cooldown_blocks_after_recent_switch);
    RUN_TEST(test_synq_matrix_rate_limit_blocks_third_switch_inside_window);
    RUN_TEST(test_synq_matrix_anti_thrash_blocks_immediate_aba_switch);
    RUN_TEST(test_synq_matrix_manual_and_show_owners_suppress_director_switches);
    RUN_TEST(test_synq_matrix_health_gate_and_recovery_window_suppress_switches);
    RUN_TEST(test_synq_matrix_restore_runtime_state_and_reset_counters);
    RUN_TEST(test_synq_matrix_assist_mode_changes_controls_without_switching);
    RUN_TEST(test_synq_matrix_director_apply_is_parameter_only_and_never_switches_effect);
    RUN_TEST(test_synq_matrix_low_confidence_suppresses_activity);
    RUN_TEST(test_synq_matrix_confidence_floor_remains_single_config_control);
    RUN_TEST(test_synq_matrix_enters_and_exits_coast_on_audio_confidence_duration);
    RUN_TEST(test_synq_matrix_coast_leaves_incoming_params_unchanged_and_suppresses_switches);
    RUN_TEST(test_synq_matrix_low_confidence_pre_coast_blocks_state_promotion);
    RUN_TEST(test_synq_matrix_policy_and_string_telemetry_helpers);
    RUN_TEST(test_synq_matrix_transition_telemetry_blocks_switch_until_complete);
    RUN_TEST(test_synq_matrix_mode_taxonomy_collapsed);
    RUN_TEST(test_synq_matrix_profile_scalars_are_real);
    RUN_TEST(test_synq_matrix_grid_boundary_gates_switching);
    RUN_TEST(test_synq_matrix_assist_handles_build_drop_without_effect_switch);
    RUN_TEST(test_synq_matrix_health_gate_uses_current_health_not_stale_counters);
    RUN_TEST(test_synq_matrix_mutable_allowlist_disables_state_policy);
    return UNITY_END();
}

#endif
