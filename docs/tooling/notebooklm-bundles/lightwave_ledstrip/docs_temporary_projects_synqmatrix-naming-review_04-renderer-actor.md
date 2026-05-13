---
abstract: "SSA-N04 naming extraction for firmware-v3/src/core/actors/RendererActor.{h,cpp} — the 120 FPS render-loop actor (Core 1). Enumerates RendererActor class, its nested structs (LedConfig, RenderStats, RenderContext, VpStackSnapshot, BandsDebugSnapshot, CaptureMetadata, EffectRegistration, EffectParamUpdate), the AudioInputMode / RendererMode / CaptureTap enums, all method signatures grouped by logical area (frame lifecycle, effect registry, audio/Trinity sync, SynqMatrix transitions, capture, transitions, NVS, parameter handlers, telemetry, validation), all m_* member fields with one-line context, and the full onMessage switch case list. Names + signatures only; no bodies. British English in prose; original code spelling preserved."
---

# SSA-N04 — RendererActor Naming Inventory

**Files inspected (2):**
- `firmware-v3/src/core/actors/RendererActor.h` (1145 lines)
- `firmware-v3/src/core/actors/RendererActor.cpp` (3054 lines)

## Subsystem Heading

Renderer subsystem. Runs on Core 1 at top priority. Owns LED buffer, runs the 120 FPS render loop, processes effect/parameter/transition/Trinity messages, hosts the MusicalGrid/EsBeatClock PLL, drives the SynqMatrix song-aware director transition state machine, and exposes capture taps + telemetry snapshots to network consumers.

## Convention Snapshot

- Namespace: `lightwaveos::actors`. Cross-namespace forward decls: `lightwaveos::zones::ZoneComposer`, `lightwaveos::transitions::TransitionEngine`/`TransitionType`, `lightwaveos::plugins::IEffect`/`plugins::runtime::LegacyEffectAdapter`, `lightwaveos::synqmatrix::SynqMatrixSwitchRequest`.
- Class naming: PascalCase (`RendererActor`, `RenderStats`, `LedConfig`, `RenderContext`, `VpStackSnapshot`, `BandsDebugSnapshot`, `CaptureMetadata`, `EffectRegistration`, `EffectParamUpdate`).
- Enum naming: `enum class` PascalCase with PascalCase values (`AudioInputMode::Live|Trinity|StimulusOverride`, `RendererMode::Unified|Independent`). One exception — `CaptureTap` uses SHOUTY_SNAKE values (`TAP_A_PRE_CORRECTION`, `TAP_B_POST_CORRECTION`, `TAP_C_PRE_WS2812`).
- Method naming: camelCase verbs (`renderFrame`, `showLeds`, `initLeds`, `handleSet…`). Accessors prefixed `get…` / `set…` / `is…`.
- Member fields: `m_` prefix + camelCase (`m_currentEffect`, `m_brightness`, `m_lastControlBus`).
- Constants in nested structs: SHOUTY_SNAKE_CASE (`LEDS_PER_STRIP`, `TOTAL_LEDS`, `TARGET_FPS`, `MAX_EFFECTS`, `PARAM_QUEUE_SIZE`, `VAL_CMD_QUEUE_SIZE`, `CAPTURE_DRAIN_TIMEOUT_MS`).
- File-scope helpers in anonymous namespace: camelCase (`computeSpeedTimeFactor`, `smoothTimingUs`, `songAwareTransitionForReason`, `toMusicalGridTuning`). One `k`-prefixed constant (`kHueAutoRotatePauseMs`, `kMinSpeedTimeFactor`, `kToneMapLUT`).
- Comments and log strings: British English ("centre", "colour", "initialise", "behaviour") consistently observed.
- Cross-cutting naming note: SynqMatrix-related transition members still carry the historical `m_songAwareDirector…` prefix (queued, preparing, activeNotified, previousEffect, targetEffect, targetFamily, targetLanguage, transitionReason); the helper `songAwareTransitionForReason()` and namespace functions `songAwareStateName` / `SongAwareState` likewise survive under the older name. The free-function helpers and instance accessor live in `lightwaveos::synqmatrix` namespace under the new name.

## Per-Class / Per-Struct Blocks

### enum `AudioInputMode : uint8_t` (header lines 93–97)
Values: `Live = 0`, `Trinity = 1`, `StimulusOverride = 2`.

### struct `LedConfig` (header lines 106–124)
Static constexprs: `LEDS_PER_STRIP`, `NUM_STRIPS`, `TOTAL_LEDS`, `STRIP1_PIN`, `STRIP2_PIN`, `TARGET_FPS`, `FRAME_TIME_US`, `DEFAULT_BRIGHTNESS`, `MAX_BRIGHTNESS`, `DEFAULT_SPEED`, `MAX_SPEED`, `CENTER_LED_INDEX`.

### struct `RenderStats` (header lines 129–152)
Fields: `framesRendered`, `frameDrops`, `avgFrameTimeUs`, `maxFrameTimeUs`, `minFrameTimeUs`, `currentFPS`, `cpuPercent`. Methods: `RenderStats()`, `void reset()`.

### enum `RendererMode : uint8_t` (header lines 164–167)
Values: `Unified = 0`, `Independent = 1`.

### struct `RenderContext` (header lines 175–189)
Fields: `leds`, `numLeds`, `brightness`, `speed`, `hue`, `intensity`, `saturation`, `complexity`, `variation`, `frameCount`, `deltaTimeMs`, `palette`.

### type alias `EffectRenderFn` (header line 194)
`using EffectRenderFn = void (*)(RenderContext& ctx);`

### class `RendererActor` (header lines 209–1142)
Bases: `Actor`, `plugins::IEffectRegistry`, `lightwaveos::zones::IZoneEffectSource`.

#### Construction / destruction
- `RendererActor()`
- `~RendererActor() override`

#### State accessors (read-only)
- `EffectId getCurrentEffect() const`
- `uint8_t getBrightness() const`
- `uint8_t getSpeed() const`
- `uint8_t getPaletteIndex() const`
- `uint8_t getHue() const`
- `uint8_t getIntensity() const`
- `uint8_t getSaturation() const`
- `uint8_t getComplexity() const`
- `uint8_t getVariation() const`
- `uint8_t getMood() const`
- `uint8_t getFadeAmount() const`
- `RendererMode getRendererMode() const`
- `void setRendererMode(RendererMode mode)`
- `EffectId getStripEffectId(uint8_t stripIdx) const`
- `void setStripEffectId(uint8_t stripIdx, EffectId eid)`
- `const RenderStats& getStats() const`
- `bool isLedOutputBusy() const`
- `const hal::LedDriverStats& getLedDriverStats() const`
- `bool isLedDitheringEnabled() const`

#### Nested `struct VpStackSnapshot` (header lines 258–315)
Fields: `effectId`, `effectName`, `paletteId`, `paletteName`, `brightness`, `speed`, `intensity`, `saturation`, `complexity`, `variation`, `hue`, `mood`, `rendererMode`, `topology`, `surfaces`, `renderStats`, `ledStats`, `lastEffectRenderUs`, `avgEffectRenderUs`, `lastColourCorrectionUs`, `avgColourCorrectionUs`, `lastShowLedsUs`, `avgShowLedsUs`, `lastOutputPrepUs`, `avgOutputPrepUs`, `lastPrePacingWorkUs`, `avgPrePacingWorkUs`, `ledDitheringEnabled`, `colourCorrectionToggleEnabled`, `colourCorrectionSkippedByEffect`, `colourCorrectionApplied`, `correctionApplyCount`, `correctionSkipCount`, `colourConfig`, `gamma`, `toneMapNeeded`, `audioAvailable`, `globalSilenceBypassed`, `globalSilenceScaleActive`, `hardSilenceGateEffect`, `silentScale`, `edgeMode`, `edgeSpatial`, `edgeTemporal`, `edgeSpread`, `edgeStrength`, `captureEnabled`, `captureTapMask`, `captureEffectId`, `capturePaletteId`, `captureBrightness`, `captureSpeed`, `captureFrameIndex`, `captureTimestampUs`, `wireFenceActive`, `expectedWireTimeUs`.

Accessor: `VpStackSnapshot getVpStackSnapshot() const`.

#### Buffer accessor
- `void getBufferCopy(CRGB* outBuffer) const`

#### Effect registration
- `bool registerEffect(EffectId id, const char* name, EffectRenderFn fn)`
- `bool registerEffect(EffectId id, plugins::IEffect* effect) override`
- `bool registerEffectFactory(EffectId id, lightwaveos::zones::EffectFactoryFn factory)`

#### `IEffectRegistry` implementation
- `bool unregisterEffect(EffectId id) override`
- `bool isEffectRegistered(EffectId id) const override`
- `uint16_t getRegisteredCount() const override`
- `uint16_t getEffectCount() const`
- `const char* getEffectName(EffectId id) const`
- `plugins::IEffect* getEffectInstance(EffectId id) const override`
- `lightwaveos::zones::EffectFactoryFn getEffectFactory(EffectId id) const override`
- `EffectId validateEffectId(EffectId effectId) const`
- `EffectId getEffectIdAt(uint16_t index) const`
- `CRGBPalette16* getPalette()`
- `CRGB* getLedBuffer()`
- `uint8_t getPaletteCount() const`
- `const char* getPaletteName(uint8_t id) const`

#### Zone-system integration
- `void setZoneComposer(zones::ZoneComposer* composer)`
- `zones::ZoneComposer* getZoneComposer()`

#### Transition-system integration
- `bool isTransitionActive() const`
- `transitions::TransitionEngine* getTransitionEngine()` (returns `nullptr` when `FEATURE_TRANSITIONS` off)

#### Audio integration (`FEATURE_AUDIO_SYNC`)
- `void setAudioBuffer(const audio::SnapshotBuffer<audio::ControlBusFrame>* buffer)`
- `void setStimulusBuffer(const audio::SnapshotBuffer<audio::ControlBusFrame>* buffer)`
- `void setAudioInputMode(AudioInputMode mode)`
- `bool isAudioEnabled() const`
- `audio::AudioContractTuning getAudioContractTuning() const`
- `void setAudioContractTuning(const audio::AudioContractTuning& tuning)`
- `void copyCachedAudioFrame(audio::ControlBusFrame& out) const`
- `const audio::MotionSemanticFrame& getMotionFrame() const`
- `const audio::MotionShaping& getMotionShaping() const`
- `void copyLastMusicalGrid(audio::MusicalGridSnapshot& out) const`
- `void copyCachedAudioSnapshot(audio::ControlBusFrame& outFrame, audio::MusicalGridSnapshot& outGrid) const`
- `const char* getAudioSyncMode() const`
- `void getBandsDebugSnapshot(BandsDebugSnapshot& out) const`
- `metrics::VRMSVector getVrmsVector() const` (gated `FEATURE_VRMS_METRICS`)
- `uint8_t getMergedParam(uint8_t idx) const` (gated `FEATURE_INPUT_MERGE_LAYER`)

#### Nested `struct BandsDebugSnapshot` (header lines 581–590)
Fields: `bands[8]`, `bass`, `mid`, `treble`, `rms`, `flux`, `hop_seq`, `valid`.

#### TempoTracker integration (non-ESV11 / non-PipelineCore branch)
- `void setTempo(lightwaveos::audio::TempoTracker* tempo)`
- `bool isTempoEnabled() const`
- `lightwaveos::audio::TempoTrackerOutput getTempoOutput() const`

#### Frame-capture system
- `enum class CaptureTap : uint8_t { TAP_A_PRE_CORRECTION, TAP_B_POST_CORRECTION, TAP_C_PRE_WS2812 }`
- `void setCaptureMode(bool enabled, uint8_t tapMask = 0x07)`
- `bool isCaptureModeEnabled() const`
- `void setAudioDebugEnabled(bool enabled)`
- `bool isAudioDebugEnabled() const`
- `bool getCapturedFrame(CaptureTap tap, CRGB* outBuffer) const`
- `bool enqueueEffectParameterUpdate(EffectId effectId, const char* name, float value)`
- Nested `struct CaptureMetadata`: `effectId`, `paletteId`, `brightness`, `speed`, `frameIndex`, `timestampUs`.
- `CaptureMetadata getCaptureMetadata() const`
- `void forceOneShotCapture(CaptureTap tap)`

#### Actor lifecycle overrides (protected)
- `void onStart() override`
- `void onMessage(const Message& msg) override`
- `void onTick() override`
- `void onStop() override`

#### Cross-core-unsafe transition starters (private)
- `void startTransition(EffectId newEffectId, uint8_t transitionType)`
- `void startRandomTransition(EffectId newEffectId)`

#### Internal frame lifecycle (private)
- `void initLeds()`
- `void renderFrame()`
- `void renderStripIndependent(uint8_t stripIdx, EffectId eid, uint32_t deltaTimeMs, const audio::ControlBusFrame& audioFrame, bool audioAvailable, bool trinityActive)` — audio-arg trio gated by `FEATURE_AUDIO_SYNC`.
- `void applyPendingAudioContractTuning()`
- `void applyPendingEffectParameterUpdates()`
- `void updateSharedOnsetContext(const audio::ControlBusFrame& frame, const audio::MusicalGridSnapshot& grid, bool available, bool trinityActive, uint32_t nowMs, float dtSeconds)`
- `void populateAudioContextForRender(plugins::AudioContext& out, const audio::ControlBusFrame& frame, const audio::MusicalGridSnapshot& grid, bool available, bool trinityActive, bool includeBehaviorContext)`
- `void showLeds()`
- `void updateStats(uint32_t frameTimeUs, uint32_t rawFrameTimeUs)`
- `void captureFrame(CaptureTap tap, const CRGB* sourceBuffer)`

#### SynqMatrix integration methods (private, audio-sync gated)
- `void queueSynqMatrixTransition(const synqmatrix::SynqMatrixSwitchRequest& request, EffectId previousEffectId)`
- `bool processSynqMatrixTransition(uint32_t nowMs)`
- `void syncSynqMatrixTransitionTelemetry(uint32_t nowMs)`

#### Message handlers (private)
- `void handleSetEffect(EffectId effectId)`
- `void handleStartTransition(EffectId effectId, uint8_t transitionType)`
- `void handleSetBrightness(uint8_t brightness)`
- `void handleSetSpeed(uint8_t speed)`
- `void handleSetPalette(uint8_t paletteIndex)`
- `void handleSetIntensity(uint8_t intensity)`
- `void handleSetSaturation(uint8_t saturation)`
- `void handleSetComplexity(uint8_t complexity)`
- `void handleSetVariation(uint8_t variation)`
- `void handleSetHue(uint8_t hue)`
- `void handleSetMood(uint8_t mood)`
- `void handleSetFadeAmount(uint8_t fadeAmount)`

#### Nested `struct EffectRegistration` (header lines 879–889)
Fields: `id`, `name`, `effect`, `legacyAdapter`, `factory`, `active`.

#### Nested `struct EffectParamUpdate` (header lines 897–901)
Fields: `effectId`, `name[24]`, `value`.

#### Registry lookup helpers (private)
- `EffectRegistration* findById(EffectId id)`
- `const EffectRegistration* findById(EffectId id) const`

#### Public validation-command queue accessor
- `bool enqueueValidationCommand(const char* cmd)`
- `bool isValidationActive() const`

### Free helpers in anonymous namespace (`.cpp` 63–135)
- `constexpr uint32_t kHueAutoRotatePauseMs`
- `TransitionType songAwareTransitionForReason(const char* reason)` (gated by `FEATURE_AUDIO_SYNC && FEATURE_TRANSITIONS`)
- `constexpr uint8_t kToneMapLUT[256]`
- `constexpr float kMinSpeedTimeFactor`
- `float computeSpeedTimeFactor(uint8_t speed)`
- `uint32_t smoothTimingUs(uint32_t avgUs, uint32_t sampleUs)`

### File-scope helpers (`.cpp` 158–168, 602–613)
- `static audio::MusicalGridTuning toMusicalGridTuning(const audio::AudioContractTuning& tuning)` (audio-sync gated)
- Frame pacer task globals: `static TaskHandle_t s_framePacerTaskHandle`, `static esp_timer_handle_t s_framePacerTimer`, `static void framePacerTimerCallback(void* arg)`.
- Legacy stub: `void setCurrentLegacyEffectId(uint8_t)` (no-op shim, namespace `lightwaveos::actors`).

## Member Variables (full inventory, `m_*` fields)

### LED buffers
- `CRGB* m_strip1`, `CRGB* m_strip2` — per-strip pointers handed to `LedDriver`.
- `CRGB m_leds[LedConfig::TOTAL_LEDS]` — unified 320-LED buffer (3 × 320 = 960 bytes in-class).

### Current effect / parameter state
- `EffectId m_currentEffect`
- `bool m_effectInitialized` — set true after first `IEffect::init()` for the current effect.
- `bool m_currentEffectValid` — cached `validateEffectId()` success.
- `EffectId m_validatedEffectId` — cached last successful validation result.
- `uint8_t m_brightness`, `m_speed`, `m_paletteIndex`, `m_hue`.
- `uint32_t m_hueLastUserSetMs` — gate timestamp for the 30 s hue auto-rotate suppression after user slider input.
- `uint8_t m_intensity`, `m_saturation`, `m_complexity`, `m_variation`, `m_mood`, `m_fadeAmount`.

### Dual-strip independence state (Phase 1B)
- `RendererMode m_rendererMode` (default `Unified`).
- `EffectId m_stripEffectId[2]` — per-strip effect IDs when `Independent`.

### Palette
- `CRGBPalette16 m_currentPalette` — initialised from `gMasterPalettes[0]` (Sunset Real).

### Effect registry
- `static constexpr uint16_t MAX_EFFECTS = limits::MAX_EFFECTS`
- `EffectRegistration m_registry[MAX_EFFECTS]`
- `uint16_t m_registryCount`

### Pending parameter queue (lock-free)
- `static constexpr uint8_t PARAM_QUEUE_SIZE = 16`
- `EffectParamUpdate m_paramQueue[PARAM_QUEUE_SIZE]`
- `std::atomic<uint8_t> m_paramQueueHead{0}`
- `std::atomic<uint8_t> m_paramQueueTail{0}`

### Timing
- `uint32_t m_lastFrameTime`
- `uint32_t m_frameCount`
- `float m_effectTimeSeconds`, `m_effectTimeSecondsRaw`, `m_effectFrameAccumulator`
- `uint32_t m_effectFrameCount`

### Statistics
- `RenderStats m_stats`
- Telemetry running-averages: `m_lastEffectRenderUs`, `m_avgEffectRenderUs`, `m_lastColourCorrectionUs`, `m_avgColourCorrectionUs`, `m_lastShowLedsUs`, `m_avgShowLedsUs`, `m_lastOutputPrepUs`, `m_avgOutputPrepUs`, `m_lastPrePacingWorkUs`, `m_avgPrePacingWorkUs`.

### Output driver / zones / contexts
- `hal::LedDriver m_ledDriver`
- `zones::ZoneComposer* m_zoneComposer`
- `plugins::EffectContext m_effectContext` — reused per frame to avoid stack allocation.

### Shared audio contexts (audio-sync gated)
- `plugins::AudioContext m_sharedAudioCtx`
- `plugins::OnsetContext m_sharedOnsetCtx`
- `audio::OnsetSemanticTrackerState m_onsetTrackerState`
- `audio::MotionSemanticEngine m_motionEngine` — Layer 2 (ControlBusFrame → 6-axis motion-semantic frame).
- `audio::MotionShaper m_motionShaper` — Layer 3 (onset-driven temporal envelope shaping).

### Validation mode (Stage 3B)
- `serial::ValidationMode m_validationMode{m_leds, LedConfig::TOTAL_LEDS}`
- Lock-free command queue: `m_valCmdQueue[VAL_CMD_QUEUE_SIZE][VAL_CMD_MAX_LEN]`, `std::atomic<uint8_t> m_valCmdHead`, `std::atomic<uint8_t> m_valCmdTail`. Sizes: `VAL_CMD_QUEUE_SIZE = 4`, `VAL_CMD_MAX_LEN = 256`.

### Transition system (`FEATURE_TRANSITIONS`)
- `transitions::TransitionEngine* m_transitionEngine`
- `CRGB m_transitionSourceBuffer[LedConfig::TOTAL_LEDS]`
- `EffectId m_pendingEffect`
- `bool m_transitionPending`

### Frame capture
- `bool m_captureEnabled`, `uint8_t m_captureTapMask`.
- `mutable volatile uint32_t m_captureLastDrainMs` — watchdog timestamp for consumer-drain auto-stop.
- `static constexpr uint32_t CAPTURE_DRAIN_TIMEOUT_MS = 10000` — 10 s drain timeout.
- `uint32_t m_correctionSkipCount`, `m_correctionApplyCount`.
- `CRGB* m_captureBlock`, `m_captureTapA`, `m_captureTapB`, `m_captureTapC` — lazy PSRAM allocations.
- `CaptureMetadata m_captureMetadata`
- `bool m_captureTapAValid`, `m_captureTapBValid`, `m_captureTapCValid`
- `CRGB m_captureScratch[LedConfig::TOTAL_LEDS]` — avoids 1920-byte stack alloc inside `forceOneShotCapture`.

### SynqMatrix director transition state (audio-sync gated; historical naming kept)
- `bool m_songAwareDirectorTransitionQueued = false`
- `bool m_songAwareDirectorTransitionPreparing = false`
- `bool m_songAwareDirectorTransitionActiveNotified = false`
- `EffectId m_songAwareDirectorPreviousEffect = INVALID_EFFECT_ID`
- `EffectId m_songAwareDirectorTargetEffect = INVALID_EFFECT_ID`
- `const char* m_songAwareDirectorTargetFamily = "none"`
- `const char* m_songAwareDirectorTargetLanguage = "none"`
- `const char* m_songAwareDirectorTransitionReason = "none"`

### MusicalGrid PLL (audio-sync gated)
- ESV11 backend: `audio::esv11::EsBeatClock m_esBeatClock`.
- Other backends: `audio::MusicalGrid m_musicalGrid`.

### Last audio snapshots
- `audio::ControlBusFrame m_lastControlBus`
- `audio::MusicalGridSnapshot m_lastMusicalGrid`
- `uint32_t m_lastControlBusSeq = 0`
- `audio::AudioTime m_lastAudioTime`
- `uint64_t m_lastAudioMicros = 0`

### Trinity proxy (audio-sync gated)
- `audio::TrinityControlBusProxy m_trinityProxy`
- `bool m_trinitySyncActive = false`
- `bool m_trinitySyncPaused = false`
- `float m_trinitySyncPosition = 0.0f`
- `uint8_t m_trinitySegmentIndex = 0xFF`
- `uint16_t m_trinitySegmentLabelHash = 0`
- `uint32_t m_trinitySegmentStartMs = 0`
- `uint32_t m_trinitySegmentEndMs = 0`

### Audio-contract tuning
- `audio::AudioContractTuning m_audioContractTuning`
- `audio::AudioContractTuning m_audioContractPending`
- `std::atomic<uint32_t> m_audioContractSeq{0}`
- `std::atomic<bool> m_audioContractDirty{false}`

### SnapshotBuffer pointers (audio-sync gated)
- `const audio::SnapshotBuffer<audio::ControlBusFrame>* m_controlBusBuffer = nullptr`
- `const audio::SnapshotBuffer<audio::ControlBusFrame>* m_stimulusControlBusBuffer = nullptr`
- `AudioInputMode m_audioInputMode = AudioInputMode::Live`
- `bool m_stimulusHasFrame = false`
- `bool m_effectHasAudioMappings = false` — cached `hasActiveMappings()` result.
- `float m_highIdReactiveSilenceGate = 1.0f`
- `float m_highIdReactiveActivityGate = 0.0f`

### Audio debug / bands snapshot (audio-sync gated)
- `bool m_audioDebugEnabled = false`
- `mutable BandsDebugSnapshot m_bandsDebugSnapshot[2]` (double-buffer)
- `mutable std::atomic<uint8_t> m_bandsDebugWriteIndex{0}`

### Optional metrics modules
- `metrics::VRMSMetricsEngine m_vrmsMetrics` (`FEATURE_VRMS_METRICS`)
- `metrics::VRMSBenchmark m_vrmsBenchmark` (`FEATURE_VRMS_BENCHMARK`)
- `merge::InputMergeLayer m_mergeLayer` (`FEATURE_INPUT_MERGE_LAYER`)

### TempoTracker pointer (non-ESV11 / non-PipelineCore branch)
- `lightwaveos::audio::TempoTracker* m_tempo = nullptr`

## `onMessage` switch — full message-type inventory

Two switch statements in `onMessage(const Message& msg)`.

### First switch (audio-sync gated — director ownership marking)
Cases fall through into a single block that calls `synqmatrix::SynqMatrix::instance().markShowControl()` or `markManualControl()`:
- `SET_EFFECT`, `SET_BRIGHTNESS`, `SET_SPEED`, `SET_PALETTE`, `SET_INTENSITY`, `SET_SATURATION`, `SET_COMPLEXITY`, `SET_VARIATION`, `SET_HUE`, `SET_MOOD`, `SET_FADE_AMOUNT`, `SET_EDGE_MIXER_MODE`, `SET_EDGE_MIXER_SPREAD`, `SET_EDGE_MIXER_STRENGTH`, `SET_EDGE_MIXER_SPATIAL`, `SET_EDGE_MIXER_TEMPORAL`, `START_TRANSITION`.

### Second switch (main dispatch)
- `SET_EFFECT` → `handleSetEffect(EffectId from param1|param2<<8)`
- `SET_BRIGHTNESS` → `handleSetBrightness(msg.param1)`
- `SET_SPEED` → `handleSetSpeed(msg.param1)`
- `SET_PALETTE` → `handleSetPalette(msg.param1)`
- `SET_INTENSITY` → `handleSetIntensity(msg.param1)`
- `SET_SATURATION` → `handleSetSaturation(msg.param1)`
- `SET_COMPLEXITY` → `handleSetComplexity(msg.param1)`
- `SET_VARIATION` → `handleSetVariation(msg.param1)`
- `SET_HUE` → `handleSetHue(msg.param1)`
- `SET_MOOD` → `handleSetMood(msg.param1)`
- `SET_FADE_AMOUNT` → `handleSetFadeAmount(msg.param1)`
- `SET_EDGE_MIXER_MODE` → `enhancement::EdgeMixer::getInstance().setMode(...)`
- `SET_EDGE_MIXER_SPREAD` → `EdgeMixer::setSpread(...)`
- `SET_EDGE_MIXER_STRENGTH` → `EdgeMixer::setStrength(...)`
- `SET_EDGE_MIXER_SPATIAL` → `EdgeMixer::setSpatial(...)`
- `SET_EDGE_MIXER_TEMPORAL` → `EdgeMixer::setTemporal(...)`
- `SAVE_EDGE_MIXER_NVS` → `EdgeMixer::saveToNVS()`
- `SET_LED_DITHERING` → `m_ledDriver.setDithering(msg.param1 != 0)`
- `START_TRANSITION` → `handleStartTransition(EffectId, msg.param3)`
- `HEALTH_CHECK` → publishes `HEALTH_STATUS` response.
- `PALETTE_CHANGED` → routes to `handleSetPalette`.
- `PING` → publishes `PONG` echo.
- Audio-sync gated cases:
  - `STIMULUS_SET_MODE` → updates `m_audioInputMode` (0 Live / 1 Trinity / 2 StimulusOverride).
  - `STIMULUS_CLEAR` → clears stimulus, returns to `Live`.
  - `TRINITY_BEAT` → unpacks BPM/phase/flags, injects external beat into `m_esBeatClock` / `m_musicalGrid`.
  - `TRINITY_MACRO` → calls `m_trinityProxy.setMacros(energy, vocal, bass, perc, bright)`.
  - `TRINITY_SYNC` → action switch: 0 start, 1 stop, 2 pause, 3 resume, 4 seek; updates `m_trinitySync*` state and external-sync mode on beat clock / musical grid.
  - `TRINITY_SEGMENT` → updates `m_trinitySegment*` quartet and republishes the message on the bus when changed.
- `default` → ignore.

Subscriber registration (`onStart`): subscribes to `MessageType::PALETTE_CHANGED` on `bus::MessageBus::instance()`.

## Parameter-Name Patterns

- Effect-related parameters consistently named `effectId`, `newEffectId`, `id`, `eid`, `previousEffectId`, `targetEffectId`.
- Strip-index parameter: `stripIdx` (uint8_t).
- Capture parameter: `tap` (`CaptureTap`), `outBuffer` (output param), `sourceBuffer` (input param), `tapMask` (bitmask).
- Audio-context parameters: `frame` (ControlBusFrame), `grid` (MusicalGridSnapshot), `available` (bool), `trinityActive` (bool), `nowMs` (uint32_t), `dtSeconds` (float), `includeBehaviorContext` (bool).
- Numeric value setters: positional `brightness`, `speed`, `paletteIndex`, `intensity`, `saturation`, `complexity`, `variation`, `hue`, `mood`, `fadeAmount` — matching the public getter spelling exactly.
- Timing parameters: `frameTimeUs`, `rawFrameTimeUs`, `deltaTimeMs`, `frameStartUs`, `frameEndUs`, `pacingWaitUs`, `waitUs`.
- Telemetry smoothing function takes `avgUs`, `sampleUs`.
- Speed-curve helper takes `speed` (uint8_t).
- Director-transition queue: `request` (`SynqMatrixSwitchRequest`), `previousEffectId`, `nowMs`.
- Parameter-update queue entries: `effectId`, `name` (24-char buffer), `value` (float).
- Validation-command queue entries: `cmd` (const char*); slot lengths `VAL_CMD_QUEUE_SIZE = 4`, `VAL_CMD_MAX_LEN = 256`.

## Anomalies / Observations

1. **Historical naming carry-over.** All director-related transition members and the helper `songAwareTransitionForReason()` retain the previous `songAwareDirector…` / `songAware…` prefix even though the responsible namespace was renamed to `lightwaveos::synqmatrix` and the public methods now read `queueSynqMatrixTransition` / `processSynqMatrixTransition` / `syncSynqMatrixTransitionTelemetry`. Touchpoints: `m_songAwareDirectorTransitionQueued` and seven sibling fields, the helper free function, plus `synqmatrix::songAwareStateName` and `currentSongState` referenced in the `LW_LOGI` line. Renaming to `SynqMatrix…` would align with the public method names and the runtime namespace; leaving the historical names creates dual vocabulary for the same concept.
2. **Centre-origin constant spelt American.** `LedConfig::CENTER_LED_INDEX` is the lone American spelling in an otherwise British-English file (comments use "centre", "colour", etc.). Likely intentional for an externally-consumed constant, but worth flagging.
3. **`CaptureTap` enum-value casing inconsistency.** Uses SHOUTY_SNAKE (`TAP_A_PRE_CORRECTION`) while every other `enum class` in this file uses PascalCase values (`AudioInputMode::Live`, `RendererMode::Unified`).
4. **Telemetry field-name overlap.** `VpStackSnapshot` mirrors many `RendererActor` private timing fields (e.g. `lastShowLedsUs` ↔ `m_lastShowLedsUs`). Names line up but the snapshot drops the `m_` prefix.
5. **Forward declarations.** `RendererActor.h` forward-declares `synqmatrix::SynqMatrixSwitchRequest` (struct) so the audio-sync include surface stays narrow; the implementation includes `core/synqmatrix/SynqMatrix.h` directly.
6. **Mixed feature gates.** SynqMatrix transition handling is double-gated: outer `FEATURE_AUDIO_SYNC` plus inner `FEATURE_TRANSITIONS`. The `getAudioSyncMode()` reads `m_trinitySyncActive && m_trinityProxy.isActive() && !m_trinitySyncPaused`, returning the literal `"trinity"` or `"es"` — these string literals would also be affected by any later renaming pass.
7. **Hue auto-rotate gate.** `kHueAutoRotatePauseMs = 30000` lives in an anonymous namespace in the `.cpp`, but the documenting comment is on the header-side `m_hueLastUserSetMs` declaration. Cross-file pairing only — the value is invisible from the header.
8. **Legacy stub.** `lightwaveos::actors::setCurrentLegacyEffectId(uint8_t)` is a free function defined as a no-op shim. Not a member but lives in the same TU as `RendererActor`; preserved here in case the rename pass touches free-function symbols.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-13 | agent:subagent (synqmatrix-rename-2026-05-05) | Created — SSA-N04 RendererActor naming inventory. Files inspected: 2. Methods enumerated: ~80 across 12 logical groups. Members enumerated: ~90 `m_*` fields. Message types enumerated: 24 across two switches. |
