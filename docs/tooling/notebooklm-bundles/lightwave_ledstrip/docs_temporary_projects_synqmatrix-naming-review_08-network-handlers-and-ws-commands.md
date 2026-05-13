---
abstract: "SSA-N08 name inventory for the K1 network handler layer — both REST handler classes under firmware-v3/src/network/webserver/handlers/ and WebSocket command modules under firmware-v3/src/network/webserver/ws/. Lists handler/registration function signatures, the REST routes that V1ApiRoutes wires into each handler, the WebSocket command names registered with WsCommandRouter, and the JSON wire-field keys emitted in responses. Surfaces the still-live songAware.* WebSocket vocabulary and /api/v1/songAware/* REST surface as the dominant rename target."
---

# SSA-N08 — Network Handlers and WebSocket Command Modules: Name Inventory

Read-only extraction. No symbols are renamed here; this catalogue is the substrate for the SynqMatrix rename review.

Scope inspected:

- `firmware-v3/src/network/webserver/handlers/` — 25 handler header files + 25 handler `.cpp` files (50 files, ~10 401 LOC).
- `firmware-v3/src/network/webserver/ws/` — 28 WS-command header files + 28 WS-command `.cpp` files (56 files, ~9 653 LOC).
- Cross-referenced `firmware-v3/src/network/webserver/V1ApiRoutes.cpp` for REST route ↔ handler binding (155 `registry.on*` registrations).

Methodology: full read of every `*.h`; for `*.cpp` files used `rg` to enumerate `WsCommandRouter::registerCommand(...)` calls, `registry.on*(...)` route registrations, and JSON wire-field literals matching the `["<key>"]` pattern. Free-function/static handler names extracted via `rg -oN '^(static\s+)?void\s+handle[A-Za-z0-9_]+'`.

## Table of Contents

### REST Handler Classes (handlers/)

1. [AudioHandlers](#handler-audiohandlers)
2. [AuthHandlers](#handler-authhandlers)
3. [BatchHandlers](#handler-batchhandlers)
4. [ColorCorrectionHandlers](#handler-colorcorrectionhandlers)
5. [DebugHandlers](#handler-debughandlers)
6. [DeviceHandlers](#handler-devicehandlers)
7. [EffectHandlers](#handler-effecthandlers)
8. [EffectPresetHandlers](#handler-effectpresethandlers)
9. [FilesystemHandlers](#handler-filesystemhandlers)
10. [FirmwareHandlers](#handler-firmwarehandlers)
11. [ModifierHandlers](#handler-modifierhandlers)
12. [NarrativeHandlers](#handler-narrativehandlers)
13. [NetworkHandlers](#handler-networkhandlers)
14. [PaletteHandlers](#handler-palettehandlers)
15. [ParameterHandlers](#handler-parameterhandlers)
16. [PluginHandlers](#handler-pluginhandlers)
17. [PresetHandlers](#handler-presethandlers)
18. [ShowHandlers](#handler-showhandlers)
19. [StimulusHandlers](#handler-stimulushandlers)
20. [SynqMatrixHandlers](#handler-synqmatrixhandlers)
21. [SystemHandlers](#handler-systemhandlers)
22. [TransitionHandlers](#handler-transitionhandlers)
23. [VrmsHandlers](#handler-vrmshandlers)
24. [ZoneHandlers](#handler-zonehandlers)
25. [ZonePresetHandlers](#handler-zonepresethandlers)

### WebSocket Command Modules (ws/)

26. [WsAudioCommands](#ws-wsaudiocommands)
27. [WsAuthCommands](#ws-wsauthcommands)
28. [WsBatchCommands](#ws-wsbatchcommands)
29. [WsColorCommands](#ws-wscolorcommands)
30. [WsDebugCommands](#ws-wsdebugcommands)
31. [WsDeviceCommands](#ws-wsdevicecommands)
32. [WsEdgeMixerCommands](#ws-wsedgemixercommands)
33. [WsEffectPresetCommands](#ws-wseffectpresetcommands)
34. [WsEffectsCommands](#ws-wseffectscommands)
35. [WsFilesystemCommands](#ws-wsfilesystemcommands)
36. [WsModifierCommands](#ws-wsmodifiercommands)
37. [WsMotionCommands](#ws-wsmotioncommands)
38. [WsNarrativeCommands](#ws-wsnarrativecommands)
39. [WsOtaCommands](#ws-wsotacommands)
40. [WsPaletteCommands](#ws-wspalettecommands)
41. [WsPluginCommands](#ws-wsplugincommands)
42. [WsPresetCommands](#ws-wspresetcommands)
43. [WsRenderCommands](#ws-wsrendercommands)
44. [WsShowCommands](#ws-wsshowcommands)
45. [WsStatusCommands](#ws-wsstatuscommands)
46. [WsStimulusCommands](#ws-wsstimuluscommands)
47. [WsStmCommands](#ws-wsstmcommands)
48. [WsStreamCommands](#ws-wsstreamcommands)
49. [WsSynqMatrixCommands](#ws-wssynqmatrixcommands)
50. [WsSysCommands](#ws-wssyscommands)
51. [WsTransitionCommands](#ws-wstransitioncommands)
52. [WsTrinityCommands](#ws-wstrinitycommands)
53. [WsZonePresetCommands](#ws-wszonepresetcommands)
54. [WsZonesCommands](#ws-wszonescommands)

### Anomalies

55. [Leftover songAware wire surface](#anomalies-leftover-songaware-wire-surface)
56. [Other naming oddities](#anomalies-other-naming-oddities)

---

## REST Handler Classes

Namespace for every handler class: `lightwaveos::network::webserver::handlers`. All public handler methods are `static`. Every method takes an `AsyncWebServerRequest*` as the first parameter; downstream parameters vary (raw body buffer + length, an `ActorSystem&` or `RendererActor*`, a `ZoneComposer*`, callbacks). Function signatures are reproduced verbatim from the corresponding header.

### Handler: AudioHandlers

Source: `handlers/AudioHandlers.h` / `handlers/AudioHandlers.cpp` (~2 100 LOC of `.cpp`).

Public methods (declared `static` inside `class AudioHandlers`):

- `handleParametersGet(AsyncWebServerRequest*, ActorSystem&, RendererActor*)`
- `handleParametersSet(AsyncWebServerRequest*, uint8_t*, size_t, ActorSystem&, RendererActor*)`
- `handleControl(AsyncWebServerRequest*, uint8_t*, size_t, ActorSystem&)`
- `handleStateGet(AsyncWebServerRequest*, ActorSystem&)`
- `handleTempoGet(AsyncWebServerRequest*, ActorSystem&)`
- `handlePresetsList(AsyncWebServerRequest*)`
- `handlePresetGet(AsyncWebServerRequest*, uint8_t presetId)`
- `handlePresetSave(AsyncWebServerRequest*, uint8_t*, size_t, ActorSystem&, RendererActor*)`
- `handlePresetApply(AsyncWebServerRequest*, uint8_t presetId, ActorSystem&, RendererActor*)`
- `handlePresetDelete(AsyncWebServerRequest*, uint8_t presetId)`
- `handleMappingsListSources(AsyncWebServerRequest*)`
- `handleMappingsListTargets(AsyncWebServerRequest*)`
- `handleMappingsListCurves(AsyncWebServerRequest*)`
- `handleMappingsList(AsyncWebServerRequest*, RendererActor*)`
- `handleMappingsGet(AsyncWebServerRequest*, EffectId, RendererActor*)`
- `handleMappingsSet(AsyncWebServerRequest*, EffectId, uint8_t*, size_t, RendererActor*)`
- `handleMappingsDelete(AsyncWebServerRequest*, EffectId)`
- `handleMappingsEnable(AsyncWebServerRequest*, EffectId, bool enable)`
- `handleMappingsStats(AsyncWebServerRequest*)`
- `handleZoneAGCGet(AsyncWebServerRequest*, ActorSystem&)`
- `handleZoneAGCSet(AsyncWebServerRequest*, uint8_t*, size_t, ActorSystem&)`
- `handleAGCToggle(AsyncWebServerRequest*, uint8_t*, size_t, ActorSystem&)`
- `handleFftGet(AsyncWebServerRequest*, ActorSystem&)`
- `handleStmGet(AsyncWebServerRequest*, ActorSystem&)`
- `handleSpikeDetectionGet(AsyncWebServerRequest*, ActorSystem&)`
- `handleSpikeDetectionReset(AsyncWebServerRequest*, ActorSystem&)`
- `handleMicGainGet(AsyncWebServerRequest*, ActorSystem&)`
- `handleMicGainSet(AsyncWebServerRequest*, uint8_t*, size_t, ActorSystem&)`
- `handleCalibrateStatus(AsyncWebServerRequest*, ActorSystem&)`
- `handleCalibrateStart(AsyncWebServerRequest*, uint8_t*, size_t, ActorSystem&)`
- `handleCalibrateCancel(AsyncWebServerRequest*, ActorSystem&)`
- `handleCalibrateApply(AsyncWebServerRequest*, ActorSystem&)`
- `handleBenchmarkGet/Start/Stop/History(...)` — guarded by `FEATURE_AUDIO_BENCHMARK`.

REST routes bound to this handler (from `V1ApiRoutes.cpp`):

- GET `/api/v1/audio/state`, `/api/v1/audio/parameters`, `/api/v1/audio/tempo`, `/api/v1/audio/fft`, `/api/v1/audio/stm`, `/api/v1/audio/spike-detection`, `/api/v1/audio/mic-gain`, `/api/v1/audio/zone-agc`, `/api/v1/audio/calibrate`, `/api/v1/audio/benchmark`, `/api/v1/audio/benchmark/history`.
- GET `/api/v1/audio/mappings`, `/api/v1/audio/mappings/sources`, `/api/v1/audio/mappings/targets`, `/api/v1/audio/mappings/curves`, `/api/v1/audio/mappings/effect`, `/api/v1/audio/mappings/stats`.
- GET `/api/v1/audio/presets`, `/api/v1/audio/presets/get`.
- POST `/api/v1/audio/parameters`, `/api/v1/audio/control`, `/api/v1/audio/mic-gain`, `/api/v1/audio/zone-agc`, `/api/v1/audio/spike-detection/reset`.
- POST `/api/v1/audio/calibrate/start`, `/api/v1/audio/calibrate/cancel`, `/api/v1/audio/calibrate/apply`.
- POST `/api/v1/audio/mappings/effect`, `/api/v1/audio/mappings/enable`, `/api/v1/audio/mappings/disable`.
- POST `/api/v1/audio/presets`, `/api/v1/audio/presets/apply`.
- POST `/api/v1/audio/benchmark/start`, `/api/v1/audio/benchmark/stop`.
- PATCH `/api/v1/audio/parameters`. PUT `/api/v1/audio/agc`. DELETE `/api/v1/audio/mappings/effect`, `/api/v1/audio/presets/delete`.

JSON wire-field keys observed in response bodies (de-duplicated, in alphabetical order — these are the rename candidates Captain wants to review):

`action`, `active`, `activeEffects`, `activeEffectsWithMappings`, `additive`, `agcAttack`, `agcClipReduce`, `agcEnabled`, `agcGain`, `agcIdleReturnRate`, `agcMaxGain`, `agcMinGain`, `agcRelease`, `agcTargetRms`, `alphaFast`, `alphaSlow`, `applyCount`, `attackRate`, `audioStalenessMs`, `available`, `avgChromaUs`, `avgCorrectionMagnitude`, `avgDcAgcUs`, `avgGoertzelUs`, `avgSpikesPerFrame`, `avgTotalUs`, `backend`, `bandAttack`, `bandCount`, `bandDbCeil`, `bandDbFloor`, `bandFloors`, `bandRelease`, `bands`, `bar_phase`, `barCorrectionGain`, `beat_in_bar`, `beat_phase`, `beatInBar`, `beats_per_bar`, `beatsPerBar`, `beatStrength`, `beatTick`, `beatUnit`, `bins`, `bins64`, `bins64Adaptive`, `bpm`, `bpmMax`, `bpmMin`, `bpmTau`, `capabilities`, `captureFail`, `captureSuccess`, `capturing`, `category`, `chroma`, `chromaCount`, `chromaDbCeil`, `chromaDbFloor`, `chromaFloors`, `clipCount`, `confidence`, `confidenceTau`, `contract`, `controlBus`, `controlBusAlphaFast`, `controlBusAlphaSlow`, `count`, `cpuLoadPercent`, `cpuPercent`, `currentAvgRms`, `curve`, `curves`, `dcAgc`, `dcAlpha`, `dcEstimate`, `decay`, `default`, `derived`, `description`, `downbeatTick`, `durationMs`, `effectId`, `effectName`, `effects`, `enabled`, `energy`, `fall`, `fftSize`, `floor`, `flux`, `fluxMapped`, `fluxScale`, `follower`, `formula`, `gain`, `gainDb`, `gateRangeFactor`, `gateRangeMin`, `gateStartFactor`, `globalEnabled`, `goertzel`, `goertzelCount`, `goertzelWindow`, `heavyBandAttack`, `heavyBandRelease`, `histogram`, `hopCount`, `hopSize`, `id`, `index`, `inputMax`, `inputMin`, `isSilent`, `lastApplyMicros`, `latest`, `load`, `lookaheadEnabled`, `mappingCount`, `mappings`, `maxAllowedRms`, `maxApplyMicros`, `maxMag`, `maxSample`, `meanSample`, `message`, `minFloor`, `minSample`, `name`, `noiseFloor`, `noiseFloorFall`, `noiseFloorMin`, `noiseFloorRise`, `novelty`, `outputMax`, `outputMin`, `overallRms`, `peakCentered`, `peakGoertzelUs`, `peakRms`, `peakTotalUs`, `perBandGains`, `perBandNoiseFloors`, `phase01AtAudioT`, `phaseCorrectionGain`, `pipeline`, `presets`, `progress`, `rangeMax`, `rangeMin`, `ready`, `reason`, `releaseRate`, `resetState`, `result`, `results`, `returned`, `rise`, `rms`, `rmsDbCeil`, `rmsDbFloor`, `rmsMapped`, `rmsPreGain`, `rmsRaw`, `safetyMultiplier`, `sampleCount`, `sampleIndex`, `sampleRate`, `samples`, `samplesCollected`, `scale`, `seq`, `silenceHysteresisMs`, `silenceThreshold`, `silentScale`, `smoothingAlpha`, `source`, `sources`, `spectral`, `spectralCentroidBin`, `spectralDominantBin`, `spectralEnergyMax`, `spectralEnergyRms`, `spectralFluxScale`, `spikesCorrected`, `spikesDetectedBands`, `spikesDetectedChroma`, `state`, `stats`, `streaming`, `style`, `styleConfidence`, `supported`, `target`, `targets`, `tauSeconds`, `tempoConfidence`, `tempoLocked`, `temporal`, `tickCount`, `timing`, `total`, `totalEnergyRemoved`, `totalFrames`, `totalMappings`, `totalMappingsConfigured`, `ts`, `updated`, `usePerBandNoiseFloor`, `useSpectralFlux`, `validValues`, `values`, `waveformPoints`, `zones`.

Note: mixed snake-case and camelCase coexist for the same conceptual fields (`beat_in_bar` vs `beatInBar`, `beats_per_bar` vs `beatsPerBar`, `bar_phase` vs `beat_phase` vs `phase01AtAudioT`). Flagged as a rename target for SSA-N08 reviewers.

### Handler: AuthHandlers

Source: `handlers/AuthHandlers.h` / `.cpp`. Guarded by `FEATURE_WEB_SERVER && FEATURE_API_AUTH`.

Public methods on `class AuthHandlers`:

- `handleStatus(AsyncWebServerRequest*, ApiKeyManager&)`
- `handleRotate(AsyncWebServerRequest*, ApiKeyManager&)`
- `handleClear(AsyncWebServerRequest*, ApiKeyManager&)`

REST routes: GET `/api/v1/auth/status`, POST `/api/v1/auth/rotate`, DELETE `/api/v1/auth/key`.

JSON wire-field keys: `enabled`, `key`, `keyConfigured`, `message`.

### Handler: BatchHandlers

Source: `handlers/BatchHandlers.h` / `.cpp`.

Public methods on `class BatchHandlers`:

- `handleExecute(AsyncWebServerRequest*, uint8_t*, size_t, ActorSystem&, std::function<bool(const String&, JsonVariant)> executeBatchAction, std::function<void()> broadcastStatus)`

REST route: POST `/api/v1/batch`.

JSON wire-field keys: `action`, `failed`, `operations`, `processed`.

### Handler: ColorCorrectionHandlers

Source: `handlers/ColorCorrectionHandlers.h` / `.cpp`. Header comment marks it `(stub)` but the `.cpp` is ~7 KB and fully implemented.

Public methods on `class ColorCorrectionHandlers`:

- `handleGetConfig(AsyncWebServerRequest*)`
- `handleSetMode(AsyncWebServerRequest*, uint8_t*, size_t)`
- `handleSetConfig(AsyncWebServerRequest*, uint8_t*, size_t)`
- `handleSave(AsyncWebServerRequest*)`
- `handleGetPresets(AsyncWebServerRequest*)`
- `handleSetPreset(AsyncWebServerRequest*, uint8_t*, size_t)`

REST routes: GET `/api/v1/colorCorrection/config`, GET `/api/v1/colorCorrection/presets`, POST `/api/v1/colorCorrection/config`, POST `/api/v1/colorCorrection/mode`, POST `/api/v1/colorCorrection/preset`, POST `/api/v1/colorCorrection/save`.

JSON wire-field keys: `autoExposureEnabled`, `autoExposureTarget`, `brownGuardrailEnabled`, `count`, `gammaEnabled`, `gammaLut`, `gammaValue`, `hsvMinSaturation`, `lutGenerationId`, `maxBluePercentOfRed`, `maxBrightness`, `maxGreenPercentOfRed`, `mode`, `modeNames`, `presets`, `rgbTargetMin`, `rgbWhiteThreshold`, `saturationBoostAmount`, `saved`, `status`, `updated`, `vClampEnabled`.

Note: route segment `colorCorrection` (camelCase) is inconsistent with siblings like `audio/zone-agc`, `effects/parameters` (kebab and slash). Flagged as a rename target.

### Handler: DebugHandlers

Source: `handlers/DebugHandlers.h` / `.cpp`. Mix of always-on debug config endpoints and audio-debug endpoints behind `FEATURE_AUDIO_SYNC`.

Public methods on `class DebugHandlers`:

- `handleDebugConfigGet(AsyncWebServerRequest*)`
- `handleDebugConfigSet(AsyncWebServerRequest*, uint8_t*, size_t)`
- `handleDebugStatus(AsyncWebServerRequest*, ActorSystem&)`
- `handleZoneMemoryStats(AsyncWebServerRequest*, ZoneComposer*)`
- `handleUdpStatsGet(AsyncWebServerRequest*, webserver::UdpStreamer*)`
- `handleAudioDebugGet(AsyncWebServerRequest*)` (guarded)
- `handleAudioDebugSet(AsyncWebServerRequest*, uint8_t*, size_t)` (guarded)

REST routes: GET `/api/v1/debug/audio`, POST `/api/v1/debug/audio`, GET `/api/v1/debug/memory/zones`, GET `/api/v1/debug/udp`.

JSON wire-field keys: `actor`, `agcGain`, `attempts`, `audio`, `baseInterval`, `beatInBar`, `beatStrength`, `beatTick`, `bpm`, `captures`, `capturesFailed`, `clips`, `composerOverhead`, `consecutiveFailures`, `cooldownRemainingMs`, `cpuFreqMHz`, `dcEstimate`, `dma`, `domains`, `downbeatTick`, `effectiveLevels`, `error`, `estimatedBufferSize`, `estimatedMetadata`, `failures`, `flux`, `globalLevel`, `heapFree`, `heapMaxBlock`, `heapMin`, `id`, `intervals`, `lastFailureAgoMs`, `lastFailureMs`, `lastSocketResetMs`, `led`, `levels`, `memory`, `message`, `micLevelDb`, `network`, `noiseFloor`, `phase01AtAudioT`, `render`, `rms`, `rmsPreGain`, `seq`, `socketResets`, `spectrum`, `spectrumInterval`, `started`, `status`, `statusInterval`, `subscribers`, `success`, `suppressed`, `system`, `tempoConfidence`, `tickCount`, `totalMemoryEstimate`, `uptimeMs`, `verbosity`, `zoneCount`, `zones`.

### Handler: DeviceHandlers

Source: `handlers/DeviceHandlers.h` / `.cpp`. Also exposes a `registerRoutes(HttpRouteRegistry&, std::function<bool(AsyncWebServerRequest*)>)` helper (rate-limit wrapper) although V1ApiRoutes performs the actual binding.

Public methods:

- `registerRoutes(HttpRouteRegistry&, std::function<bool(AsyncWebServerRequest*)>)`
- `handleStatus(AsyncWebServerRequest*, ActorSystem&, RendererActor*, uint32_t startTime, bool apMode, size_t wsClientCount)`
- `handleInfo(AsyncWebServerRequest*, ActorSystem&, RendererActor*)`

REST routes: GET `/api/v1/device/status`, GET `/api/v1/device/info`, GET `/api/v1/device/ota-token`, POST `/api/v1/device/ota-token` (the OTA-token routes call into `FirmwareHandlers::handleGet/SetOtaToken`).

JSON wire-field keys: `apMode`, `architecture`, `audioSyncMode`, `board`, `boardFamily`, `connected`, `cpuFreq`, `cpuPercent`, `firmware`, `firmwareVersionNumber`, `flashSize`, `fps`, `framesRendered`, `freeHeap`, `freeSketch`, `heapSize`, `hostname`, `ip`, `name`, `network`, `rssi`, `sdk`, `sketchSize`, `uptime`, `wsClients`.

### Handler: EffectHandlers

Source: `handlers/EffectHandlers.h` / `.cpp` (~21 KB). Exposes its own `registerRoutes(HttpRouteRegistry&)` although V1ApiRoutes also binds these endpoints.

Public methods:

- `registerRoutes(HttpRouteRegistry&)`
- `handleList(AsyncWebServerRequest*, RendererActor*)`
- `handleCurrent(AsyncWebServerRequest*, RendererActor*)`
- `handleSet(AsyncWebServerRequest*, uint8_t*, size_t, ActorSystem&, const WebServer::CachedRendererState&, std::function<void()> broadcastStatus)`
- `handleMetadata(AsyncWebServerRequest*, RendererActor*)`
- `handleFamilies(AsyncWebServerRequest*)`
- `handleParametersGet(AsyncWebServerRequest*, RendererActor*)`
- `handleParametersSet(AsyncWebServerRequest*, uint8_t*, size_t, RendererActor*)`

REST routes: GET `/api/v1/effects`, `/api/v1/effects/current`, `/api/v1/effects/families`, `/api/v1/effects/metadata`, `/api/v1/effects/parameters`. POST `/api/v1/effects/parameters`, `/api/v1/effects/set`. PATCH `/api/v1/effects/parameters`. PUT `/api/v1/effects/current`.

JSON wire-field keys: `author`, `brightness`, `category`, `categoryId`, `centerOrigin`, `complexity`, `count`, `default`, `description`, `displayName`, `effectId`, `failed`, `families`, `family`, `familyId`, `features`, `hasParameters`, `hue`, `id`, `ieffectCategory`, `intensity`, `isAudioReactive`, `isExperimental`, `isIEffect`, `max`, `min`, `name`, `opticalIntent`, `paletteAware`, `paletteId`, `parameters`, `properties`, `queued`, `recommended`, `saturation`, `speed`, `speedResponsive`, `story`, `symmetricStrips`, `tags`, `total`, `transition`, `transitionType`, `usesPalette`, `usesSpeed`, `value`, `variation`, `version`, `zoneAware`.

### Handler: EffectPresetHandlers

Source: `handlers/EffectPresetHandlers.h` / `.cpp`. Header docstring marks as `(stub)` but the `.cpp` is implemented.

Public methods:

- `handleList(AsyncWebServerRequest*)`
- `handleSave(AsyncWebServerRequest*, uint8_t*, size_t, actors::RendererActor*)`
- `handleGet(AsyncWebServerRequest*, uint8_t id)`
- `handleApply(AsyncWebServerRequest*, uint8_t id, actors::ActorSystem&, actors::RendererActor*)`
- `handleDelete(AsyncWebServerRequest*, uint8_t id)`

REST routes: GET `/api/v1/effect-presets`, GET `/api/v1/effect-presets/get`, POST `/api/v1/effect-presets`, POST `/api/v1/effect-presets/apply`, DELETE `/api/v1/effect-presets/delete`. Also bound to `/api/v1/presets/effects` and `/api/v1/presets/effects/save-current` via V1ApiRoutes (aliasing/legacy).

JSON wire-field keys: `brightness`, `complexity`, `count`, `effectId`, `hue`, `id`, `intensity`, `message`, `mood`, `name`, `paletteId`, `parameters`, `presets`, `saturation`, `slot`, `speed`, `timestamp`, `trails`, `variation`.

### Handler: FilesystemHandlers

Source: `handlers/FilesystemHandlers.h` / `.cpp`. Marked `(stub)` but routes are live.

Public methods:

- `handleFilesystemStatus(AsyncWebServerRequest*, WebServer*)`
- `handleFilesystemMount(AsyncWebServerRequest*, WebServer*)`
- `handleFilesystemUnmount(AsyncWebServerRequest*, WebServer*)`
- `handleFilesystemRestart(AsyncWebServerRequest*, WebServer*)`

REST routes: GET `/api/v1/filesystem/status`, POST `/api/v1/filesystem/mount`, POST `/api/v1/filesystem/unmount`, POST `/api/v1/filesystem/restart`.

JSON wire-field keys: `message`, `mounted`, `status`, `totalBytes`, `type`, `usedBytes`.

### Handler: FirmwareHandlers

Source: `handlers/FirmwareHandlers.h` / `.cpp` (~51 KB).

Public methods:

- `handleVersion(AsyncWebServerRequest*)`
- `handleV1Update(AsyncWebServerRequest*, std::function<bool(AsyncWebServerRequest*)> checkToken)`
- `handleLegacyUpdate(AsyncWebServerRequest*, std::function<bool(AsyncWebServerRequest*)> checkToken)`
- `handleUpload(AsyncWebServerRequest*, const String& filename, size_t index, uint8_t*, size_t, bool final)`
- `handleV1FsUpdate(AsyncWebServerRequest*, std::function<bool(AsyncWebServerRequest*)> checkToken)`
- `handleFsUpload(AsyncWebServerRequest*, const String& filename, size_t index, uint8_t*, size_t, bool final)`
- `checkOTAToken(AsyncWebServerRequest*) -> bool`
- `isRestOtaInProgress() -> bool`
- `handleGetOtaToken(AsyncWebServerRequest*)`
- `handleSetOtaToken(AsyncWebServerRequest*, uint8_t*, size_t)`

REST routes: GET `/api/v1/firmware/version`, POST `/api/v1/firmware/update`, POST `/api/v1/firmware/filesystem`, POST `/update` (legacy plain-text). GET/POST `/api/v1/device/ota-token`.

JSON wire-field keys: `action`, `bytesWritten`, `durationMs`, `freeSketchSpace`, `message`, `platform`, `rebooting`, `sketchSize`, `source`, `status`, `target`, `token`, `tokenLength`, `version`, `versionNumber`.

### Handler: ModifierHandlers

Source: `handlers/ModifierHandlers.h` / `.cpp`. Marked `(stub)` in header docstring.

Public methods:

- `handleListModifiers(AsyncWebServerRequest*, actors::RendererActor*)`
- `handleAddModifier(AsyncWebServerRequest*, uint8_t*, size_t, actors::RendererActor*)`
- `handleRemoveModifier(AsyncWebServerRequest*, uint8_t*, size_t, actors::RendererActor*)`
- `handleClearModifiers(AsyncWebServerRequest*, actors::RendererActor*)`
- `handleUpdateModifier(AsyncWebServerRequest*, uint8_t*, size_t, actors::RendererActor*)`

REST routes: GET `/api/v1/modifiers/list`, POST `/api/v1/modifiers/add`, POST `/api/v1/modifiers/clear`, POST `/api/v1/modifiers/remove`, POST `/api/v1/modifiers/update`.

JSON wire-field keys: `cleared`, `count`, `maxSlots`, `modifiers`, `status`.

### Handler: NarrativeHandlers

Source: `handlers/NarrativeHandlers.h` / `.cpp`.

Public methods:

- `handleStatus(AsyncWebServerRequest*)`
- `handleConfigGet(AsyncWebServerRequest*)`
- `handleConfigSet(AsyncWebServerRequest*, uint8_t*, size_t)`

REST routes: GET `/api/v1/narrative/status`, GET `/api/v1/narrative/config`, POST `/api/v1/narrative/config`.

JSON wire-field keys: `build`, `complexityScaling`, `curves`, `cycleT`, `durations`, `durationVariance`, `enabled`, `hold`, `holdBreathe`, `message`, `phase`, `phaseId`, `phaseT`, `release`, `rest`, `snapAmount`, `tempoMultiplier`, `tension`, `total`, `updated`.

### Handler: NetworkHandlers

Source: `handlers/NetworkHandlers.h` / `.cpp` (~15 KB). Header carries a strongly-worded AP-only constraint banner — see hard-stop note.

Public methods:

- `registerRoutes(HttpRouteRegistry&)`
- `handleStatus(AsyncWebServerRequest*)`
- `handleScan(AsyncWebServerRequest*)`
- `handleConnect(AsyncWebServerRequest*, uint8_t*, size_t)`
- `handleDisconnect(AsyncWebServerRequest*)`
- `handleSavedList(AsyncWebServerRequest*)`
- `handleSavedAdd(AsyncWebServerRequest*, uint8_t*, size_t)`
- `handleSavedDelete(AsyncWebServerRequest*)`
- `handleListNetworks(AsyncWebServerRequest*)`
- `handleAddNetwork(AsyncWebServerRequest*, uint8_t*, size_t)`
- `handleDeleteNetwork(AsyncWebServerRequest*, const String& ssid)`
- `handleScanNetworks(AsyncWebServerRequest*)`
- `handleScanStatus(AsyncWebServerRequest*)`
- `handleEnableSTA(AsyncWebServerRequest*, uint8_t*, size_t)`
- `handleEnableAPOnly(AsyncWebServerRequest*)`
- `authModeToString(uint8_t) -> const char*` (private helper)

REST routes: GET `/api/v1/network/status`, `/api/v1/network/scan`, `/api/v1/network/scan/status`, `/api/v1/network/networks`. POST `/api/v1/network/connect`, `/api/v1/network/disconnect`, `/api/v1/network/networks`, `/api/v1/network/sta/enable`, `/api/v1/network/ap/enable`. No DELETE binding for saved networks was observed in V1ApiRoutes.cpp despite the header documenting `DELETE /api/v1/network/saved/:ssid` — flagged as a stale doc, not a rename concern.

JSON wire-field keys: `apIP`, `apMode`, `apOnly`, `autoRevertSeconds`, `bssid`, `channel`, `complete`, `connected`, `connectionAttempts`, `count`, `durationSeconds`, `encryption`, `ip`, `lastScanTime`, `maxNetworks`, `message`, `networks`, `password`, `revertToApOnly`, `rssi`, `save`, `scanning`, `ssid`, `staEnabled`, `state`, `stats`, `status`, `successfulConnections`, `uptimeSeconds`.

### Handler: PaletteHandlers

Source: `handlers/PaletteHandlers.h` / `.cpp`.

Public methods:

- `handleList(AsyncWebServerRequest*, RendererActor*)`
- `handleCurrent(AsyncWebServerRequest*, RendererActor*)`
- `handleSet(AsyncWebServerRequest*, uint8_t*, size_t, ActorSystem&, std::function<void()> broadcastStatus)`

REST routes: GET `/api/v1/palettes`, `/api/v1/palettes/current`, POST `/api/v1/palettes/set`.

JSON wire-field keys: `avgBrightness`, `calm`, `category`, `cool`, `cvdFriendly`, `flags`, `id`, `maxBrightness`, `name`, `paletteId`, `vivid`, `warm`, `whiteHeavy`.

### Handler: ParameterHandlers

Source: `handlers/ParameterHandlers.h` / `.cpp`.

Public methods:

- `handleGet(AsyncWebServerRequest*, const WebServer::CachedRendererState&)`
- `handleSet(AsyncWebServerRequest*, uint8_t*, size_t, ActorSystem&, std::function<void()> broadcastStatus)`

REST routes: GET `/api/v1/parameters`, POST `/api/v1/parameters`, PATCH `/api/v1/parameters`.

JSON wire-field keys: `brightness`, `complexity`, `fadeAmount`, `hue`, `intensity`, `mood`, `paletteId`, `saturation`, `speed`, `variation`.

### Handler: PluginHandlers

Source: `handlers/PluginHandlers.h` / `.cpp`.

Public methods:

- `handleList(AsyncWebServerRequest*, plugins::PluginManagerActor*)`
- `handleManifests(AsyncWebServerRequest*, plugins::PluginManagerActor*)`
- `handleReload(AsyncWebServerRequest*, plugins::PluginManagerActor*)`

REST routes (documented by header comment but not all surface in `V1ApiRoutes.cpp` enumeration — the registration for these may live in plugin-subsystem-specific glue not in scope here): GET `/api/v1/plugins`, GET `/api/v1/plugins/manifests`, POST `/api/v1/plugins/reload`.

JSON wire-field keys: `count`, `disabledByOverride`, `effectCount`, `error`, `errorCount`, `errors`, `file`, `files`, `lastErrorSummary`, `lastReloadMillis`, `lastReloadOk`, `loadedFromLittleFS`, `manifestCount`, `mode`, `name`, `overrideModeEnabled`, `registeredCount`, `registrationsFailed`, `reloadSuccess`, `stats`, `unregistrations`, `valid`.

### Handler: PresetHandlers

Source: `handlers/PresetHandlers.h` / `.cpp`. Generic preset bag (separate from EffectPresets/ZonePresets).

Public methods:

- `handleList(AsyncWebServerRequest*, persistence::PresetManager*)`
- `handleGet(AsyncWebServerRequest*, persistence::PresetManager*)`
- `handleSave(AsyncWebServerRequest*, uint8_t*, size_t, persistence::PresetManager*)`
- `handleUpdate(AsyncWebServerRequest*, uint8_t*, size_t, persistence::PresetManager*)`
- `handleDelete(AsyncWebServerRequest*, persistence::PresetManager*)`
- `handleRename(AsyncWebServerRequest*, uint8_t*, size_t, persistence::PresetManager*)`
- `handleLoad(AsyncWebServerRequest*, zones::ZoneComposer*, persistence::ZoneConfigManager*, persistence::PresetManager*, std::function<void()> broadcastFn)`
- `handleSaveCurrent(AsyncWebServerRequest*, uint8_t*, size_t, zones::ZoneComposer*, persistence::ZoneConfigManager*, persistence::PresetManager*)`

REST routes: GET `/api/v1/presets`, GET `/api/v1/presets/effects`, GET `/api/v1/presets/zones`, POST `/api/v1/presets`, POST `/api/v1/presets/save-current`, POST `/api/v1/presets/effects/save-current`, POST `/api/v1/presets/zones/save-current`.

JSON wire-field keys: `count`, `presets`, `status`.

### Handler: ShowHandlers

Source: `handlers/ShowHandlers.h` / `.cpp` (~14 KB). Header marked `(stub)` but `.cpp` is implemented.

Public methods:

- `handleCurrent(AsyncWebServerRequest*, actors::ActorSystem&)`
- `handleGet(AsyncWebServerRequest*, const String& showId, const String& format, actors::ActorSystem&)`
- `handleList(AsyncWebServerRequest*, actors::ActorSystem&)`
- `handleCreate(AsyncWebServerRequest*, uint8_t*, size_t, actors::ActorSystem&)`
- `handleUpdate(AsyncWebServerRequest*, const String& showId, uint8_t*, size_t, actors::ActorSystem&)`
- `handleDelete(AsyncWebServerRequest*, const String& showId, actors::ActorSystem&)`
- `handleControl(AsyncWebServerRequest*, uint8_t*, size_t, actors::ActorSystem&)`

REST routes: GET `/api/v1/shows`, GET `/api/v1/shows/current`, POST `/api/v1/shows`, POST `/api/v1/shows/control`, PUT `/api/v1/shows`, DELETE `/api/v1/shows`.

JSON wire-field keys: `action`, `builtin`, `chapterCount`, `chapters`, `count`, `cueCount`, `deleted`, `durationMs`, `dynamicShowCount`, `dynamicShowRamBytes`, `elapsedMs`, `id`, `looping`, `maxDynamic`, `name`, `narrativePhase`, `paused`, `playing`, `ramBytes`, `ramUsageBytes`, `seekMs`, `showId`, `showName`, `shows`, `slot`, `startTimeMs`, `state`, `tensionLevel`.

### Handler: StimulusHandlers

Source: `handlers/StimulusHandlers.h` / `.cpp`.

Public methods:

- `handleMode(AsyncWebServerRequest*, uint8_t*, size_t, ActorSystem&, RendererActor*)`
- `handlePatch(AsyncWebServerRequest*, uint8_t*, size_t, ActorSystem&, RendererActor*)`
- `handleClear(AsyncWebServerRequest*, ActorSystem&, RendererActor*)`
- `handleStatus(AsyncWebServerRequest*, ActorSystem&, RendererActor*)`

REST routes: GET `/api/v1/stimulus/status`, POST `/api/v1/stimulus/mode`, POST `/api/v1/stimulus/patch`, POST `/api/v1/stimulus/clear`.

JSON wire-field keys (snake-case bus mirror): `bands`, `beat_strength`, `beat_tick`, `chroma`, `flux`, `hop_seq`, `mode`, `rms`, `sequence`, `tempo_bpm`, `usingStimulus`.

Note: stimulus payload mirrors `audio.subscribe` schema but emits snake_case (`beat_strength`, `beat_tick`, `hop_seq`, `tempo_bpm`) while the streaming counterpart emits camelCase. Flagged as rename target.

### Handler: SynqMatrixHandlers

Source: `handlers/SynqMatrixHandlers.h` / `.cpp` (~18 KB). This is the REST counterpart of `WsSynqMatrixCommands` and carries the same `songAware*` leak.

Public methods:

- `handleGetConfig(AsyncWebServerRequest*)`
- `handleSetConfig(AsyncWebServerRequest*, uint8_t*, size_t)`
- `handleGetStatus(AsyncWebServerRequest*)`
- `handleGetAllowlist(AsyncWebServerRequest*)`
- `handleSetAllowlist(AsyncWebServerRequest*, uint8_t*, size_t)`
- `handleResetAllowlist(AsyncWebServerRequest*)`

REST routes (all still namespaced `/songAware/...`): GET `/api/v1/songAware/config`, POST `/api/v1/songAware/config`, PATCH `/api/v1/songAware/config`, GET `/api/v1/songAware/status`, GET `/api/v1/songAware/allowlist`, PATCH `/api/v1/songAware/allowlist`, POST `/api/v1/songAware/allowlist/reset`.

JSON wire-field keys: `action`, `actionPlan`, `activeEffectId`, `allowlist`, `allowlistCount`, `antiThrashRemainingMs`, `antiThrashWindowMs`, `audioConfidence`, `automaticEffectSwitches`, `bootGraceMs`, `bootGraceRemainingMs`, `boundaryConfidence`, `boundaryGate`, `boundaryReady`, `bpm`, `candidateAgeMs`, `candidateHoldRemainingMs`, `candidateSongState`, `classificationReason`, `confidence`, `confidenceFloor`, `config`, `constrainedSwitching`, `cooldownRemainingMs`, `count`, `currentSongState`, `dropStateHoldMs`, `dwellRemainingMs`, `effectId`, `effectiveMode`, `enabled`, `enableGraceRemainingMs`, `failures`, `family`, `familyMorphing`, `flux`, `health`, `healthCleanForMs`, `healthCleanWindowMs`, `healthCleanWindowRemainingMs`, `healthDegraded`, `intensityScalar`, `intent`, `lastAction`, `lastDecisionAtMs`, `lastSwitchAtMs`, `lastSwitchFromEffectId`, `lastSwitchReason`, `lastSwitchToEffectId`, `maxSwitchesPerWindow`, `minConfidence`, `minimumDwellMs`, `mode`, `motionScalar`, `owner`, `parameterUpdates`, `policies`, `policy`, `postEnableGraceMs`, `previousEffectId`, `previousSongState`, `previousSuppressedReason`, `profile`, `rawSongState`, `reason`, `reset`, `restored`, `rms`, `rmtErrors`, `selectedEffectId`, `selectedFamily`, `selectedVisualLanguage`, `selectionScore`, `sensitivity`, `showSkips`, `stableStateHoldMs`, `state`, `stateAgeMs`, `status`, `suppressedReason`, `switchCooldownMs`, `switchesInWindow`, `switchingEnabled`, `switchWindowMs`, `switchWindowRemainingMs`, `transitionActive`, `transitionDurationMs`, `transitionPreviousEffectId`, `transitionProgress`, `transitionRemainingMs`, `transitionStartedAtMs`, `transitionTargetEffectId`, `underruns`, `visualLanguage`, `waitingForBoundary`.

Internal helpers (`synqmatrix::*` namespace) still referenced verbatim in this `.cpp`: `synqmatrix::songAwareModeName`, `songAwareProfileName`, `songAwareStateName`, `songAwareOwnerName`, `songAwareSuppressedReasonName`, `songAwareClassificationReasonName`, `songAwareIntentName`, `songAwareActionPlanName`, `songAwareBoundaryGateName`, `songAwareLastActionName`, `songAwareSwitchReasonName`. These are non-wire identifiers but they ARE leaked into wire-field VALUES via `data["mode"] = synqmatrix::songAwareModeName(...)` etc., so the enum-name strings reaching clients depend on what these helpers return.

Error strings emitted to clients (verbatim): "Invalid songAware config", "No songAware restore point captured by REST config", "Invalid songAware state". These are wire-visible.

### Handler: SystemHandlers

Source: `handlers/SystemHandlers.h` / `.cpp`. HATEOAS-style API root.

Public methods:

- `handleHealth(AsyncWebServerRequest*, RendererActor*, AsyncWebSocket*)`
- `handleApiDiscovery(AsyncWebServerRequest*)`
- `handleOpenApiSpec(AsyncWebServerRequest*)`

REST routes: GET `/api/v1/`, GET `/api/v1/health`, GET `/api/v1/ping`, GET `/api/v1/openapi.json`, GET `/api/v1/sync/status` (likely binds here based on namespace; not explicitly verified beyond V1ApiRoutes enumeration).

JSON wire-field keys: `_links`, `apiVersion`, `audioParameters`, `batch`, `centerPoint`, `cpuPercent`, `data`, `description`, `device`, `effects`, `fps`, `freeHeap`, `get`, `hardware`, `info`, `ledsTotal`, `maxZones`, `minFreeHeap`, `name`, `openapi`, `operationId`, `parameters`, `paths`, `post`, `put`, `queueCapacity`, `queueLength`, `queueUtilization`, `rendererRunning`, `responses`, `self`, `servers`, `status`, `strips`, `success`, `summary`, `title`, `totalHeap`, `transitions`, `uptime`, `url`, `version`, `websocket`, `wsClients`, `wsMaxClients`.

### Handler: TransitionHandlers

Source: `handlers/TransitionHandlers.h` / `.cpp`.

Public methods:

- `handleTypes(AsyncWebServerRequest*)`
- `handleTrigger(AsyncWebServerRequest*, uint8_t*, size_t, ActorSystem&, const WebServer::CachedRendererState&, std::function<void()> broadcastStatus)`
- `handleConfigGet(AsyncWebServerRequest*, RendererActor*)`
- `handleConfigSet(AsyncWebServerRequest*, uint8_t*, size_t)`

REST routes: GET `/api/v1/transitions/types`, GET `/api/v1/transitions/config`, POST `/api/v1/transitions/trigger`, POST `/api/v1/transitions/config`.

JSON wire-field keys: `centerOrigin`, `defaultDuration`, `defaultType`, `defaultTypeName`, `duration`, `easings`, `effectId`, `enabled`, `id`, `message`, `name`, `random`, `toEffect`, `transitionType`, `type`, `types`.

### Handler: VrmsHandlers

Source: `handlers/VrmsHandlers.h` / `.cpp`. Guarded by `FEATURE_VRMS_METRICS`.

Public methods:

- `handleGetVrms(AsyncWebServerRequest*, RendererActor*)`

REST route: GET `/api/v1/vrms`.

JSON wire-field keys: `audioVisualCorr`, `brightnessMean`, `brightnessVariance`, `colourVariance`, `dominantHue`, `spatialCentroid`, `symmetryScore`, `temporalFreq`. (Note: `colourVariance` is the only British-English wire field located in this entire surface; siblings use American `color*`. Flagged as inconsistency.)

### Handler: ZoneHandlers

Source: `handlers/ZoneHandlers.h` / `.cpp` (~27 KB).

Public methods:

- `handleList(AsyncWebServerRequest*, ActorSystem&, const WebServer::CachedRendererState&, ZoneComposer*)`
- `handleLayout(AsyncWebServerRequest*, uint8_t*, size_t, ZoneComposer*, std::function<void()> broadcastZoneState)`
- `handleGet(AsyncWebServerRequest*, ActorSystem&, const WebServer::CachedRendererState&, ZoneComposer*)`
- `handleSetEffect(AsyncWebServerRequest*, uint8_t*, size_t, ActorSystem&, const WebServer::CachedRendererState&, ZoneComposer*, std::function<void()> broadcastZoneState)`
- `handleSetBrightness/Speed/Palette/Blend/Enabled(AsyncWebServerRequest*, uint8_t*, size_t, ZoneComposer*, std::function<void()> broadcastZoneState)`
- `handleConfigGet/Save/Load(AsyncWebServerRequest*, ZoneComposer*, void* zoneConfigMgr [, std::function<void()> broadcastZoneState])`
- `handleTimingGet/Reset(AsyncWebServerRequest*, ZoneComposer*)`
- `handleAudioConfigGet(AsyncWebServerRequest*, uint8_t zoneId, ZoneComposer*)`
- `handleAudioConfigSet(AsyncWebServerRequest*, uint8_t*, size_t, uint8_t zoneId, ZoneComposer*, std::function<void()> broadcastZoneState)`
- `handleBeatTriggerGet/Set(AsyncWebServerRequest*, uint8_t zoneId, ZoneComposer*, ...)`
- `handleReorder(AsyncWebServerRequest*, uint8_t*, size_t, ZoneComposer*, std::function<void()> broadcastZoneState)`
- `extractZoneIdFromPath(AsyncWebServerRequest*) -> uint8_t` (private)

REST routes: GET `/api/v1/zones`, GET `/api/v1/zones/config`, POST `/api/v1/zones/config/save`, POST `/api/v1/zones/config/load`, POST `/api/v1/zones/enabled`, POST `/api/v1/zones/layout`, POST `/api/v1/zones/reorder`, GET `/api/v1/zones/timing`, POST `/api/v1/zones/timing/reset`.

JSON wire-field keys: `blendMode`, `blendModeName`, `brightness`, `effectId`, `effectName`, `enabled`, `id`, `name`, `paletteId`, `paletteName`, `presets`, `s1LeftEnd`, `s1LeftStart`, `s1RightEnd`, `s1RightStart`, `segments`, `speed`, `totalLeds`, `zoneCount`, `zoneId`, `zones`.

### Handler: ZonePresetHandlers

Source: `handlers/ZonePresetHandlers.h` / `.cpp` (~17 KB).

Public methods:

- `handleList(AsyncWebServerRequest*)`
- `handleSave(AsyncWebServerRequest*, uint8_t*, size_t, zones::ZoneComposer*)`
- `handleGet(AsyncWebServerRequest*, uint8_t id)`
- `handleApply(AsyncWebServerRequest*, uint8_t id, actors::ActorSystem&, zones::ZoneComposer*, std::function<void()> broadcastFn)`
- `handleDelete(AsyncWebServerRequest*, uint8_t id)`

REST routes: GET `/api/v1/zone-presets`, GET `/api/v1/zone-presets/get`, POST `/api/v1/zone-presets`, POST `/api/v1/zone-presets/apply`, DELETE `/api/v1/zone-presets/delete`.

JSON wire-field keys: `applied`, `blendMode`, `brightness`, `builtin`, `builtinCount`, `count`, `deleted`, `effectId`, `enabled`, `id`, `maxUserPresets`, `message`, `name`, `paletteId`, `presets`, `s1LeftEnd`, `s1LeftStart`, `s1RightEnd`, `s1RightStart`, `segments`, `speed`, `timestamp`, `zoneCount`, `zones`.

---

## WebSocket Command Modules

All WS files live in namespace `lightwaveos::network::webserver::ws`. Each module exposes a single free function `registerWsXxxCommands(const WebServerContext& ctx)` that calls `WsCommandRouter::registerCommand("<cmd.name>", handleXxx)` for every command in the module. Handlers are file-static free functions with the signature `void handleXxx(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& context)` (occasionally taking `const JsonObject& params` plus a `const char* requestId`). Below: registration function, the set of registered command strings, the handler function names, and JSON wire-field keys in response bodies.

### Ws: WsAudioCommands

Registration: `registerWsAudioCommands(const WebServerContext& ctx)`.

WS commands registered: `audio.parameters.get`, `audio.parameters.set`, `audio.subscribe`, `audio.unsubscribe`, `audio.zone-agc.get`, `audio.zone-agc.set`, `audio.spike-detection.get`, `audio.spike-detection.reset`, `audio.mic-gain.get`, `audio.mic-gain.set`.

Handlers: `handleAudioParametersGet`, `handleAudioParametersSet`, `handleAudioSubscribe`, `handleAudioUnsubscribe`, `handleAudioZoneAgcGet`, `handleAudioZoneAgcSet`, `handleAudioSpikeDetectionGet`, `handleAudioSpikeDetectionReset`, `handleAudioMicGainGet`, `handleAudioMicGainSet`.

JSON wire-field keys (response payloads): mostly mirror `AudioHandlers` (`agc*`, `band*`, `bins64`, `bpm*`, `beatStrength`, `beatTick`, `beatInBar`, `beatUnit`, `beatsPerBar`, `confidenceTau`, `contract`, `controlBus`, `dcEstimate`, `flux`, `gateRange*`, `goertzelWindow`, `noiseFloor*`, `phaseCorrectionGain`, `pipeline`, `rms*`, `spikes*`, `streamVersion`, `tempoConfidence`, `udpPort`, `waveformPoints`, `waveformSize`, `zones`, etc.), plus envelope fields `clientId`, `code`, `error`, `requestId`, `success`, `transport`, `type`.

### Ws: WsAuthCommands

Registration: `registerWsAuthCommands(const WebServerContext&)`.

WS commands: `auth.status`, `auth.rotate`.

Handlers: `handleAuthStatus`, `handleAuthRotate`.

JSON wire-field keys: `enabled`, `key`, `keyConfigured`, `message`, `requestId`.

### Ws: WsBatchCommands

Registration: `registerWsBatchCommands(const WebServerContext&)`.

WS command: `batch`.

Handler: `handleBatch`.

JSON wire-field keys: `action`, `failed`, `operations`, `processed`, `requestId`.

### Ws: WsColorCommands

Registration: `registerWsColorCommands(const WebServerContext&)`.

WS commands: `color.getStatus`, `color.enableBlend`, `color.setBlendPalettes`, `color.setBlendFactors`, `color.enableRotation`, `color.setRotationSpeed`, `color.enableDiffusion`, `color.setDiffusionAmount`, `colorCorrection.getConfig`, `colorCorrection.setMode`, `colorCorrection.setConfig`, `colorCorrection.save`.

Handlers: `handleColorGetStatus`, `handleColorEnableBlend`, `handleColorSetBlendPalettes`, `handleColorSetBlendFactors`, `handleColorEnableRotation`, `handleColorSetRotationSpeed`, `handleColorEnableDiffusion`, `handleColorSetDiffusionAmount`, `handleColorCorrectionGetConfig`, `handleColorCorrectionSetMode`, `handleColorCorrectionSetConfig`, `handleColorCorrectionSave`.

JSON wire-field keys: `active`, `autoExposureEnabled`, `autoExposureTarget`, `blendEnabled`, `blendFactors`, `blendPalettes`, `brownGuardrailEnabled`, `diffusionAmount`, `diffusionEnabled`, `gammaEnabled`, `gammaLut`, `gammaValue`, `hsvMinSaturation`, `lutGenerationId`, `maxBluePercentOfRed`, `maxGreenPercentOfRed`, `mode`, `modeName`, `modeNames`, `requestId`, `rgbTargetMin`, `rgbWhiteThreshold`, `rotationEnabled`, `rotationPhase`, `rotationSpeed`, `saved`, `updated`.

### Ws: WsDebugCommands

Registration: `registerWsDebugCommands(const webserver::WebServerContext&)`.

WS commands: `debug.audio.get`, `debug.audio.set`, `debug.udp.get`.

Handlers: `handleDebugAudioGet`, `handleDebugAudioSet`, `handleDebugUdpGet`.

JSON wire-field keys: `attempts`, `audio`, `baseInterval`, `consecutiveFailures`, `cooldownRemainingMs`, `dma`, `failures`, `intervals`, `lastFailureAgoMs`, `lastFailureMs`, `lastSocketResetMs`, `led`, `levels`, `requestId`, `socketResets`, `started`, `subscribers`, `success`, `suppressed`, `verbosity`.

### Ws: WsDeviceCommands

Registration: `registerWsDeviceCommands(const WebServerContext&)`.

WS commands: `getStatus` (legacy alias), `device.getStatus`, `device.getInfo`.

Handlers: `handleLegacyGetStatus`, `handleDeviceGetStatus`, `handleDeviceGetInfo`.

JSON wire-field keys: envelope (`_id`, `_time`, `_type`, `type`, `requestId`) plus device fields (`apMode`, `brightness`, `chipCores`, `chipModel`, `chipRevision`, `complexity`, `connected`, `cpuFreq`, `cpuFreqMHz`, `cpuPercent`, `edgeMixerMode`, `edgeMixerModeName`, `edgeMixerSpatial`, `edgeMixerSpatialName`, `edgeMixerSpread`, `edgeMixerStrength`, `edgeMixerTemporal`, `edgeMixerTemporalName`, `effectCount`, `effectId`, `effectName`, `flashSize`, `fps`, `framesRendered`, `freeHeap`, `freeSketchSpace`, `heapSize`, `hue`, `intensity`, `ip`, `network`, `paletteId`, `rssi`, `saturation`, `sketchSize`, `speed`, `uptime`, `variation`).

### Ws: WsEdgeMixerCommands

Registration: `registerWsEdgeMixerCommands(const webserver::WebServerContext&)`.

WS commands: `edge_mixer.set`, `edge_mixer.get`, `edge_mixer.save`, `edgemixer_mode` (legacy alias — note inconsistent casing).

Handlers: `handleEdgeMixerSet`, `handleEdgeMixerGet`, `handleEdgeMixerSave`, `handleEdgeMixerMode`.

JSON wire-field keys: `mode`, `modeName`, `requestId`, `saved`, `spatial`, `spatialName`, `spread`, `strength`, `temporal`, `temporalName`.

Note: this is the only WS namespace that mixes underscore-style command names (`edge_mixer.set`, `edgemixer_mode`) with the otherwise dominant dot-camelCase convention. Flagged as rename target.

### Ws: WsEffectPresetCommands

Registration: `registerWsEffectPresetCommands(const webserver::WebServerContext&)`.

WS commands: `effectPresets.list`, `effectPresets.get`, `effectPresets.saveCurrent`, `effectPresets.load`, `effectPresets.delete`.

Handlers: `handleEffectPresetsList`, `handleEffectPresetsGet`, `handleEffectPresetsSaveCurrent`, `handleEffectPresetsLoad`, `handleEffectPresetsDelete`.

JSON wire-field keys: `brightness`, `complexity`, `count`, `effectId`, `hue`, `id`, `intensity`, `maxSlots`, `mood`, `name`, `occupied`, `paletteId`, `preset`, `presets`, `requestId`, `saturation`, `slot`, `speed`, `timestamp`, `trails`, `type`, `variation`.

### Ws: WsEffectsCommands

Registration: `registerWsEffectsCommands(const webserver::WebServerContext&)`.

WS commands (20 total): `effects.getMetadata`, `effects.getCurrent`, `effects.list`, `setEffect` (legacy), `nextEffect` (legacy), `prevEffect` (legacy), `setBrightness` (legacy), `setSpeed` (legacy), `setPalette` (legacy), `effects.setCurrent`, `effects.parameters.get`, `effects.parameters.set`, `effects.getCategories`, `effects.getByFamily`, `parameters.get`, `parameters.set`, `cameraMode.set`, `cameraMode.get`, `factoryPresets.list`, `factoryPresets.load`.

Handlers: `handleEffectsGetMetadata`, `handleEffectsGetCurrent`, `handleEffectsList`, `handleSetEffect`, `handleNextEffect`, `handlePrevEffect`, `handleSetBrightness`, `handleSetSpeed`, `handleSetPalette`, `handleEffectsSetCurrent`, `handleEffectsParametersGet`, `handleEffectsParametersSet`, `handleEffectsGetCategories`, `handleEffectsGetByFamily`, `handleParametersGet`, `handleParametersSet`, `handleCameraModeSet`, `handleCameraModeGet`, `handleFactoryPresetsList`, `handleFactoryPresetsLoad`.

JSON wire-field keys: `activeIndex`, `data`, `effectId`, `enabled`, `index`, `name`, `paletteIndex`, `presets`, `requestId`, `type`.

Note: this is the most aliased namespace — `setEffect`/`setBrightness`/`setSpeed`/`setPalette` are top-level legacy commands that duplicate `effects.*` and `parameters.set`. Captain should review whether legacy aliases survive the rename.

### Ws: WsFilesystemCommands

Registration: `registerWsFilesystemCommands(const webserver::WebServerContext&)`.

WS commands: NONE actually registered. Header inlines stub handler classes `WsFilesystemCommands::handleList/Read/Write/Delete/Info` that all return `FEATURE_DISABLED` errors. The `.cpp` `registerWsFilesystemCommands` function is empty (all `registerCommand` calls are commented out — see anomalies).

Handlers: stub class methods only — not reachable.

JSON wire-field keys: none emitted (error stub).

### Ws: WsModifierCommands

Registration: `registerWsModifierCommands(const webserver::WebServerContext&)`. Guarded by `FEATURE_WEB_SERVER`.

WS commands: header docs claim `modifiers.list`, `modifiers.add`, `modifiers.remove`, `modifiers.clear`, `modifiers.update` but `.cpp` shows NO `registerCommand` calls — surface is currently dead at the WS level (REST counterpart in `ModifierHandlers` is live).

Handlers: none extracted from `.cpp`.

JSON wire-field keys: none observed in `.cpp` extraction.

### Ws: WsMotionCommands

Registration: `registerWsMotionCommands(const webserver::WebServerContext&)`.

WS commands: `motion.getStatus`, `motion.enable`, `motion.disable`, `motion.phase.setOffset`, `motion.phase.enableAutoRotate`, `motion.phase.getPhase`, `motion.speed.setModulation`, `motion.speed.setBaseSpeed`, `motion.momentum.getStatus`, `motion.momentum.addParticle`, `motion.momentum.applyForce`, `motion.momentum.getParticle`, `motion.momentum.reset`, `motion.momentum.update`.

Handlers: `handleMotionGetStatus`, `handleMotionEnable`, `handleMotionDisable`, `handleMotionPhaseSetOffset`, `handleMotionPhaseEnableAutoRotate`, `handleMotionPhaseGetPhase`, `handleMotionSpeedSetModulation`, `handleMotionSpeedSetBaseSpeed`, `handleMotionMomentumGetStatus`, `handleMotionMomentumAddParticle`, `handleMotionMomentumApplyForce`, `handleMotionMomentumGetParticle`, `handleMotionMomentumReset`, `handleMotionMomentumUpdate`.

JSON wire-field keys: `activeCount`, `alive`, `applied`, `autoRotate`, `autoRotateSpeed`, `baseSpeed`, `boundary`, `degrees`, `degreesPerSecond`, `deltaTime`, `depth`, `enabled`, `force`, `mass`, `maxParticles`, `message`, `particleId`, `phaseOffset`, `position`, `radians`, `requestId`, `speed`, `success`, `type`, `updated`, `velocity`.

### Ws: WsNarrativeCommands

Registration: `registerWsNarrativeCommands(const webserver::WebServerContext&)`.

WS commands: `narrative.getStatus`, `narrative.config`.

Handlers: `handleNarrativeGetStatus`, `handleNarrativeConfig`.

JSON wire-field keys: none captured by bracket-access pattern in this file. (Either narrative uses dotted JsonVariant or the response is delegated to the REST helper. Flagged for spot-check.)

### Ws: WsOtaCommands

Registration: `registerWsOtaCommands(const WebServerContext&)`. Also exports: `handleOtaClientDisconnect(uint32_t clientId)`, `forceAbortStaleOtaSession(const char* reason)`, `isWsOtaInProgress() -> bool`.

WS commands: `ota.check`, `ota.begin`, `ota.chunk`, `ota.abort`, `ota.verify`.

Handlers: `handleOtaCheck`, `handleOtaBegin`, `handleOtaChunk`, `handleOtaAbort`, `handleOtaVerify` (plus `handleOtaClientDisconnect`).

JSON wire-field keys: none captured by bracket-access pattern (OTA almost certainly uses dynamic doc population — verify in a separate pass).

### Ws: WsPaletteCommands

Registration: `registerWsPaletteCommands(const webserver::WebServerContext&)`.

WS commands: `palettes.list`, `palettes.get`, `palettes.set`.

Handlers: `handlePalettesList`, `handlePalettesGet`, `handlePalettesSet`.

JSON wire-field keys: `avgBrightness`, `calm`, `category`, `cool`, `cvdFriendly`, `flags`, `id`, `limit`, `maxBrightness`, `name`, `page`, `pages`, `pagination`, `palette`, `paletteId`, `palettes`, `requestId`, `total`, `vivid`, `warm`, `whiteHeavy`.

### Ws: WsPluginCommands

Registration: `registerWsPluginCommands(const WebServerContext&)`.

WS commands: `plugins.list`, `plugins.stats`, `plugins.reload`.

Handlers: `handlePluginsList`, `handlePluginsStats`, `handlePluginsReload`.

JSON wire-field keys: none captured by bracket-access pattern (likely doc-build helpers).

### Ws: WsPresetCommands

Registration: `registerWsPresetCommands(const webserver::WebServerContext&)`.

WS commands: NONE actually registered (`.cpp` has no `registerCommand` calls).

Handlers: none extracted.

JSON wire-field keys: none.

Status: declared but inert. Flagged.

### Ws: WsRenderCommands

Registration: `registerWsRenderCommands(const webserver::WebServerContext&)`.

WS commands: `render.dithering.get`, `render.dithering.set`.

Handlers: `handleRenderDitheringGet`, `handleRenderDitheringSet`.

JSON wire-field keys: `enabled`, `requestId`.

### Ws: WsShowCommands

Registration: `registerWsShowCommands(const WebServerContext&)`.

WS commands: `show.upload`, `show.play`, `show.pause`, `show.resume`, `show.stop`, `show.seek`, `show.status`, `show.list`, `show.delete`, `show.cue.inject`, `prim8.set`.

Handlers: `handleShowUpload`, `handleShowPlay`, `handleShowPause`, `handleShowResume`, `handleShowStop`, `handleShowSeek`, `handleShowStatus`, `handleShowList`, `handleShowDelete`, `handleShowCueInject`, `handlePrim8Set`.

JSON wire-field keys: `brightness`, `builtin`, `builtinIndex`, `chapterCount`, `cmd`, `complexity`, `cue`, `cueCount`, `data`, `durationMs`, `dynamicShowCount`, `dynamicShowRamBytes`, `effectId`, `elapsedMs`, `error`, `fadeAmount`, `gravity`, `heat`, `hue`, `id`, `impact`, `intensity`, `looping`, `mapped`, `mass`, `momentum`, `mood`, `name`, `paletteId`, `paused`, `playing`, `pressure`, `ramBytes`, `ramUsageBytes`, `saturation`, `showId`, `showName`, `shows`, `slot`, `source`, `space`, `speed`, `success`, `texture`, `timeMs`, `transitionType`, `type`, `variation`, `zone`.

### Ws: WsStatusCommands

Registration: `registerWsStatusCommands(const WebServerContext&)`.

WS commands: `status.subscribe`, `status.unsubscribe` (acks: `status.subscribed`, `status.unsubscribed`).

Handlers: `handleStatusSubscribe`, `handleStatusUnsubscribe`.

JSON wire-field keys: `clientId`, `code`, `error`, `message`, `periodMs`, `requestId`, `success`, `type`.

### Ws: WsStimulusCommands

Registration: `registerWsStimulusCommands(const WebServerContext&)`.

WS commands: `stimulus.mode`, `stimulus.patch`, `stimulus.clear`, `stimulus.status`.

Handlers: `handleStimulusMode`, `handleStimulusPatch`, `handleStimulusClear`, `handleStimulusStatus`.

JSON wire-field keys: `bands`, `beat_strength`, `beat_tick`, `chroma`, `error`, `flux`, `hop_seq`, `id`, `mode`, `op`, `patch`, `rms`, `sequence`, `success`, `tempo_bpm`, `usingStimulus`. (Same snake-case mirror as `StimulusHandlers`.)

### Ws: WsStmCommands

Registration: `registerWsStmCommands(const webserver::WebServerContext&)`. Guarded by `FEATURE_AUDIO_SYNC`.

WS commands: `stm.subscribe`, `stm.unsubscribe`.

Handlers: `handleStmSubscribe`, `handleStmUnsubscribe`.

JSON wire-field keys: `clientId`, `frameSize`, `magic`, `requestId`, `spectralBins`, `targetFps`, `temporalBands`.

### Ws: WsStreamCommands

Registration: `registerWsStreamCommands(const webserver::WebServerContext&)`. Also exports `hasFftStreamSubscribers() -> bool`, `broadcastFftFrame(...)` (inline stubs), `hasBeatEventSubscribers() -> bool`, `removeBeatSubscriber(uint32_t)`, and (guarded) `hasVrmsStreamSubscribers/removeVrmsSubscriber/getVrmsSubscriberTable`.

WS commands: `ledStream.subscribe`, `ledStream.unsubscribe`, `validation.subscribe`, `validation.unsubscribe`, `benchmark.subscribe`, `benchmark.unsubscribe`, `benchmark.start`, `benchmark.stop`, `benchmark.get`, `beat.subscribe`, `beat.unsubscribe`, `vrms.subscribe`, `vrms.unsubscribe`, `merge.submit`.

Handlers: `handleLedStreamSubscribe`, `handleLedStreamUnsubscribe`, `handleValidationSubscribe`, `handleValidationUnsubscribe`, `handleBenchmarkSubscribe`, `handleBenchmarkUnsubscribe`, `handleBenchmarkStart`, `handleBenchmarkStop`, `handleBenchmarkGet`, `handleBeatSubscribe`, `handleBeatUnsubscribe`, `handleVrmsSubscribe`, `handleVrmsUnsubscribe`, `handleMergeSubmit`.

JSON wire-field keys: `clientId`, `frameSize`, `frameVersion`, `intervalMs`, `ledsPerStrip`, `magic`, `numStrips`, `params`, `paramsApplied`, `paramsDropped`, `requestId`, `source`, `targetFps`, `transport`, `udpPort`.

### Ws: WsSynqMatrixCommands

Registration: `registerWsSynqMatrixCommands(const WebServerContext&)`.

WS commands (ALL still `songAware.*`): `songAware.config.get`, `songAware.config.set`, `songAware.status`, `songAware.reset`, `songAware.restore`, `songAware.debug`, `songAware.policy`, `songAware.allowlist`, `songAware.allowlist.set`, `songAware.allowlist.reset`, `songAware.health`, `songAware.counters.reset`, `songAware.countersReset` (legacy alias).

Handlers: `handleSynqMatrixConfigGet`, `handleSynqMatrixConfigSet`, `handleSynqMatrixStatus`, `handleSynqMatrixReset`, `handleSynqMatrixRestore`, `handleSynqMatrixDebug`, `handleSynqMatrixPolicy`, `handleSynqMatrixAllowlist`, `handleSynqMatrixAllowlistSet`, `handleSynqMatrixAllowlistReset`, `handleSynqMatrixHealth`, `handleSynqMatrixCountersReset`.

Response event names emitted via `buildWsResponse("songAware.<x>", ...)` (verbatim from .cpp): `songAware.config`, `songAware.status`, `songAware.reset`, `songAware.restore`, `songAware.debug`, `songAware.policy`, `songAware.allowlist`, `songAware.health`, `songAware.counters.reset`.

JSON wire-field keys: same set as `SynqMatrixHandlers` REST counterpart — `actionPlan`, `activeEffectId`, `allowlist`, `allowlistCount`, `antiThrashRemainingMs`, `antiThrashWindowMs`, `audioConfidence`, `automaticEffectSwitches`, `bootGraceMs`, `bootGraceRemainingMs`, `boundaryConfidence`, `boundaryGate`, `boundaryReady`, `bpm`, `candidateAgeMs`, `candidateHoldRemainingMs`, `candidateSongState`, `classificationReason`, `confidence`, `confidenceFloor`, `config`, `constrainedSwitching`, `cooldownRemainingMs`, `count`, `currentSongState`, `dropStateHoldMs`, `dwellRemainingMs`, `effectId`, `effectiveMode`, `enabled`, `enableGraceRemainingMs`, `failures`, `family`, `familyMorphing`, `flux`, `health`, `healthCleanForMs`, `healthCleanWindowMs`, `healthCleanWindowRemainingMs`, `healthDegraded`, `intensityScalar`, `intent`, `lastAction`, `lastDecisionAtMs`, `lastSwitchAtMs`, `lastSwitchFromEffectId`, `lastSwitchReason`, `lastSwitchToEffectId`, `maxSwitchesPerWindow`, `minConfidence`, `minimumDwellMs`, `mode`, `motionScalar`, `owner`, `parameterUpdates`, `policies`, `policy`, `postEnableGraceMs`, `previousEffectId`, `previousSongState`, `previousSuppressedReason`, `profile`, `rawSongState`, `reason`, `requestId`, `reset`, `restored`, `rms`, `rmtErrors`, `selectedEffectId`, `selectedFamily`, `selectedVisualLanguage`, `selectionScore`, `sensitivity`, `showSkips`, `stableStateHoldMs`, `state`, `stateAgeMs`, `status`, `suppressedReason`, `switchCooldownMs`, `switchesInWindow`, `switchingEnabled`, `switchWindowMs`, `switchWindowRemainingMs`, `transitionActive`, `transitionDurationMs`, `transitionPreviousEffectId`, `transitionProgress`, `transitionRemainingMs`, `transitionStartedAtMs`, `transitionTargetEffectId`, `underruns`, `visualLanguage`, `waitingForBoundary`.

Helper function calls (non-wire but produce wire VALUES): `synqmatrix::songAwareModeName/songAwareProfileName/songAwareStateName/songAwareOwnerName/songAwareSuppressedReasonName/songAwareClassificationReasonName/songAwareIntentName/songAwareActionPlanName/songAwareBoundaryGateName/songAwareLastActionName/songAwareSwitchReasonName`.

User-facing error strings: "Invalid songAware config", "No songAware restore point captured by WebSocket", "Invalid songAware state".

### Ws: WsSysCommands

Registration: `registerWsSysCommands(const WebServerContext&)`.

WS command: `sys.capabilities`.

Handler: `handleSysCapabilities`.

JSON wire-field keys: `audio_sync`, `ota`, `pattern_registry`, `transitions`, `trinity`, `type`, `zones` — note the snake_case `audio_sync` and `pattern_registry` here vs camelCase elsewhere. Flagged as a rename target.

### Ws: WsTransitionCommands

Registration: `registerWsTransitionCommands(const webserver::WebServerContext&)`.

WS commands: `transition.trigger`, `transition.getTypes`, `transition.config` (lambda handler inline), `transitions.list`, `transitions.trigger`.

Handlers: `handleTransitionTrigger`, `handleTransitionGetTypes`, lambda for `transition.config` (also `handleTransitionConfigGet`/`handleTransitionConfigSet` defined locally), `handleTransitionsList`, `handleTransitionsTrigger`.

JSON wire-field keys: none captured by the bracket-access regex. (Either keys built via builder helpers or via lambda-local doc — verify.)

Note: BOTH singular (`transition.*`) and plural (`transitions.*`) command namespaces exist for the same surface; the REST surface is plural-only (`/api/v1/transitions/...`). Flagged as a rename target.

### Ws: WsTrinityCommands

Registration: `registerWsTrinityCommands(const WebServerContext&)`.

WS commands: `trinity.beat`, `trinity.macro`, `trinity.segment`, `trinity.sync`.

Handlers: `handleTrinityBeat`, `handleTrinityMacro`, `handleTrinitySegment`, `handleTrinitySync`.

JSON wire-field keys: envelope (`_id`, `_type`), then snake-case payload (`action`, `bass_weight`, `beat_in_bar`, `beat_phase`, `bpm`, `brightness`, `downbeat`, `end_sec`, `end`, `energy`, `index`, `label`, `percussiveness`, `position_sec`, `start_sec`, `start`, `tick`, `vocal_presence`).

Note: Trinity uses snake_case (`bass_weight`, `beat_in_bar`, `position_sec`) where the audio chain uses camelCase. Flagged as rename target. `start`/`end` shadow `start_sec`/`end_sec` — confusing.

### Ws: WsZonePresetCommands

Registration: `registerWsZonePresetCommands(const WebServerContext&)`.

WS commands: `zonePresets.list`, `zonePresets.get`, `zonePresets.saveCurrent`, `zonePresets.load`, `zonePresets.delete`.

Handlers: `handleZonePresetsList`, `handleZonePresetsGet`, `handleZonePresetsSaveCurrent`, `handleZonePresetsLoad`, `handleZonePresetsDelete`.

JSON wire-field keys: `blendMode`, `brightness`, `count`, `effectId`, `id`, `maxSlots`, `name`, `paletteId`, `preset`, `presets`, `requestId`, `s1LeftEnd`, `s1LeftStart`, `s1RightEnd`, `s1RightStart`, `segment`, `slot`, `speed`, `timestamp`, `zoneCount`, `zoneId`, `zones`.

### Ws: WsZonesCommands

Registration: `registerWsZonesCommands(const webserver::WebServerContext&)`.

WS commands (15): `zone.enable`, `zone.enableZone`, `zone.setEffect`, `zone.setBrightness`, `zone.setSpeed`, `zone.setPalette`, `zone.setBlend`, `zone.loadPreset`, `zones.get`, `zones.list`, `zones.update`, `zones.setEffect`, `zones.setLayout`, `getZoneState` (legacy).

Handlers: `handleZoneEnable`, `handleZoneEnableZone`, `handleZoneSetEffect`, `handleZoneSetBrightness`, `handleZoneSetSpeed`, `handleZoneSetPalette`, `handleZoneSetBlend`, `handleZoneLoadPreset`, `handleZonesGet`, `handleZonesList`, `handleZonesUpdate`, `handleZonesSetEffect`, `handleZonesSetLayout`, `handleGetZoneState`.

JSON wire-field keys: `enabled`, `requestId`, `zoneId`. (Most responses delegate to shared zone-state builder; deeper field set lives in the helper.)

Note: BOTH singular (`zone.*`) and plural (`zones.*`) namespaces exist. `zone.enable` AND `zone.enableZone` both exist (semantic split unclear). `getZoneState` is a legacy top-level alias. Flagged.

---

## Anomalies

### Anomalies: Leftover songAware wire surface

This is the dominant rename anomaly in the network layer. Despite the C++ class names having been migrated to `SynqMatrix*` and the canonical project name being `synqmatrix`, the **client-visible wire vocabulary is still entirely `songAware`**:

REST routes (live in `firmware-v3/src/network/webserver/V1ApiRoutes.cpp`):

- GET `/api/v1/songAware/config`
- POST `/api/v1/songAware/config`
- PATCH `/api/v1/songAware/config`
- GET `/api/v1/songAware/status`
- GET `/api/v1/songAware/allowlist`
- PATCH `/api/v1/songAware/allowlist`
- POST `/api/v1/songAware/allowlist/reset`

WebSocket commands (in `WsSynqMatrixCommands.cpp`): `songAware.config.get`, `songAware.config.set`, `songAware.status`, `songAware.reset`, `songAware.restore`, `songAware.debug`, `songAware.policy`, `songAware.allowlist`, `songAware.allowlist.set`, `songAware.allowlist.reset`, `songAware.health`, `songAware.counters.reset`, `songAware.countersReset`.

WebSocket response event names (response `type` field via `buildWsResponse("songAware.<x>", ...)`): `songAware.config`, `songAware.status`, `songAware.reset`, `songAware.restore`, `songAware.debug`, `songAware.policy`, `songAware.allowlist`, `songAware.health`, `songAware.counters.reset`.

Wire-value generators (these produce the literal strings clients receive in `mode`, `profile`, `state`, `owner`, `intent`, etc.): `synqmatrix::songAwareModeName`, `songAwareProfileName`, `songAwareStateName`, `songAwareOwnerName`, `songAwareSuppressedReasonName`, `songAwareClassificationReasonName`, `songAwareIntentName`, `songAwareActionPlanName`, `songAwareBoundaryGateName`, `songAwareLastActionName`, `songAwareSwitchReasonName`. The enum-name strings they return have not been audited in this SSA — they may also leak `songAware*` tokens into wire values, not just keys.

Wire-visible error strings (returned in error envelopes): "Invalid songAware config", "Invalid songAware state", "No songAware restore point captured by REST config", "No songAware restore point captured by WebSocket".

JSON wire-field keys carrying `Song` (response payloads of `songAware.*`): `currentSongState`, `previousSongState`, `candidateSongState`, `rawSongState`. These are the only response keys containing `Song` — they enumerate the state-machine state and are the strongest candidates for a `currentState`/`previousState`/`candidateState`/`rawState` rename (or a different domain prefix).

Total `songAware|song_aware|songaware|songState|song_state` hits inside the two SynqMatrix files: **69** lines.

### Anomalies: Other naming oddities

These are not `songAware` but worth Captain visibility:

1. **WsFilesystemCommands / WsModifierCommands / WsPresetCommands** — declared and exported but register ZERO WS commands. Either dead code or intentional stubs. The header for `WsFilesystemCommands` even ships inline stub handlers that always return `FEATURE_DISABLED`, never wired.
2. **Singular/plural duplication.** `WsZonesCommands` registers both `zone.*` and `zones.*` (some overlap: `zone.setEffect` vs `zones.setEffect`). `WsTransitionCommands` registers both `transition.*` and `transitions.*`. REST surface is plural-only for transitions and `/zones`. Wire schema disagreement.
3. **Casing drift in wire fields.**
   - Snake-case islands inside an otherwise camelCase surface: `bar_phase`, `beat_in_bar`, `beat_phase`, `beats_per_bar` (in `AudioHandlers.cpp`) — coexist with their camelCase mirrors `beatInBar`, `beatsPerBar`, `phase01AtAudioT`.
   - `StimulusHandlers` + `WsStimulusCommands` use snake_case (`beat_strength`, `beat_tick`, `hop_seq`, `tempo_bpm`) where the streaming audio counterpart uses camelCase (`beatStrength`, `beatTick`, `bpm`).
   - `WsTrinityCommands` is entirely snake_case (`bass_weight`, `vocal_presence`, `position_sec`, `start_sec`, `end_sec`).
   - `WsSysCommands` emits snake_case `audio_sync` and `pattern_registry` next to camelCase peers in the same `sys.capabilities` response.
   - `WsDeviceCommands` emits envelope fields `_id`, `_time`, `_type` (underscore-prefixed) — unique to that module.
4. **REST route segment casing.** Most routes are kebab-case (`zone-presets`, `effect-presets`, `audio/zone-agc`, `device/ota-token`) but four are camelCase segments: `/api/v1/colorCorrection/*`, `/api/v1/edgeMixer`, `/api/v1/factoryPresets`, `/api/v1/songAware/*`. Inconsistent with the kebab-case norm.
5. **WS edge-mixer command names** are underscore-style (`edge_mixer.set`, `edge_mixer.get`, `edge_mixer.save`, plus the legacy `edgemixer_mode` with no separator at all) while the REST route is camelCase (`/api/v1/edgeMixer`) and the JSON payload uses camelCase (`edgeMixerMode`, `edgeMixerSpatialName`). Three different conventions for the same concept across one feature.
6. **`colourVariance` is the only British-English wire field** found in the network surface (in `VrmsHandlers`), while sibling colour-correction fields use American `color*`. CLAUDE.md mandates British English; the wire surface is American almost everywhere.
7. **WsEffectsCommands legacy duplicates.** `setEffect`/`setBrightness`/`setSpeed`/`setPalette`/`nextEffect`/`prevEffect` and `parameters.get`/`parameters.set` are top-level legacy aliases that duplicate `effects.*` and `effects.parameters.*`. `WsZonesCommands` has the same pattern with `getZoneState`. Rename should explicitly decide whether to keep legacy aliases or break clients.
8. **PluginHandlers REST routes** (GET `/api/v1/plugins`, GET `/api/v1/plugins/manifests`, POST `/api/v1/plugins/reload`) are documented in the header but not present in the V1ApiRoutes enumeration I scanned. Either bound from elsewhere (plugin subsystem glue) or stale doc. Out of scope to chase further.
9. **NetworkHandlers DELETE `/api/v1/network/saved/:ssid`** documented in header but not present in V1ApiRoutes binding enumeration. Probably stale doc, not a rename target.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-13 | agent:Claude-Opus-4.7-1M (subagent SSA-N08) | Created. Inventoried 25 REST handler classes + 28 WS command modules under `firmware-v3/src/network/webserver/{handlers,ws}/`. Documented 155 REST routes from V1ApiRoutes.cpp, ~140 WebSocket command names, and the full JSON wire-field key set per file. Flagged the still-live `songAware.*` REST and WS surface (12 WS commands, 7 REST routes, ~69 `songAware*` references in two files) as the dominant SynqMatrix rename target, plus 9 secondary naming-inconsistency anomalies. |
