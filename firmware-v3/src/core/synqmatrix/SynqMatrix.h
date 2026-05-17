/**
 * @file SynqMatrix.h
 * @brief Runtime-only synq-matrix parameter and visual-language director.
 */

#pragma once

#include <atomic>
#include <cstdint>

#include "../../config/effect_ids.h"
#include "../../config/features.h"

#if FEATURE_AUDIO_SYNC
#include "../../audio/contracts/ControlBus.h"
#include "../../audio/contracts/MusicalGrid.h"
#endif

namespace lightwaveos {
namespace synqmatrix {

enum class SynqMatrixMode : uint8_t {
    Off = 0,
    Assist = 1,
    Director = 2
};

enum class SynqMatrixProfile : uint8_t {
    Subtle = 0,
    Balanced = 1,
    High = 2
};

enum class SynqMatrixOwner : uint8_t {
    None = 0,
    Director = 1,
    Manual = 2,
    Show = 3
};

enum class SynqMatrixSuppressedReason : uint8_t {
    None = 0,
    Disabled = 1,
    NoAudio = 2,
    LowConfidence = 3,
    UnsupportedMode = 4,
    ManualOwner = 5,
    ShowOwner = 6,
    SwitchingDisabled = 7,
    Dwell = 8,
    Cooldown = 9,
    RateLimit = 10,
    Health = 11,
    SameEffect = 12,
    TargetUnavailable = 13,
    BootGrace = 14,
    EnableGrace = 15,
    CandidateUnstable = 16,
    AntiThrash = 17,
    TransitionActive = 18,
    HealthRecovering = 19,
    BoundaryDeferred = 20,
    AllowlistDisabled = 21,
    ImpossibleTransition = 22
};

enum class SynqMatrixState : uint8_t {
    Unknown = 0,
    Silence = 1,
    Ambient = 2,
    Steady = 3,
    Build = 4,
    Drop = 5,
    Breakdown = 6,
    Dense = 7,
    Transition = 8
};

enum class SynqMatrixLastAction : uint8_t {
    None = 0,
    ParameterUpdate = 1,
    EffectSwitch = 2,
    SwitchSuppressed = 3
};

enum class SynqMatrixActionPlan : uint8_t {
    None = 0,
    ParameterModulation = 1,
    PaletteShift = 2,
    ColourModifierShift = 3,
    EdgeMixerAdjust = 4,
    ZoneComposerAdjust = 5,
    EffectSwitch = 6
};

// Director Effect Registry markers. Captain's locked semantic markers govern
// which Tier 1 effects the Director may select per state. ATMOSPHERE is the
// passive fallback pool, not an active high-energy marker.
enum class DirectorMarker : uint8_t {
    None = 0,
    Groove = 1,
    Tension = 2,
    Impact = 3,
    Atmosphere = 4
};

enum class SynqMatrixIntent : uint8_t {
    QuietHold = 0,
    CalmHold = 1,
    ReadableMotion = 2,
    BuildPressure = 3,
    DropImpact = 4,
    ReleaseSpace = 5,
    LegibilityControl = 6,
    TransitionBridge = 7
};

enum class SynqMatrixBoundaryGate : uint8_t {
    NotRequired = 0,
    WaitingForBoundary = 1,
    BeatBoundary = 2,
    DownbeatBoundary = 3,
    PhaseFallback = 4
};

enum class SynqMatrixSwitchReason : uint8_t {
    None = 0,
    AmbientPosture = 1,
    SteadyReadability = 2,
    BuildPressure = 3,
    DropImpact = 4,
    BreakdownRelease = 5,
    DenseLegibility = 6,
    TransitionBridge = 7
};

enum class SynqMatrixClassificationReason : uint8_t {
    None = 0,
    NoAudio = 1,
    SilentFrame = 2,
    LowConfidence = 3,
    DropOnset = 4,
    SpectralTransition = 5,
    QuietBreakdown = 6,
    DenseEnergy = 7,
    BuildEnergy = 8,
    AmbientLowEnergy = 9,
    SteadyDefault = 10
};

static constexpr uint8_t kSynqMatrixMaxPolicySnapshotCount = 12;

struct SynqMatrixConfig {
    bool enabled = false;
    SynqMatrixMode mode = SynqMatrixMode::Off;
    SynqMatrixProfile profile = SynqMatrixProfile::Balanced;
    bool familyMorphing = false;
    bool constrainedSwitching = false;
    bool switchingEnabled = false;
    float sensitivity = 1.0f;
    float intensityScalar = 1.0f;
    float motionScalar = 1.0f;
    float confidenceFloor = 0.20f;
};

struct SynqMatrixStatus {
    bool enabled = false;
    SynqMatrixMode effectiveMode = SynqMatrixMode::Off;
    SynqMatrixProfile profile = SynqMatrixProfile::Balanced;
    SynqMatrixOwner owner = SynqMatrixOwner::None;
    SynqMatrixSuppressedReason suppressedReason = SynqMatrixSuppressedReason::Disabled;
    SynqMatrixSuppressedReason previousSuppressedReason = SynqMatrixSuppressedReason::Disabled;
    SynqMatrixClassificationReason classificationReason = SynqMatrixClassificationReason::None;
    SynqMatrixState rawState = SynqMatrixState::Unknown;
    SynqMatrixState previousState = SynqMatrixState::Unknown;
    SynqMatrixState currentState = SynqMatrixState::Unknown;
    SynqMatrixState candidateState = SynqMatrixState::Unknown;
    SynqMatrixLastAction lastAction = SynqMatrixLastAction::None;
    SynqMatrixIntent intent = SynqMatrixIntent::QuietHold;
    SynqMatrixActionPlan actionPlan = SynqMatrixActionPlan::None;
    SynqMatrixBoundaryGate boundaryGate = SynqMatrixBoundaryGate::NotRequired;
    bool boundaryReady = false;
    bool waitingForBoundary = false;
    float boundaryConfidence = 0.0f;
    float confidence = 0.0f;
    float selectionScore = 0.0f;
    uint32_t parameterUpdates = 0;
    uint32_t automaticEffectSwitches = 0;
    bool coasting = false;
    uint32_t audioConfidenceBelowFloorMs = 0;
    uint32_t missedPredictionCount = 0;
    uint32_t tempoWinnerChanges = 0;
    uint32_t lastDecisionAtMs = 0;
    uint32_t lastSwitchAtMs = 0;
    uint32_t stateAgeMs = 0;
    uint32_t candidateAgeMs = 0;
    uint32_t candidateHoldRemainingMs = 0;
    uint32_t dwellRemainingMs = 0;
    uint32_t cooldownRemainingMs = 0;
    uint32_t bootGraceRemainingMs = 0;
    uint32_t enableGraceRemainingMs = 0;
    uint32_t switchWindowRemainingMs = 0;
    uint8_t switchesInWindow = 0;
    uint8_t maxSwitchesPerWindow = 0;
    uint32_t antiThrashRemainingMs = 0;
    uint16_t activeEffectId = INVALID_EFFECT_ID;
    uint16_t previousEffectId = INVALID_EFFECT_ID;
    uint16_t selectedEffectId = INVALID_EFFECT_ID;
    uint16_t lastSwitchFromEffectId = INVALID_EFFECT_ID;
    uint16_t lastSwitchToEffectId = INVALID_EFFECT_ID;
    const char* activeEffectName = "Unknown";
    const char* selectedFamily = "none";
    const char* selectedVisualLanguage = "none";
    const char* lastSwitchReason = "none";
    bool transitionActive = false;
    uint16_t transitionPreviousEffectId = INVALID_EFFECT_ID;
    uint16_t transitionTargetEffectId = INVALID_EFFECT_ID;
    uint32_t transitionStartedAtMs = 0;
    uint32_t transitionDurationMs = 0;
    uint32_t transitionRemainingMs = 0;
    float transitionProgress = 0.0f;
    uint32_t showSkips = 0;
    uint32_t failures = 0;
    uint32_t rmtErrors = 0;
    uint32_t underruns = 0;
    bool healthDegraded = false;
    uint32_t healthCleanForMs = 0;
    uint32_t healthCleanWindowRemainingMs = 0;
    float rms = 0.0f;
    float flux = 0.0f;
    float bpm = 0.0f;
    float audioConfidence = 0.0f;
};

struct SynqMatrixParams {
    uint16_t effectId = INVALID_EFFECT_ID;
    uint8_t brightness = 0;
    uint8_t speed = 1;
    uint8_t intensity = 0;
    uint8_t saturation = 0;
    uint8_t complexity = 0;
    uint8_t variation = 0;
    uint8_t hue = 0;
};

struct SynqMatrixHealthCounters {
    uint32_t showSkips = 0;
    uint32_t failures = 0;
    uint32_t rmtErrors = 0;
    uint32_t underruns = 0;
};

struct SynqMatrixContext {
    SynqMatrixHealthCounters health;
};

struct SynqMatrixSwitchRequest {
    bool requested = false;
    uint16_t targetEffectId = INVALID_EFFECT_ID;
    const char* targetFamily = "none";
    const char* targetVisualLanguage = "none";
    const char* reason = "none";
    // PaletteShift: when requested fires, also rotate to this palette index.
    // 0xFF = leave palette unchanged.
    uint8_t targetPaletteIndex = 0xFF;
    // ColourModifierShift: when requested fires, also apply this global hue
    // offset (0..255 = full hue wheel). applyColourModifier=false leaves
    // hue unchanged. Cannot use a sentinel value because every uint8_t is a
    // valid hue, so a separate flag is required.
    bool applyColourModifier = false;
    uint8_t targetColourModifier = 0;
    // Director Effect Registry: per-effect speed cap. 0xFF = no cap. When set,
    // the renderer clamps m_speed down (never raises) so registry-curated slow
    // effects don't get over-driven by user speed slider state at switch time.
    uint8_t speedCap = 0xFF;
    // EdgeMixerAdjust: per-state EdgeMixer mode (0..8 = mode ordinal; see
    // EdgeMixerMode enum). 0xFF = leave EdgeMixer unchanged. Applied at the
    // same state-change trigger as EffectSwitch + PaletteShift + ColourModifierShift.
    uint8_t edgeMixerMode = 0xFF;
    // ZoneComposer safety clamp: 0 = disable zones, 1 = enable zones,
    // 0xFF = leave unchanged. Director sets 0 on every non-Unknown state
    // to guarantee its Tier 1 effect renders unified across the strip
    // (zones use loadPreset which contains non-Tier-1 effects). Full
    // ZoneComposerAdjust with per-state presets requires a Captain-
    // approved zone-effect allowlist (future scope).
    uint8_t zoneEnabled = 0xFF;
};

struct SynqMatrixPolicySnapshot {
    SynqMatrixState state = SynqMatrixState::Unknown;
    uint16_t effectId = INVALID_EFFECT_ID;
    const char* family = "none";
    const char* visualLanguage = "none";
    SynqMatrixSwitchReason reason = SynqMatrixSwitchReason::None;
    float minConfidence = 1.0f;
    bool enabled = true;
};

struct SynqMatrixSelectionSnapshot {
    bool valid = false;
    SynqMatrixPolicySnapshot policy;
    float score = 0.0f;
};

struct SynqMatrixAllowlistSnapshot {
    uint8_t count = 0;
    SynqMatrixPolicySnapshot policies[kSynqMatrixMaxPolicySnapshotCount] = {};
};

struct SynqMatrixDebugSnapshot {
    SynqMatrixConfig config;
    SynqMatrixStatus status;
    SynqMatrixAllowlistSnapshot allowlist;
    uint32_t bootGraceMs = 0;
    uint32_t postEnableGraceMs = 0;
    uint32_t stableStateHoldMs = 0;
    uint32_t dropStateHoldMs = 0;
    uint32_t minimumDwellMs = 0;
    uint32_t switchCooldownMs = 0;
    uint32_t switchWindowMs = 0;
    uint8_t maxSwitchesPerWindow = 0;
    uint32_t antiThrashWindowMs = 0;
    uint32_t healthCleanWindowMs = 0;
};

struct SynqMatrixRuntimeState {
    SynqMatrixConfig config;
    SynqMatrixStatus status;
    uint16_t policyAllowMask = 0;
    uint32_t manualSuppressUntilMs = 0;
    uint32_t showSuppressUntilMs = 0;
    uint32_t bootGraceUntilMs = 0;
    uint32_t enableGraceUntilMs = 0;
    uint32_t healthCleanSinceMs = 0;
    uint32_t healthLastDegradedAtMs = 0;
    uint32_t antiThrashUntilMs = 0;
};

class SynqMatrix {
public:
    static SynqMatrix& instance();

    void reset();
    void resetCounters();
    void setConfig(const SynqMatrixConfig& config);
    SynqMatrixConfig getConfig() const;
    SynqMatrixStatus getStatus() const;
    SynqMatrixRuntimeState exportRuntimeState() const;
    void restoreRuntimeState(const SynqMatrixRuntimeState& state);
    SynqMatrixDebugSnapshot getDebugSnapshot() const;
    SynqMatrixSelectionSnapshot resolveSelection(SynqMatrixState state,
                                                float confidence,
                                                uint16_t activeEffectId) const;
    static uint8_t policyCount();
    static bool policySnapshot(uint8_t index, SynqMatrixPolicySnapshot& snapshot);
    static uint8_t copyPolicyTable(SynqMatrixPolicySnapshot* out, uint8_t capacity);
    static SynqMatrixAllowlistSnapshot policyTableSnapshot();
    SynqMatrixAllowlistSnapshot getAllowlistSnapshot() const;
    bool setPolicyAllowed(SynqMatrixState state, bool enabled);
    bool isPolicyAllowed(SynqMatrixState state) const;
    void resetPolicyAllowlist();
    void markManualControl(uint32_t nowMs);
    void markShowControl(uint32_t nowMs);
    bool isShowOwnerActive(uint32_t nowMs) const;

#if FEATURE_AUDIO_SYNC
    bool tick(const audio::ControlBusFrame& frame,
                          const audio::MusicalGridSnapshot& grid,
                          bool audioAvailable,
                          uint32_t nowMs,
                          uint16_t activeEffectId,
                          const SynqMatrixContext& context,
                          SynqMatrixSwitchRequest& request);
    void notifySwitchApplied(uint16_t previousEffectId,
                             uint16_t targetEffectId,
                             uint32_t nowMs,
                             const char* activeEffectName);
    void notifySwitchRejected(uint16_t targetEffectId,
                              uint32_t nowMs,
                              SynqMatrixSuppressedReason reason);
    void notifyTransitionStarted(uint16_t previousEffectId,
                                 uint16_t targetEffectId,
                                 uint32_t nowMs,
                                 uint32_t durationMs);
    void notifyTransitionCompleted(uint32_t nowMs);
    bool apply(const audio::ControlBusFrame& frame,
               const audio::MusicalGridSnapshot& grid,
               bool audioAvailable,
               float dtSeconds,
               uint32_t nowMs,
               SynqMatrixParams& params);
#endif

private:
    static float clamp01(float value);
    static uint8_t clampU8(int value, uint8_t minValue, uint8_t maxValue);
    static uint16_t scaleFloat(float value);
    static float unscaleFloat(uint16_t value);

#if FEATURE_AUDIO_SYNC
    struct SynqMatrixFeatureSnapshot {
        float energy = 0.0f;
        float slowEnergy = 0.0f;
        float fastSlowRatio = 1.0f;
        float flux = 0.0f;
        float onsetStrength = 0.0f;
        float beatStrength = 0.0f;
        float tempoConfidence = 0.0f;
        float liveliness = 0.0f;
        float saliency = 0.0f;
        float adaptiveFloor = 0.0f;
        bool boundaryReady = false;
        SynqMatrixBoundaryGate boundaryGate = SynqMatrixBoundaryGate::WaitingForBoundary;
        float boundaryConfidence = 0.0f;
    };

    SynqMatrixFeatureSnapshot buildFeatureSnapshot(const audio::ControlBusFrame& frame,
                                                  const audio::MusicalGridSnapshot& grid,
                                                  bool audioAvailable,
                                                  float dtSeconds);
    SynqMatrixState classifyState(const audio::ControlBusFrame& frame,
                                 const SynqMatrixFeatureSnapshot& features,
                                 bool audioAvailable,
                                 float confidence);
    SynqMatrixState updateStableState(SynqMatrixState rawState,
                                     float confidence,
                                     uint32_t nowMs);
    bool updateConfidenceOperatingPhase(float confidence,
                                        bool audioAvailable,
                                        uint32_t nowMs);
    void consumeTimebaseTelemetry(const audio::ControlBusFrame& frame, bool audioAvailable);
    void updateAudioSummary(const audio::ControlBusFrame& frame, bool audioAvailable);
#endif
    SynqMatrixIntent planIntent(SynqMatrixState state) const;
    SynqMatrixActionPlan resolveActionPlan(SynqMatrixMode mode,
                                          SynqMatrixProfile profile,
                                          SynqMatrixState state,
                                          bool switching) const;
#if FEATURE_AUDIO_SYNC
    bool transitionIsAllowed(SynqMatrixState from,
                             SynqMatrixState to,
                             const SynqMatrixFeatureSnapshot& features) const;
    void updateIntentTelemetry(SynqMatrixState state,
                               SynqMatrixActionPlan actionPlan,
                               const SynqMatrixFeatureSnapshot& features);
#endif
    void setSuppressed(SynqMatrixSuppressedReason reason, SynqMatrixOwner owner);
    void updateRemainingGates(uint32_t nowMs);
    void updateTransitionTelemetry(uint32_t nowMs);
    void updateHealthTracking(SynqMatrixHealthCounters health, uint32_t nowMs);
    bool graceSuppresses(uint32_t nowMs, SynqMatrixSuppressedReason& reason);
    bool wouldCreateAbaSwitch(uint16_t activeEffectId, uint16_t targetEffectId, uint32_t nowMs) const;
    bool healthIsDegraded(SynqMatrixHealthCounters health) const;
    float scorePolicy(SynqMatrixState state, float confidence, const SynqMatrixPolicySnapshot& policy) const;
    // Director Effect Registry: pick an approved effect for the current state
    // per Captain's default-deny allowlist (GROOVE/TENSION/IMPACT/ATMOSPHERE).
    // Returns INVALID_EFFECT_ID if no candidate matches (e.g. Unknown state).
    uint16_t selectDirectorEffect(SynqMatrixState state, uint16_t activeEffectId);

    std::atomic<bool> m_enabled{false};
    std::atomic<uint8_t> m_mode{static_cast<uint8_t>(SynqMatrixMode::Off)};
    std::atomic<uint8_t> m_profile{static_cast<uint8_t>(SynqMatrixProfile::Balanced)};
    std::atomic<bool> m_familyMorphing{false};
    std::atomic<bool> m_constrainedSwitching{false};
    std::atomic<bool> m_switchingEnabled{false};
    std::atomic<uint16_t> m_sensitivityQ1000{1000};
    std::atomic<uint16_t> m_intensityScalarQ1000{1000};
    std::atomic<uint16_t> m_motionScalarQ1000{1000};
    std::atomic<uint16_t> m_confidenceFloorQ1000{200};

    std::atomic<uint8_t> m_effectiveMode{static_cast<uint8_t>(SynqMatrixMode::Off)};
    std::atomic<uint8_t> m_owner{static_cast<uint8_t>(SynqMatrixOwner::None)};
    std::atomic<uint8_t> m_suppressedReason{static_cast<uint8_t>(SynqMatrixSuppressedReason::Disabled)};
    std::atomic<uint8_t> m_previousSuppressedReason{static_cast<uint8_t>(SynqMatrixSuppressedReason::Disabled)};
    std::atomic<uint8_t> m_classificationReason{static_cast<uint8_t>(SynqMatrixClassificationReason::None)};
    std::atomic<uint8_t> m_rawState{static_cast<uint8_t>(SynqMatrixState::Unknown)};
    std::atomic<uint8_t> m_previousState{static_cast<uint8_t>(SynqMatrixState::Unknown)};
    std::atomic<uint8_t> m_currentState{static_cast<uint8_t>(SynqMatrixState::Unknown)};
    std::atomic<uint8_t> m_lastAction{static_cast<uint8_t>(SynqMatrixLastAction::None)};
    std::atomic<uint8_t> m_intent{static_cast<uint8_t>(SynqMatrixIntent::QuietHold)};
    std::atomic<uint8_t> m_actionPlan{static_cast<uint8_t>(SynqMatrixActionPlan::None)};
    std::atomic<uint8_t> m_boundaryGate{static_cast<uint8_t>(SynqMatrixBoundaryGate::NotRequired)};
    std::atomic<bool> m_boundaryReady{false};
    std::atomic<bool> m_waitingForBoundary{false};
    std::atomic<uint16_t> m_boundaryConfidenceQ1000{0};
    std::atomic<uint16_t> m_confidenceQ1000{0};
    std::atomic<uint16_t> m_driveQ1000{0};
    std::atomic<uint16_t> m_slowEnergyQ1000{0};
    std::atomic<uint16_t> m_adaptiveFloorQ1000{40};
    std::atomic<uint16_t> m_selectionScoreQ1000{0};
    std::atomic<uint32_t> m_parameterUpdates{0};
    std::atomic<uint32_t> m_automaticEffectSwitches{0};
    std::atomic<bool> m_coasting{false};
    std::atomic<uint32_t> m_audioConfidenceBelowFloorSinceMs{0};
    std::atomic<uint32_t> m_audioConfidenceRecoveredSinceMs{0};
    std::atomic<uint32_t> m_audioConfidenceBelowFloorMs{0};
    std::atomic<uint32_t> m_missedPredictionCount{0};
    std::atomic<uint32_t> m_tempoWinnerChanges{0};
    std::atomic<uint32_t> m_lastSourceMissedPredictionCount{0};
    std::atomic<uint32_t> m_lastSourceTempoWinnerChanges{0};
    std::atomic<uint32_t> m_lastTimebaseTelemetryHopSeq{0};
    std::atomic<bool> m_timebaseTelemetryPrimed{false};
    std::atomic<uint32_t> m_lastDecisionAtMs{0};
    std::atomic<uint32_t> m_lastSwitchAtMs{0};
    std::atomic<uint32_t> m_stateAgeMs{0};
    std::atomic<uint32_t> m_candidateAgeMs{0};
    std::atomic<uint32_t> m_candidateHoldRemainingMs{0};
    std::atomic<uint32_t> m_dwellRemainingMs{0};
    std::atomic<uint32_t> m_cooldownRemainingMs{0};
    std::atomic<uint32_t> m_bootGraceUntilMs{3000};
    std::atomic<uint32_t> m_enableGraceUntilMs{0};
    std::atomic<bool> m_enableGracePending{false};
    std::atomic<uint32_t> m_switchWindowRemainingMs{0};
    std::atomic<uint32_t> m_antiThrashUntilMs{0};
    std::atomic<uint32_t> m_activeEffectId{INVALID_EFFECT_ID};
    std::atomic<uint32_t> m_previousEffectId{INVALID_EFFECT_ID};
    std::atomic<uint32_t> m_selectedEffectId{INVALID_EFFECT_ID};
    std::atomic<uint8_t> m_selectedPolicyIndex{0};
    std::atomic<uint8_t> m_lastSwitchReason{static_cast<uint8_t>(SynqMatrixSwitchReason::None)};
    std::atomic<uint16_t> m_rmsQ1000{0};
    std::atomic<uint16_t> m_fluxQ1000{0};
    std::atomic<uint16_t> m_bpmQ10{0};
    std::atomic<uint16_t> m_audioConfidenceQ1000{0};
    std::atomic<uint32_t> m_showSkips{0};
    std::atomic<uint32_t> m_failures{0};
    std::atomic<uint32_t> m_rmtErrors{0};
    std::atomic<uint32_t> m_underruns{0};
    std::atomic<uint32_t> m_manualSuppressUntilMs{0};
    std::atomic<uint32_t> m_showSuppressUntilMs{0};
    std::atomic<uint32_t> m_lastEvaluationAtMs{0};
    std::atomic<uint32_t> m_stateEnteredAtMs{0};
    std::atomic<uint8_t> m_candidateState{static_cast<uint8_t>(SynqMatrixState::Unknown)};
    std::atomic<uint32_t> m_candidateSinceMs{0};
    std::atomic<uint32_t> m_switchWindowStartMs{0};
    std::atomic<uint8_t> m_switchesInWindow{0};
    std::atomic<uint32_t> m_lastSwitchFromEffectId{INVALID_EFFECT_ID};
    std::atomic<uint32_t> m_lastSwitchToEffectId{INVALID_EFFECT_ID};
    std::atomic<bool> m_transitionActive{false};
    std::atomic<uint32_t> m_transitionPreviousEffectId{INVALID_EFFECT_ID};
    std::atomic<uint32_t> m_transitionTargetEffectId{INVALID_EFFECT_ID};
    std::atomic<uint32_t> m_transitionStartedAtMs{0};
    std::atomic<uint32_t> m_transitionDurationMs{0};
    std::atomic<uint32_t> m_transitionRemainingMs{0};
    std::atomic<uint16_t> m_transitionProgressQ1000{0};
    std::atomic<bool> m_healthDegraded{false};
    std::atomic<uint32_t> m_healthCleanSinceMs{0};
    std::atomic<uint32_t> m_healthLastDegradedAtMs{0};
    std::atomic<uint32_t> m_healthCleanForMs{0};
    std::atomic<uint32_t> m_healthCleanWindowRemainingMs{0};
    std::atomic<uint16_t> m_policyAllowMask{0x01FF};

    // Director Effect Registry: per-state round-robin index into kDirectorRegistry[].
    // Provides variety across repeat visits to the same SynqMatrixState. Stateless
    // across reboots (acceptable — boot starts at index 0 per state). Index [0]
    // tracks Unknown state but is unused (selectDirectorEffect returns INVALID
    // for Unknown).
    std::atomic<uint8_t> m_directorRoundRobin[9]{};
};

const char* synqMatrixModeName(SynqMatrixMode mode);
const char* synqMatrixProfileName(SynqMatrixProfile profile);
const char* synqMatrixOwnerName(SynqMatrixOwner owner);
const char* synqMatrixSuppressedReasonName(SynqMatrixSuppressedReason reason);
const char* synqMatrixStateName(SynqMatrixState state);
const char* synqMatrixLastActionName(SynqMatrixLastAction action);
const char* synqMatrixActionPlanName(SynqMatrixActionPlan action);
const char* directorMarkerName(DirectorMarker marker);
// Look up the marker of an effect in the Director Effect Registry. Returns
// DirectorMarker::None for effects not in the registry (e.g. boot baseline).
DirectorMarker directorMarkerForEffect(uint16_t effectId);
const char* synqMatrixIntentName(SynqMatrixIntent intent);
const char* synqMatrixBoundaryGateName(SynqMatrixBoundaryGate gate);
const char* synqMatrixSwitchReasonName(SynqMatrixSwitchReason reason);
const char* synqMatrixClassificationReasonName(SynqMatrixClassificationReason reason);
SynqMatrixMode parseSynqMatrixMode(const char* value, bool* ok = nullptr);
SynqMatrixProfile parseSynqMatrixProfile(const char* value, bool* ok = nullptr);
SynqMatrixState parseSynqMatrixState(const char* value, bool* ok = nullptr);

} // namespace synqmatrix
} // namespace lightwaveos
