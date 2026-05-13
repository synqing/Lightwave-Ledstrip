---
abstract: "Naming inventory for the core utilities subsystem (non-actor, non-synqmatrix): bus, diagnostics, merge, narrative, persistence (NVS), shows, state (CQRS), system (heap/stack/leak/OTA). Surfaces NVS key strings, NarrativeEngine API (SynqMatrix predecessor), Show types, Commands enum, monitor APIs, OTA token/session APIs. ~40 files."
---

# Subsystem: Core Utilities (non-actor, non-synqmatrix)

> Scope: core/{bus, diagnostics, merge, narrative, persistence, shows, state, system}, EffectTypes.h, SystemInit
> Files inspected: 40 (15 headers + 7 cpp surfaces inferred via headers; bodies not opened)
> Generated 2026-05-13 for SynqMatrix naming review.

Notable surface-area considerations flagged for SynqMatrix renaming review:

1. **NVS namespace + key strings** — these are the persistent wire surface stored in flash. Renaming requires migration logic. Listed in full per manager.
2. **`NarrativeEngine`** — the predecessor of SynqMatrix; "global temporal conductor for visual drama" with BUILD/HOLD/RELEASE/REST phase machine, per-zone offsets, tension override. Strong candidate for naming alignment.
3. **`ShowDefinition` / `ShowCue` / `CueScheduler`** — show subsystem type vocabulary, including `prism::` namespace for dynamic shows uploaded from PRISM Studio.
4. **Commands.h** — CQRS command class names dispatched through `StateStore`.
5. **System monitors** — `HeapMonitor`, `StackMonitor`, `MemoryLeakDetector`, `ValidationProfiler` — naming is descriptive but inconsistent (some `lightwaveos::core::system`, others elsewhere).
6. **OTA token/session** — `OtaTokenManager`, `OtaSessionLock`, `OtaBootVerifier`, `OtaLedFeedback` — token persists across firmware updates; namespace/key strings affect upgrade compatibility.

---

## Top-level core

### File: `core/EffectTypes.h`

Namespace: `lightwaveos::effects`

- struct `VisualParams`
  - members: `intensity`, `saturation`, `complexity`, `variation` (uint8_t each)
  - methods: `getIntensityNorm()`, `getSaturationNorm()`, `getComplexityNorm()`, `getVariationNorm()` (float)
- enum `EasingCurve : uint8_t`
  - values: `EASE_LINEAR`, `EASE_IN_QUAD`, `EASE_OUT_QUAD`, `EASE_IN_OUT_QUAD`, `EASE_IN_CUBIC`, `EASE_OUT_CUBIC`, `EASE_IN_OUT_CUBIC`, `EASE_IN_ELASTIC`, `EASE_OUT_ELASTIC`, `EASE_IN_OUT_ELASTIC`, `EASE_IN_BOUNCE`, `EASE_OUT_BOUNCE`, `EASE_IN_BACK`, `EASE_OUT_BACK`, `EASE_IN_OUT_BACK`
- namespace `Easing`
  - free functions: `clamp01(float t)`, `ease(float t, EasingCurve curve)`
- enum `NarrativePhase : uint8_t`
  - values: `PHASE_BUILD`, `PHASE_HOLD`, `PHASE_RELEASE`, `PHASE_REST`
- struct `NarrativeCycle`
  - fields: `buildDuration`, `holdDuration`, `releaseDuration`, `restDuration` (float seconds), `buildCurve`, `releaseCurve` (EasingCurve), `holdBreathe`, `snapAmount`, `durationVariance` (float 0–1), `phase` (NarrativePhase), `phaseStartMs`, `cycleStartMs` (uint32_t), `initialized` (bool), `currentCycleDuration` (float)
  - methods: `getTotalDuration() const`, `reset()`, `getPhaseDuration(NarrativePhase p) const`, `update()`, `getPhaseT() const`, `applySnap(float t) const`, `applyBreathe(float t) const`, `getIntensity() const`, `getPhase() const`, `isIn(NarrativePhase p) const`, `trigger()`

### File: `core/SystemInit.h` / `core/SystemInit.cpp`

Namespace: `lightwaveos::core`

- struct `BootFlags` — fields `wdtSafeMode`, `nvsCorrupted` (bool)
- Free functions (boot phase pipeline, 13 phases):
  - `initSerial()`
  - `initPSRAMScratch(serial::CaptureStreamer&)`
  - `initOtaAndWiFiReset()`
  - `checkWdtSafeMode(BootFlags&)`
  - `initSystemMonitoring()`
  - `initActorSystem(actors::ActorSystem&, actors::RendererActor*&, serial::CaptureStreamer&)`
  - `initNvsAndZones(actors::RendererActor*, zones::ZoneComposer&, persistence::ZoneConfigManager*&)`
  - `initStatusStripAndButton()`
  - `startActorsAndPlugins(actors::ActorSystem&, actors::RendererActor*, plugins::PluginManagerActor*&)`
  - `initWiFiAP()`
  - `initWebServer(actors::ActorSystem&, actors::RendererActor*, plugins::PluginManagerActor*)`
  - `postBootValidation(const BootFlags&)`
  - `printHelpBanner()`

---

## Group: Bus (publish/subscribe)

### File: `core/bus/MessageBus.h` / `MessageBus.cpp`

Namespace: `lightwaveos::bus` (re-uses `actors::Actor`, `actors::Message`, `actors::MessageType`)

- constants: `MAX_SUBSCRIBERS_PER_TYPE = 8`, `MAX_TRACKED_TYPES = 32`
- struct `SubscriptionEntry`
  - fields: `type` (MessageType), `subscribers[MAX_SUBSCRIBERS_PER_TYPE]` (Actor*), `count` (uint8_t), `active` (bool)
- class `MessageBus` (singleton)
  - static `instance()`
  - `subscribe(MessageType, Actor*) -> bool`
  - `unsubscribe(MessageType, Actor*) -> bool`
  - `unsubscribeAll(Actor*)`
  - `publish(const Message&, TickType_t timeout = 0) -> uint8_t`
  - `publishFromISR(const Message&) -> uint8_t`
  - `getSubscriberCount(MessageType) const -> uint8_t`
  - `getActiveEntryCount() const -> uint8_t`
  - `getTotalPublished() const -> uint32_t`
  - `getTotalDelivered() const -> uint32_t`
  - `getFailedDeliveries() const -> uint32_t`
  - `resetStats()`
  - `dumpSubscriptions()`
  - private: `findEntry(MessageType)`, `findOrCreateEntry(MessageType)`; members `m_entries`, `m_mutex`, `m_totalPublished` / `m_totalDelivered` / `m_failedDeliveries` (atomic uint32_t)
- macros: `MSG_BUS`, `SUBSCRIBE(type)`, `UNSUBSCRIBE(type)`, `PUBLISH(msg)`

---

## Group: Diagnostics (visual-pipeline surface introspection)

### File: `core/diagnostics/VpStackIntrospection.h`

Namespace: `lightwaveos::diagnostics`

- enum class `VpTopology : uint8_t` — `Unified`, `ZoneUnified`, `DirectStrip`
- enum class `VpAuthoredSurface : uint8_t` — `UnifiedLeds`, `PhysicalStrips`
- enum class `VpCorrectionSurface : uint8_t` — `None`, `UnifiedLeds`
- enum class `VpOutputSurface : uint8_t` — `PhysicalStrips`
- struct `VpSurfaceState` — fields `authored`, `correction`, `output`, `surfaceMismatch` (bool)
- inline functions: `topologyName(VpTopology)`, `authoredSurfaceName(VpAuthoredSurface)`, `correctionSurfaceName(VpCorrectionSurface)`, `outputSurfaceName(VpOutputSurface)`, `deriveVpSurfaces(VpTopology, bool dualChannelMode, bool correctionApplied) -> VpSurfaceState`

---

## Group: Merge (input arbitration)

### File: `core/merge/InputMergeLayer.h` (header-only, gated by `FEATURE_INPUT_MERGE_LAYER`)

Namespace: `lightwaveos::merge`

- constants: `kParamCount = 10`, `kSourceCount = 4`
- parameter index constants: `kParamBrightness=0`, `kParamSpeed=1`, `kParamIntensity=2`, `kParamSaturation=3`, `kParamComplexity=4`, `kParamVariation=5`, `kParamHue=6`, `kParamMood=7`, `kParamFadeAmount=8`, `kParamPaletteIdx=9`
- enum class `SourceId : uint8_t` — `MANUAL=0`, `AUDIO=1`, `AI_AGENT=2`, `GESTURE=3`
- enum class `MergeMode : uint8_t` — `HTP` (Highest Takes Precedence), `LTP` (Latest Takes Precedence)
- struct `SourceConfig` — fields `priority` (uint8_t), `staleTimeoutMs` (uint32_t), `tauSeconds` (float)
- struct `SourceParamState` — `rawValue` (uint8_t), `smoothedValue` (float), `written` (bool)
- struct `SourceState` — `params[kParamCount]`, `lastUpdateMs` (uint32_t), `valid` (bool)
- class `InputMergeLayer`
  - `init()`
  - `updateSource(SourceId, uint8_t paramIndex, uint8_t value)`
  - `updateSourceAll(SourceId, const uint8_t values[kParamCount])`
  - `merge(uint32_t nowMs, float dtSeconds)`
  - `getMerged(uint8_t paramIndex) const -> uint8_t`
  - `setMergeMode(uint8_t paramIndex, MergeMode)`
  - private: `mergeHTP(uint8_t pi)`, `mergeLTP(uint8_t pi)`, `clampU8(float)`; static `kSourceConfigs[kSourceCount]`; members `m_sources`, `m_merged`, `m_mergeMode`, `m_lastNowMs`

---

## Group: Narrative (SynqMatrix predecessor — naming alignment candidate)

### File: `core/narrative/NarrativeEngine.h` / `NarrativeEngine.cpp`

Namespace: `lightwaveos::narrative`

- class `NarrativeEngine` (singleton, "global temporal conductor for visual drama")
  - static `getInstance()`
  - Core: `update()`
  - Enable/disable: `enable()`, `disable()`, `isEnabled() const`
  - Duration setters: `setBuildDuration(float)`, `setHoldDuration(float)`, `setReleaseDuration(float)`, `setRestDuration(float)`, `setTempo(float totalCycleDuration)`
  - Duration getters: `getBuildDuration()`, `getHoldDuration()`, `getReleaseDuration()`, `getRestDuration()`, `getTotalDuration()`, `getBuildCurve()`, `getReleaseCurve()`, `getHoldBreathe()`, `getSnapAmount()`, `getDurationVariance()`
  - Curve setters: `setBuildCurve(EasingCurve)`, `setReleaseCurve(EasingCurve)`, `setHoldBreathe(float)`, `setSnapAmount(float)`, `setDurationVariance(float)`
  - Per-zone offsets: `setZonePhaseOffset(uint8_t zoneId, float offsetRatio)`, `getZonePhaseOffset(uint8_t zoneId) const`
  - Query: `getIntensity() const`, `getTension() const` (v1 alias), `getIntensity(uint8_t zoneId) const`, `getTempoMultiplier() const`, `getComplexityScaling() const`, `getPhase() const`, `getPhase(uint8_t zoneId) const`, `getPhaseT() const`, `getPhaseProgress() const` (alias), `getPhaseT(uint8_t zoneId) const`, `getCycleT() const`, `getCycleT(uint8_t zoneId) const`, `justEntered(NarrativePhase) const`, `isIn(NarrativePhase) const`
  - Manual control: `trigger()`, `pause()`, `resume()`, `reset()`, `setTensionOverride(float)`, `setPhase(NarrativePhase, uint32_t durationMs)`
  - Debug: `printStatus() const`
  - private: `getIntensityAtCycleT(float)`, `getPhaseAtCycleT(float)`, `getPhaseTAtCycleT(float)`; members `m_cycle` (NarrativeCycle), `m_zoneOffsets[MAX_ZONES=3]`, `m_lastPhase`, `m_justEnteredPhase`, `m_phaseJustChanged`, `m_enabled`, `m_paused`, `m_pauseStartMs`, `m_totalPausedMs`, `m_tensionOverride`, `m_manualPhaseControl`
- macro: `NARRATIVE` → singleton accessor

> **SynqMatrix relevance:** Per project memory, NarrativeEngine "maps song structure to intensity curves" and is the predecessor of SynqMatrix. The phase enum (`PHASE_BUILD/HOLD/RELEASE/REST`), the v1 aliases (`getTension`, `getPhaseProgress`), and the `NARRATIVE` macro are the primary outward surface to rename if SynqMatrix subsumes this engine.

---

## Group: Persistence (NVS-backed storage)

### File: `core/persistence/NVSManager.h` / `NVSManager.cpp`

Namespace: `lightwaveos::persistence`

- enum class `NVSResult : uint8_t` — `OK=0`, `NOT_INITIALIZED`, `NOT_FOUND`, `INVALID_HANDLE`, `READ_ERROR`, `WRITE_ERROR`, `CHECKSUM_ERROR`, `SIZE_MISMATCH`, `COMMIT_FAILED`, `FLASH_ERROR`
- class `NVSManager` (singleton)
  - static `instance()`
  - `init() -> bool`, `isInitialized() const -> bool`
  - Blob ops: `saveBlob(const char* ns, const char* key, const void* data, size_t size) -> NVSResult`, `loadBlob(...) -> NVSResult`, `getBlobSize(const char* ns, const char* key, size_t* outSize) -> NVSResult`, `eraseKey(const char* ns, const char* key) -> NVSResult`
  - Scalar ops: `saveUint8 / loadUint8 / saveUint16 / loadUint16 / saveUint32 / loadUint32` (each with default-value variant on load)
  - Utility: `calculateCRC32(const void* data, size_t size) -> uint32_t` (static), `resultToString(NVSResult) -> const char*` (static), `getStats(size_t* usedEntries, size_t* freeEntries) -> bool`
  - private: `m_initialized`, static `CRC32_TABLE[256]`
- macro: `NVS_MANAGER` → singleton accessor

**NVS surface:** generic — namespace + key are caller-supplied. No fixed strings in this file.

---

### File: `core/persistence/AudioTuningManager.h` / `AudioTuningManager.cpp`

Namespace: `lightwaveos::persistence` (gated by `FEATURE_AUDIO_SYNC`)

- struct `AudioTuningPreset`
  - constants: `CURRENT_VERSION=2`, `NAME_MAX_LEN=32`
  - fields: `version` (uint8_t), `name[NAME_MAX_LEN]`, `pipeline` (audio::AudioPipelineTuning), `contract` (audio::AudioContractTuning), `checksum` (uint32_t)
  - methods: `calculateChecksum()`, `isValid() const`
- class `AudioTuningManager` (singleton)
  - constants: `MAX_PRESETS = 10`, `NVS_NAMESPACE = "audio_tune"`
  - static `instance()`
  - `savePreset(const char* name, const AudioPipelineTuning&, const AudioContractTuning&) -> int8_t`
  - `loadPreset(uint8_t id, AudioPipelineTuning&, AudioContractTuning&, char* nameOut = nullptr) -> bool`
  - `deletePreset(uint8_t id) -> bool`
  - `listPresets(char names[][NAME_MAX_LEN], uint8_t* ids) -> uint8_t`
  - `hasPreset(uint8_t id) const -> bool`
  - `getPresetCount() const -> uint8_t`
  - `findFreeSlot() const -> int8_t`
  - private static `makeKey(uint8_t id, char* key)`

**NVS surface:**
- Namespace: `"audio_tune"`
- Key format: `"preset_0"` … `"preset_9"` (per `makeKey` comment)

---

### File: `core/persistence/EffectPresetManager.h` / `EffectPresetManager.cpp`

Namespace: `lightwaveos::persistence`

- struct `EffectPreset`
  - constants: `CURRENT_VERSION=1`, `NAME_MAX_LEN=32`
  - fields: `version`, `effectId` (EffectId), `paletteId`, `brightness`, `speed`, `name[NAME_MAX_LEN]`, `mood`, `trails`, `hue`, `saturation`, `intensity`, `complexity`, `variation`, `timestamp` (uint32_t), `crc32`
  - methods: `calculateChecksum()`, `isValid() const`, `reset()`
- struct `EffectPresetMetadata`
  - fields: `slot`, `name[NAME_MAX_LEN]`, `effectId`, `paletteId`, `timestamp`, `occupied`
- class `EffectPresetManager` (singleton)
  - constants: `MAX_PRESETS = 16`, `NVS_NAMESPACE = "effects"`
  - static `instance()`
  - `init() -> bool`, `isInitialised() const -> bool`
  - CRUD: `save(uint8_t slot, const EffectPreset&) -> NVSResult`, `load(uint8_t slot, EffectPreset&) -> NVSResult`, `list(EffectPresetMetadata*, uint8_t& count) -> NVSResult`, `remove(uint8_t slot) -> NVSResult`
  - Convenience: `saveCurrentEffect(uint8_t slot, const char* name, actors::RendererActor*) -> NVSResult`, `isSlotOccupied(uint8_t slot) const -> bool`, `getPresetCount() const -> uint8_t`, `findFreeSlot() const -> int8_t`, `getLastError() const -> NVSResult`
  - static `makeKey(uint8_t slot, char* key)`
  - private: `scanSlots()`, `updateSlotBitmap(uint8_t slot, bool occupied)`; members `m_initialised`, `m_lastError`, `m_slotBitmap` (uint16_t)
- macro: `EFFECT_PRESET_MANAGER` → singleton accessor

**NVS surface:**
- Namespace: `"effects"`
- Key format: generated by `makeKey(slot, buf)` (16-byte buffer; convention not declared in header but implied as `"slot_<n>"` or similar — see .cpp for exact format)

---

### File: `core/persistence/ZoneConfigManager.h` / `ZoneConfigManager.cpp`

Namespace: `lightwaveos::persistence`

- struct `ZoneConfigData`
  - fields: `version` (uint8_t), `segments[MAX_ZONES]` (ZoneSegment), `zoneCount`, `systemEnabled` (bool), `zoneEffects[MAX_ZONES]` (EffectId), `zoneEnabled[MAX_ZONES]`, `zoneBrightness[MAX_ZONES]`, `zoneSpeed[MAX_ZONES]`, `zonePalette[MAX_ZONES]`, `zoneBlendMode[MAX_ZONES]`, `checksum` (uint32_t)
  - methods: `calculateChecksum()`, `isValid() const`
- struct `SystemExpressionParams`
  - fields (all uint8_t, default 128): `hue`, `saturation`, `mood`, `trails`, `intensity`, `complexity`, `variation`
- struct `SystemConfigData`
  - fields: `version`, `effectId` (EffectId), `brightness`, `speed`, `paletteId`, `factoryPresetIndex`, `expr_hue`, `expr_saturation`, `expr_mood`, `expr_trails`, `expr_intensity`, `expr_complexity`, `expr_variation`, `checksum`
  - methods: `calculateChecksum()`, `isValid() const`
- struct `ZonePreset` (built-in)
  - fields: `name` (const char*), `config` (ZoneConfigData)
- constants: `ZONE_PRESET_COUNT = 5`
- extern: `ZONE_PRESETS[ZONE_PRESET_COUNT]`
- class `ZoneConfigManager`
  - ctor: `ZoneConfigManager(ZoneComposer*)`
  - NVS: `saveToNVS() -> bool`, `loadFromNVS() -> bool`
  - System state: `saveSystemState(EffectId, uint8_t brightness, uint8_t speed, uint8_t paletteId, uint8_t factoryPresetIndex = 0, const SystemExpressionParams* expr = nullptr) -> bool`, `loadSystemState(EffectId&, uint8_t& brightness, uint8_t& speed, uint8_t& paletteId, uint8_t* factoryPresetIndex = nullptr, SystemExpressionParams* expr = nullptr) -> bool`
  - Presets: `loadPreset(uint8_t presetId) -> bool`, static `getPresetName(uint8_t presetId) -> const char*`, static `getPresetCount() -> uint8_t`
  - Export/import: `exportConfig(ZoneConfigData&)`, `importConfig(const ZoneConfigData&)`
  - Validation: `validateConfig(const ZoneConfigData&) const -> bool`, `getLastError() const -> NVSResult`
  - private constants: `NVS_NAMESPACE = "zone_config"`, `NVS_KEY_ZONES = "zones"`, `NVS_NS_SYSTEM = "system_cfg"`, `NVS_KEY_STATE = "state"`, `CONFIG_VERSION = 3`, `SYSTEM_CONFIG_VERSION = 5`, `MAX_EFFECT_ID = 0x1FFF`, `MIN_SPEED = 1`, `MAX_SPEED = 100`, `MAX_PALETTE_ID = limits::MAX_PALETTES - 1`
  - members: `m_composer`, `m_lastError`

**NVS surface (two namespaces):**
- `"zone_config"` / `"zones"` — zone segment + per-zone effect config blob (CONFIG_VERSION=3)
- `"system_cfg"` / `"state"` — global brightness/speed/effect/palette + 7 expression sliders (SYSTEM_CONFIG_VERSION=5)

---

### File: `core/persistence/ZonePresetManager.h` / `ZonePresetManager.cpp`

Namespace: `lightwaveos::persistence`

- constants: `ZONE_PRESET_MAX_SLOTS = 16`, `ZONE_PRESET_MAX_ZONES = 3`, `ZONE_PRESET_NAME_LENGTH = 32`
- struct `ZonePresetEntry`
  - fields: `effectId` (EffectId), `paletteId`, `brightness`, `speed`, `blendMode`, `s1LeftStart`, `s1LeftEnd`, `s1RightStart`, `s1RightEnd` (uint16_t segment ranges, strip 1; strip 2 mirrors +160)
- struct `ZonePreset` (user-saveable; note name clash with ZoneConfigManager's built-in `ZonePreset` — both in `lightwaveos::persistence` namespace)
  - fields: `version`, `zoneCount`, `name[ZONE_PRESET_NAME_LENGTH]`, `zones[ZONE_PRESET_MAX_ZONES]` (ZonePresetEntry), `timestamp`, `crc32`
  - methods: `calculateChecksum()`, `isValid() const`
- struct `ZonePresetMetadata`
  - fields: `slot`, `name`, `zoneCount`, `timestamp`, `occupied`
- class `ZonePresetManager` (singleton)
  - constants: `MAX_PRESETS = ZONE_PRESET_MAX_SLOTS`, `NVS_NAMESPACE = "zones"`, `PRESET_VERSION = 1`, `MAX_EFFECT_ID = 0x1FFF`, `MIN_SPEED = 1`, `MAX_SPEED = 100`, `MAX_PALETTE_ID = limits::MAX_PALETTES - 1`, `MAX_BLEND_MODE = 7`
  - static `instance()`
  - `init() -> bool`, `isInitialized() const -> bool`
  - CRUD: `save(uint8_t slot, const ZonePreset&) -> NVSResult`, `load(uint8_t slot, ZonePreset&) -> NVSResult`, `list(ZonePresetMetadata*, uint8_t& count) -> NVSResult`, `remove(uint8_t slot) -> NVSResult`, `isSlotOccupied(uint8_t slot) -> bool`
  - ZoneComposer integration: `saveCurrentZones(uint8_t slot, const char* name, zones::ZoneComposer*) -> NVSResult`, `applyToZones(uint8_t slot, zones::ZoneComposer*) -> NVSResult`
  - Utilities: static `getMaxPresets() -> uint8_t`, `getLastError() const -> NVSResult`
  - private: `slotToKey(uint8_t slot, char* buffer)`, `validatePreset(const ZonePreset&) const`, `populateFromComposer(ZonePreset&, const char* name, zones::ZoneComposer*)`, `applyToComposer(const ZonePreset&, zones::ZoneComposer*)`; members `m_initialized`, `m_lastError`, `m_slotOccupied[ZONE_PRESET_MAX_SLOTS]`
- macro: `ZONE_PRESET_MANAGER` → singleton accessor

**NVS surface:**
- Namespace: `"zones"`
- Key format: generated by `slotToKey(slot, buf)` (12-byte buffer)

> **Naming caution:** Two distinct `ZonePreset` types both in `lightwaveos::persistence`:
> 1. `ZoneConfigManager`'s `ZonePreset` = `{const char* name; ZoneConfigData config;}` (built-in factory preset, in ZoneConfigManager.h)
> 2. `ZonePresetManager`'s `ZonePreset` = full user-saveable struct with version/zones/crc32 (in ZonePresetManager.h)
> SynqMatrix rename should disambiguate. Likewise `MAX_EFFECT_ID = 0x1FFF` is duplicated as a private constant in both managers.

---

## Group: Show subsystem

### File: `core/shows/ShowTypes.h`

No enclosing namespace (anonymous / global) — types are used by both `lightwaveos::shows` and `prism::`.

- constant: `ZONE_GLOBAL = 0xFF`
- enum `CueType : uint8_t` — `CUE_EFFECT=0`, `CUE_PARAMETER_SWEEP`, `CUE_ZONE_CONFIG`, `CUE_TRANSITION`, `CUE_NARRATIVE`, `CUE_PALETTE`, `CUE_MARKER`
- enum `ParamId : uint8_t` — `PARAM_BRIGHTNESS=0`, `PARAM_SPEED`, `PARAM_INTENSITY`, `PARAM_SATURATION`, `PARAM_COMPLEXITY`, `PARAM_VARIATION`, `PARAM_COUNT`
- enum `ShowNarrativePhase : uint8_t` — `SHOW_PHASE_BUILD=0`, `SHOW_PHASE_HOLD`, `SHOW_PHASE_RELEASE`, `SHOW_PHASE_REST`
- struct `ShowCue` (10 bytes)
  - fields: `timeMs`, `type` (CueType), `targetZone`, `data[4]`
  - accessors: `effectId()`, `effectTransition()`, `sweepParamId()`, `sweepTargetValue()`, `sweepDurationMs()`, `zoneCount()`, `zoneEnabled()`, `paletteId()`, `narrativePhase()`, `narrativeTempoMs()`, `transitionType()`, `transitionDurationMs()`
- struct `ShowChapter` (20 bytes)
  - fields: `name` (const char*, PROGMEM), `startTimeMs`, `durationMs`, `narrativePhase`, `tensionLevel`, `cueStartIndex`, `cueCount`
- struct `ShowDefinition`
  - fields: `id`, `name`, `totalDurationMs`, `chapterCount`, `totalCues`, `looping`, `chapters` (const ShowChapter*), `cues` (const ShowCue*)
- struct `ShowPlaybackState` (20 bytes RAM)
  - fields: `currentShowId`, `currentChapterIndex`, `nextCueIndex`, `playing`, `paused`, `_padding`, `startTimeMs`, `pauseStartMs`, `totalPausedMs`
  - methods: `reset()`, `getElapsedMs() const`
- struct `ActiveSweep` (10 bytes)
  - fields: `paramId`, `targetZone`, `startValue`, `targetValue`, `startTimeMs`, `durationMs`
  - methods: `isActive()`, `clear()`, `getCurrentValue(uint32_t currentMs) const`, `isComplete(uint32_t currentMs) const`
- struct `ShowInfo`
  - fields: `id`, `name`, `durationMs`, `looping`

> **Naming note:** Two parallel "narrative phase" enums exist — `lightwaveos::effects::NarrativePhase` (PHASE_BUILD…) used by NarrativeEngine, and global `ShowNarrativePhase` (SHOW_PHASE_BUILD…) used by show cues. SynqMatrix rename should unify.

---

### File: `core/shows/BuiltinShows.h`

No namespace (PROGMEM data tables). Helper macros: `DUR_LO(ms)`, `DUR_HI(ms)`, `EID_LO(eid)`, `EID_HI(eid)`.

- 10 PROGMEM show entries, each declares `<NAME>_ID`, `<NAME>_NAME`, `<NAME>_CH0_NAME` … `<NAME>_CHn_NAME`, `<NAME>_CUES[]`, `<NAME>_CHAPTERS[]`
- Show identifiers (lowercase string IDs): `"dawn"`, `"storm"`, `"meditation"`, `"celebration"`, `"cosmos"`, `"forest"`, `"heartbeat"`, `"ocean"`, `"energy"`, `"ambient"`
- Display names: `"Dawn"`, `"Storm"`, `"Meditation"`, `"Celebration"`, `"Cosmos"`, `"Forest"`, `"Heartbeat"`, `"Ocean"`, `"Energy"`, `"Ambient"`
- master array: `BUILTIN_SHOWS[]` (ShowDefinition PROGMEM)
- constant: `BUILTIN_SHOW_COUNT`

---

### File: `core/shows/CueScheduler.h`

Namespace: `lightwaveos::shows`

- class `CueScheduler`
  - constant: `MAX_CUES_PER_FRAME = 4`
  - ctor `CueScheduler()`
  - `loadCues(const ShowCue* cues, uint8_t count)`
  - `reset()`
  - `seekTo(uint32_t timeMs)`
  - `getReadyCues(uint32_t currentTimeMs, ShowCue* outCues) -> uint8_t`
  - `hasMoreCues() const -> bool`
  - `getNextIndex() const -> uint8_t`
  - `getCueCount() const -> uint8_t`
  - `peekNextCueTime() const -> uint32_t`
  - private members: `m_cues`, `m_cueCount`, `m_nextIndex`

---

### File: `core/shows/ParameterSweeper.h` / `ParameterSweeper.cpp`

Namespace: `lightwaveos::shows`

- typedefs:
  - `ParamApplyCallback = void(*)(ParamId, uint8_t zone, uint8_t value)`
  - `ParamGetCallback = uint8_t(*)(ParamId, uint8_t zone)`
- class `ParameterSweeper`
  - constant: `MAX_SWEEPS = 8`
  - ctor `ParameterSweeper(ParamApplyCallback, ParamGetCallback)`
  - `startSweep(ParamId, uint8_t zone, uint8_t startVal, uint8_t targetVal, uint16_t durationMs) -> bool`
  - `startSweepFromCurrent(ParamId, uint8_t zone, uint8_t targetVal, uint16_t durationMs) -> bool`
  - `update(uint32_t currentTimeMs)`
  - `cancelAll()`
  - `cancelParam(ParamId)`
  - `cancelZone(uint8_t zone)`
  - `activeSweepCount() const -> uint8_t`
  - `hasActiveSweeps() const -> bool`
  - private: `applyValue(ParamId, uint8_t, uint8_t)`, `getCurrentParamValue(ParamId, uint8_t)`, `findFreeSlot() const -> int8_t`, `findSweep(ParamId, uint8_t) const -> int8_t`; members `m_sweeps[MAX_SWEEPS]`, `m_applyCallback`, `m_getCallback`

---

### File: `core/shows/DynamicShowStore.h`

Namespace: `prism::` (NOTE: distinct from `lightwaveos::shows`)

- constants: `MAX_DYNAMIC_SHOWS = 4`, `MAX_CUES_PER_SHOW = 512`, `MAX_CHAPTERS_PER_SHOW = 32`, `MAX_SHOW_JSON_SIZE = 32768`, `MAX_SHOW_ID_LEN = 33`, `MAX_SHOW_NAME_LEN = 65`, `MAX_CHAPTER_NAME_LEN = 33`
- struct `DynamicShowData`
  - fields: `id[MAX_SHOW_ID_LEN]`, `name[MAX_SHOW_NAME_LEN]`, `chapterNames[MAX_CHAPTERS_PER_SHOW][MAX_CHAPTER_NAME_LEN]`, `totalDurationMs`, `bpm`, `looping`, `chapters` (ShowChapter*, PSRAM), `chapterCount`, `cues` (ShowCue*, PSRAM), `cueCount`, `definition` (ShowDefinition facade), `totalRamBytes`
  - method: `buildDefinition()`
- class `DynamicShowStore`
  - ctor / dtor (clears slots)
  - `findById(const char* id) const -> int8_t`
  - `findFreeSlot() const -> int8_t`
  - `allocateShowData(uint16_t cueCount, uint8_t chapterCount) -> DynamicShowData*` (heap_caps_malloc MALLOC_CAP_SPIRAM)
  - `registerShow(uint8_t slot, DynamicShowData*) -> bool`
  - `deleteShow(uint8_t slot)`
  - `deleteShowById(const char* id) -> bool`
  - `getDefinition(uint8_t slot) const -> const ShowDefinition*`
  - `getShowData(uint8_t slot) const -> const DynamicShowData*`
  - `count() const -> uint8_t`
  - `totalRamUsage() const -> size_t`
  - `isOccupied(uint8_t slot) const -> bool`
  - private: `freeSlot(uint8_t)`; members `m_slots[MAX_DYNAMIC_SHOWS]`, `m_occupied[MAX_DYNAMIC_SHOWS]`

---

### File: `core/shows/ShowBundleParser.h`

Namespace: `prism::`

- struct `ParseResult`
  - fields: `success`, `errorMessage` (const char*, static), `cueCount`, `chapterCount`, `ramUsageBytes`, `showId[MAX_SHOW_ID_LEN]`
  - static `ok(const char* id, uint16_t cues, uint8_t chapters, size_t ram) -> ParseResult`
  - static `error(const char* msg) -> ParseResult`
- class `ShowBundleParser`
  - static `parse(const uint8_t* json, size_t jsonLen, DynamicShowStore& store, uint8_t& outSlot) -> ParseResult`
  - private static helpers: `parseNarrativePhase(const char* phase) -> uint8_t`, `parseParamId(const char* paramStr) -> ParamId`, `parseCue(const char* typeStr, JsonObjectConst data, ShowCue& out) -> bool`

JSON schema vocabulary parsed (top-level): `version` (must be 1), `id`, `name`, `durationMs`, `looping`, `bpm`, `chapters[]`, `cues[]`. Chapter fields: `name`, `startTimeMs`, `durationMs`, `tensionLevel`, `narrativePhase`. Cue fields: `timeMs`, `zone`, `type`, `data{}`. Cue type strings: `"effect"`, `"parameter_sweep"`, `"zone_config"`, `"palette"`, `"narrative"`, `"transition"`, `"marker"`. Param strings: `"brightness"`, `"speed"`, `"intensity"`, `"saturation"`, `"complexity"`, `"variation"`. Phase strings: `"build"`, `"hold"`, `"release"`, `"rest"`.

---

### File: `core/shows/Prim8Adapter.h`

Namespace: `prism::`

- free function: `clamp8(float) -> uint8_t`
- struct `Prim8Vector` (8 float dimensions, all normalised [0,1])
  - fields: `pressure`, `impact`, `mass`, `momentum`, `heat`, `space`, `texture`, `gravity`
  - methods: `clamp()`, static `neutral()`
- struct `FirmwareParams` (10 uint8_t parameters)
  - fields: `brightness`, `speed`, `paletteId`, `hue`, `intensity`, `saturation`, `complexity`, `variation`, `mood`, `fadeAmount`
- free function: `mapPrim8ToParams(const Prim8Vector&, uint8_t paletteId = 0) -> FirmwareParams`

> **SynqMatrix relevance:** Prim8 is the PRISM Studio semantic vector ("creative expression"). The 8 dimension names (pressure, impact, mass, momentum, heat, space, texture, gravity) are a parallel vocabulary to NarrativeEngine's BUILD/HOLD/RELEASE/REST. Both belong on the SynqMatrix naming review.

---

## Group: State (CQRS)

### File: `core/state/SystemState.h` / `SystemState.cpp`

Namespace: `lightwaveos::state`

- using/aliases: `MAX_ZONES` (from limits), `MAX_PALETTE_COUNT = limits::MAX_PALETTES`, `MAX_EFFECT_COUNT = limits::MAX_EFFECTS`
- struct `ZoneState`
  - fields: `effectId` (EffectId), `paletteId`, `brightness`, `speed`, `enabled` (bool)
  - default ctor (effectId=EID_FIRE, brightness=255, speed=15, enabled=false)
- struct `SystemState` (immutable snapshot, ~100 bytes)
  - fields:
    - `version` (uint32_t)
    - global: `currentEffectId` (EffectId), `currentPaletteId`, `brightness`, `speed`, `gHue`
    - visual: `intensity`, `saturation`, `complexity`, `variation`
    - zone: `zoneModeEnabled`, `activeZoneCount`, `zones` (std::array<ZoneState, MAX_ZONES>)
    - transition: `transitionActive`, `transitionType`, `transitionProgress`
  - default ctor; copy/assign default
  - functional update methods (all return new SystemState):
    - `withEffect(EffectId)`, `withBrightness(uint8_t)`, `withPalette(uint8_t)`, `withSpeed(uint8_t)`
    - `withZoneEnabled(uint8_t zoneId, bool)`, `withZoneEffect(uint8_t, EffectId)`, `withZonePalette(uint8_t, uint8_t)`, `withZoneBrightness(uint8_t, uint8_t)`, `withZoneSpeed(uint8_t, uint8_t)`, `withZoneMode(bool, uint8_t zoneCount)`
    - `withTransition(uint8_t type, uint8_t progress)`, `withTransitionStarted(uint8_t)`, `withTransitionCompleted()`
    - `withIncrementedHue()`
    - `withVisualParams(uint8_t intensity, uint8_t saturation, uint8_t complexity, uint8_t variation)`, `withIntensity(uint8_t)`, `withSaturation(uint8_t)`, `withComplexity(uint8_t)`, `withVariation(uint8_t)`

---

### File: `core/state/ICommand.h`

Namespace: `lightwaveos::state`

- interface `ICommand`
  - virtual `apply(const SystemState& current) const -> SystemState` = 0
  - virtual `getName() const -> const char*` = 0
  - virtual `validate(const SystemState& current) const -> bool` (default true)

---

### File: `core/state/Commands.h`

Namespace: `lightwaveos::state`

Concrete command classes (each derives from `ICommand`, implements `apply`/`getName`/optional `validate`). Command class name → `getName()` string returned:

| Class | `getName()` string | Constructor signature |
|---|---|---|
| `SetEffectCommand` | `"SetEffect"` | `(EffectId)` |
| `SetBrightnessCommand` | `"SetBrightness"` | `(uint8_t)` |
| `SetPaletteCommand` | `"SetPalette"` | `(uint8_t)` |
| `SetSpeedCommand` | `"SetSpeed"` | `(uint8_t)` |
| `ZoneEnableCommand` | `"ZoneEnable"` | `(uint8_t zoneId, bool enabled)` |
| `ZoneSetEffectCommand` | `"ZoneSetEffect"` | `(uint8_t, EffectId)` |
| `ZoneSetPaletteCommand` | `"ZoneSetPalette"` | `(uint8_t, uint8_t)` |
| `ZoneSetBrightnessCommand` | `"ZoneSetBrightness"` | `(uint8_t, uint8_t)` |
| `ZoneSetSpeedCommand` | `"ZoneSetSpeed"` | `(uint8_t, uint8_t)` |
| `SetZoneModeCommand` | `"SetZoneMode"` | `(bool, uint8_t zoneCount)` |
| `TriggerTransitionCommand` | `"TriggerTransition"` | `(uint8_t transitionType)` |
| `UpdateTransitionCommand` | `"UpdateTransition"` | `(uint8_t, uint8_t)` |
| `CompleteTransitionCommand` | `"CompleteTransition"` | `()` |
| `IncrementHueCommand` | `"IncrementHue"` | `()` |
| `SetVisualParamsCommand` | `"SetVisualParams"` | `(uint8_t intensity, uint8_t saturation, uint8_t complexity, uint8_t variation)` |
| `SetIntensityCommand` | `"SetIntensity"` | `(uint8_t)` |
| `SetSaturationCommand` | `"SetSaturation"` | `(uint8_t)` |
| `SetComplexityCommand` | `"SetComplexity"` | `(uint8_t)` |
| `SetVariationCommand` | `"SetVariation"` | `(uint8_t)` |

(19 commands total — these are the load-bearing CQRS command vocabulary.)

---

### File: `core/state/StateStore.h` / `StateStore.cpp`

Namespace: `lightwaveos::state`

- typedef `StateChangeCallback = void(*)(const SystemState& newState)`
- class `StateStore`
  - constant: `MAX_SUBSCRIBERS = 8`
  - ctor / dtor
  - Queries (lock-free): `getState() const -> const SystemState&`, `getVersion() const -> uint32_t`, `getCurrentEffect() const -> EffectId`, `getCurrentPalette() const -> uint8_t`, `getBrightness() const -> uint8_t`, `getSpeed() const -> uint8_t`, `isZoneModeEnabled() const -> bool`, `getActiveZoneCount() const -> uint8_t`, `getZoneConfig(uint8_t zoneId) const -> ZoneState`, `isTransitionActive() const -> bool`
  - Commands: `dispatch(const ICommand&) -> bool`, `dispatchBatch(const ICommand* const* commands, uint8_t count) -> bool`
  - Subscriptions: `subscribe(StateChangeCallback) -> bool`, `unsubscribe(StateChangeCallback) -> bool`, `getSubscriberCount() const -> uint8_t`
  - Utility: `reset()`, `getStats(uint32_t& outCommandCount, uint32_t& outLastCommandDuration) const`
  - private: `notifySubscribers(const SystemState&)`, `getInactiveIndex() const -> uint8_t`, `swapActiveIndex()`, `validateActiveIndex() const -> uint8_t`
  - private members: `m_states[2]` (double-buffer), `m_activeIndex` (std::atomic<uint8_t>), `m_writeMutex` (SemaphoreHandle_t), `m_subscribers[MAX_SUBSCRIBERS]`, `m_subscriberCount`, `m_commandCount`, `m_lastCommandDuration`

---

## Group: System monitors + OTA

### File: `core/system/HeapMonitor.h` / `HeapMonitor.cpp`

Namespace: `lightwaveos::core::system`

- class `HeapMonitor` (static)
  - `init()`, `checkHeapIntegrity() -> bool`
  - `getFreeHeap() -> size_t`, `getMinFreeHeap() -> size_t`, `getLargestFreeBlock() -> size_t`, `getFragmentationPercent() -> uint8_t`
  - `onHeapCorruption()`, `onMallocFailed(size_t size)`
  - private: `s_initialized`, `s_minFreeHeap`
- C-linkage hooks: `heap_corruption_hook()`, `vApplicationMallocFailedHook()`

---

### File: `core/system/StackMonitor.h` / `StackMonitor.cpp`

Namespace: `lightwaveos::core::system`

- class `StackMonitor` (static)
  - `init()`
  - `getStackHighWaterMark(TaskHandle_t = nullptr) -> uint32_t`
  - `getStackUsagePercent(TaskHandle_t, uint32_t stackSize) -> uint8_t`
  - `checkAllTasks()`
  - `setWarningThreshold(uint8_t)`, `getWarningThreshold() -> uint8_t`
  - `onStackOverflow(TaskHandle_t, char* taskName)`
  - Profiling: `startProfiling()`, `stopProfiling()`, `generateProfileReport()`
  - struct `TaskStackProfile` — fields `taskName`, `stackSize`, `currentFree`, `peakUsed`, `avgUsed`, `currentPercent`, `peakPercent`, `avgPercent`
  - private struct `TaskProfileData` — `taskName`, `stackSize`, `peakUsed`, `totalUsed` (uint64), `sampleCount`, `active`
  - private struct `WarningState` — `taskName`, `lastWarnedFreeBytes`, `active`
  - constants: `MAX_PROFILED_TASKS = 16`, `CRITICAL_FREE_BYTES = 256`, `WARNING_DELTA_BYTES = 64`
- C-linkage hook: `vApplicationStackOverflowHook(TaskHandle_t, char*)`

---

### File: `core/system/MemoryLeakDetector.h` / `MemoryLeakDetector.cpp`

Namespace: `lightwaveos::core::system`

- class `MemoryLeakDetector` (static)
  - `init()`
  - `recordAllocation(void* ptr, size_t size, const char* file, int line)`, `recordDeallocation(void* ptr)`
  - `scanForLeaks()`
  - `getHeapDelta() -> int32_t`, `resetBaseline()`
  - `setLeakThreshold(uint32_t seconds)`, `setEnabled(bool)`
  - private struct `AllocationRecord` — `ptr`, `size`, `timestamp`, `file`, `line`, `active`
  - private: `s_records[MAX_RECORDS=256]`, `s_recordCount`, `s_leakThresholdMs`, `s_baselineHeap`, `s_enabled`, `s_initialized`; `findRecord(void*)`, `removeRecord(size_t)`

---

### File: `core/system/ValidationProfiler.h` / `ValidationProfiler.cpp`

Namespace: `lightwaveos::core::system`

- class `ValidationProfiler` (static)
  - `init()`
  - `recordCall(const char* functionName, int64_t timeUs)`, `updateFrame()`
  - `getTotalCalls() -> uint32_t`, `getAvgTimeUs() -> float`, `getOverheadPerFrameUs() -> float`, `getCPUOverheadPercent(uint32_t frameBudgetUs = 8333) -> float`
  - `generateReport()`
  - `setEnabled(bool)`, `isEnabled() -> bool`
  - private: `s_totalCalls`, `s_totalTimeUs` (uint64), `s_frameCalls`, `s_frameTimeUs`, `s_frameCount`, `s_peakTimeUs`, `s_enabled`, `s_initialized`

---

### File: `core/system/OtaBootVerifier.h` (header-only)

Namespace: `lightwaveos::core::system` (gated by `FEATURE_OTA_UPDATE && !NATIVE_BUILD`; native stub also provided)

- log tag macro: `LW_LOG_TAG_OTA_BOOT = "OtaBoot"`
- class `OtaBootVerifier` (static)
  - constants: `MIN_HEALTHY_HEAP = 100 * 1024` (100 KB), `VALIDATION_TIMEOUT_MS = 30000` (30 s)
  - `init()` — emits JSON event `"ota.boot.check"` with `ts_mono_ms`, `rollbackPossible`, `partition`
  - `markAppValidIfHealthy(bool wifiConnectedOrAP, bool webServerStarted)` — emits `"ota.boot.validated"` or invokes `rollback()`
  - `isFirstBootAfterOta() -> bool`
  - `isValidated() -> bool`
  - `rollback(const char* reason)` — emits JSON event `"ota.boot.rollback"`, calls `esp_ota_mark_app_invalid_rollback_and_reboot()`; does not return
  - private static: `s_initialized`, `s_rollbackPossible`, `s_validated`, `s_bootTimeMs`

**Telemetry event names (JSON):** `"ota.boot.check"`, `"ota.boot.validated"`, `"ota.boot.rollback"`.

---

### File: `core/system/OtaLedFeedback.h` (header-only)

Namespace: `lightwaveos::core::system` (gated by `FEATURE_OTA_UPDATE && !NATIVE_BUILD`; native stub provided)

- class `OtaLedFeedback` (static)
  - constants: `LEDS_PER_STRIP = 160`, `CENTER_POINT = 80`, `AMBER_R/G/B = 255/184/77`, `SUCCESS_R/G/B = 0/255/0`, `FAILURE_R/G/B = 255/0/0`, `FLASH_COUNT = 3`, `FLASH_ON_MS = 200`, `FLASH_OFF_MS = 150`, `OTA_BRIGHTNESS = 80`
  - `showProgress(uint8_t percent)` — centre-outward fill from LED 79/80
  - `showSuccess()` — 3 green flashes
  - `showFailure()` — 3 red flashes
  - `restore()` — clears active flag (does not blank LEDs)
  - `isActive() -> bool`
  - private: `flashColor(CRGB)`, `s_otaFeedbackActive`

---

### File: `core/system/OtaSessionLock.h` (header-only)

Namespace: `lightwaveos::core::system`

- enum class `OtaTransport : uint8_t` — `None=0`, `Rest`, `WebSocket`
- class `OtaSessionLock` (static, portMUX_TYPE spinlock-based)
  - constant: `OTA_SESSION_MAX_MS = 5 * 60 * 1000` (5 minutes)
  - `tryAcquire(OtaTransport) -> bool`
  - `release()`
  - `isOtaInProgress() -> bool`
  - `activeTransport() -> OtaTransport`
  - `sessionStartMs() -> uint32_t`
  - `isStale(uint32_t nowMs) -> bool`
  - private: `s_mux` (portMUX_TYPE), `s_transport`, `s_sessionStartMs`

---

### File: `core/system/OtaTokenManager.h` / `OtaTokenManager.cpp`

Namespace: `lightwaveos::core::system` (gated by `FEATURE_OTA_UPDATE && FEATURE_WEB_SERVER && !NATIVE_BUILD`; native stub provided)

- class `OtaTokenManager` (static)
  - constants: `TOKEN_LENGTH = 32` (32 hex chars = 128 bits), `NVS_NAMESPACE = "ota"`, `NVS_KEY = "token"`
  - `init() -> bool`
  - `getToken() -> const String&`
  - `regenerateToken() -> bool` (hardware RNG via `esp_random()`)
  - `setToken(const char*) -> bool`
  - `isInitialized() -> bool`
  - `isUsingNvsToken() -> bool`
  - private: `generateRandomHexToken(char* outBuf)`, `s_initialized`, `s_usingNvs`, `s_cachedToken` (String)

**NVS surface:**
- Namespace: `"ota"`
- Key: `"token"`
- Value: 32-char lowercase hex string

> **SynqMatrix caution:** Renaming the OTA NVS namespace `"ota"` or key `"token"` will invalidate every existing device's stored auth token, forcing token regeneration on first boot of the renamed firmware. Persistent compatibility flag.

---

## Summary — naming surface for SynqMatrix review

**NVS key strings (persistent wire surface; renaming requires migration):**

| Owner | Namespace | Key(s) |
|---|---|---|
| `AudioTuningManager` | `"audio_tune"` | `"preset_0"` … `"preset_9"` |
| `EffectPresetManager` | `"effects"` | per-slot key generated by `makeKey()` |
| `ZoneConfigManager` (zones) | `"zone_config"` | `"zones"` |
| `ZoneConfigManager` (system) | `"system_cfg"` | `"state"` |
| `ZonePresetManager` | `"zones"` | per-slot key generated by `slotToKey()` |
| `OtaTokenManager` | `"ota"` | `"token"` |

> **Conflict:** `"zones"` is used both as a *key* (by ZoneConfigManager inside namespace `"zone_config"`) and as a *namespace* (by ZonePresetManager). Disambiguating in a SynqMatrix rename pass is recommended.

**Telemetry event names (JSON event surface, OtaBootVerifier):** `"ota.boot.check"`, `"ota.boot.validated"`, `"ota.boot.rollback"`.

**CQRS command names (logged by `getName()`, 19 commands):** SetEffect, SetBrightness, SetPalette, SetSpeed, ZoneEnable, ZoneSetEffect, ZoneSetPalette, ZoneSetBrightness, ZoneSetSpeed, SetZoneMode, TriggerTransition, UpdateTransition, CompleteTransition, IncrementHue, SetVisualParams, SetIntensity, SetSaturation, SetComplexity, SetVariation.

**Show subsystem vocabulary (JSON wire format for ShowBundle upload):** version, id, name, durationMs, looping, bpm, chapters[], cues[]. Cue types: `"effect"`, `"parameter_sweep"`, `"zone_config"`, `"palette"`, `"narrative"`, `"transition"`, `"marker"`. Param strings: `"brightness"`, `"speed"`, `"intensity"`, `"saturation"`, `"complexity"`, `"variation"`. Phase strings: `"build"`, `"hold"`, `"release"`, `"rest"`.

**Prim8 (PRISM Studio semantic vector) dimension names:** pressure, impact, mass, momentum, heat, space, texture, gravity.

**Parallel narrative phase enums (candidate for unification):** `lightwaveos::effects::NarrativePhase` (PHASE_BUILD/HOLD/RELEASE/REST) vs global `ShowNarrativePhase` (SHOW_PHASE_BUILD/HOLD/RELEASE/REST).

**Macro-level singleton accessors:** `MSG_BUS`, `NVS_MANAGER`, `EFFECT_PRESET_MANAGER`, `ZONE_PRESET_MANAGER`, `NARRATIVE`, plus `SUBSCRIBE/UNSUBSCRIBE/PUBLISH`.

**Anomalies / inconsistencies surfaced:**

1. Two distinct `ZonePreset` types both in `lightwaveos::persistence` (ZoneConfigManager built-in vs ZonePresetManager user-saveable). Disambiguate.
2. Two narrative phase enums (`NarrativePhase` vs `ShowNarrativePhase`). Unify under SynqMatrix.
3. NVS namespace `"zones"` collides with NVS key `"zones"` used in a different namespace. Disambiguate.
4. `prism::` namespace (DynamicShowStore, ShowBundleParser, Prim8Adapter) sits alongside `lightwaveos::shows` (CueScheduler, ParameterSweeper). Both serve the show subsystem but use different namespace conventions.
5. `MAX_EFFECT_ID = 0x1FFF` is duplicated as a private constant in ZoneConfigManager and ZonePresetManager.
6. NarrativeEngine carries v1-compatibility aliases (`getTension`, `getPhaseProgress`) — candidates for removal during SynqMatrix rename.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-13 | agent:claude-opus-4-7 | Created. Naming inventory for core utilities subsystem (40 files). Surfaces NVS namespaces/keys, NarrativeEngine API, Show types, Commands enum, OTA token/session APIs. Flags 6 naming anomalies for SynqMatrix renaming review. |
