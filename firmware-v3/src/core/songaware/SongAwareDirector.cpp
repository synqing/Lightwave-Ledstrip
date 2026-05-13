/**
 * @file SongAwareDirector.cpp
 * @brief Runtime-only song-aware parameter and visual-language director.
 */

#include "SongAwareDirector.h"

#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace songaware {

namespace {

static constexpr float kSmoothTauSeconds = 0.18f;
static constexpr uint32_t kBootGraceMs = 3000;
static constexpr uint32_t kPostEnableGraceMs = 4000;
static constexpr uint32_t kEvaluationPeriodMs = 500;
static constexpr uint32_t kStableStateHoldMs = 400;
static constexpr uint32_t kDropStateHoldMs = 250;
static constexpr uint32_t kMinimumDwellMs = 8000;
static constexpr uint32_t kSwitchCooldownMs = 20000;
static constexpr uint32_t kSwitchWindowMs = 60000;
static constexpr uint8_t kMaxSwitchesPerWindow = 2;
static constexpr uint32_t kAntiThrashWindowMs = 45000;
static constexpr uint32_t kHealthCleanWindowMs = 3000;
static constexpr uint32_t kManualSuppressMs = 15000;
static constexpr uint32_t kShowSuppressMs = 1000;

struct DirectorPolicy {
    SongAwareState state;
    EffectId effectId;
    const char* family;
    const char* visualLanguage;
    SongAwareSwitchReason reason;
    float minConfidence;
};

static constexpr DirectorPolicy kPolicies[] = {
    {SongAwareState::Unknown, EID_SB_K1_WAVEFORM, "baseline", "k1_waveform_restore_baseline", SongAwareSwitchReason::None, 1.0f},
    {SongAwareState::Silence, EID_MODAL_RESONANCE, "interference", "modal_low_density_hold", SongAwareSwitchReason::AmbientPosture, 1.0f},
    {SongAwareState::Ambient, EID_MODAL_RESONANCE, "interference", "calm_modal_resonance", SongAwareSwitchReason::AmbientPosture, 0.30f},
    {SongAwareState::Steady, EID_LGP_HOLOGRAPHIC, "interference", "flagship_holographic_depth", SongAwareSwitchReason::SteadyReadability, 0.32f},
    {SongAwareState::Build, EID_LGP_WAVE_COLLISION, "interference", "colliding_wave_pressure", SongAwareSwitchReason::BuildPressure, 0.45f},
    {SongAwareState::Drop, EID_LGP_PHOTONIC_CRYSTAL, "advanced_optical", "photonic_drop_texture", SongAwareSwitchReason::DropImpact, 0.60f},
    {SongAwareState::Breakdown, EID_LGP_CHROMATIC_LENS, "advanced_optical", "chromatic_space_release", SongAwareSwitchReason::BreakdownRelease, 0.35f},
    {SongAwareState::Dense, EID_LGP_KDV_SOLITON_PAIR, "mathematical", "dense_soliton_pair", SongAwareSwitchReason::DenseLegibility, 0.55f},
    {SongAwareState::Transition, EID_LGP_CHROMATIC_PULSE, "advanced_optical", "chromatic_transition_pulse", SongAwareSwitchReason::TransitionBridge, 0.45f},
};

static constexpr uint8_t kPolicyCount = sizeof(kPolicies) / sizeof(kPolicies[0]);
static constexpr uint16_t kAllPoliciesMask = (static_cast<uint16_t>(1U) << kPolicyCount) - 1U;

bool deadlineActive(uint32_t nowMs, uint32_t deadlineMs) {
    return static_cast<int32_t>(deadlineMs - nowMs) > 0;
}

uint32_t remainingUntil(uint32_t nowMs, uint32_t deadlineMs) {
    return deadlineActive(nowMs, deadlineMs) ? (deadlineMs - nowMs) : 0;
}

const DirectorPolicy& policyByIndex(uint8_t index) {
    if (index < kPolicyCount) {
        return kPolicies[index];
    }
    return kPolicies[0];
}

const DirectorPolicy& policyForState(SongAwareState state, uint8_t& outIndex) {
    for (uint8_t i = 1; i < kPolicyCount; ++i) {
        if (kPolicies[i].state == state) {
            outIndex = i;
            return kPolicies[i];
        }
    }
    outIndex = 0;
    return kPolicies[0];
}

uint16_t policyBit(uint8_t index) {
    return (index < 16U) ? static_cast<uint16_t>(1U << index) : 0U;
}

uint16_t policyBitForState(SongAwareState state) {
    uint8_t policyIndex = 0;
    (void)policyForState(state, policyIndex);
    return policyBit(policyIndex);
}

SongAwarePolicySnapshot snapshotForPolicy(const DirectorPolicy& policy, bool enabled = true) {
    SongAwarePolicySnapshot snapshot;
    snapshot.state = policy.state;
    snapshot.effectId = static_cast<uint16_t>(policy.effectId);
    snapshot.family = policy.family;
    snapshot.visualLanguage = policy.visualLanguage;
    snapshot.reason = policy.reason;
    snapshot.minConfidence = policy.minConfidence;
    snapshot.enabled = enabled;
    return snapshot;
}

uint32_t elapsedSince(uint32_t nowMs, uint32_t sinceMs) {
    if (sinceMs == 0 || static_cast<int32_t>(nowMs - sinceMs) < 0) {
        return 0;
    }
    return nowMs - sinceMs;
}

} // namespace

SongAwareDirector& SongAwareDirector::instance() {
    static SongAwareDirector director;
    return director;
}

void SongAwareDirector::reset() {
    setConfig(SongAwareConfig{});
    m_effectiveMode.store(static_cast<uint8_t>(SongAwareMode::Off), std::memory_order_release);
    m_profile.store(static_cast<uint8_t>(SongAwareProfile::Balanced), std::memory_order_release);
    m_owner.store(static_cast<uint8_t>(SongAwareOwner::None), std::memory_order_release);
    m_suppressedReason.store(static_cast<uint8_t>(SongAwareSuppressedReason::Disabled), std::memory_order_release);
    m_previousSuppressedReason.store(static_cast<uint8_t>(SongAwareSuppressedReason::Disabled), std::memory_order_release);
    m_classificationReason.store(static_cast<uint8_t>(SongAwareClassificationReason::None), std::memory_order_release);
    m_rawSongState.store(static_cast<uint8_t>(SongAwareState::Unknown), std::memory_order_release);
    m_previousSongState.store(static_cast<uint8_t>(SongAwareState::Unknown), std::memory_order_release);
    m_currentSongState.store(static_cast<uint8_t>(SongAwareState::Unknown), std::memory_order_release);
    m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::None), std::memory_order_release);
    m_intent.store(static_cast<uint8_t>(SongAwareIntent::QuietHold), std::memory_order_release);
    m_actionPlan.store(static_cast<uint8_t>(SongAwareActionPlan::None), std::memory_order_release);
    m_boundaryGate.store(static_cast<uint8_t>(SongAwareBoundaryGate::NotRequired), std::memory_order_release);
    m_boundaryReady.store(false, std::memory_order_release);
    m_waitingForBoundary.store(false, std::memory_order_release);
    m_boundaryConfidenceQ1000.store(0, std::memory_order_release);
    m_lastSwitchReason.store(static_cast<uint8_t>(SongAwareSwitchReason::None), std::memory_order_release);
    m_confidenceQ1000.store(0, std::memory_order_release);
    m_driveQ1000.store(0, std::memory_order_release);
    m_slowEnergyQ1000.store(0, std::memory_order_release);
    m_adaptiveFloorQ1000.store(40, std::memory_order_release);
    m_selectionScoreQ1000.store(0, std::memory_order_release);
    m_parameterUpdates.store(0, std::memory_order_release);
    m_automaticEffectSwitches.store(0, std::memory_order_release);
    m_lastDecisionAtMs.store(0, std::memory_order_release);
    m_lastSwitchAtMs.store(0, std::memory_order_release);
    m_stateAgeMs.store(0, std::memory_order_release);
    m_candidateAgeMs.store(0, std::memory_order_release);
    m_candidateHoldRemainingMs.store(0, std::memory_order_release);
    m_dwellRemainingMs.store(0, std::memory_order_release);
    m_cooldownRemainingMs.store(0, std::memory_order_release);
    m_bootGraceUntilMs.store(kBootGraceMs, std::memory_order_release);
    m_enableGraceUntilMs.store(0, std::memory_order_release);
    m_enableGracePending.store(false, std::memory_order_release);
    m_switchWindowRemainingMs.store(0, std::memory_order_release);
    m_antiThrashUntilMs.store(0, std::memory_order_release);
    m_activeEffectId.store(INVALID_EFFECT_ID, std::memory_order_release);
    m_previousEffectId.store(INVALID_EFFECT_ID, std::memory_order_release);
    m_selectedEffectId.store(INVALID_EFFECT_ID, std::memory_order_release);
    m_selectedPolicyIndex.store(0, std::memory_order_release);
    m_rmsQ1000.store(0, std::memory_order_release);
    m_fluxQ1000.store(0, std::memory_order_release);
    m_bpmQ10.store(0, std::memory_order_release);
    m_audioConfidenceQ1000.store(0, std::memory_order_release);
    m_showSkips.store(0, std::memory_order_release);
    m_failures.store(0, std::memory_order_release);
    m_rmtErrors.store(0, std::memory_order_release);
    m_underruns.store(0, std::memory_order_release);
    m_manualSuppressUntilMs.store(0, std::memory_order_release);
    m_showSuppressUntilMs.store(0, std::memory_order_release);
    m_lastEvaluationAtMs.store(0, std::memory_order_release);
    m_stateEnteredAtMs.store(0, std::memory_order_release);
    m_candidateState.store(static_cast<uint8_t>(SongAwareState::Unknown), std::memory_order_release);
    m_candidateSinceMs.store(0, std::memory_order_release);
    m_switchWindowStartMs.store(0, std::memory_order_release);
    m_switchesInWindow.store(0, std::memory_order_release);
    m_lastSwitchFromEffectId.store(INVALID_EFFECT_ID, std::memory_order_release);
    m_lastSwitchToEffectId.store(INVALID_EFFECT_ID, std::memory_order_release);
    m_transitionActive.store(false, std::memory_order_release);
    m_transitionPreviousEffectId.store(INVALID_EFFECT_ID, std::memory_order_release);
    m_transitionTargetEffectId.store(INVALID_EFFECT_ID, std::memory_order_release);
    m_transitionStartedAtMs.store(0, std::memory_order_release);
    m_transitionDurationMs.store(0, std::memory_order_release);
    m_transitionRemainingMs.store(0, std::memory_order_release);
    m_transitionProgressQ1000.store(0, std::memory_order_release);
    m_healthDegraded.store(false, std::memory_order_release);
    m_healthCleanSinceMs.store(0, std::memory_order_release);
    m_healthLastDegradedAtMs.store(0, std::memory_order_release);
    m_healthCleanForMs.store(0, std::memory_order_release);
    m_healthCleanWindowRemainingMs.store(0, std::memory_order_release);
    m_policyAllowMask.store(kAllPoliciesMask, std::memory_order_release);
}

void SongAwareDirector::resetCounters() {
    m_parameterUpdates.store(0, std::memory_order_release);
    m_automaticEffectSwitches.store(0, std::memory_order_release);
    m_showSkips.store(0, std::memory_order_release);
    m_failures.store(0, std::memory_order_release);
    m_rmtErrors.store(0, std::memory_order_release);
    m_underruns.store(0, std::memory_order_release);
    m_switchWindowStartMs.store(0, std::memory_order_release);
    m_switchesInWindow.store(0, std::memory_order_release);
    m_switchWindowRemainingMs.store(0, std::memory_order_release);
    m_healthDegraded.store(false, std::memory_order_release);
    m_healthCleanSinceMs.store(0, std::memory_order_release);
    m_healthLastDegradedAtMs.store(0, std::memory_order_release);
    m_healthCleanForMs.store(0, std::memory_order_release);
    m_healthCleanWindowRemainingMs.store(0, std::memory_order_release);
}

void SongAwareDirector::setConfig(const SongAwareConfig& config) {
    const bool wasEnabled = m_enabled.load(std::memory_order_acquire);
    const SongAwareMode previousMode = static_cast<SongAwareMode>(m_mode.load(std::memory_order_acquire));
    const bool switchingEnabled = config.switchingEnabled || config.constrainedSwitching;

    m_enabled.store(config.enabled, std::memory_order_release);
    m_mode.store(static_cast<uint8_t>(config.mode), std::memory_order_release);
    m_profile.store(static_cast<uint8_t>(config.profile), std::memory_order_release);
    m_familyMorphing.store(config.familyMorphing, std::memory_order_release);
    m_constrainedSwitching.store(switchingEnabled, std::memory_order_release);
    m_switchingEnabled.store(switchingEnabled, std::memory_order_release);
    m_sensitivityQ1000.store(scaleFloat(config.sensitivity), std::memory_order_release);
    m_intensityScalarQ1000.store(scaleFloat(config.intensityScalar), std::memory_order_release);
    m_motionScalarQ1000.store(scaleFloat(config.motionScalar), std::memory_order_release);
    m_confidenceFloorQ1000.store(scaleFloat(config.confidenceFloor), std::memory_order_release);
    if (config.enabled && config.mode != SongAwareMode::Off &&
        (!wasEnabled || previousMode == SongAwareMode::Off)) {
        m_enableGracePending.store(true, std::memory_order_release);
        m_enableGraceUntilMs.store(0, std::memory_order_release);
    }
    if (!config.enabled || config.mode == SongAwareMode::Off) {
        m_effectiveMode.store(static_cast<uint8_t>(SongAwareMode::Off), std::memory_order_release);
        m_enableGracePending.store(false, std::memory_order_release);
        m_enableGraceUntilMs.store(0, std::memory_order_release);
        m_transitionActive.store(false, std::memory_order_release);
        m_transitionRemainingMs.store(0, std::memory_order_release);
        m_transitionProgressQ1000.store(0, std::memory_order_release);
        setSuppressed(SongAwareSuppressedReason::Disabled, SongAwareOwner::None);
    }
}

SongAwareConfig SongAwareDirector::getConfig() const {
    SongAwareConfig config;
    config.enabled = m_enabled.load(std::memory_order_acquire);
    config.mode = static_cast<SongAwareMode>(m_mode.load(std::memory_order_acquire));
    config.profile = static_cast<SongAwareProfile>(m_profile.load(std::memory_order_acquire));
    config.familyMorphing = m_familyMorphing.load(std::memory_order_acquire);
    config.constrainedSwitching = m_switchingEnabled.load(std::memory_order_acquire);
    config.switchingEnabled = m_switchingEnabled.load(std::memory_order_acquire);
    config.sensitivity = unscaleFloat(m_sensitivityQ1000.load(std::memory_order_acquire));
    config.intensityScalar = unscaleFloat(m_intensityScalarQ1000.load(std::memory_order_acquire));
    config.motionScalar = unscaleFloat(m_motionScalarQ1000.load(std::memory_order_acquire));
    config.confidenceFloor = unscaleFloat(m_confidenceFloorQ1000.load(std::memory_order_acquire));
    return config;
}

SongAwareStatus SongAwareDirector::getStatus() const {
    SongAwareStatus status;
    status.enabled = m_enabled.load(std::memory_order_acquire);
    status.effectiveMode = static_cast<SongAwareMode>(m_effectiveMode.load(std::memory_order_acquire));
    status.profile = static_cast<SongAwareProfile>(m_profile.load(std::memory_order_acquire));
    status.owner = static_cast<SongAwareOwner>(m_owner.load(std::memory_order_acquire));
    status.suppressedReason =
        static_cast<SongAwareSuppressedReason>(m_suppressedReason.load(std::memory_order_acquire));
    status.previousSuppressedReason =
        static_cast<SongAwareSuppressedReason>(m_previousSuppressedReason.load(std::memory_order_acquire));
    status.classificationReason =
        static_cast<SongAwareClassificationReason>(m_classificationReason.load(std::memory_order_acquire));
    status.rawSongState =
        static_cast<SongAwareState>(m_rawSongState.load(std::memory_order_acquire));
    status.previousSongState =
        static_cast<SongAwareState>(m_previousSongState.load(std::memory_order_acquire));
    status.currentSongState =
        static_cast<SongAwareState>(m_currentSongState.load(std::memory_order_acquire));
    status.candidateSongState =
        static_cast<SongAwareState>(m_candidateState.load(std::memory_order_acquire));
    status.lastAction = static_cast<SongAwareLastAction>(m_lastAction.load(std::memory_order_acquire));
    status.intent = static_cast<SongAwareIntent>(m_intent.load(std::memory_order_acquire));
    status.actionPlan = static_cast<SongAwareActionPlan>(m_actionPlan.load(std::memory_order_acquire));
    status.boundaryGate = static_cast<SongAwareBoundaryGate>(m_boundaryGate.load(std::memory_order_acquire));
    status.boundaryReady = m_boundaryReady.load(std::memory_order_acquire);
    status.waitingForBoundary = m_waitingForBoundary.load(std::memory_order_acquire);
    status.boundaryConfidence = unscaleFloat(m_boundaryConfidenceQ1000.load(std::memory_order_acquire));
    status.confidence = unscaleFloat(m_confidenceQ1000.load(std::memory_order_acquire));
    status.selectionScore = unscaleFloat(m_selectionScoreQ1000.load(std::memory_order_acquire));
    status.parameterUpdates = m_parameterUpdates.load(std::memory_order_acquire);
    status.automaticEffectSwitches = m_automaticEffectSwitches.load(std::memory_order_acquire);
    status.lastDecisionAtMs = m_lastDecisionAtMs.load(std::memory_order_acquire);
    status.lastSwitchAtMs = m_lastSwitchAtMs.load(std::memory_order_acquire);
    status.stateAgeMs = m_stateAgeMs.load(std::memory_order_acquire);
    status.candidateAgeMs = m_candidateAgeMs.load(std::memory_order_acquire);
    status.candidateHoldRemainingMs = m_candidateHoldRemainingMs.load(std::memory_order_acquire);
    status.dwellRemainingMs = m_dwellRemainingMs.load(std::memory_order_acquire);
    status.cooldownRemainingMs = m_cooldownRemainingMs.load(std::memory_order_acquire);
    status.bootGraceRemainingMs =
        remainingUntil(status.lastDecisionAtMs, m_bootGraceUntilMs.load(std::memory_order_acquire));
    status.enableGraceRemainingMs =
        remainingUntil(status.lastDecisionAtMs, m_enableGraceUntilMs.load(std::memory_order_acquire));
    status.switchWindowRemainingMs = m_switchWindowRemainingMs.load(std::memory_order_acquire);
    status.switchesInWindow = m_switchesInWindow.load(std::memory_order_acquire);
    status.maxSwitchesPerWindow = kMaxSwitchesPerWindow;
    status.antiThrashRemainingMs =
        remainingUntil(status.lastDecisionAtMs, m_antiThrashUntilMs.load(std::memory_order_acquire));
    status.activeEffectId = static_cast<uint16_t>(m_activeEffectId.load(std::memory_order_acquire));
    status.previousEffectId = static_cast<uint16_t>(m_previousEffectId.load(std::memory_order_acquire));
    status.selectedEffectId = static_cast<uint16_t>(m_selectedEffectId.load(std::memory_order_acquire));
    status.lastSwitchFromEffectId =
        static_cast<uint16_t>(m_lastSwitchFromEffectId.load(std::memory_order_acquire));
    status.lastSwitchToEffectId =
        static_cast<uint16_t>(m_lastSwitchToEffectId.load(std::memory_order_acquire));

    const DirectorPolicy& policy = policyByIndex(m_selectedPolicyIndex.load(std::memory_order_acquire));
    status.selectedFamily = policy.family;
    status.selectedVisualLanguage = policy.visualLanguage;
    status.lastSwitchReason = songAwareSwitchReasonName(
        static_cast<SongAwareSwitchReason>(m_lastSwitchReason.load(std::memory_order_acquire)));

    status.transitionActive = m_transitionActive.load(std::memory_order_acquire);
    status.transitionPreviousEffectId =
        static_cast<uint16_t>(m_transitionPreviousEffectId.load(std::memory_order_acquire));
    status.transitionTargetEffectId =
        static_cast<uint16_t>(m_transitionTargetEffectId.load(std::memory_order_acquire));
    status.transitionStartedAtMs = m_transitionStartedAtMs.load(std::memory_order_acquire);
    status.transitionDurationMs = m_transitionDurationMs.load(std::memory_order_acquire);
    status.transitionRemainingMs = m_transitionRemainingMs.load(std::memory_order_acquire);
    status.transitionProgress = unscaleFloat(m_transitionProgressQ1000.load(std::memory_order_acquire));

    status.rms = unscaleFloat(m_rmsQ1000.load(std::memory_order_acquire));
    status.flux = unscaleFloat(m_fluxQ1000.load(std::memory_order_acquire));
    status.bpm = static_cast<float>(m_bpmQ10.load(std::memory_order_acquire)) * 0.1f;
    status.audioConfidence = unscaleFloat(m_audioConfidenceQ1000.load(std::memory_order_acquire));
    status.showSkips = m_showSkips.load(std::memory_order_acquire);
    status.failures = m_failures.load(std::memory_order_acquire);
    status.rmtErrors = m_rmtErrors.load(std::memory_order_acquire);
    status.underruns = m_underruns.load(std::memory_order_acquire);
    status.healthDegraded = m_healthDegraded.load(std::memory_order_acquire);
    status.healthCleanForMs = m_healthCleanForMs.load(std::memory_order_acquire);
    status.healthCleanWindowRemainingMs = m_healthCleanWindowRemainingMs.load(std::memory_order_acquire);
    return status;
}

SongAwareRuntimeState SongAwareDirector::exportRuntimeState() const {
    SongAwareRuntimeState state;
    state.config = getConfig();
    state.status = getStatus();
    state.policyAllowMask = m_policyAllowMask.load(std::memory_order_acquire);
    state.manualSuppressUntilMs = m_manualSuppressUntilMs.load(std::memory_order_acquire);
    state.showSuppressUntilMs = m_showSuppressUntilMs.load(std::memory_order_acquire);
    state.bootGraceUntilMs = m_bootGraceUntilMs.load(std::memory_order_acquire);
    state.enableGraceUntilMs = m_enableGraceUntilMs.load(std::memory_order_acquire);
    state.healthCleanSinceMs = m_healthCleanSinceMs.load(std::memory_order_acquire);
    state.healthLastDegradedAtMs = m_healthLastDegradedAtMs.load(std::memory_order_acquire);
    state.antiThrashUntilMs = m_antiThrashUntilMs.load(std::memory_order_acquire);
    return state;
}

void SongAwareDirector::restoreRuntimeState(const SongAwareRuntimeState& state) {
    setConfig(state.config);
    m_effectiveMode.store(static_cast<uint8_t>(state.status.effectiveMode), std::memory_order_release);
    m_owner.store(static_cast<uint8_t>(state.status.owner), std::memory_order_release);
    m_suppressedReason.store(static_cast<uint8_t>(state.status.suppressedReason), std::memory_order_release);
    m_previousSuppressedReason.store(static_cast<uint8_t>(state.status.previousSuppressedReason), std::memory_order_release);
    m_classificationReason.store(static_cast<uint8_t>(state.status.classificationReason), std::memory_order_release);
    m_rawSongState.store(static_cast<uint8_t>(state.status.rawSongState), std::memory_order_release);
    m_previousSongState.store(static_cast<uint8_t>(state.status.previousSongState), std::memory_order_release);
    m_currentSongState.store(static_cast<uint8_t>(state.status.currentSongState), std::memory_order_release);
    m_candidateState.store(static_cast<uint8_t>(state.status.candidateSongState), std::memory_order_release);
    m_lastAction.store(static_cast<uint8_t>(state.status.lastAction), std::memory_order_release);
    m_intent.store(static_cast<uint8_t>(state.status.intent), std::memory_order_release);
    m_actionPlan.store(static_cast<uint8_t>(state.status.actionPlan), std::memory_order_release);
    m_boundaryGate.store(static_cast<uint8_t>(state.status.boundaryGate), std::memory_order_release);
    m_boundaryReady.store(state.status.boundaryReady, std::memory_order_release);
    m_waitingForBoundary.store(state.status.waitingForBoundary, std::memory_order_release);
    m_boundaryConfidenceQ1000.store(scaleFloat(state.status.boundaryConfidence), std::memory_order_release);
    m_confidenceQ1000.store(scaleFloat(state.status.confidence), std::memory_order_release);
    m_selectionScoreQ1000.store(scaleFloat(state.status.selectionScore), std::memory_order_release);
    m_parameterUpdates.store(state.status.parameterUpdates, std::memory_order_release);
    m_automaticEffectSwitches.store(state.status.automaticEffectSwitches, std::memory_order_release);
    m_lastDecisionAtMs.store(state.status.lastDecisionAtMs, std::memory_order_release);
    m_lastSwitchAtMs.store(state.status.lastSwitchAtMs, std::memory_order_release);
    m_stateAgeMs.store(state.status.stateAgeMs, std::memory_order_release);
    m_candidateAgeMs.store(state.status.candidateAgeMs, std::memory_order_release);
    m_candidateHoldRemainingMs.store(state.status.candidateHoldRemainingMs, std::memory_order_release);
    m_dwellRemainingMs.store(state.status.dwellRemainingMs, std::memory_order_release);
    m_cooldownRemainingMs.store(state.status.cooldownRemainingMs, std::memory_order_release);
    m_switchWindowRemainingMs.store(state.status.switchWindowRemainingMs, std::memory_order_release);
    m_switchesInWindow.store(state.status.switchesInWindow, std::memory_order_release);
    m_activeEffectId.store(state.status.activeEffectId, std::memory_order_release);
    m_previousEffectId.store(state.status.previousEffectId, std::memory_order_release);
    m_selectedEffectId.store(state.status.selectedEffectId, std::memory_order_release);
    m_lastSwitchFromEffectId.store(state.status.lastSwitchFromEffectId, std::memory_order_release);
    m_lastSwitchToEffectId.store(state.status.lastSwitchToEffectId, std::memory_order_release);
    m_transitionActive.store(state.status.transitionActive, std::memory_order_release);
    m_transitionPreviousEffectId.store(state.status.transitionPreviousEffectId, std::memory_order_release);
    m_transitionTargetEffectId.store(state.status.transitionTargetEffectId, std::memory_order_release);
    m_transitionStartedAtMs.store(state.status.transitionStartedAtMs, std::memory_order_release);
    m_transitionDurationMs.store(state.status.transitionDurationMs, std::memory_order_release);
    m_transitionRemainingMs.store(state.status.transitionRemainingMs, std::memory_order_release);
    m_transitionProgressQ1000.store(scaleFloat(state.status.transitionProgress), std::memory_order_release);
    m_showSkips.store(state.status.showSkips, std::memory_order_release);
    m_failures.store(state.status.failures, std::memory_order_release);
    m_rmtErrors.store(state.status.rmtErrors, std::memory_order_release);
    m_underruns.store(state.status.underruns, std::memory_order_release);
    m_healthDegraded.store(state.status.healthDegraded, std::memory_order_release);
    m_healthCleanForMs.store(state.status.healthCleanForMs, std::memory_order_release);
    m_healthCleanWindowRemainingMs.store(state.status.healthCleanWindowRemainingMs, std::memory_order_release);
    m_manualSuppressUntilMs.store(state.manualSuppressUntilMs, std::memory_order_release);
    m_showSuppressUntilMs.store(state.showSuppressUntilMs, std::memory_order_release);
    m_bootGraceUntilMs.store(state.bootGraceUntilMs, std::memory_order_release);
    m_enableGraceUntilMs.store(state.enableGraceUntilMs, std::memory_order_release);
    m_enableGracePending.store(false, std::memory_order_release);
    m_healthCleanSinceMs.store(state.healthCleanSinceMs, std::memory_order_release);
    m_healthLastDegradedAtMs.store(state.healthLastDegradedAtMs, std::memory_order_release);
    m_antiThrashUntilMs.store(state.antiThrashUntilMs, std::memory_order_release);
    m_policyAllowMask.store((state.policyAllowMask == 0U) ? kAllPoliciesMask
                                                         : (state.policyAllowMask & kAllPoliciesMask),
                            std::memory_order_release);
}

SongAwareDebugSnapshot SongAwareDirector::getDebugSnapshot() const {
    SongAwareDebugSnapshot snapshot;
    snapshot.config = getConfig();
    snapshot.status = getStatus();
    snapshot.allowlist = getAllowlistSnapshot();
    snapshot.bootGraceMs = kBootGraceMs;
    snapshot.postEnableGraceMs = kPostEnableGraceMs;
    snapshot.stableStateHoldMs = kStableStateHoldMs;
    snapshot.dropStateHoldMs = kDropStateHoldMs;
    snapshot.minimumDwellMs = kMinimumDwellMs;
    snapshot.switchCooldownMs = kSwitchCooldownMs;
    snapshot.switchWindowMs = kSwitchWindowMs;
    snapshot.maxSwitchesPerWindow = kMaxSwitchesPerWindow;
    snapshot.antiThrashWindowMs = kAntiThrashWindowMs;
    snapshot.healthCleanWindowMs = kHealthCleanWindowMs;
    return snapshot;
}

SongAwareSelectionSnapshot SongAwareDirector::resolveSelection(SongAwareState state,
                                                               float confidence,
                                                               uint16_t activeEffectId) const {
    uint8_t policyIndex = 0;
    const DirectorPolicy& policy = policyForState(state, policyIndex);
    const bool policyEnabled =
        (m_policyAllowMask.load(std::memory_order_acquire) & policyBit(policyIndex)) != 0U;

    SongAwareSelectionSnapshot selection;
    selection.policy = snapshotForPolicy(policy, policyEnabled);
    selection.score = scorePolicy(state, confidence, selection.policy);
    selection.valid = policyEnabled &&
                      policy.effectId != INVALID_EFFECT_ID &&
                      policy.effectId != activeEffectId &&
                      confidence >= policy.minConfidence;
    return selection;
}

uint8_t SongAwareDirector::policyCount() {
    return kPolicyCount;
}

bool SongAwareDirector::policySnapshot(uint8_t index, SongAwarePolicySnapshot& snapshot) {
    if (index >= kPolicyCount) {
        return false;
    }
    snapshot = snapshotForPolicy(kPolicies[index]);
    return true;
}

uint8_t SongAwareDirector::copyPolicyTable(SongAwarePolicySnapshot* out, uint8_t capacity) {
    if (out == nullptr || capacity == 0) {
        return 0;
    }
    const uint8_t count = (capacity < kPolicyCount) ? capacity : kPolicyCount;
    for (uint8_t i = 0; i < count; ++i) {
        out[i] = snapshotForPolicy(kPolicies[i]);
    }
    return count;
}

SongAwareAllowlistSnapshot SongAwareDirector::policyTableSnapshot() {
    SongAwareAllowlistSnapshot snapshot;
    snapshot.count = copyPolicyTable(snapshot.policies, kSongAwareMaxPolicySnapshotCount);
    return snapshot;
}

SongAwareAllowlistSnapshot SongAwareDirector::getAllowlistSnapshot() const {
    SongAwareAllowlistSnapshot snapshot;
    const uint16_t mask = m_policyAllowMask.load(std::memory_order_acquire);
    snapshot.count = (kPolicyCount < kSongAwareMaxPolicySnapshotCount)
                         ? kPolicyCount
                         : kSongAwareMaxPolicySnapshotCount;
    for (uint8_t i = 0; i < snapshot.count; ++i) {
        snapshot.policies[i] = snapshotForPolicy(kPolicies[i], (mask & policyBit(i)) != 0U);
    }
    return snapshot;
}

bool SongAwareDirector::setPolicyAllowed(SongAwareState state, bool enabled) {
    const uint16_t bit = policyBitForState(state);
    if (bit == 0U) {
        return false;
    }
    uint16_t mask = m_policyAllowMask.load(std::memory_order_acquire);
    if (enabled) {
        mask |= bit;
    } else {
        mask &= static_cast<uint16_t>(~bit);
    }
    m_policyAllowMask.store(mask & kAllPoliciesMask, std::memory_order_release);
    return true;
}

bool SongAwareDirector::isPolicyAllowed(SongAwareState state) const {
    const uint16_t bit = policyBitForState(state);
    return bit != 0U &&
           ((m_policyAllowMask.load(std::memory_order_acquire) & bit) != 0U);
}

void SongAwareDirector::resetPolicyAllowlist() {
    m_policyAllowMask.store(kAllPoliciesMask, std::memory_order_release);
}

void SongAwareDirector::markManualControl(uint32_t nowMs) {
    m_manualSuppressUntilMs.store(nowMs + kManualSuppressMs, std::memory_order_release);
}

void SongAwareDirector::markShowControl(uint32_t nowMs) {
    m_showSuppressUntilMs.store(nowMs + kShowSuppressMs, std::memory_order_release);
}

bool SongAwareDirector::isShowOwnerActive(uint32_t nowMs) const {
    return deadlineActive(nowMs, m_showSuppressUntilMs.load(std::memory_order_acquire));
}

#if FEATURE_AUDIO_SYNC
bool SongAwareDirector::evaluateDirector(const audio::ControlBusFrame& frame,
                                         const audio::MusicalGridSnapshot& grid,
                                         bool audioAvailable,
                                         uint32_t nowMs,
                                         uint16_t activeEffectId,
                                         const SongAwareDirectorContext& context,
                                         SongAwareSwitchRequest& request) {
    request = SongAwareSwitchRequest{};
    const SongAwareFeatureSnapshot features = buildFeatureSnapshot(frame, grid, audioAvailable, 0.0f);

    m_activeEffectId.store(activeEffectId, std::memory_order_release);
    m_showSkips.store(context.health.showSkips, std::memory_order_release);
    m_failures.store(context.health.failures, std::memory_order_release);
    m_rmtErrors.store(context.health.rmtErrors, std::memory_order_release);
    m_underruns.store(context.health.underruns, std::memory_order_release);
    updateAudioSummary(frame, audioAvailable);
    updateRemainingGates(nowMs);
    updateTransitionTelemetry(nowMs);
    updateHealthTracking(context.health, nowMs);
    m_lastDecisionAtMs.store(nowMs, std::memory_order_release);

    const bool enabled = m_enabled.load(std::memory_order_acquire);
    const SongAwareMode mode = static_cast<SongAwareMode>(m_mode.load(std::memory_order_acquire));
    if (!enabled || mode == SongAwareMode::Off) {
        m_effectiveMode.store(static_cast<uint8_t>(SongAwareMode::Off), std::memory_order_release);
        m_currentSongState.store(static_cast<uint8_t>(SongAwareState::Silence), std::memory_order_release);
        m_selectedPolicyIndex.store(0, std::memory_order_release);
        m_selectedEffectId.store(INVALID_EFFECT_ID, std::memory_order_release);
        updateIntentTelemetry(SongAwareState::Silence, SongAwareActionPlan::None, features);
        setSuppressed(SongAwareSuppressedReason::Disabled, SongAwareOwner::None);
        return false;
    }

    m_effectiveMode.store(static_cast<uint8_t>(mode), std::memory_order_release);
    SongAwareSuppressedReason graceReason = SongAwareSuppressedReason::None;
    if (graceSuppresses(nowMs, graceReason)) {
        setSuppressed(graceReason, SongAwareOwner::None);
        m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::SwitchSuppressed), std::memory_order_release);
        return false;
    }
    if (m_familyMorphing.load(std::memory_order_acquire)) {
        setSuppressed(SongAwareSuppressedReason::UnsupportedMode, SongAwareOwner::Director);
        m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::SwitchSuppressed), std::memory_order_release);
        return false;
    }
    if (mode != SongAwareMode::Director) {
        updateIntentTelemetry(static_cast<SongAwareState>(m_currentSongState.load(std::memory_order_acquire)),
                              SongAwareActionPlan::ParameterModulation,
                              features);
        setSuppressed(SongAwareSuppressedReason::SwitchingDisabled, SongAwareOwner::Director);
        return false;
    }

    const uint32_t lastEval = m_lastEvaluationAtMs.load(std::memory_order_acquire);
    if (lastEval != 0 && nowMs - lastEval < kEvaluationPeriodMs) {
        return false;
    }
    m_lastEvaluationAtMs.store(nowMs, std::memory_order_release);

    if (!audioAvailable) {
        m_currentSongState.store(static_cast<uint8_t>(SongAwareState::Silence), std::memory_order_release);
        setSuppressed(SongAwareSuppressedReason::NoAudio, SongAwareOwner::None);
        m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::SwitchSuppressed), std::memory_order_release);
        return false;
    }

    const float confidence = clamp01(frame.audioConfidence);
    const float confidenceFloor = unscaleFloat(m_confidenceFloorQ1000.load(std::memory_order_acquire));
    m_confidenceQ1000.store(scaleFloat(confidence), std::memory_order_release);

    const SongAwareState rawState = classifyState(frame, features, audioAvailable, confidence);
    const SongAwareState previousStable =
        static_cast<SongAwareState>(m_currentSongState.load(std::memory_order_acquire));
    const SongAwareState stableState = updateStableState(rawState, confidence, nowMs);
    updateIntentTelemetry(stableState,
                          resolveActionPlan(mode,
                                            static_cast<SongAwareProfile>(m_profile.load(std::memory_order_acquire)),
                                            stableState,
                                            true),
                          features);

    uint8_t policyIndex = 0;
    const DirectorPolicy& policy = policyForState(stableState, policyIndex);
    const SongAwareSelectionSnapshot selection = resolveSelection(stableState, confidence, activeEffectId);
    m_selectedPolicyIndex.store(policyIndex, std::memory_order_release);
    m_selectedEffectId.store(policy.effectId, std::memory_order_release);
    m_lastSwitchReason.store(static_cast<uint8_t>(policy.reason), std::memory_order_release);
    m_selectionScoreQ1000.store(scaleFloat(selection.score), std::memory_order_release);

    if (!selection.policy.enabled) {
        setSuppressed(SongAwareSuppressedReason::AllowlistDisabled, SongAwareOwner::Director);
        m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::SwitchSuppressed), std::memory_order_release);
        return false;
    }

    if (!transitionIsAllowed(previousStable, stableState, features)) {
        setSuppressed(SongAwareSuppressedReason::ImpossibleTransition, SongAwareOwner::Director);
        m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::SwitchSuppressed), std::memory_order_release);
        return false;
    }

    if (confidence < confidenceFloor || stableState == SongAwareState::Silence ||
        stableState == SongAwareState::Unknown || confidence < policy.minConfidence) {
        setSuppressed(SongAwareSuppressedReason::LowConfidence, SongAwareOwner::None);
        m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::SwitchSuppressed), std::memory_order_release);
        return false;
    }

    if (m_candidateHoldRemainingMs.load(std::memory_order_acquire) > 0) {
        setSuppressed(SongAwareSuppressedReason::CandidateUnstable, SongAwareOwner::Director);
        m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::SwitchSuppressed), std::memory_order_release);
        return false;
    }

    if (deadlineActive(nowMs, m_showSuppressUntilMs.load(std::memory_order_acquire))) {
        setSuppressed(SongAwareSuppressedReason::ShowOwner, SongAwareOwner::Show);
        m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::SwitchSuppressed), std::memory_order_release);
        return false;
    }
    if (deadlineActive(nowMs, m_manualSuppressUntilMs.load(std::memory_order_acquire))) {
        setSuppressed(SongAwareSuppressedReason::ManualOwner, SongAwareOwner::Manual);
        m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::SwitchSuppressed), std::memory_order_release);
        return false;
    }

    if (!m_switchingEnabled.load(std::memory_order_acquire)) {
        setSuppressed(SongAwareSuppressedReason::SwitchingDisabled, SongAwareOwner::Director);
        m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::SwitchSuppressed), std::memory_order_release);
        return false;
    }

    if (healthIsDegraded(context.health)) {
        setSuppressed(SongAwareSuppressedReason::Health, SongAwareOwner::Director);
        m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::SwitchSuppressed), std::memory_order_release);
        return false;
    }
    if (m_healthCleanWindowRemainingMs.load(std::memory_order_acquire) > 0) {
        setSuppressed(SongAwareSuppressedReason::HealthRecovering, SongAwareOwner::Director);
        m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::SwitchSuppressed), std::memory_order_release);
        return false;
    }

    if (policy.effectId == INVALID_EFFECT_ID) {
        setSuppressed(SongAwareSuppressedReason::TargetUnavailable, SongAwareOwner::Director);
        m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::SwitchSuppressed), std::memory_order_release);
        return false;
    }

    if (m_transitionActive.load(std::memory_order_acquire)) {
        setSuppressed(SongAwareSuppressedReason::TransitionActive, SongAwareOwner::Director);
        m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::SwitchSuppressed), std::memory_order_release);
        return false;
    }

    if (policy.effectId == activeEffectId) {
        setSuppressed(SongAwareSuppressedReason::SameEffect, SongAwareOwner::Director);
        m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::None), std::memory_order_release);
        return false;
    }

    if (wouldCreateAbaSwitch(activeEffectId, static_cast<uint16_t>(policy.effectId), nowMs)) {
        setSuppressed(SongAwareSuppressedReason::AntiThrash, SongAwareOwner::Director);
        m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::SwitchSuppressed), std::memory_order_release);
        return false;
    }

    const uint32_t stateEntered = m_stateEnteredAtMs.load(std::memory_order_acquire);
    if (stateEntered != 0 && nowMs - stateEntered < kMinimumDwellMs) {
        m_dwellRemainingMs.store(kMinimumDwellMs - (nowMs - stateEntered), std::memory_order_release);
        setSuppressed(SongAwareSuppressedReason::Dwell, SongAwareOwner::Director);
        m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::SwitchSuppressed), std::memory_order_release);
        return false;
    }

    const uint32_t lastSwitch = m_lastSwitchAtMs.load(std::memory_order_acquire);
    if (lastSwitch != 0 && nowMs - lastSwitch < kSwitchCooldownMs) {
        m_cooldownRemainingMs.store(kSwitchCooldownMs - (nowMs - lastSwitch), std::memory_order_release);
        setSuppressed(SongAwareSuppressedReason::Cooldown, SongAwareOwner::Director);
        m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::SwitchSuppressed), std::memory_order_release);
        return false;
    }

    uint32_t windowStart = m_switchWindowStartMs.load(std::memory_order_acquire);
    uint8_t switchesInWindow = m_switchesInWindow.load(std::memory_order_acquire);
    if (windowStart == 0 || nowMs - windowStart >= kSwitchWindowMs) {
        windowStart = nowMs;
        switchesInWindow = 0;
        m_switchWindowStartMs.store(windowStart, std::memory_order_release);
        m_switchesInWindow.store(switchesInWindow, std::memory_order_release);
    }
    if (switchesInWindow >= kMaxSwitchesPerWindow) {
        setSuppressed(SongAwareSuppressedReason::RateLimit, SongAwareOwner::Director);
        m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::SwitchSuppressed), std::memory_order_release);
        return false;
    }

    if (!features.boundaryReady) {
        setSuppressed(SongAwareSuppressedReason::BoundaryDeferred, SongAwareOwner::Director);
        m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::SwitchSuppressed), std::memory_order_release);
        return false;
    }

    request.requested = true;
    request.targetEffectId = policy.effectId;
    request.targetFamily = policy.family;
    request.targetVisualLanguage = policy.visualLanguage;
    request.reason = songAwareSwitchReasonName(policy.reason);
    m_dwellRemainingMs.store(0, std::memory_order_release);
    m_cooldownRemainingMs.store(0, std::memory_order_release);
    m_actionPlan.store(static_cast<uint8_t>(SongAwareActionPlan::EffectSwitch), std::memory_order_release);
    m_owner.store(static_cast<uint8_t>(SongAwareOwner::Director), std::memory_order_release);
    m_suppressedReason.store(static_cast<uint8_t>(SongAwareSuppressedReason::None), std::memory_order_release);
    return true;
}

void SongAwareDirector::notifySwitchApplied(uint16_t previousEffectId,
                                            uint16_t targetEffectId,
                                            uint32_t nowMs,
                                            const char* activeEffectName) {
    (void)activeEffectName;
    m_previousEffectId.store(previousEffectId, std::memory_order_release);
    m_activeEffectId.store(targetEffectId, std::memory_order_release);
    m_selectedEffectId.store(targetEffectId, std::memory_order_release);
    m_lastSwitchAtMs.store(nowMs, std::memory_order_release);
    m_lastDecisionAtMs.store(nowMs, std::memory_order_release);
    m_cooldownRemainingMs.store(kSwitchCooldownMs, std::memory_order_release);
    m_automaticEffectSwitches.fetch_add(1, std::memory_order_acq_rel);
    m_lastSwitchFromEffectId.store(previousEffectId, std::memory_order_release);
    m_lastSwitchToEffectId.store(targetEffectId, std::memory_order_release);
    m_antiThrashUntilMs.store(nowMs + kAntiThrashWindowMs, std::memory_order_release);

    uint32_t windowStart = m_switchWindowStartMs.load(std::memory_order_acquire);
    if (windowStart == 0 || nowMs - windowStart >= kSwitchWindowMs) {
        windowStart = nowMs;
        m_switchWindowStartMs.store(windowStart, std::memory_order_release);
        m_switchesInWindow.store(1, std::memory_order_release);
    } else {
        const uint8_t count = m_switchesInWindow.load(std::memory_order_acquire);
        if (count < 255U) {
            m_switchesInWindow.store(static_cast<uint8_t>(count + 1U), std::memory_order_release);
        }
    }

    m_owner.store(static_cast<uint8_t>(SongAwareOwner::Director), std::memory_order_release);
    m_suppressedReason.store(static_cast<uint8_t>(SongAwareSuppressedReason::None), std::memory_order_release);
    m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::EffectSwitch), std::memory_order_release);
    m_actionPlan.store(static_cast<uint8_t>(SongAwareActionPlan::EffectSwitch), std::memory_order_release);
}

void SongAwareDirector::notifySwitchRejected(uint16_t targetEffectId,
                                             uint32_t nowMs,
                                             SongAwareSuppressedReason reason) {
    m_selectedEffectId.store(targetEffectId, std::memory_order_release);
    m_lastDecisionAtMs.store(nowMs, std::memory_order_release);
    setSuppressed(reason, SongAwareOwner::Director);
    m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::SwitchSuppressed), std::memory_order_release);
}

void SongAwareDirector::notifyTransitionStarted(uint16_t previousEffectId,
                                                uint16_t targetEffectId,
                                                uint32_t nowMs,
                                                uint32_t durationMs) {
    m_transitionActive.store(durationMs > 0, std::memory_order_release);
    m_transitionPreviousEffectId.store(previousEffectId, std::memory_order_release);
    m_transitionTargetEffectId.store(targetEffectId, std::memory_order_release);
    m_transitionStartedAtMs.store(nowMs, std::memory_order_release);
    m_transitionDurationMs.store(durationMs, std::memory_order_release);
    m_transitionRemainingMs.store(durationMs, std::memory_order_release);
    m_transitionProgressQ1000.store(durationMs > 0 ? 0 : 1000, std::memory_order_release);
}

void SongAwareDirector::notifyTransitionCompleted(uint32_t nowMs) {
    (void)nowMs;
    m_transitionActive.store(false, std::memory_order_release);
    m_transitionRemainingMs.store(0, std::memory_order_release);
    m_transitionProgressQ1000.store(1000, std::memory_order_release);
}

bool SongAwareDirector::apply(const audio::ControlBusFrame& frame,
                              const audio::MusicalGridSnapshot& grid,
                              bool audioAvailable,
                              float dtSeconds,
                              uint32_t nowMs,
                              SongAwareParams& params) {
    const bool enabled = m_enabled.load(std::memory_order_acquire);
    const SongAwareMode mode = static_cast<SongAwareMode>(m_mode.load(std::memory_order_acquire));
    const SongAwareProfile profile =
        static_cast<SongAwareProfile>(m_profile.load(std::memory_order_acquire));
    const SongAwareFeatureSnapshot features = buildFeatureSnapshot(frame, grid, audioAvailable, dtSeconds);

    m_activeEffectId.store(params.effectId, std::memory_order_release);
    updateAudioSummary(frame, audioAvailable);
    updateRemainingGates(nowMs);
    updateTransitionTelemetry(nowMs);
    m_lastDecisionAtMs.store(nowMs, std::memory_order_release);

    if (!enabled || mode == SongAwareMode::Off) {
        m_effectiveMode.store(static_cast<uint8_t>(SongAwareMode::Off), std::memory_order_release);
        m_currentSongState.store(static_cast<uint8_t>(SongAwareState::Silence), std::memory_order_release);
        m_driveQ1000.store(0, std::memory_order_release);
        updateIntentTelemetry(SongAwareState::Silence, SongAwareActionPlan::None, features);
        setSuppressed(SongAwareSuppressedReason::Disabled, SongAwareOwner::None);
        m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::None), std::memory_order_release);
        return false;
    }

    m_effectiveMode.store(static_cast<uint8_t>(mode), std::memory_order_release);
    SongAwareSuppressedReason graceReason = SongAwareSuppressedReason::None;
    if (graceSuppresses(nowMs, graceReason)) {
        setSuppressed(graceReason, SongAwareOwner::None);
        m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::None), std::memory_order_release);
        return false;
    }
    if (m_familyMorphing.load(std::memory_order_acquire)) {
        setSuppressed(SongAwareSuppressedReason::UnsupportedMode, SongAwareOwner::Director);
        m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::None), std::memory_order_release);
        return false;
    }

    if (!audioAvailable) {
        setSuppressed(SongAwareSuppressedReason::NoAudio, SongAwareOwner::None);
        return false;
    }

    const float confidence = clamp01(frame.audioConfidence);
    const float confidenceFloor = unscaleFloat(m_confidenceFloorQ1000.load(std::memory_order_acquire));
    m_confidenceQ1000.store(scaleFloat(confidence), std::memory_order_release);
    if (confidence < confidenceFloor) {
        setSuppressed(SongAwareSuppressedReason::LowConfidence, SongAwareOwner::None);
        return false;
    }

    const SongAwareState rawState = classifyState(frame, features, audioAvailable, confidence);
    const SongAwareState state = updateStableState(rawState, confidence, nowMs);
    const SongAwareActionPlan plannedAction = resolveActionPlan(mode, profile, state, false);
    updateIntentTelemetry(state, plannedAction, features);
    m_selectionScoreQ1000.store(scaleFloat(resolveSelection(state, confidence, params.effectId).score),
                                std::memory_order_release);
    if (state == SongAwareState::Silence || state == SongAwareState::Unknown) {
        setSuppressed(SongAwareSuppressedReason::LowConfidence, SongAwareOwner::None);
        return false;
    }

    const float sensitivity = unscaleFloat(m_sensitivityQ1000.load(std::memory_order_acquire));
    float intensityScalar = unscaleFloat(m_intensityScalarQ1000.load(std::memory_order_acquire));
    float motionScalar = unscaleFloat(m_motionScalarQ1000.load(std::memory_order_acquire));
    float complexityScalar = intensityScalar;
    float responseScalar = 1.0f;
    if (profile == SongAwareProfile::Subtle) {
        intensityScalar *= 0.55f;
        motionScalar *= 0.55f;
        complexityScalar *= 0.60f;
        responseScalar = 0.75f;
    } else if (profile == SongAwareProfile::High) {
        intensityScalar *= 1.35f;
        motionScalar *= 1.25f;
        complexityScalar *= 1.20f;
        responseScalar = 1.20f;
    }
    const float saliency = features.saliency;
    const float rawDrive = clamp01((features.energy * 0.30f + features.flux * 0.25f +
                                   features.liveliness * 0.30f + saliency * 0.15f) *
                                  sensitivity);
    const float safeDt = (dtSeconds > 0.0001f && dtSeconds < 0.100f) ? dtSeconds : (1.0f / 120.0f);
    float alpha = (1.0f - expf(-safeDt / kSmoothTauSeconds)) * responseScalar;
    if (alpha < 0.02f) alpha = 0.02f;
    if (alpha > 0.95f) alpha = 0.95f;

    const float prevDrive = unscaleFloat(m_driveQ1000.load(std::memory_order_acquire));
    const float drive = clamp01(prevDrive + alpha * (rawDrive - prevDrive));
    m_driveQ1000.store(scaleFloat(drive), std::memory_order_release);

    SongAwareParams before = params;
    int speedDelta = static_cast<int>(lroundf(drive * motionScalar * 18.0f));
    int intensityDelta = static_cast<int>(lroundf(drive * intensityScalar * 48.0f));
    int complexityDelta = static_cast<int>(lroundf(clamp01((drive + saliency) * 0.5f) *
                                                   complexityScalar * 42.0f));
    int saturationDelta = 0;
    int variationDelta = 0;
    int hueDelta = 0;

    if (mode == SongAwareMode::Director || profile != SongAwareProfile::Subtle) {
        if (state == SongAwareState::Ambient || state == SongAwareState::Breakdown) {
            speedDelta = static_cast<int>(lroundf(drive * motionScalar * 8.0f));
            complexityDelta = static_cast<int>(lroundf(clamp01(drive) * complexityScalar * 18.0f));
            saturationDelta = (profile == SongAwareProfile::Subtle) ? 0 : -6;
        } else if (state == SongAwareState::Build) {
            speedDelta += (profile == SongAwareProfile::High) ? 12 : 8;
            intensityDelta += (profile == SongAwareProfile::High) ? 22 : 16;
            saturationDelta = (profile == SongAwareProfile::High) ? 8 : 4;
            hueDelta = (profile == SongAwareProfile::High) ? 4 : 2;
        } else if (state == SongAwareState::Drop) {
            const bool beatConfirmed = features.boundaryConfidence >= 0.45f;
            intensityDelta += beatConfirmed ? ((profile == SongAwareProfile::High) ? 42 : 32)
                                            : ((profile == SongAwareProfile::High) ? 20 : 12);
            variationDelta = beatConfirmed ? ((profile == SongAwareProfile::High) ? 3 : 1) : 0;
            hueDelta = beatConfirmed ? ((profile == SongAwareProfile::High) ? 8 : 5) : 2;
        } else if (state == SongAwareState::Dense) {
            complexityDelta = static_cast<int>(lroundf(clamp01((drive + saliency) * 0.5f) *
                                                       complexityScalar * 28.0f));
            intensityDelta += 10;
            saturationDelta = (profile == SongAwareProfile::High) ? -10 : -4;
            hueDelta = (profile == SongAwareProfile::High) ? -3 : 0;
        } else if (state == SongAwareState::Transition) {
            hueDelta = (profile == SongAwareProfile::High) ? 6 : 3;
        }
    }

    params.speed = clampU8(static_cast<int>(params.speed) + speedDelta, 1, 100);
    params.intensity = clampU8(static_cast<int>(params.intensity) + intensityDelta, 0, 255);
    params.complexity = clampU8(static_cast<int>(params.complexity) + complexityDelta, 0, 255);
    params.saturation = clampU8(static_cast<int>(params.saturation) + saturationDelta, 0, 255);
    params.variation = clampU8(static_cast<int>(params.variation) + variationDelta, 0, 255);
    params.hue = clampU8(static_cast<int>(params.hue) + hueDelta, 0, 255);

    const bool changed =
        params.speed != before.speed ||
        params.intensity != before.intensity ||
        params.complexity != before.complexity ||
        params.saturation != before.saturation ||
        params.variation != before.variation ||
        params.hue != before.hue;

    if (changed) {
        m_parameterUpdates.fetch_add(1, std::memory_order_acq_rel);
        const uint32_t lastSwitch = m_lastSwitchAtMs.load(std::memory_order_acquire);
        const SongAwareLastAction lastAction =
            static_cast<SongAwareLastAction>(m_lastAction.load(std::memory_order_acquire));
        if (!(lastAction == SongAwareLastAction::EffectSwitch && lastSwitch != 0 &&
              nowMs - lastSwitch < 2000U)) {
            m_lastAction.store(static_cast<uint8_t>(SongAwareLastAction::ParameterUpdate), std::memory_order_release);
        }
    }

    if (static_cast<SongAwareSuppressedReason>(m_suppressedReason.load(std::memory_order_acquire)) ==
        SongAwareSuppressedReason::None) {
        m_owner.store(static_cast<uint8_t>(SongAwareOwner::Director), std::memory_order_release);
    }
    return changed;
}

SongAwareDirector::SongAwareFeatureSnapshot
SongAwareDirector::buildFeatureSnapshot(const audio::ControlBusFrame& frame,
                                        const audio::MusicalGridSnapshot& grid,
                                        bool audioAvailable,
                                        float dtSeconds) {
    SongAwareFeatureSnapshot features;
    if (!audioAvailable) {
        m_boundaryGate.store(static_cast<uint8_t>(SongAwareBoundaryGate::WaitingForBoundary),
                             std::memory_order_release);
        m_boundaryReady.store(false, std::memory_order_release);
        m_waitingForBoundary.store(true, std::memory_order_release);
        m_boundaryConfidenceQ1000.store(0, std::memory_order_release);
        return features;
    }

    features.energy = clamp01((frame.fast_rms > frame.rms) ? frame.fast_rms : frame.rms);
    features.slowEnergy = clamp01(frame.rms);
    features.fastSlowRatio = (features.slowEnergy > 0.01f)
                                 ? clamp01(features.energy / features.slowEnergy)
                                 : 1.0f;
    features.flux = clamp01((frame.fast_flux > frame.flux) ? frame.fast_flux : frame.flux);
    features.onsetStrength = clamp01((frame.onsetEvent > features.flux) ? frame.onsetEvent : features.flux);
    features.beatStrength = clamp01((frame.es_beat_strength > frame.tempoBeatStrength)
                                        ? frame.es_beat_strength
                                        : frame.tempoBeatStrength);
    if (grid.beat_strength > features.beatStrength) {
        features.beatStrength = clamp01(grid.beat_strength);
    }
    features.tempoConfidence = clamp01((frame.es_tempo_confidence > frame.tempoConfidence)
                                           ? frame.es_tempo_confidence
                                           : frame.tempoConfidence);
    if (grid.tempo_confidence > features.tempoConfidence) {
        features.tempoConfidence = clamp01(grid.tempo_confidence);
    }
    features.liveliness = clamp01(frame.liveliness);
    features.saliency = clamp01(frame.saliency.overallSaliency);

    const float safeDt = (dtSeconds > 0.0001f && dtSeconds < 0.100f) ? dtSeconds : (1.0f / 120.0f);
    const float alpha = 1.0f - expf(-safeDt / 1.50f);
    const float previousSlow = unscaleFloat(m_slowEnergyQ1000.load(std::memory_order_acquire));
    const float slow = clamp01(previousSlow + alpha * (features.energy - previousSlow));
    m_slowEnergyQ1000.store(scaleFloat(slow), std::memory_order_release);
    features.adaptiveFloor = clamp01(0.025f + slow * 0.22f);
    m_adaptiveFloorQ1000.store(scaleFloat(features.adaptiveFloor), std::memory_order_release);

    if (grid.downbeat_tick && features.tempoConfidence >= 0.35f && features.beatStrength >= 0.25f) {
        features.boundaryReady = true;
        features.boundaryGate = SongAwareBoundaryGate::DownbeatBoundary;
        features.boundaryConfidence = clamp01((features.tempoConfidence + features.beatStrength) * 0.5f);
    } else if (grid.beat_tick && (features.tempoConfidence >= 0.45f || features.beatStrength >= 0.55f)) {
        features.boundaryReady = true;
        features.boundaryGate = SongAwareBoundaryGate::BeatBoundary;
        features.boundaryConfidence = clamp01((features.tempoConfidence * 0.45f) +
                                              (features.beatStrength * 0.55f));
    } else {
        // Conservative fallback when the grid has phase but no tick on this frame.
        const bool nearBeat = grid.beat_phase01 <= 0.06f || grid.beat_phase01 >= 0.94f;
        if (nearBeat && features.tempoConfidence >= 0.60f && features.beatStrength >= 0.40f) {
            features.boundaryReady = true;
            features.boundaryGate = SongAwareBoundaryGate::PhaseFallback;
            features.boundaryConfidence = clamp01((features.tempoConfidence * 0.60f) +
                                                  (features.beatStrength * 0.40f));
        } else {
            features.boundaryReady = false;
            features.boundaryGate = SongAwareBoundaryGate::WaitingForBoundary;
            features.boundaryConfidence = 0.0f;
        }
    }

    m_boundaryGate.store(static_cast<uint8_t>(features.boundaryGate), std::memory_order_release);
    m_boundaryReady.store(features.boundaryReady, std::memory_order_release);
    m_waitingForBoundary.store(!features.boundaryReady, std::memory_order_release);
    m_boundaryConfidenceQ1000.store(scaleFloat(features.boundaryConfidence), std::memory_order_release);
    return features;
}

SongAwareState SongAwareDirector::classifyState(const audio::ControlBusFrame& frame,
                                                const SongAwareFeatureSnapshot& features,
                                                bool audioAvailable,
                                                float confidence) {
    SongAwareState state = SongAwareState::Steady;
    SongAwareClassificationReason reason = SongAwareClassificationReason::SteadyDefault;

    const SongAwareState stable =
        static_cast<SongAwareState>(m_currentSongState.load(std::memory_order_acquire));
    const float silenceEnter = (stable == SongAwareState::Silence) ? 0.10f : 0.06f;
    if (!audioAvailable || frame.isSilent || frame.silentScale < 0.08f ||
        confidence < 0.05f ||
        (features.energy < (features.adaptiveFloor + silenceEnter) &&
         features.flux < 0.05f && features.beatStrength < 0.20f)) {
        state = SongAwareState::Silence;
        if (!audioAvailable) {
            reason = SongAwareClassificationReason::NoAudio;
        } else if (confidence < 0.05f) {
            reason = SongAwareClassificationReason::LowConfidence;
        } else {
            reason = SongAwareClassificationReason::SilentFrame;
        }
        m_rawSongState.store(static_cast<uint8_t>(state), std::memory_order_release);
        m_classificationReason.store(static_cast<uint8_t>(reason), std::memory_order_release);
        return state;
    }

    const float rms = features.energy;
    const float flux = features.flux;
    const float liveliness = features.liveliness;
    const float saliency = features.saliency;
    const float beatStrength = features.beatStrength;
    const float tempoConfidence = features.tempoConfidence;
    const float dropEnter = (stable == SongAwareState::Drop) ? 0.52f : 0.62f;
    const float buildEnter = (stable == SongAwareState::Build) ? 0.32f : 0.38f;
    const float denseEnter = (stable == SongAwareState::Dense) ? 0.60f : 0.68f;

    if ((frame.onsetEvent > dropEnter || (beatStrength > 0.78f && features.onsetStrength > 0.45f)) &&
        (rms > 0.34f || liveliness > 0.52f || saliency > 0.55f)) {
        state = SongAwareState::Drop;
        reason = SongAwareClassificationReason::DropOnset;
    } else if ((saliency > 0.70f || flux > 0.72f) && rms < 0.46f) {
        state = SongAwareState::Transition;
        reason = SongAwareClassificationReason::SpectralTransition;
    } else if (rms < 0.24f && liveliness < 0.36f && tempoConfidence > 0.35f) {
        state = SongAwareState::Breakdown;
        reason = SongAwareClassificationReason::QuietBreakdown;
    } else if (rms > denseEnter || (liveliness > 0.72f && flux > 0.42f)) {
        state = SongAwareState::Dense;
        reason = SongAwareClassificationReason::DenseEnergy;
    } else if (flux > buildEnter || liveliness > 0.48f || saliency > 0.48f) {
        state = SongAwareState::Build;
        reason = SongAwareClassificationReason::BuildEnergy;
    } else if (rms < 0.20f || liveliness < 0.22f) {
        state = SongAwareState::Ambient;
        reason = SongAwareClassificationReason::AmbientLowEnergy;
    }

    m_rawSongState.store(static_cast<uint8_t>(state), std::memory_order_release);
    m_classificationReason.store(static_cast<uint8_t>(reason), std::memory_order_release);
    return state;
}

SongAwareState SongAwareDirector::updateStableState(SongAwareState rawState,
                                                    float confidence,
                                                    uint32_t nowMs) {
    SongAwareState stable =
        static_cast<SongAwareState>(m_currentSongState.load(std::memory_order_acquire));
    if (stable == SongAwareState::Unknown) {
        stable = rawState;
        m_currentSongState.store(static_cast<uint8_t>(stable), std::memory_order_release);
        m_stateEnteredAtMs.store(nowMs, std::memory_order_release);
        m_candidateState.store(static_cast<uint8_t>(rawState), std::memory_order_release);
        m_candidateSinceMs.store(nowMs, std::memory_order_release);
        m_stateAgeMs.store(0, std::memory_order_release);
        m_candidateAgeMs.store(0, std::memory_order_release);
        m_candidateHoldRemainingMs.store(0, std::memory_order_release);
        return stable;
    }

    if (rawState == stable) {
        m_candidateState.store(static_cast<uint8_t>(rawState), std::memory_order_release);
        m_candidateSinceMs.store(nowMs, std::memory_order_release);
        m_stateAgeMs.store(elapsedSince(nowMs, m_stateEnteredAtMs.load(std::memory_order_acquire)),
                           std::memory_order_release);
        m_candidateAgeMs.store(0, std::memory_order_release);
        m_candidateHoldRemainingMs.store(0, std::memory_order_release);
        return stable;
    }

    SongAwareState candidate =
        static_cast<SongAwareState>(m_candidateState.load(std::memory_order_acquire));
    uint32_t candidateSince = m_candidateSinceMs.load(std::memory_order_acquire);
    if (rawState != candidate) {
        m_candidateState.store(static_cast<uint8_t>(rawState), std::memory_order_release);
        m_candidateSinceMs.store(nowMs, std::memory_order_release);
        m_stateAgeMs.store(elapsedSince(nowMs, m_stateEnteredAtMs.load(std::memory_order_acquire)),
                           std::memory_order_release);
        m_candidateAgeMs.store(0, std::memory_order_release);
        m_candidateHoldRemainingMs.store(kStableStateHoldMs, std::memory_order_release);
        return stable;
    }

    const uint32_t holdMs = (rawState == SongAwareState::Drop && confidence > 0.60f)
                                ? kDropStateHoldMs
                                : kStableStateHoldMs;
    const uint32_t candidateAgeMs = elapsedSince(nowMs, candidateSince);
    m_stateAgeMs.store(elapsedSince(nowMs, m_stateEnteredAtMs.load(std::memory_order_acquire)),
                       std::memory_order_release);
    m_candidateAgeMs.store(candidateAgeMs, std::memory_order_release);
    if (candidateSince == 0 || nowMs - candidateSince < holdMs) {
        m_candidateHoldRemainingMs.store((candidateSince == 0) ? holdMs : (holdMs - (nowMs - candidateSince)),
                                         std::memory_order_release);
        return stable;
    }

    m_previousSongState.store(static_cast<uint8_t>(stable), std::memory_order_release);
    stable = rawState;
    m_currentSongState.store(static_cast<uint8_t>(stable), std::memory_order_release);
    m_stateEnteredAtMs.store(nowMs, std::memory_order_release);
    m_stateAgeMs.store(0, std::memory_order_release);
    m_candidateAgeMs.store(candidateAgeMs, std::memory_order_release);
    m_candidateHoldRemainingMs.store(0, std::memory_order_release);
    return stable;
}

void SongAwareDirector::updateAudioSummary(const audio::ControlBusFrame& frame, bool audioAvailable) {
    if (!audioAvailable) {
        m_rmsQ1000.store(0, std::memory_order_release);
        m_fluxQ1000.store(0, std::memory_order_release);
        m_bpmQ10.store(0, std::memory_order_release);
        m_audioConfidenceQ1000.store(0, std::memory_order_release);
        return;
    }

    m_rmsQ1000.store(scaleFloat((frame.fast_rms > frame.rms) ? frame.fast_rms : frame.rms), std::memory_order_release);
    m_fluxQ1000.store(scaleFloat((frame.fast_flux > frame.flux) ? frame.fast_flux : frame.flux), std::memory_order_release);
    const float bpm = (frame.es_bpm > 1.0f) ? frame.es_bpm : frame.tempoBpm;
    const float bpmClamped = (bpm < 0.0f) ? 0.0f : ((bpm > 6553.5f) ? 6553.5f : bpm);
    m_bpmQ10.store(static_cast<uint16_t>(lroundf(bpmClamped * 10.0f)), std::memory_order_release);
    m_audioConfidenceQ1000.store(scaleFloat(frame.audioConfidence), std::memory_order_release);
}
#endif

SongAwareIntent SongAwareDirector::planIntent(SongAwareState state) const {
    switch (state) {
        case SongAwareState::Silence:
            return SongAwareIntent::QuietHold;
        case SongAwareState::Ambient:
            return SongAwareIntent::CalmHold;
        case SongAwareState::Steady:
            return SongAwareIntent::ReadableMotion;
        case SongAwareState::Build:
            return SongAwareIntent::BuildPressure;
        case SongAwareState::Drop:
            return SongAwareIntent::DropImpact;
        case SongAwareState::Breakdown:
            return SongAwareIntent::ReleaseSpace;
        case SongAwareState::Dense:
            return SongAwareIntent::LegibilityControl;
        case SongAwareState::Transition:
            return SongAwareIntent::TransitionBridge;
        case SongAwareState::Unknown:
        default:
            return SongAwareIntent::QuietHold;
    }
}

SongAwareActionPlan SongAwareDirector::resolveActionPlan(SongAwareMode mode,
                                                         SongAwareProfile profile,
                                                         SongAwareState state,
                                                         bool switching) const {
    if (mode == SongAwareMode::Off || state == SongAwareState::Unknown ||
        state == SongAwareState::Silence) {
        return SongAwareActionPlan::None;
    }
    if (switching && mode == SongAwareMode::Director) {
        return SongAwareActionPlan::EffectSwitch;
    }
    if (profile == SongAwareProfile::Subtle) {
        return SongAwareActionPlan::ParameterModulation;
    }
    if (profile == SongAwareProfile::Balanced) {
        if (state == SongAwareState::Build || state == SongAwareState::Drop ||
            state == SongAwareState::Transition) {
            return SongAwareActionPlan::ColourModifierShift;
        }
        return SongAwareActionPlan::PaletteShift;
    }
    if (state == SongAwareState::Build || state == SongAwareState::Drop ||
        state == SongAwareState::Dense || state == SongAwareState::Transition) {
        return SongAwareActionPlan::ZoneComposerAdjust;
    }
    return SongAwareActionPlan::EdgeMixerAdjust;
}

#if FEATURE_AUDIO_SYNC
bool SongAwareDirector::transitionIsAllowed(SongAwareState from,
                                            SongAwareState to,
                                            const SongAwareFeatureSnapshot& features) const {
    if (from == SongAwareState::Unknown || from == to) {
        return true;
    }
    if (from == SongAwareState::Silence && to == SongAwareState::Drop) {
        return features.energy > 0.45f &&
               features.onsetStrength > 0.70f &&
               features.beatStrength > 0.50f &&
               features.tempoConfidence > 0.35f;
    }
    return true;
}

void SongAwareDirector::updateIntentTelemetry(SongAwareState state,
                                              SongAwareActionPlan actionPlan,
                                              const SongAwareFeatureSnapshot& features) {
    m_intent.store(static_cast<uint8_t>(planIntent(state)), std::memory_order_release);
    m_actionPlan.store(static_cast<uint8_t>(actionPlan), std::memory_order_release);
    m_boundaryGate.store(static_cast<uint8_t>(features.boundaryGate), std::memory_order_release);
    m_boundaryReady.store(features.boundaryReady, std::memory_order_release);
    m_waitingForBoundary.store(!features.boundaryReady, std::memory_order_release);
    m_boundaryConfidenceQ1000.store(scaleFloat(features.boundaryConfidence), std::memory_order_release);
}
#endif

void SongAwareDirector::setSuppressed(SongAwareSuppressedReason reason, SongAwareOwner owner) {
    const uint8_t previous = m_suppressedReason.load(std::memory_order_acquire);
    m_previousSuppressedReason.store(previous, std::memory_order_release);
    m_suppressedReason.store(static_cast<uint8_t>(reason), std::memory_order_release);
    m_owner.store(static_cast<uint8_t>(owner), std::memory_order_release);
}

void SongAwareDirector::updateRemainingGates(uint32_t nowMs) {
    const uint32_t stateEntered = m_stateEnteredAtMs.load(std::memory_order_acquire);
    if (stateEntered != 0 && nowMs - stateEntered < kMinimumDwellMs) {
        m_dwellRemainingMs.store(kMinimumDwellMs - (nowMs - stateEntered), std::memory_order_release);
    } else {
        m_dwellRemainingMs.store(0, std::memory_order_release);
    }

    const uint32_t lastSwitch = m_lastSwitchAtMs.load(std::memory_order_acquire);
    if (lastSwitch != 0 && nowMs - lastSwitch < kSwitchCooldownMs) {
        m_cooldownRemainingMs.store(kSwitchCooldownMs - (nowMs - lastSwitch), std::memory_order_release);
    } else {
        m_cooldownRemainingMs.store(0, std::memory_order_release);
    }

    const uint32_t windowStart = m_switchWindowStartMs.load(std::memory_order_acquire);
    if (windowStart != 0 && nowMs - windowStart < kSwitchWindowMs) {
        m_switchWindowRemainingMs.store(kSwitchWindowMs - (nowMs - windowStart), std::memory_order_release);
    } else {
        m_switchWindowRemainingMs.store(0, std::memory_order_release);
    }
}

void SongAwareDirector::updateTransitionTelemetry(uint32_t nowMs) {
    if (!m_transitionActive.load(std::memory_order_acquire)) {
        return;
    }

    const uint32_t startedAt = m_transitionStartedAtMs.load(std::memory_order_acquire);
    const uint32_t duration = m_transitionDurationMs.load(std::memory_order_acquire);
    if (duration == 0 || startedAt == 0 || nowMs - startedAt >= duration) {
        notifyTransitionCompleted(nowMs);
        return;
    }

    const uint32_t elapsed = nowMs - startedAt;
    m_transitionRemainingMs.store(duration - elapsed, std::memory_order_release);
    const float progress = static_cast<float>(elapsed) / static_cast<float>(duration);
    m_transitionProgressQ1000.store(scaleFloat(progress), std::memory_order_release);
}

void SongAwareDirector::updateHealthTracking(SongAwareHealthCounters health, uint32_t nowMs) {
    const bool degraded = healthIsDegraded(health);
    m_healthDegraded.store(degraded, std::memory_order_release);
    if (degraded) {
        m_healthLastDegradedAtMs.store(nowMs, std::memory_order_release);
        m_healthCleanSinceMs.store(0, std::memory_order_release);
        m_healthCleanForMs.store(0, std::memory_order_release);
        m_healthCleanWindowRemainingMs.store(kHealthCleanWindowMs, std::memory_order_release);
        return;
    }

    uint32_t cleanSince = m_healthCleanSinceMs.load(std::memory_order_acquire);
    if (cleanSince == 0) {
        cleanSince = nowMs;
        m_healthCleanSinceMs.store(cleanSince, std::memory_order_release);
    }
    const uint32_t cleanForMs = elapsedSince(nowMs, cleanSince);
    m_healthCleanForMs.store(cleanForMs, std::memory_order_release);
    if (m_healthLastDegradedAtMs.load(std::memory_order_acquire) == 0) {
        m_healthCleanWindowRemainingMs.store(0, std::memory_order_release);
        return;
    }
    m_healthCleanWindowRemainingMs.store((cleanForMs < kHealthCleanWindowMs)
                                             ? (kHealthCleanWindowMs - cleanForMs)
                                             : 0,
                                         std::memory_order_release);
}

bool SongAwareDirector::graceSuppresses(uint32_t nowMs, SongAwareSuppressedReason& reason) {
    if (m_enableGracePending.load(std::memory_order_acquire)) {
        m_enableGraceUntilMs.store(nowMs + kPostEnableGraceMs, std::memory_order_release);
        m_enableGracePending.store(false, std::memory_order_release);
    }

    if (deadlineActive(nowMs, m_bootGraceUntilMs.load(std::memory_order_acquire))) {
        reason = SongAwareSuppressedReason::BootGrace;
        return true;
    }
    if (deadlineActive(nowMs, m_enableGraceUntilMs.load(std::memory_order_acquire))) {
        reason = SongAwareSuppressedReason::EnableGrace;
        return true;
    }
    reason = SongAwareSuppressedReason::None;
    return false;
}

bool SongAwareDirector::wouldCreateAbaSwitch(uint16_t activeEffectId,
                                             uint16_t targetEffectId,
                                             uint32_t nowMs) const {
    if (!deadlineActive(nowMs, m_antiThrashUntilMs.load(std::memory_order_acquire))) {
        return false;
    }
    const uint16_t lastFrom =
        static_cast<uint16_t>(m_lastSwitchFromEffectId.load(std::memory_order_acquire));
    const uint16_t lastTo =
        static_cast<uint16_t>(m_lastSwitchToEffectId.load(std::memory_order_acquire));
    return lastFrom != INVALID_EFFECT_ID &&
           lastTo != INVALID_EFFECT_ID &&
           activeEffectId == lastTo &&
           targetEffectId == lastFrom;
}

bool SongAwareDirector::healthIsDegraded(SongAwareHealthCounters health) const {
    return health.showSkips > 0 || health.failures > 0 ||
           health.rmtErrors > 0 || health.underruns > 0;
}

float SongAwareDirector::scorePolicy(SongAwareState state,
                                     float confidence,
                                     const SongAwarePolicySnapshot& policy) const {
    if (!policy.enabled || policy.effectId == INVALID_EFFECT_ID || state == SongAwareState::Unknown ||
        state == SongAwareState::Silence) {
        return 0.0f;
    }

    const float confidenceSpan = (policy.minConfidence < 0.99f) ? (1.0f - policy.minConfidence) : 1.0f;
    float score = clamp01((confidence - policy.minConfidence) / confidenceSpan);
    switch (state) {
        case SongAwareState::Drop:
            score += 0.25f;
            break;
        case SongAwareState::Build:
        case SongAwareState::Transition:
            score += 0.18f;
            break;
        case SongAwareState::Dense:
            score += 0.14f;
            break;
        case SongAwareState::Steady:
            score += 0.10f;
            break;
        case SongAwareState::Ambient:
        case SongAwareState::Breakdown:
            score += 0.06f;
            break;
        default:
            break;
    }
    return clamp01(score);
}

float SongAwareDirector::clamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

uint8_t SongAwareDirector::clampU8(int value, uint8_t minValue, uint8_t maxValue) {
    if (value < static_cast<int>(minValue)) return minValue;
    if (value > static_cast<int>(maxValue)) return maxValue;
    return static_cast<uint8_t>(value);
}

uint16_t SongAwareDirector::scaleFloat(float value) {
    return static_cast<uint16_t>(lroundf(clamp01(value) * 1000.0f));
}

float SongAwareDirector::unscaleFloat(uint16_t value) {
    if (value > 1000U) value = 1000U;
    return static_cast<float>(value) * 0.001f;
}

const char* songAwareModeName(SongAwareMode mode) {
    switch (mode) {
        case SongAwareMode::Off: return "off";
        case SongAwareMode::Assist: return "assist";
        case SongAwareMode::Director: return "director";
        default: return "unknown";
    }
}

const char* songAwareProfileName(SongAwareProfile profile) {
    switch (profile) {
        case SongAwareProfile::Subtle: return "subtle";
        case SongAwareProfile::Balanced: return "balanced";
        case SongAwareProfile::High: return "high";
        default: return "unknown";
    }
}

const char* songAwareOwnerName(SongAwareOwner owner) {
    switch (owner) {
        case SongAwareOwner::None: return "none";
        case SongAwareOwner::Director: return "director";
        case SongAwareOwner::Manual: return "manual";
        case SongAwareOwner::Show: return "show";
        default: return "unknown";
    }
}

const char* songAwareSuppressedReasonName(SongAwareSuppressedReason reason) {
    switch (reason) {
        case SongAwareSuppressedReason::None: return "none";
        case SongAwareSuppressedReason::Disabled: return "disabled";
        case SongAwareSuppressedReason::NoAudio: return "no_audio";
        case SongAwareSuppressedReason::LowConfidence: return "low_confidence";
        case SongAwareSuppressedReason::UnsupportedMode: return "unsupported_mode";
        case SongAwareSuppressedReason::ManualOwner: return "manual_owner";
        case SongAwareSuppressedReason::ShowOwner: return "show_owner";
        case SongAwareSuppressedReason::SwitchingDisabled: return "switching_disabled";
        case SongAwareSuppressedReason::Dwell: return "dwell";
        case SongAwareSuppressedReason::Cooldown: return "cooldown";
        case SongAwareSuppressedReason::RateLimit: return "rate_limit";
        case SongAwareSuppressedReason::Health: return "health";
        case SongAwareSuppressedReason::SameEffect: return "same_effect";
        case SongAwareSuppressedReason::TargetUnavailable: return "target_unavailable";
        case SongAwareSuppressedReason::BootGrace: return "boot_grace";
        case SongAwareSuppressedReason::EnableGrace: return "enable_grace";
        case SongAwareSuppressedReason::CandidateUnstable: return "candidate_unstable";
        case SongAwareSuppressedReason::AntiThrash: return "anti_thrash";
        case SongAwareSuppressedReason::TransitionActive: return "transition_active";
        case SongAwareSuppressedReason::HealthRecovering: return "health_recovering";
        case SongAwareSuppressedReason::BoundaryDeferred: return "boundary_deferred";
        case SongAwareSuppressedReason::AllowlistDisabled: return "allowlist_disabled";
        case SongAwareSuppressedReason::ImpossibleTransition: return "impossible_transition";
        default: return "unknown";
    }
}

const char* songAwareStateName(SongAwareState state) {
    switch (state) {
        case SongAwareState::Unknown: return "unknown";
        case SongAwareState::Silence: return "silence";
        case SongAwareState::Ambient: return "ambient";
        case SongAwareState::Steady: return "steady";
        case SongAwareState::Build: return "build";
        case SongAwareState::Drop: return "drop";
        case SongAwareState::Breakdown: return "breakdown";
        case SongAwareState::Dense: return "dense";
        case SongAwareState::Transition: return "transition";
        default: return "unknown";
    }
}

const char* songAwareLastActionName(SongAwareLastAction action) {
    switch (action) {
        case SongAwareLastAction::None: return "none";
        case SongAwareLastAction::ParameterUpdate: return "parameter_update";
        case SongAwareLastAction::EffectSwitch: return "effect_switch";
        case SongAwareLastAction::SwitchSuppressed: return "switch_suppressed";
        default: return "unknown";
    }
}

const char* songAwareActionPlanName(SongAwareActionPlan action) {
    switch (action) {
        case SongAwareActionPlan::None: return "none";
        case SongAwareActionPlan::ParameterModulation: return "parameter_modulation";
        case SongAwareActionPlan::PaletteShift: return "palette_shift";
        case SongAwareActionPlan::ColourModifierShift: return "colour_modifier_shift";
        case SongAwareActionPlan::EdgeMixerAdjust: return "edgemixer_adjust";
        case SongAwareActionPlan::ZoneComposerAdjust: return "zonecomposer_adjust";
        case SongAwareActionPlan::EffectSwitch: return "effect_switch";
        default: return "unknown";
    }
}

const char* songAwareIntentName(SongAwareIntent intent) {
    switch (intent) {
        case SongAwareIntent::QuietHold: return "quiet_hold";
        case SongAwareIntent::CalmHold: return "calm_hold";
        case SongAwareIntent::ReadableMotion: return "readable_motion";
        case SongAwareIntent::BuildPressure: return "build_pressure";
        case SongAwareIntent::DropImpact: return "drop_impact";
        case SongAwareIntent::ReleaseSpace: return "release_space";
        case SongAwareIntent::LegibilityControl: return "legibility_control";
        case SongAwareIntent::TransitionBridge: return "transition_bridge";
        default: return "unknown";
    }
}

const char* songAwareBoundaryGateName(SongAwareBoundaryGate gate) {
    switch (gate) {
        case SongAwareBoundaryGate::NotRequired: return "not_required";
        case SongAwareBoundaryGate::WaitingForBoundary: return "waiting_for_boundary";
        case SongAwareBoundaryGate::BeatBoundary: return "beat_boundary";
        case SongAwareBoundaryGate::DownbeatBoundary: return "downbeat_boundary";
        case SongAwareBoundaryGate::PhaseFallback: return "phase_fallback";
        default: return "unknown";
    }
}

const char* songAwareSwitchReasonName(SongAwareSwitchReason reason) {
    switch (reason) {
        case SongAwareSwitchReason::None: return "none";
        case SongAwareSwitchReason::AmbientPosture: return "ambient_posture";
        case SongAwareSwitchReason::SteadyReadability: return "steady_readability";
        case SongAwareSwitchReason::BuildPressure: return "build_pressure";
        case SongAwareSwitchReason::DropImpact: return "drop_impact";
        case SongAwareSwitchReason::BreakdownRelease: return "breakdown_release";
        case SongAwareSwitchReason::DenseLegibility: return "dense_legibility";
        case SongAwareSwitchReason::TransitionBridge: return "transition_bridge";
        default: return "unknown";
    }
}

const char* songAwareClassificationReasonName(SongAwareClassificationReason reason) {
    switch (reason) {
        case SongAwareClassificationReason::None: return "none";
        case SongAwareClassificationReason::NoAudio: return "no_audio";
        case SongAwareClassificationReason::SilentFrame: return "silent_frame";
        case SongAwareClassificationReason::LowConfidence: return "low_confidence";
        case SongAwareClassificationReason::DropOnset: return "drop_onset";
        case SongAwareClassificationReason::SpectralTransition: return "spectral_transition";
        case SongAwareClassificationReason::QuietBreakdown: return "quiet_breakdown";
        case SongAwareClassificationReason::DenseEnergy: return "dense_energy";
        case SongAwareClassificationReason::BuildEnergy: return "build_energy";
        case SongAwareClassificationReason::AmbientLowEnergy: return "ambient_low_energy";
        case SongAwareClassificationReason::SteadyDefault: return "steady_default";
        default: return "unknown";
    }
}

SongAwareMode parseSongAwareMode(const char* value, bool* ok) {
    if (ok) *ok = true;
    if (!value || strcmp(value, "off") == 0) return SongAwareMode::Off;
    if (strcmp(value, "assist") == 0 ||
        strcmp(value, "on") == 0 ||
        strcmp(value, "parameter") == 0 ||
        strcmp(value, "subtle") == 0 ||
        strcmp(value, "balanced") == 0 ||
        strcmp(value, "high") == 0 ||
        strcmp(value, "high_energy") == 0) {
        return SongAwareMode::Assist;
    }
    if (strcmp(value, "director") == 0) return SongAwareMode::Director;
    if (ok) *ok = false;
    return SongAwareMode::Off;
}

SongAwareProfile parseSongAwareProfile(const char* value, bool* ok) {
    if (ok) *ok = true;
    if (!value || strcmp(value, "balanced") == 0) return SongAwareProfile::Balanced;
    if (strcmp(value, "subtle") == 0) return SongAwareProfile::Subtle;
    if (strcmp(value, "high") == 0 || strcmp(value, "high_energy") == 0) return SongAwareProfile::High;
    if (ok) *ok = false;
    return SongAwareProfile::Balanced;
}

SongAwareState parseSongAwareState(const char* value, bool* ok) {
    if (ok) *ok = true;
    if (!value || strcmp(value, "unknown") == 0) return SongAwareState::Unknown;
    if (strcmp(value, "silence") == 0) return SongAwareState::Silence;
    if (strcmp(value, "ambient") == 0) return SongAwareState::Ambient;
    if (strcmp(value, "steady") == 0) return SongAwareState::Steady;
    if (strcmp(value, "build") == 0) return SongAwareState::Build;
    if (strcmp(value, "drop") == 0) return SongAwareState::Drop;
    if (strcmp(value, "breakdown") == 0) return SongAwareState::Breakdown;
    if (strcmp(value, "dense") == 0) return SongAwareState::Dense;
    if (strcmp(value, "transition") == 0) return SongAwareState::Transition;
    if (ok) *ok = false;
    return SongAwareState::Unknown;
}

} // namespace songaware
} // namespace lightwaveos
