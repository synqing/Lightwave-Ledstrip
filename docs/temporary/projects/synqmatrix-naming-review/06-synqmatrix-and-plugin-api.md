---
abstract: "Read-only naming enumeration of the renamed SynqMatrix subsystem and the plugin API surface (EffectContext, IEffect, OnsetContext, BehaviorSelection, registries, runtime adapter). Per-class blocks, enum value lists, struct field tables, and accessor signatures verbatim from headers. Documents leftover `songAware*`/`parseSynqMatrix*` rename anomalies (anchor #51311) and identifies AudioContext accessor rename surface for post-migration polish."
---

# 06 — SynqMatrix Core + Plugins API + Plugin Runtime — Name Inventory

Branch: `feature/synqmatrix-rename-2026-05-13`. Sources: `firmware-v3/src/core/synqmatrix/{SynqMatrix.h,SynqMatrix.cpp}`, `firmware-v3/src/plugins/api/{BehaviorSelection.h,EffectContext.h,IEffect.h,IEffectRegistry.h,OnsetContext.h}`, `firmware-v3/src/plugins/{BuiltinEffectRegistry.{h,cpp},PluginManagerActor.{h,cpp}}`, `firmware-v3/src/plugins/runtime/LegacyEffectAdapter.{h,cpp}`.

Scope excludes effect implementations under `src/effects/*`.

---

## 1. SynqMatrix subsystem (`namespace lightwaveos::synqmatrix`)

### 1.1 Enum classes

| Enum | Underlying | Values |
|---|---|---|
| `SynqMatrixMode` | `uint8_t` | `Off=0`, `Assist=1`, `Director=2` |
| `SynqMatrixProfile` | `uint8_t` | `Subtle=0`, `Balanced=1`, `High=2` |
| `SynqMatrixOwner` | `uint8_t` | `None=0`, `Director=1`, `Manual=2`, `Show=3` |
| `SynqMatrixSuppressedReason` | `uint8_t` | `None=0`, `Disabled=1`, `NoAudio=2`, `LowConfidence=3`, `UnsupportedMode=4`, `ManualOwner=5`, `ShowOwner=6`, `SwitchingDisabled=7`, `Dwell=8`, `Cooldown=9`, `RateLimit=10`, `Health=11`, `SameEffect=12`, `TargetUnavailable=13`, `BootGrace=14`, `EnableGrace=15`, `CandidateUnstable=16`, `AntiThrash=17`, `TransitionActive=18`, `HealthRecovering=19`, `BoundaryDeferred=20`, `AllowlistDisabled=21`, `ImpossibleTransition=22` |
| `SynqMatrixState` | `uint8_t` | `Unknown=0`, `Silence=1`, `Ambient=2`, `Steady=3`, `Build=4`, `Drop=5`, `Breakdown=6`, `Dense=7`, `Transition=8` |
| `SynqMatrixLastAction` | `uint8_t` | `None=0`, `ParameterUpdate=1`, `EffectSwitch=2`, `SwitchSuppressed=3` |
| `SynqMatrixActionPlan` | `uint8_t` | `None=0`, `ParameterModulation=1`, `PaletteShift=2`, `ColourModifierShift=3`, `EdgeMixerAdjust=4`, `ZoneComposerAdjust=5`, `EffectSwitch=6` |
| `SynqMatrixIntent` | `uint8_t` | `QuietHold=0`, `CalmHold=1`, `ReadableMotion=2`, `BuildPressure=3`, `DropImpact=4`, `ReleaseSpace=5`, `LegibilityControl=6`, `TransitionBridge=7` |
| `SynqMatrixBoundaryGate` | `uint8_t` | `NotRequired=0`, `WaitingForBoundary=1`, `BeatBoundary=2`, `DownbeatBoundary=3`, `PhaseFallback=4` |
| `SynqMatrixSwitchReason` | `uint8_t` | `None=0`, `AmbientPosture=1`, `SteadyReadability=2`, `BuildPressure=3`, `DropImpact=4`, `BreakdownRelease=5`, `DenseLegibility=6`, `TransitionBridge=7` |
| `SynqMatrixClassificationReason` | `uint8_t` | `None=0`, `NoAudio=1`, `SilentFrame=2`, `LowConfidence=3`, `DropOnset=4`, `SpectralTransition=5`, `QuietBreakdown=6`, `DenseEnergy=7`, `BuildEnergy=8`, `AmbientLowEnergy=9`, `SteadyDefault=10` |

### 1.2 Constants

- `static constexpr uint8_t kSynqMatrixMaxPolicySnapshotCount = 12;`

### 1.3 Structs

`SynqMatrixConfig` — `enabled`, `mode`, `profile`, `familyMorphing`, `constrainedSwitching`, `switchingEnabled`, `sensitivity`, `intensityScalar`, `motionScalar`, `confidenceFloor`.

`SynqMatrixStatus` — fields: `enabled`, `effectiveMode`, `profile`, `owner`, `suppressedReason`, `previousSuppressedReason`, `classificationReason`, `rawSongState`, `previousSongState`, `currentSongState`, `candidateSongState`, `lastAction`, `intent`, `actionPlan`, `boundaryGate`, `boundaryReady`, `waitingForBoundary`, `boundaryConfidence`, `confidence`, `selectionScore`, `parameterUpdates`, `automaticEffectSwitches`, `lastDecisionAtMs`, `lastSwitchAtMs`, `stateAgeMs`, `candidateAgeMs`, `candidateHoldRemainingMs`, `dwellRemainingMs`, `cooldownRemainingMs`, `bootGraceRemainingMs`, `enableGraceRemainingMs`, `switchWindowRemainingMs`, `switchesInWindow`, `maxSwitchesPerWindow`, `antiThrashRemainingMs`, `activeEffectId`, `previousEffectId`, `selectedEffectId`, `lastSwitchFromEffectId`, `lastSwitchToEffectId`, `activeEffectName`, `selectedFamily`, `selectedVisualLanguage`, `lastSwitchReason`, `transitionActive`, `transitionPreviousEffectId`, `transitionTargetEffectId`, `transitionStartedAtMs`, `transitionDurationMs`, `transitionRemainingMs`, `transitionProgress`, `showSkips`, `failures`, `rmtErrors`, `underruns`, `healthDegraded`, `healthCleanForMs`, `healthCleanWindowRemainingMs`, `rms`, `flux`, `bpm`, `audioConfidence`.

Note the `*SongState` suffixed fields (`rawSongState`, `previousSongState`, `currentSongState`, `candidateSongState`) — these are residual "song" semantic leakage in struct field names (see Anomalies §4.1).

`SynqMatrixParams` — `effectId`, `brightness`, `speed`, `intensity`, `saturation`, `complexity`, `variation`, `hue`.

`SynqMatrixHealthCounters` — `showSkips`, `failures`, `rmtErrors`, `underruns`.

`SynqMatrixContext` — `health` (SynqMatrixHealthCounters).

`SynqMatrixSwitchRequest` — `requested`, `targetEffectId`, `targetFamily`, `targetVisualLanguage`, `reason`.

`SynqMatrixPolicySnapshot` — `state`, `effectId`, `family`, `visualLanguage`, `reason`, `minConfidence`, `enabled`.

`SynqMatrixSelectionSnapshot` — `valid`, `policy`, `score`.

`SynqMatrixAllowlistSnapshot` — `count`, `policies[kSynqMatrixMaxPolicySnapshotCount]`.

`SynqMatrixDebugSnapshot` — `config`, `status`, `allowlist`, `bootGraceMs`, `postEnableGraceMs`, `stableStateHoldMs`, `dropStateHoldMs`, `minimumDwellMs`, `switchCooldownMs`, `switchWindowMs`, `maxSwitchesPerWindow`, `antiThrashWindowMs`, `healthCleanWindowMs`.

`SynqMatrixRuntimeState` — `config`, `status`, `policyAllowMask`, `manualSuppressUntilMs`, `showSuppressUntilMs`, `bootGraceUntilMs`, `enableGraceUntilMs`, `healthCleanSinceMs`, `healthLastDegradedAtMs`, `antiThrashUntilMs`.

`SynqMatrixFeatureSnapshot` (private, inside `class SynqMatrix`) — `energy`, `slowEnergy`, `fastSlowRatio`, `flux`, `onsetStrength`, `beatStrength`, `tempoConfidence`, `liveliness`, `saliency`, `adaptiveFloor`, `boundaryReady`, `boundaryGate`, `boundaryConfidence`.

### 1.4 `class SynqMatrix` — singleton director

Public:

```
static SynqMatrix& instance();
void reset();
void resetCounters();
void setConfig(const SynqMatrixConfig& config);
SynqMatrixConfig getConfig() const;
SynqMatrixStatus getStatus() const;
SynqMatrixRuntimeState exportRuntimeState() const;
void restoreRuntimeState(const SynqMatrixRuntimeState& state);
SynqMatrixDebugSnapshot getDebugSnapshot() const;
SynqMatrixSelectionSnapshot resolveSelection(SynqMatrixState state, float confidence, uint16_t activeEffectId) const;
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
void notifySwitchApplied(uint16_t previousEffectId, uint16_t targetEffectId,
                         uint32_t nowMs, const char* activeEffectName);
void notifySwitchRejected(uint16_t targetEffectId, uint32_t nowMs,
                          SynqMatrixSuppressedReason reason);
void notifyTransitionStarted(uint16_t previousEffectId, uint16_t targetEffectId,
                             uint32_t nowMs, uint32_t durationMs);
void notifyTransitionCompleted(uint32_t nowMs);
bool apply(const audio::ControlBusFrame& frame,
           const audio::MusicalGridSnapshot& grid,
           bool audioAvailable, float dtSeconds, uint32_t nowMs,
           SynqMatrixParams& params);
#endif
```

Private helpers:

```
static float clamp01(float value);
static uint8_t clampU8(int value, uint8_t minValue, uint8_t maxValue);
static uint16_t scaleFloat(float value);
static float unscaleFloat(uint16_t value);
SynqMatrixFeatureSnapshot buildFeatureSnapshot(...);
SynqMatrixState classifyState(...);
SynqMatrixState updateStableState(SynqMatrixState rawState, float confidence, uint32_t nowMs);
void updateAudioSummary(const audio::ControlBusFrame& frame, bool audioAvailable);
SynqMatrixIntent planIntent(SynqMatrixState state) const;
SynqMatrixActionPlan resolveActionPlan(SynqMatrixMode mode, SynqMatrixProfile profile, SynqMatrixState state, bool switching) const;
bool transitionIsAllowed(SynqMatrixState from, SynqMatrixState to, const SynqMatrixFeatureSnapshot& features) const;
void updateIntentTelemetry(SynqMatrixState state, SynqMatrixActionPlan actionPlan, const SynqMatrixFeatureSnapshot& features);
void setSuppressed(SynqMatrixSuppressedReason reason, SynqMatrixOwner owner);
void updateRemainingGates(uint32_t nowMs);
void updateTransitionTelemetry(uint32_t nowMs);
void updateHealthTracking(SynqMatrixHealthCounters health, uint32_t nowMs);
bool graceSuppresses(uint32_t nowMs, SynqMatrixSuppressedReason& reason);
bool wouldCreateAbaSwitch(uint16_t activeEffectId, uint16_t targetEffectId, uint32_t nowMs) const;
bool healthIsDegraded(SynqMatrixHealthCounters health) const;
float scorePolicy(SynqMatrixState state, float confidence, const SynqMatrixPolicySnapshot& policy) const;
```

Internal atomic state fields (`std::atomic<…> m_*`): `m_enabled`, `m_mode`, `m_profile`, `m_familyMorphing`, `m_constrainedSwitching`, `m_switchingEnabled`, `m_sensitivityQ1000`, `m_intensityScalarQ1000`, `m_motionScalarQ1000`, `m_confidenceFloorQ1000`, `m_effectiveMode`, `m_owner`, `m_suppressedReason`, `m_previousSuppressedReason`, `m_classificationReason`, `m_rawSongState`, `m_previousSongState`, `m_currentSongState`, `m_lastAction`, `m_intent`, `m_actionPlan`, `m_boundaryGate`, `m_boundaryReady`, `m_waitingForBoundary`, `m_boundaryConfidenceQ1000`, `m_confidenceQ1000`, `m_driveQ1000`, `m_slowEnergyQ1000`, `m_adaptiveFloorQ1000`, `m_selectionScoreQ1000`, `m_parameterUpdates`, `m_automaticEffectSwitches`, `m_lastDecisionAtMs`, `m_lastSwitchAtMs`, `m_stateAgeMs`, `m_candidateAgeMs`, `m_candidateHoldRemainingMs`, `m_dwellRemainingMs`, `m_cooldownRemainingMs`, `m_bootGraceUntilMs`, `m_enableGraceUntilMs`, `m_enableGracePending`, `m_switchWindowRemainingMs`, `m_antiThrashUntilMs`, `m_activeEffectId`, `m_previousEffectId`, `m_selectedEffectId`, `m_selectedPolicyIndex`, `m_lastSwitchReason`, `m_rmsQ1000`, `m_fluxQ1000`, `m_bpmQ10`, `m_audioConfidenceQ1000`, `m_showSkips`, `m_failures`, `m_rmtErrors`, `m_underruns`, `m_manualSuppressUntilMs`, `m_showSuppressUntilMs`, `m_lastEvaluationAtMs`, `m_stateEnteredAtMs`, `m_candidateState`, `m_candidateSinceMs`, `m_switchWindowStartMs`, `m_switchesInWindow`, `m_lastSwitchFromEffectId`, `m_lastSwitchToEffectId`, `m_transitionActive`, `m_transitionPreviousEffectId`, `m_transitionTargetEffectId`, `m_transitionStartedAtMs`, `m_transitionDurationMs`, `m_transitionRemainingMs`, `m_transitionProgressQ1000`, `m_healthDegraded`, `m_healthCleanSinceMs`, `m_healthLastDegradedAtMs`, `m_healthCleanForMs`, `m_healthCleanWindowRemainingMs`, `m_policyAllowMask`.

### 1.5 Policy table (file-scope, `SynqMatrix.cpp` anonymous namespace)

```
struct DirectorPolicy {
    SynqMatrixState state;
    EffectId effectId;
    const char* family;
    const char* visualLanguage;
    SynqMatrixSwitchReason reason;
    float minConfidence;
};
```

Constants: `kSmoothTauSeconds`, `kBootGraceMs`, `kPostEnableGraceMs`, `kEvaluationPeriodMs`, `kStableStateHoldMs`, `kDropStateHoldMs`, `kMinimumDwellMs`, `kSwitchCooldownMs`, `kSwitchWindowMs`, `kMaxSwitchesPerWindow`, `kAntiThrashWindowMs`, `kHealthCleanWindowMs`, `kManualSuppressMs`, `kShowSuppressMs`, `kMatrix[]`, `kPolicyCount`, `kAllPoliciesMask`.

File-scope helpers: `deadlineActive`, `remainingUntil`, `policyByIndex`, `policyForState`, `policyBit`, `policyBitForState`, `snapshotForPolicy`, `elapsedSince`.

### 1.6 Free helper functions (free in `namespace lightwaveos::synqmatrix`)

```
const char* songAwareModeName(SynqMatrixMode mode);
const char* songAwareProfileName(SynqMatrixProfile profile);
const char* songAwareOwnerName(SynqMatrixOwner owner);
const char* songAwareSuppressedReasonName(SynqMatrixSuppressedReason reason);
const char* songAwareStateName(SynqMatrixState state);
const char* songAwareLastActionName(SynqMatrixLastAction action);
const char* songAwareActionPlanName(SynqMatrixActionPlan action);
const char* songAwareIntentName(SynqMatrixIntent intent);
const char* songAwareBoundaryGateName(SynqMatrixBoundaryGate gate);
const char* songAwareSwitchReasonName(SynqMatrixSwitchReason reason);
const char* songAwareClassificationReasonName(SynqMatrixClassificationReason reason);
SynqMatrixMode  parseSynqMatrixMode(const char* value, bool* ok = nullptr);
SynqMatrixProfile parseSynqMatrixProfile(const char* value, bool* ok = nullptr);
SynqMatrixState parseSynqMatrixState(const char* value, bool* ok = nullptr);
```

11 of these still use the legacy `songAware` prefix — see Anomalies §4.

---

## 2. Plugin API (`namespace lightwaveos::plugins`)

### 2.1 `IEffect.h`

Enum `EffectCategory : uint8_t` — `UNCATEGORIZED=0`, `FIRE`, `WATER`, `NATURE`, `GEOMETRIC`, `QUANTUM`, `SHOCKWAVE`, `AMBIENT`, `PARTY`, `CUSTOM`, `LEGACY_LINEAR`.

Enum `EffectRoleFlags : uint8_t` (bitmask) — `NONE=0`, `SELF_TRAILING=1<<0`, `RENDERS_COLOUR_ONLY=1<<1`, `RENDERS_GEOMETRY_ONLY=1<<2`, `INVERT_INPUT_OK=1<<3`, `BACKGROUND=1<<4`, `OPTS_OUT_OF_PERSISTENCE=1<<5`, `DUAL_CHANNEL=1<<6`.

Enum `EffectParameterType : uint8_t` — `FLOAT=0`, `INT=1`, `BOOL=2`, `ENUM=3`.

Struct `EffectMetadata` — fields: `name`, `description`, `category`, `version`, `author`, `roleFlags`, `id`. Constructor `EffectMetadata(n,d,c,v,a,r)`.

Struct `EffectParameter` — fields: `name`, `displayName`, `minValue`, `maxValue`, `defaultValue`, `type`, `step`, `group`, `unit`, `advanced`. Two constructors (10-arg full, 5-arg backward-compat).

Class `IEffect` (abstract):

```
virtual bool init(EffectContext& ctx) = 0;
virtual void render(EffectContext& ctx) = 0;
virtual void cleanup() = 0;
virtual const EffectMetadata& getMetadata() const = 0;
virtual uint8_t getParameterCount() const;          // default 0
virtual const EffectParameter* getParameter(uint8_t index) const;  // default nullptr
virtual bool setParameter(const char* name, float value);  // default false
virtual float getParameter(const char* name) const;        // default 0.0f
```

### 2.2 `OnsetContext.h`

Struct `OnsetRawSignals` — `flux`, `env`, `event`, `bassFlux`, `midFlux`, `highFlux`.
Struct `OnsetChannel` — `fired`, `strength01`, `level01`, `ageMs`, `intervalMs`, `sequence`, `reliable`.
Struct `OnsetContext` — `beat`, `downbeat`, `transient`, `kick`, `snare`, `hihat` (all `OnsetChannel`); `raw` (OnsetRawSignals); `phase01`, `bpm=120.0f`, `tempoConfidence`, `timingReliable`.

### 2.3 `IEffectRegistry.h`

```
class IEffectRegistry {
    virtual bool registerEffect(EffectId id, IEffect* effect) = 0;
    virtual bool unregisterEffect(EffectId id) = 0;
    virtual bool isEffectRegistered(EffectId id) const = 0;
    virtual uint16_t getRegisteredCount() const = 0;
};
```

### 2.4 `BehaviorSelection.h`

Enum `VisualBehavior : uint8_t` — `PULSE_ON_BEAT=0`, `DRIFT_WITH_HARMONY=1`, `SHIMMER_WITH_MELODY=2`, `BREATHE_WITH_DYNAMICS=3`, `TEXTURE_FLOW=4`.

Enum `PaletteStrategy : uint8_t` — `RHYTHMIC_SNAP=0`, `HARMONIC_COMMIT=1`, `MELODIC_DRIFT=2`, `TEXTURE_EVOLVE=3`, `DYNAMIC_WARMTH=4`.

Struct `BehaviorContext` — `currentStyle`, `styleConfidence`, `recommendedPrimary`, `recommendedSecondary`, `saliencyFrame` (`const audio::MusicalSaliencyFrame*`). Methods: `bool isConfident(float threshold=0.3f) const`, `float getPrimaryBlend() const`.

Struct `StyleTiming` — `phraseGateDuration`, `buildThreshold`, `holdDuration`, `releaseSpeed`, `quietThreshold`, `colorTransitionSpeed`, `motionTransitionSpeed`, `attackMultiplier`, `decayMultiplier`. Method `static StyleTiming forStyle(audio::MusicStyle)`.

Struct `SaliencyEmphasis` — `colorEmphasis`, `motionEmphasis`, `textureEmphasis`, `intensityEmphasis`. Methods `static SaliencyEmphasis fromSaliency(const audio::MusicalSaliencyFrame&)`, `static SaliencyEmphasis neutral()`.

Free functions:

```
const char* getVisualBehaviorName(VisualBehavior behavior);
BehaviorContext selectBehavior(audio::MusicStyle style,
                               const audio::MusicalSaliencyFrame& saliency,
                               float confidence = 0.5f);
PaletteStrategy selectPaletteStrategy(audio::MusicStyle style);
```

`#if !FEATURE_AUDIO_SYNC` stub: `audio::MusicStyle` enum mirror (`UNKNOWN=0`, `RHYTHMIC_DRIVEN=1`, `HARMONIC_DRIVEN=2`, `MELODIC_DRIVEN=3`, `TEXTURE_DRIVEN=4`, `DYNAMIC_DRIVEN=5`).

### 2.5 `EffectContext.h`

Enum `MusicalRange : uint8_t` — `SUB_BASS`, `BASS`, `LOW_MID`, `MID`, `PRESENCE`, `TREBLE`, `FULL`.

#### 2.5.1 `struct AudioContext` (FEATURE_AUDIO_SYNC variant — primary surface)

Fields:

- `audio::ControlBusFrame controlBus`
- `audio::MusicalGridSnapshot musicalGrid`
- `bool available = false`
- `bool trinityActive = false`
- `OnsetContext onset`
- `BehaviorContext behaviorContext`
- `audio::MotionSemanticFrame motionFrame`
- `audio::MotionShaping motionShaping`

Accessor inventory (grouped by domain):

| Group | Accessors |
|---|---|
| Energy | `rms()`, `fastRms()`, `flux()`, `fastFlux()` |
| Bands | `getBand(i)`, `getHeavyBand(i)`, `bands()`, `heavyBands()`, `bass()`, `heavyBass()`, `mid()`, `heavyMid()`, `treble()`, `heavyTreble()` |
| Beat / tempo | `beatPhase()`, `isOnBeat()`, `isOnDownbeat()`, `bpm()`, `tempoConfidence()`, `audioConfidence()`, `tempoBeatTick()`, `tempoBeatConfidence()`, `beatInBar()`, `beatStrength()` |
| Scene | `sceneParameters()`, `motionType()`, `phraseProgress()`, `tension()`, `beatPulse()`, `timingReliable()` |
| Waveform | `waveformSize()`, `getWaveformSample(i)`, `getWaveformAmplitude(i)`, `getWaveformNormalized(i)`, `waveform()`, `sbWaveform()`, `sbWaveformPeakScaled()`, `hasSbWaveform()`, `preferredWaveform()` |
| Chord | `chordState()`, `chordType()`, `rootNote()`, `chordConfidence()`, `isMajor()`, `isMinor()`, `isDiminished()`, `isAugmented()`, `hasChord()` |
| Hop/Chroma/State | `hopSequence()`, `getChroma(i)`, `getHeavyChroma(i)`, `chroma()`, `heavyChroma()`, `liveliness()`, `silentScale()`, `isSilent()` |
| Onset surface | `hasOnsetEvent()`, `onsetEnv()`, `onsetEvent()`, `onsetFlux()`, `kickFlux()`, `snareFlux()`, `hihatFlux()`, `kickLevel()`, `snare()`, `hihat()`, `isKickHit()`, `isSnareHit()`, `isHihatHit()` |
| HF semantics (`FEATURE_AUDIO_HF_SEMANTICS`) | `hfEnergy()`, `hfFlux()`, `hatEvent()`, `hatEventInfo()`, `cymbalSustain()`, `airEnergy()`, `spectralBrightness()`, `brightness()`, `spectralBrightnessDelta()` |
| 64-bin FFT | static `bins64Count()`, `bin(i)`, `bins64()`, `binAdaptive(i)`, `bins64Adaptive()`, `musicalBin(i)`, `musicalRange(lo,hi)`, `musicalRange(MusicalRange)` |
| 256-bin FFT (PipelineCore) | static `bins256Count()`, `bins256()`, `binHz()`, `namedBandEnergy(::audio::NamedBand)`, `subBass()`, `kick()`, `lowMid()`, `midPresence()`, `shimmer()`, `air()`, `energyInRange(freqLo,freqHi)` |
| Saliency | `saliencyFrame()`, `overallSaliency()`, `isHarmonicDominant()`, `isRhythmicDominant()`, `isTimbralDominant()`, `isDynamicDominant()`, `harmonicSaliency()`, `rhythmicSaliency()`, `timbralSaliency()`, `dynamicSaliency()` |
| Music style | `musicStyle()`, `styleConfidence()`, `isRhythmicMusic()`, `isHarmonicMusic()`, `isMelodicMusic()`, `isTextureMusic()`, `isDynamicMusic()` |
| Motion semantics | `motionWeight()`, `motionTime()`, `motionSpace()`, `motionFlow()`, `motionFluidity()`, `motionImpulse()`, `motionConfidence()` |
| Motion shaping (Layer 3) | `shapedIntensity()`, `shapedDecayMs()`, `shapedAccent()`, `shapingActive()` |
| Behaviour | `recommendedBehavior()`, `shouldPulseOnBeat()`, `shouldDriftWithHarmony()`, `shouldShimmerWithMelody()`, `shouldBreatheWithDynamics()`, `shouldTextureFlow()` |

#### 2.5.2 `struct AudioContext` (stub variant, `!FEATURE_AUDIO_SYNC`)

Mirrors the same accessor names with neutral defaults; defines internal stubs `StubChordState`, `StubSaliencyFrame`, `StubMotionFrame`. Notable signature drift in stub: `chordType()` returns `uint8_t` instead of `audio::ChordType`; `musicStyle()` returns `uint8_t`; `saliencyFrame()` and `chordState()` return their stub structs by value.

#### 2.5.3 `class PaletteRef`

```
PaletteRef();
explicit PaletteRef(const CRGBPalette16* palette);     // !NATIVE_BUILD
explicit PaletteRef(const void* palette);              // NATIVE_BUILD
CRGB getColor(uint8_t index, uint8_t brightness = 255) const;
bool isValid() const;
const CRGBPalette16* getRaw() const;                   // !NATIVE_BUILD
const void* getRaw() const;                            // NATIVE_BUILD
```

#### 2.5.4 `struct EffectContext`

Fields:

- LED buffer: `CRGB* leds`, `uint16_t ledCount`, `uint16_t centerPoint`
- Palette: `PaletteRef palette`
- Global animation: `uint8_t brightness`, `uint8_t speed`, `uint8_t gHue`, `uint8_t mood`
- Visual enhancement: `uint8_t intensity`, `uint8_t saturation`, `uint8_t complexity`, `uint8_t variation`, `uint8_t fadeAmount`
- Timing: `uint32_t deltaTimeMs`, `float deltaTimeSeconds`, `uint32_t rawDeltaTimeMs`, `float rawDeltaTimeSeconds`, `uint32_t frameNumber`, `uint32_t totalTimeMs`, `uint32_t rawTotalTimeMs`
- Zone: `uint8_t zoneId`, `uint16_t zoneStart`, `uint16_t zoneLength`
- Dual-strip channel API: `CRGB* stripLeds[2]`, `uint16_t stripLength`, `uint8_t stripCount`, `uint16_t stripCenter`, `bool dualChannelMode`
- Audio: `AudioContext audio`

Methods:

```
float getDistanceFromCenter(uint16_t index) const;
float getSignedPosition(uint16_t index) const;
uint16_t mirrorIndex(uint16_t index) const;
float getDistanceFromStripCenter(uint16_t ledIdx) const;
float getPhase(float frequencyHz) const;
float getSineWave(float frequencyHz) const;
bool isZoneRender() const;
float getMoodNormalized() const;
void getMoodSmoothing(float& riseOut, float& fallOut) const;
float getSafeDeltaSeconds() const;
float getSafeRawDeltaSeconds() const;
```

---

## 3. Plugin runtime + registry (`namespace lightwaveos::plugins`)

### 3.1 `struct PluginConfig` (constants)

`MAX_EFFECTS=256`, `MAX_MANIFESTS=16`, `LITTLEFS_PLUGIN_PATH_MAX=64`, `MANIFEST_CAPACITY=2048`, `ERROR_MSG_MAX=128`, `PLUGIN_NAME_MAX=64`.

### 3.2 `struct ParsedManifest`

`filePath[…]`, `pluginName[…]`, `valid`, `errorMsg[…]`, `overrideMode`, `effectIds[…]`, `effectCount`.

### 3.3 `struct PluginStats`

`registeredCount`, `loadedFromLittleFS`, `registrationsFailed`, `unregistrations`, `overrideModeEnabled`, `disabledByOverride`, `lastReloadMillis`, `lastReloadOk`, `manifestCount`, `errorCount`, `lastErrorSummary`.

### 3.4 `class PluginManagerActor : public IEffectRegistry`

```
PluginManagerActor();
void setTargetRegistry(IEffectRegistry* target);
void onStart();
// IEffectRegistry:
bool registerEffect(EffectId id, IEffect* effect) override;
bool unregisterEffect(EffectId id) override;
bool isEffectRegistered(EffectId id) const override;
uint16_t getRegisteredCount() const override;
// Plugin loading:
void loadPluginsFromLittleFS();
bool reloadFromLittleFS();
// Diagnostics:
const PluginStats& getStats() const;
const ParsedManifest* getManifest(uint8_t index) const;
const ParsedManifest* getManifests() const;
uint8_t getManifestCount() const;
// Private:
uint8_t scanManifestFiles();
bool parseManifest(const char* path, ParsedManifest& manifest);
bool validateManifest(ParsedManifest& manifest);
bool applyManifests();
void clearRegistrations();
int16_t findEffectSlot(EffectId id) const;
bool isEffectAllowed(EffectId id) const;
```

Nested `struct EffectSlot { EffectId id; IEffect* effect; }`. Members: `m_targetRegistry`, `m_effectSlots[MAX_EFFECTS]`, `m_effectSlotCount`, `m_stats`, `m_overrideMode`, `m_manifests[MAX_MANIFESTS]`, `m_manifestCount`, `m_allowedIds[MAX_EFFECTS]`, `m_allowedIdCount`.

### 3.5 `class BuiltinEffectRegistry` (static singleton)

```
static constexpr uint16_t MAX_EFFECTS = 256;
static bool registerBuiltin(EffectId id, IEffect* effect);
static IEffect* getBuiltin(EffectId id);
static bool hasBuiltin(EffectId id);
static uint16_t getBuiltinCount();
static void clear();
// private:
struct Entry { EffectId id; IEffect* effect; };
static Entry* s_entries;
static uint16_t s_count;
static bool ensureStorage();
```

### 3.6 `class LegacyEffectAdapter : public IEffect` (`namespace lightwaveos::plugins::runtime`)

```
LegacyEffectAdapter(const char* name, actors::EffectRenderFn fn);
~LegacyEffectAdapter() override = default;
bool init(EffectContext& ctx) override;
void render(EffectContext& ctx) override;
void cleanup() override;
const EffectMetadata& getMetadata() const override;
// members:
const char* m_name;
actors::EffectRenderFn m_fn;
mutable EffectMetadata m_metadata;
actors::RenderContext m_tempRenderContext;
```

---

## 4. Anomalies — leftover `songAware` / `SongAware` / "song" residue

Anchor #51311 confirmed and extended.

### 4.1 Helper-function rename oversight (`SynqMatrix.h` lines 496–509, `SynqMatrix.cpp` lines 1444–1593)

Eleven free helpers retain the `songAware*` prefix despite the class and enums having been renamed to `SynqMatrix*`. Definitions in `SynqMatrix.cpp` match these declarations verbatim, and two internal call sites still invoke them:

- `SynqMatrix.cpp:313` — `status.lastSwitchReason = songAwareSwitchReasonName(...)`
- `SynqMatrix.cpp:739` — `request.reason = songAwareSwitchReasonName(policy.reason)`

Recommended rename: `songAware<X>Name(...)` → `synqMatrix<X>Name(...)`.

### 4.2 Parser-function naming inconsistency (`SynqMatrix.h:507–509`)

```
SynqMatrixMode    parseSynqMatrixMode(const char* value, bool* ok = nullptr);
SynqMatrixProfile parseSynqMatrixProfile(const char* value, bool* ok = nullptr);
SynqMatrixState   parseSynqMatrixState(const char* value, bool* ok = nullptr);
```

These three were renamed but their `songAware*Name` counterparts were not — naming convention is inconsistent within the same translation unit. Either rename helpers to `synqMatrix<X>Name` (matches camel-cased struct prefix elsewhere) or rename parsers to `parseSongAware*` (regression — not recommended). The forward-direction rename is the only consistent fix.

### 4.3 Struct field "song" leakage (`SynqMatrixStatus`, `SynqMatrix.h:163–166`)

Four status fields still embed "song":

```
SynqMatrixState rawSongState;
SynqMatrixState previousSongState;
SynqMatrixState currentSongState;
SynqMatrixState candidateSongState;
```

The atomic mirrors retain the same naming: `m_rawSongState`, `m_previousSongState`, `m_currentSongState` (no `m_candidateSongState` — the candidate atomic is `m_candidateState`, which is itself inconsistent with the status field name `candidateSongState`).

Recommended: rename status fields to `rawState`, `previousState`, `currentState`, `candidateState`; rename atomics symmetrically (`m_rawState`, …); confirm renderer/REST/WS contract is updated (anchor #51311 reports REST/JSON serialisation downstream of these field names — not in scope here, flag for next SSA).

### 4.4 File-header comment drift (`SynqMatrix.h:3`, `SynqMatrix.cpp:3`)

Both files describe themselves as "Runtime-only synq-matrix parameter and visual-language director." The class itself is `SynqMatrix` — the dual-spelling ("synq-matrix" hyphenated lowercase vs `SynqMatrix` camel-cased) is cosmetic but suggests the brand naming was applied at the symbol level and not at the prose level.

### 4.5 No song-aware residue in plugins surface

Grep across `plugins/api/`, `plugins/runtime/`, `plugins/BuiltinEffectRegistry.{h,cpp}`, `plugins/PluginManagerActor.{h,cpp}` returns zero matches for `songAware`, `SongAware`, `SynqMatrix`. The plugin API and runtime do not reference SynqMatrix — coupling flows only from RendererActor / consumers, which are out of scope for this enumeration.

### 4.6 Rename surface in plugin API (no anomaly, suggestion-only)

`AudioContext` accessors continue to expose effect-facing names that mirror legacy `ControlBus` semantics (e.g. `es_*` field-level access reflected only in `tempoBeatTick()` / `tempoBeatConfidence()` / `beatInBar()` via OR-fallback to `controlBus.es_*` fields). Post-migration polish opportunity: if the `es_*` prefix in `ControlBusFrame` is itself being renamed (per related anchor #51315), audit accessor implementations rather than accessor names — the accessor names are already clean.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-05-13 | agent:claude-opus-4-7 | Created. Read-only enumeration across SynqMatrix.{h,cpp} + plugins/api/* + plugins/{BuiltinEffectRegistry,PluginManagerActor}.{h,cpp} + plugins/runtime/LegacyEffectAdapter.{h,cpp}. Anchor #51311 leftover `songAware*` and "song" struct field residue documented. |
