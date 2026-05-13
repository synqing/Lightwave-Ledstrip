---
abstract: "SSA-N02 audio subsystem name inventory — AudioActor (triple-implementation branches ESV11/PIPELINECORE/GOERTZEL), AudioCapture, ChromaAnalyzer, GoertzelAnalyzer, StyleDetector, TrinityControlBusProxy, AudioBehaviorSelector, AudioTuning, and the ESV11 backend (EsV11Backend, EsBeatClock, EsV11Adapter, vendor shims). Extracted via clangd document symbols + targeted Read passes 2026-05-13. Read this before any audio-actor/backend rename or refactor."
---

# SSA-N02 — Audio Actor + Backends Naming Inventory

**Branch:** `feature/synqmatrix-rename-2026-05-13`
**Extraction date:** 2026-05-13
**Method:** clangd `get_document_symbols` (every header + cpp) augmented with targeted Read passes for parameter names and the inactive preprocessor branches of `AudioActor.{h,cpp}`.
**Scope rule:** names and signatures only; bodies never reproduced. British spelling in prose; original code spellings preserved verbatim (US spellings such as `behavior`, `initialized`, `normalize`, `analyze`, `colour vs color`, `serialize` are *as they appear in source*, not normalised).

## Convention snapshot

Across the subsystem the prevailing conventions are:

- **Namespaces** — `lightwaveos::audio::*` for all subsystem code, with `lightwaveos::audio::esv11` for the ESV11 backend and `lightwaveos::audio::ActorConfigs` for actor sizing presets.
- **Classes / structs** — `PascalCase`. Examples: `AudioActor`, `AudioCapture`, `ChromaAnalyzer`, `GoertzelAnalyzer`, `StyleDetector`, `TrinityControlBusProxy`, `AudioBehaviorSelector`, `BehaviorEntry`, `EsV11Backend`, `EsV11Adapter`, `EsBeatClock`, `EsV11Outputs`, `ChunkTiming`, `TempoParams`.
- **Methods (public + private)** — `lowerCamelCase`. Examples: `getZoneAgcSnapshot`, `captureHopNonBlocking`, `analyze64`, `setExternalSyncMode`, `injectExternalBeat`, `bandRatioDetect`.
- **Member variables** — `m_lowerCamelCase`. Examples: `m_controlBus`, `m_hopCount`, `m_stmExtractor`, `m_esBackend`, `m_esAdapter`, `m_sbWaveformHistory`. Exceptions noted below.
- **Constants / `static constexpr`** — `UPPER_SNAKE_CASE` or `PascalCase` depending on file. `WINDOW_SIZE`, `NUM_CHROMA`, `NUM_OCTAVES`, `SAMPLE_RATE_HZ`, `MAX_BLOCK_SIZE`, `HANN_LUT_SIZE`, `NUM_BINS`, `NUM_BANDS`, `BAND_HISTORY_SIZE`, `BENCHMARK_RING_SIZE`, `HOP_BUDGET_US`, `MAX_BEHAVIORS`, `SB_WAVEFORM_POINTS`, `STALENESS_TIMEOUT_US`, `BENCHMARK_AGGREGATE_INTERVAL`, `GOERTZEL_LOG_INTERVAL`.
- **Enums** — enum type is `PascalCase`, enumerators are `UPPER_SNAKE_CASE`. `AudioActorState::{UNINITIALIZED, INITIALIZING, RUNNING, PAUSED, ERROR}`, `CaptureResult::{SUCCESS, NOT_INITIALIZED, DMA_TIMEOUT, READ_ERROR, BUFFER_OVERFLOW}`, `CalibrationState::{IDLE, REQUESTED, MEASURING, COMPLETE, FAILED}`, `AudioPreset::{LIGHTWAVE_V2, SENSORY_BRIDGE, AGGRESSIVE_AGC, CONSERVATIVE_AGC, LGP_SMOOTH, CUSTOM}`, `NarrativePhase::{REST, BUILD, HOLD, RELEASE}`.
- **Vendor (ESV11) shim layer** — `snake_case` for free functions, globals, and ChunkTiming fields (vendored from SensoryBridge / ES v1.1). Examples: `esv11_init_buffers`, `esv11_set_time`, `sample_history`, `window_lookup`, `novelty_curve`, `vu_curve`, `frequencies_musical`, `spectrogram_average`, `noise_history`, `tempi`, `t_now_us`, `t_now_ms`, `capture_us`, `gpu_tick_us`. Also struct fields in `EsV11Outputs` and `EsV11Backend::ChunkTiming` use `snake_case` to preserve vendor parity.
- **Parameter names** — broadly `lowerCamelCase` but with several vendor-style `snake_case` cases at the ES boundary (`now_us`, `phase01`, `sample_rate_hz`, `render_now`).
- **British vs American spelling** — code uses American (`Initialized`, `behavior`, `analyze`, `normalize`, `colour` is used in prose only as `m_sbHue*` / `Auto colour shift` comment). British is reserved for prose comments (`initialise`, `colour`) per repo policy but does *not* appear in identifiers.

## Per-file inventory

---

### `firmware-v3/src/audio/AudioActor.h`

Namespace: `lightwaveos::audio` (with `ActorConfigs` sub-namespace at file tail).

#### Enum `AudioActorState`
Enumerators: `UNINITIALIZED`, `INITIALIZING`, `RUNNING`, `PAUSED`, `ERROR`.

#### Struct `AudioActorStats`
- Fields: `tickCount`, `captureSuccessCount`, `captureFailCount`, `lastTickTimeUs`, `state`.
- Methods: `reset()`.

#### Struct `AudioPipelineDiagnostics`
- Fields: `diagStartTimeUs`, `captureAttempts`, `captureSuccesses`, `captureDmaTimeouts`, `captureReadErrors`, `publishCount`, `publishSeqGaps`, `lastPublishSeq`, `lastRawMin`, `lastRawMax`, `lastRawRms`, `samplesNonZero`, `zeroHopCount`, `lastCaptureStartUs`, `lastCaptureEndUs`, `lastProcessEndUs`, `lastPublishTimeUs`, `maxCaptureLatencyUs`, `maxProcessLatencyUs`, `avgCaptureLatencyUs`, `avgProcessLatencyUs`.
- Methods: `reset()`.

#### Struct `AudioDspState`
- Fields: `rmsRaw`, `rmsMapped`, `rmsPreGain`, `fluxMapped`, `agcGain`, `dcEstimate`, `noiseFloor`, `minSample`, `maxSample`, `peakCentered`, `meanSample`, `clipCount`.

#### Class `AudioActor`

**Cross-branch public API (visible always):**
- Ctors: `AudioActor()`, `~AudioActor()`.
- Lifecycle: `pause()`, `resume()`, `resetStats()`, `printStatus()`, `printSpectrum()`, `printBeat()`, `printDiagnostics()`.
- Accessors: `getState()`, `getStats()`, `getDiagnostics()`, `isCapturing()`, `getLastHop()`, `hasNewHop()`, `getControlBusBuffer()`, `hasControlBusBuffer()`, `getSampleIndex()`, `getHopCount()`.
- Buffer diagnostics: `logControlBusBufferPlacement() const`.
- Protected overrides: `onStart()`, `onMessage(const actors::Message& msg)`, `onTick()`, `onStop()`.

**ESV11-only public API (`#if FEATURE_AUDIO_BACKEND_ESV11`):**
- Backend accessor: `esBackend() const`.

**Non-ESV11 (PIPELINECORE + GOERTZEL) public API (`#if !FEATURE_AUDIO_BACKEND_ESV11`):**
- Tuning: `getPipelineTuning() const`, `setPipelineTuning(const AudioPipelineTuning& tuning)`, `resetDspState()`, `getDspState() const`.
- Zone AGC: nested struct `ZoneAgcSnapshot { bool enabled; bool lookaheadEnabled; float followers[CONTROLBUS_NUM_ZONES]; float maxMags[CONTROLBUS_NUM_ZONES]; }`; `getZoneAgcSnapshot() const`, `setZoneAgcEnabled(bool enabled)`, `setLookaheadEnabled(bool enabled)`, `setZoneAgcRates(float attackRate, float releaseRate)`, `setZoneMinFloor(float minFloor)`.
- Spike detection: `getSpikeDetectionStats() const`, `resetSpikeDetectionStats()`.
- ControlBus: `getControlBusFrameSnapshot() const`.
- Calibration: `startNoiseCalibration(uint32_t durationMs = 3000, float safetyMultiplier = 1.2f)`, `cancelNoiseCalibration()`, `getCalibrationState() const`, `getCalibrationResult() const`, `getNoiseCalibrationState() const`, `applyCalibrationResults()`.
- Tempo: `isTempoEnabled() const`. In `FEATURE_AUDIO_BACKEND_PIPELINECORE` branch this is the only tempo accessor; in the GOERTZEL `#else` branch, also `getTempo() const`, `getTempoMut()`.
- Benchmark (`#if FEATURE_AUDIO_BENCHMARK`): `getBenchmarkStats() const`, `getBenchmarkRing() const`, `resetBenchmarkStats()`.

**Members — ESV11 branch (`#if FEATURE_AUDIO_BACKEND_ESV11`):**
- Shared: `m_stmExtractor` (`STMExtractor`), `m_translationState`, `m_translationLastAudioTime`, `m_translationHaveLastAudioTime`, `m_smoothedTempoConfidence`, `m_translationLastDebugLogUs` (translation engine — guarded by `FEATURE_TRANSLATION_ENGINE`). Translation-debug-only: `m_translationLastLatchValid`, `m_translationLastLatchBpm`, `m_translationLastLatchConfidence`, `m_translationLastLatchUs`, `m_translationLastLatchTempoLocked`, `m_translationLastLatchBeatTick`, `m_translationLastLatchDownbeatTick`.
- Standard: `m_standardBeatInBar`.
- Core state: `m_state`, `m_stats`, `m_diag`, `m_controlBusBuffer` (`InternalSnapshotBufferOwner<ControlBusFrame>`), `m_sampleIndex`, `m_hopCount`.
- Backend: `m_esBackend` (`esv11::EsV11Backend`), `m_esAdapter` (`esv11::EsV11Adapter`), `m_stmFftBuffer`, `m_stmBins256`, `m_esHopSeq`, `m_esChunkCounter`.
- Surface 3 trace: `m_hopStartUs`, `m_hopAccumWorkUs`, `m_lastHopEndUs`, `m_audioChunkDeadlineMissTotal`, `m_audioHopDeadlineMissTotal`.
- Onset telemetry: `m_onsetDetector` (`OnsetDetector`), `m_lastOnsetInputRms`, `m_lastOnsetNoiseFloor`, `m_lastOnsetActivity`, `m_lastOnsetGateFlags`.
- Band-ratio detector — nested struct `BandRatioConfig { varSlope, varIntercept, minMul, maxMul, refractory, brRmsGate, brGateHold, kickAbsFloor, snareAbsFloor, hihatAbsFloor }`.
- Constant: `BAND_HISTORY_SIZE`.
- Nested struct `BandRatioChannel { history, writeIdx, count, runSum, runSumSq, lastTrigger }`.
- Fields: `m_bandRatioCfg`, `m_brGateHoldCounter`, `m_kickChannel`, `m_snareChannel`, `m_hihatChannel`, `m_bandRatioFrame`.
- Method: `bandRatioDetect(BandRatioChannel& ch, float energy, uint8_t refractory, float absFloor)`.
- Audio confidence — nested struct `AudioConfidenceConfig { rmsFloor, noveltyFloor, holdFrames, releaseAlpha, attackAlpha }`. Fields: `m_confidenceCfg`, `m_prevBands`, `m_audioConfidence`, `m_confidenceHoldCounter`.
- ControlBus: `m_controlBus` (`ControlBus`).

**Members — PIPELINECORE branch (`#elif FEATURE_AUDIO_BACKEND_PIPELINECORE`):**
- `m_capture` (`AudioCapture`), `m_state`, `m_stats`, `m_diag`, `m_hopBuffer[HOP_SIZE]`, `m_newHopAvailable` (`std::atomic<bool>`), `m_pipeline` (`PipelineCore`), `m_adapter` (`PipelineAdapter`), `m_lastFrame` (`FeatureFrame`).
- Style: `m_styleDetector` (`StyleDetector`) guarded by `FEATURE_STYLE_DETECTION`.
- `m_prevChordRoot`, `m_controlBus` (`ControlBus`), `m_controlBusApiMux` (`portMUX_TYPE`), `m_controlBusBuffer`, `m_sampleIndex`, `m_hopCount`, `m_consecutiveZeroHops`, `m_lastRecoveryAttemptHop`, `m_consecutiveDmaTimeouts`, `m_dmaFailureSignalled`, `m_pipelineTuning`, `m_pipelineTuningSeq`, `m_dspState`, `m_dspStateSeq`, `m_dspResetPending`, `m_noiseCalibration` (`NoiseCalibrationState`).
- Benchmark: `m_benchmarkRing`, `m_benchmarkStats`, `m_benchmarkAggregateCounter`, constant `BENCHMARK_AGGREGATE_INTERVAL`, method `aggregateBenchmarkStats()`.
- Internal: `captureHop()`, `recoverCapturePath()`, `processHop()`, `computeRMS(const int16_t* samples, size_t count)`, `handleCaptureError(CaptureResult result)`, `processNoiseCalibration(float rms, const float* bands, const float* chroma, uint32_t nowMs)`, `processSbWaveformSidecar(const ControlBusRawInput& raw)`, `processSbBloomSidecar(const ControlBusRawInput& raw)`, `updateSbNoveltyAndHueShift()`.
- SB-parity constants: `SB_WAVEFORM_POINTS`, `SB_WAVEFORM_HISTORY`, `SB_NUM_FREQS`, `SB_SPECTRAL_HISTORY`.
- SB-parity fields: `m_sbWaveformHistory`, `m_sbWaveformHistoryIndex`, `m_sbMaxWaveformValFollower`, `m_sbWaveformPeakScaled`, `m_sbWaveformPeakScaledLast`, `m_sbNoteChroma`, `m_sbChromaMaxVal`, `m_sbSpectrogram`, `m_sbSpectrogramSmooth`, `m_sbChromagramSmooth`, `m_sbChromagramMaxPeak`, `m_sbWaveform`, `m_sbSpectralHistory`, `m_sbNoveltyCurve`, `m_sbSpectralHistoryIndex`, `m_sbHuePosition`, `m_sbHueShiftSpeed`, `m_sbHuePushDirection`, `m_sbHueDestination`, `m_sbHueShiftingMix`, `m_sbHueShiftingMixTarget`, `m_sbRand`.

**Members — GOERTZEL branch (`#else`, the legacy default):**
- `m_capture`, `m_state`, `m_stats`, `m_diag`, `m_hopBuffer[HOP_SIZE]`, `m_hopBufferCentered[HOP_SIZE]`, `m_prevHopCentered[HOP_SIZE]`, `m_prevHopValid`, `m_newHopAvailable`.
- Analysers: `m_analyzer` (`GoertzelAnalyzer`), `m_chromaAnalyzer` (`ChromaAnalyzer`), `m_styleDetector`.
- `m_prevChordRoot`, `m_controlBus`, `m_controlBusApiMux`, `m_controlBusBuffer`, `m_sampleIndex`, `m_hopCount`.
- DSP history: `m_prevRMS`, `m_prevBands[8]`, `m_lastBands[8]`, `m_lastBands64[8]`, `m_analyze64Ready`, `m_lastRmsRaw`, `m_lastRmsMapped`, `m_lastFluxMapped`, `m_lastMinSample`, `m_lastMaxSample`, `m_lastPeakCentered`, `m_lastMeanSample`, `m_lastRmsPreGain`, `m_lastAgcGain`, `m_lastDcEstimate`, `m_lastClipCount`, `m_lastOnsetInputRms`, `m_lastOnsetNoiseFloor`, `m_lastOnsetActivity`, `m_lastOnsetGateFlags`.
- Live DSP: `m_dcEstimate`, `m_agcGain`, `m_noiseFloor`, `m_pipelineTuning`, `m_pipelineTuningSeq`, `m_dspState`, `m_dspStateSeq`, `m_dspResetPending`.
- Throttles: `m_goertzelLogCounter`, constant `GOERTZEL_LOG_INTERVAL`, `m_goertzel64LogCounter`, constant `GOERTZEL64_LOG_INTERVAL`.
- Tempo: `m_tempo` (`TempoTracker`), `m_lastTempoOutput` (`TempoTrackerOutput`), `m_bins64Cached[64]`, `m_bins64AdaptiveMax`.
- Stack-reduction buffers: `m_bins64Raw[GoertzelAnalyzer::NUM_BINS]`, `m_bands64Folded[8]`.
- Calibration: `m_noiseCalibration`.
- Benchmark: same naming as PIPELINECORE branch (`m_benchmarkRing`, `m_benchmarkStats`, `m_benchmarkAggregateCounter`, constant `BENCHMARK_AGGREGATE_INTERVAL`).
- Internal: `captureHop()`, `processHop()`, `computeRMS(const int16_t* samples, size_t count)`, `handleCaptureError(CaptureResult result)`, `processNoiseCalibration(float rms, const float* bands, const float* chroma, uint32_t nowMs)`, `processSbWaveformSidecar(const ControlBusRawInput& raw)`, `processSbBloomSidecar(const ControlBusRawInput& raw)`, `updateSbNoveltyAndHueShift()`, `aggregateBenchmarkStats()`.
- SB-parity buffers — identical names to PIPELINECORE branch (`SB_*` constants and `m_sb*` fields).

#### Namespace `lightwaveos::ActorConfigs`
- Function: `Audio()` (constexpr ActorConfig builder).

---

### `firmware-v3/src/audio/AudioActor.cpp`

The file contains three full implementations gated by preprocessor branches plus a cross-branch helper block.

**Anonymous namespace at file head (lines 58–297, always compiled):**
- Constants: `PERCEPTUAL_BAND_WEIGHTS`, `PERCEPTUAL_BAND_WEIGHT_SUM`.
- Free functions: `shouldRunSbSidecar()`, `clamp01(float)`, `clampf(float, float, float)`, `computeTranslationDeltaSeconds(...)`, `buildTranslationFeatures(...)`.

**Branch helper block (`#if !FEATURE_AUDIO_BACKEND_ESV11`, lines 374–465) — shared by PIPELINECORE + GOERTZEL:**
- `AudioActor::getZoneAgcSnapshot() const`
- `AudioActor::setZoneAgcEnabled(bool enabled)`
- `AudioActor::setLookaheadEnabled(bool enabled)`
- `AudioActor::setZoneAgcRates(float attackRate, float releaseRate)`
- `AudioActor::setZoneMinFloor(float minFloor)`
- `AudioActor::getSpikeDetectionStats() const`
- `AudioActor::resetSpikeDetectionStats()`
- `AudioActor::getControlBusFrameSnapshot() const`

**Inner anonymous namespace inside `lightwaveos::audio` (lines 310–340) — file-scope helpers:**
- `applyControlBusBenchToggles()`, `memoryRegionName(...)`, `memoryRegionCode(...)`.

**Method: `AudioActor::logControlBusBufferPlacement() const`** — lines 342–371.

#### Branch A — `#if FEATURE_AUDIO_BACKEND_ESV11` (lines 467–1245)
- `AudioActor::AudioActor()`
- `AudioActor::~AudioActor()` (defaulted)
- `AudioActor::pause()`
- `AudioActor::resume()`
- `AudioActor::resetStats()`
- `AudioActor::printDiagnostics()`
- `AudioActor::printStatus()`
- `AudioActor::printSpectrum()`
- `AudioActor::printBeat()`
- `AudioActor::getLastHop()`
- `AudioActor::hasNewHop()`
- `AudioActor::onStart()`
- `AudioActor::onMessage(const actors::Message& msg)`
- `AudioActor::onTick()` — large (lines 645–1238)
- `AudioActor::onStop()`

#### Branch B — `#elif FEATURE_AUDIO_BACKEND_PIPELINECORE` (lines 1246–2459)
- `AudioActor::AudioActor()`
- `AudioActor::~AudioActor()`
- `AudioActor::pause()`
- `AudioActor::resume()`
- `AudioActor::resetStats()`
- `AudioActor::printStatus()`
- `AudioActor::printSpectrum()`
- `AudioActor::printBeat()`
- `AudioActor::printDiagnostics()`
- `AudioActor::getPipelineTuning() const`
- `AudioActor::setPipelineTuning(const AudioPipelineTuning& tuning)`
- `AudioActor::resetDspState()`
- `AudioActor::getDspState() const`
- `AudioActor::hasNewHop()`
- `AudioActor::onStart()`
- `AudioActor::onMessage(const actors::Message& msg)`
- `AudioActor::onTick()`
- `AudioActor::onStop()`
- `AudioActor::captureHop()`
- `AudioActor::recoverCapturePath()`
- `AudioActor::processHop()`
- `AudioActor::processSbWaveformSidecar(const ControlBusRawInput& raw)`
- `AudioActor::processSbBloomSidecar(const ControlBusRawInput& raw)`
- `AudioActor::updateSbNoveltyAndHueShift()`
- `AudioActor::computeRMS(const int16_t* samples, size_t count)`
- `AudioActor::handleCaptureError(CaptureResult result)`
- `AudioActor::aggregateBenchmarkStats()`
- `AudioActor::startNoiseCalibration(uint32_t durationMs, float safetyMultiplier)`
- `AudioActor::cancelNoiseCalibration()`
- `AudioActor::applyCalibrationResults()`
- `AudioActor::processNoiseCalibration(float rms, const float* bands, const float* chroma, uint32_t nowMs)`

#### Branch C — `#else` GOERTZEL legacy (lines 2460–4277)
- `AudioActor::AudioActor()`
- `AudioActor::~AudioActor()`
- `AudioActor::pause()`
- `AudioActor::resume()`
- `AudioActor::resetStats()`
- `AudioActor::printStatus()`
- `AudioActor::printSpectrum()`
- `AudioActor::printBeat()`
- `AudioActor::printDiagnostics()`
- `AudioActor::getPipelineTuning() const`
- `AudioActor::setPipelineTuning(const AudioPipelineTuning& tuning)`
- `AudioActor::resetDspState()`
- `AudioActor::getDspState() const`
- `AudioActor::hasNewHop()`
- `AudioActor::onStart()`
- `AudioActor::onMessage(const actors::Message& msg)`
- `AudioActor::onTick()`
- `AudioActor::onStop()`
- `AudioActor::captureHop()`
- `AudioActor::processHop()`
- `AudioActor::processSbWaveformSidecar(const ControlBusRawInput& raw)`
- `AudioActor::processSbBloomSidecar(const ControlBusRawInput& raw)`
- `AudioActor::updateSbNoveltyAndHueShift()`
- `AudioActor::computeRMS(const int16_t* samples, size_t count)`
- `AudioActor::handleCaptureError(CaptureResult result)`
- `AudioActor::aggregateBenchmarkStats()`
- `AudioActor::startNoiseCalibration(uint32_t durationMs, float safetyMultiplier)`
- `AudioActor::cancelNoiseCalibration()`
- `AudioActor::applyCalibrationResults()`
- `AudioActor::processNoiseCalibration(float rms, const float* bands, const float* chroma, uint32_t nowMs)`

(Branches B + C duplicate method names by design — they implement the same `AudioActor` interface against three different DSP back-ends.)

---

### `firmware-v3/src/audio/AudioCapture.h` and `AudioCapture.cpp`

Namespace: `lightwaveos::audio`.

#### Enum `CaptureResult`
`SUCCESS`, `NOT_INITIALIZED`, `DMA_TIMEOUT`, `READ_ERROR`, `BUFFER_OVERFLOW`.

#### Struct `CaptureStats`
- Fields: `hopsCapured` *(sic — typo preserved in source: missing `t`)*, `dmaTimeouts`, `readErrors`, `maxReadTimeUs`, `avgReadTimeUs`, `peakSample`.
- Methods: `reset()`.

#### Class `AudioCapture`
- Ctors: `AudioCapture()`, `~AudioCapture()`, deleted copy/assign (`AudioCapture(const AudioCapture&)`, `operator=`).
- Public: `init()`, `deinit()`, `isInitialized() const`, `captureHop(int16_t* buffer)`, `captureHopNonBlocking(int16_t* buffer)`, `captureHopWithTimeout(int16_t* buffer, uint32_t timeoutMs)`, `getStats() const`, `resetStats()`.
- ESP32-P4-only public (guarded `CHIP_ESP32_P4`): `getMicGainDb() const`, `setMicGainDb(int8_t gainDb)`.
- Private members: `m_initialized`, `m_stats`, `m_dcPrevInput`, `m_dcPrevOutput`.
  - ESP32-P4 branch: `m_rxChannel` (`i2s_chan_handle_t`), `m_dmaBuffer[HOP_SIZE]` (`int16_t`), `m_es8311Handle`, `m_micGainDb`.
  - ESP32-S3 branch: constant `I2S_PORT = I2S_NUM_0`, `m_dmaBuffer[HOP_SIZE * 2]` (`int32_t`).
- Private method: `configureI2S()`.

#### Free symbols in `AudioCapture.cpp`
- Anonymous-namespace constants: `RECIP_SCALE`, `DC_BLOCK_ALPHA`.

---

### `firmware-v3/src/audio/ChromaAnalyzer.{h,cpp}`

Namespace: `lightwaveos::audio`.

#### Class `ChromaAnalyzer`
- Constants: `WINDOW_SIZE`, `NUM_CHROMA`, `NUM_OCTAVES`, `SAMPLE_RATE_HZ`, `NOTE_FREQS[NUM_OCTAVES * NUM_CHROMA]`.
- Ctor: `ChromaAnalyzer()`.
- Public: `accumulate(const int16_t* samples, size_t count)`, `analyze(float* chromaOut)`, `analyzeWindow(const int16_t* window, size_t N, float* chromaOut)`, `reset()`.
- Private fields: `m_accumBuffer`, `m_accumIndex`, `m_windowFull`, `m_coefficients`, `m_normFactors`.
- Private methods: `computeGoertzel(const int16_t* buffer, size_t N, float coeff) const`, `static computeCoefficient(float targetFreq, uint32_t sampleRate, size_t windowSize)`.

---

### `firmware-v3/src/audio/GoertzelAnalyzer.{h,cpp}`

Namespace: `lightwaveos::audio`.

#### Class `GoertzelAnalyzer`
- Constants: `WINDOW_SIZE`, `NUM_BANDS`, `NUM_BINS`, `SAMPLE_RATE_HZ`, `MAX_BLOCK_SIZE`, `MIN_BLOCK_SIZE`, `HANN_LUT_SIZE`, `TARGET_FREQS[NUM_BANDS]`.
- Nested struct `GoertzelBin { target_freq, block_size, block_size_recip, coeff_q14, window_mult, zone }` — **note vendor-style snake_case for fields**.
- Ctor: `GoertzelAnalyzer()`.
- Public: `accumulate(const int16_t* samples, size_t count)`, `analyze64(float* binsOut)`, `analyze(float* bandsOut)`, `analyzeWindow(const int16_t* window, size_t N, float* bandsOut)`, `reset()`, `getBin(size_t index) const`, `getMagnitudes64() const`, `getBinFrequency(size_t index) const`, `hasEnoughSamples() const`, `setInterlacedProcessing(bool enabled)`, `getInterlacedProcessing() const`, `getLastProcessedParity() const`.
- Private fields: `m_hannLUT`, `m_bins[NUM_BINS]`, `m_magnitudes64[NUM_BINS]`, `m_sampleHistory[SAMPLE_HISTORY_LENGTH]`, `m_historyWriteIndex`, `m_sampleCount`, `m_accumBuffer`, `m_accumIndex`, `m_windowFull`, `m_interlacedEnabled`, `m_processOddBins`, `m_coefficients`, `m_normFactors`.
- Private methods: `initHannLUT()`, `initBins()`, `computeGoertzelBin(size_t binIndex)`, `computeGoertzel(const int16_t* buffer, size_t N, float coeff) const`, `static computeCoefficient(float targetFreq, uint32_t sampleRate, size_t windowSize)`, `getHistorySample(size_t indexFromOldest) const`.

---

### `firmware-v3/src/audio/AudioBehaviorSelector.{h,cpp}` + `_esv11_stub.cpp`

Namespace: `lightwaveos::audio`. Plugin types referenced via `lightwaveos::plugins::{EffectContext, VisualBehavior}`.

#### Enum `NarrativePhase`
`REST`, `BUILD`, `HOLD`, `RELEASE`.

#### Free function
- `getNarrativePhaseName(NarrativePhase phase)`.

#### Struct `BehaviorEntry`
- Fields: `behavior` (`plugins::VisualBehavior`), `priority`, `enabled`.
- Ctor: `BehaviorEntry()`.

#### Class `AudioBehaviorSelector`
- Constant: `MAX_BEHAVIORS`.
- Ctor: `AudioBehaviorSelector()`.
- Public configuration: `registerBehavior(plugins::VisualBehavior behavior, float priority = 1.0f)`, `unregisterBehavior(plugins::VisualBehavior behavior)`, `setFallbackBehavior(plugins::VisualBehavior behavior)`, `setTransitionTime(uint16_t ms)`, `setEnergyThresholds(float rest, float build, float hold)`, `reset()`.
- Per-frame: `update(const plugins::EffectContext& ctx)`.
- Query state: `narrativePhase() const`, `currentBehavior() const`, `targetBehavior() const`, `previousBehavior() const`, `isTransitioning() const`, `transitionProgress() const`, `phaseIntensity() const`, `energy() const`, `flux() const`.
- Convenience accessors: `isOnBeat() const`, `isOnDownbeat() const`, `beatPhase() const`, `audioAvailable() const`.
- Private fields: `m_behaviors`, `m_fallbackBehavior`, `m_phase`, `m_previousPhase`, `m_currentBehavior`, `m_targetBehavior`, `m_previousBehavior`, `m_transitionProgress`, `m_transitionTimeMs`, `m_transitionStartMs`, `m_phaseIntensity`, `m_phaseStartEnergy`, `m_phaseStartMs`, `m_energySmoothed`, `m_fluxSmoothed`, `m_previousEnergy`, `m_peakEnergy`, `m_wasOnBeat`, `m_wasOnDownbeat`, `m_beatPhase`, `m_audioAvailable`, `m_lastBeatTick`, `m_restThreshold`, `m_buildThreshold`, `m_holdThreshold`, `m_lastUpdateMs`.
- Private methods: `hasBehavior(plugins::VisualBehavior behavior) const`, `getBehaviorEntry(plugins::VisualBehavior behavior)`, `analyzeNarrativePhase(const plugins::EffectContext& ctx)`, `selectBehaviorForPhase(NarrativePhase phase)`, `findBestMatch(NarrativePhase phase)`, `beginTransition(plugins::VisualBehavior next)`, `updatePhaseIntensity(NarrativePhase phase)`.

`_esv11_stub.cpp` exposes the same method names — `registerBehavior`, `unregisterBehavior`, `setFallbackBehavior`, `setTransitionTime`, `setEnergyThresholds`, `reset`, `update` — as no-op stubs used on the ESV11 build.

---

### `firmware-v3/src/audio/AudioTuning.h`

Namespace: `lightwaveos::audio`.

#### Enum `AudioPreset`
`LIGHTWAVE_V2`, `SENSORY_BRIDGE`, `AGGRESSIVE_AGC`, `CONSERVATIVE_AGC`, `LGP_SMOOTH`, `CUSTOM`.

#### Enum `CalibrationState`
`IDLE`, `REQUESTED`, `MEASURING`, `COMPLETE`, `FAILED`.

#### Struct `PerBandNoiseFloor`
- Fields: `bands` (per-band float array), `multiplier`.

#### Struct `NoiseCalibrationResult`
- Fields: `bandFloors`, `chromaFloors`, `overallRms`, `peakRms`, `sampleCount`, `valid`.

#### Struct `NoiseCalibrationState`
- Fields: `state` (`CalibrationState`), `startTimeMs`, `durationMs`, `safetyMultiplier`, `maxAllowedRms`, `bandSum`, `chromaSum`, `rmsSum`, `peakRms`, `sampleCount`, `result`.
- Method: `reset()`.

#### Struct `AudioPipelineTuning`
- DC: `dcAlpha`.
- AGC: `agcTargetRms`, `agcMinGain`, `agcMaxGain`, `agcAttack`, `agcRelease`, `agcClipReduce`, `agcIdleReturnRate`.
- Noise floor: `noiseFloorMin`, `noiseFloorRise`, `noiseFloorFall`.
- Gate: `gateStartFactor`, `gateRangeFactor`, `gateRangeMin`.
- dB mapping: `rmsDbFloor`, `rmsDbCeil`, `bandDbFloor`, `bandDbCeil`, `chromaDbFloor`, `chromaDbCeil`.
- Flux: `fluxScale`.
- Smoothing: `controlBusAlphaFast`, `controlBusAlphaSlow`, `bandAttack`, `bandRelease`, `heavyBandAttack`, `heavyBandRelease`.
- Per-band: `perBandGains` (array), `perBandNoiseFloors` (array of `PerBandNoiseFloor`), `usePerBandNoiseFloor`.
- Silence: `silenceHysteresisMs`, `silenceThreshold`.
- Novelty: `noveltyUseSpectralFlux`, `noveltySpectralFluxScale`.
- Bins64 adaptive: `bins64AdaptiveScale`, `bins64AdaptiveFloor`, `bins64AdaptiveRise`, `bins64AdaptiveFall`, `bins64AdaptiveDecay`.

#### Struct `AudioContractTuning`
- Fields: `audioStalenessMs`, `bpmMin`, `bpmMax`, `bpmTau`, `confidenceTau`, `phaseCorrectionGain`, `barCorrectionGain`, `beatsPerBar`, `beatUnit`.

#### Struct `GoertzelNoveltyTuning`
- Fields: `useSpectralFlux`, `spectralFluxScale`.

#### Free functions
- `clampf(float v, float lo, float hi)`
- `clampAudioPipelineTuning(AudioPipelineTuning& t)`
- `clampAudioContractTuning(AudioContractTuning& t)`
- `clampGoertzelNoveltyTuning(GoertzelNoveltyTuning& t)`
- `getPreset(AudioPreset preset)`
- `getPresetName(AudioPreset preset)`

---

### `firmware-v3/src/audio/StyleDetector.{h,cpp}`

Namespace: `lightwaveos::audio`.

#### Struct `StyleClassification`
- Fields: `detected` (`MusicStyle`), `confidence`, `styleWeights`, `framesAnalyzed`.
- Method: `getWeight(MusicStyle style) const`.

#### Struct `StyleDetectorTuning`
- Fields: `analysisWindowHops`, `minHopsForClassification`, `beatConfidenceThreshold`, `bassRatioThreshold`, `trebleRatioThreshold`, `dynamicRangeThreshold`, `fluxVarianceThreshold`, `chordChangeRateThreshold`, `styleAlpha`, `hysteresisThreshold`.

#### Struct `StyleFeatures`
- Fields: `beatConfidenceAvg`, `beatConfidenceVar`, `bassRatio`, `midRatio`, `trebleRatio`, `rmsMin`, `rmsMax`, `dynamicRange`, `chordChangeRate`, `chordChanges`, `fluxMean`, `fluxVariance`.
- Method: `reset()`.

#### Class `StyleDetector`
- Ctor: `StyleDetector()`.
- Public: `update(float rms, float flux, const float* bands, float beatConfidence, bool chordChanged)`, `getClassification() const`, `getStyle() const`, `getConfidence() const`, `getFeatures() const`, `reset()`, `setTuning(const StyleDetectorTuning& tuning)`.
- Private fields: `m_classification`, `m_features`, `m_tuning`, `m_hopCount`, `m_windowStartTimeS`, `m_prevChordRoot`, `m_prevFlux`, `m_rmsSum`, `m_fluxSum`, `m_fluxSqSum`, `m_beatConfSum`, `m_beatConfSqSum`, `m_bandSums[8]`.
- Private methods: `computeStyleWeights()`, `selectDominantStyle()`.

#### Free function in `StyleDetector.cpp`
- Anonymous-namespace: `clamp01(float)`.

---

### `firmware-v3/src/audio/TrinityControlBusProxy.{h,cpp}`

Namespace: `lightwaveos::audio`.

#### Class `TrinityControlBusProxy`
- Ctor: `TrinityControlBusProxy()`.
- Public: `setMacros(float energy, float vocal, float bass, float perc, float bright)`, `getFrame() const`, `isActive() const`, `markActive()`, `reset()`.
- Private fields: `m_frame` (`ControlBusFrame`), `m_lastUpdate`.
- Constant: `STALENESS_TIMEOUT_US` (250 ms in micros).
- Anonymous-namespace constant in `.cpp`: `kDefaultControlBusFrame`.

---

### `firmware-v3/src/audio/AudioMath.h`

Namespace: `lightwaveos::audio`. Header-only inline helpers.

- `computeEmaAlpha(float dtSeconds, float tauSeconds)`
- `tauFromAlpha(float alpha, float dtSeconds)`
- `alphaFromHalfLife(float dtSeconds, float halfLifeSeconds)`
- `retunedAlpha(float oldAlpha, float oldRateHz, float newRateHz)`

(Parameter names from header comments. No member state.)

---

### `firmware-v3/src/audio/AudioNode.h`

Namespace: `lightwaveos::audio`.

- Stub class `AudioNode`. No public methods defined in current state of header — placeholder.

---

### `firmware-v3/src/audio/AudioMappingRegistry.h`

No symbols extracted (file is presently a pure forward-declaration / stub or contains preprocessor-only content).

---

### `firmware-v3/src/audio/AudioDebugConfig.{h,cpp}`

Namespace: `lightwaveos::audio`.

- Struct `AudioDebugConfig`: fields `verbosity`, `baseInterval`; methods `interval8Band(uint32_t v) const`, `interval64Bin(uint32_t v) const`, `intervalDMA(uint32_t v) const`.
- Free function: `getAudioDebugConfig()`.
- File-scope variable in `.cpp`: `s_audioDebugConfig`.

---

### `firmware-v3/src/audio/AudioBenchmarkMacros.h`

Namespace: `lightwaveos::audio`. Macro-driven header.

- Struct `AudioBenchmarkSample` (minimal variant): fields `timestamp_us`, `totalProcessUs`.
- (Macros not enumerated — they are preprocessor identifiers, not C++ names.)

---

### `firmware-v3/src/audio/AudioBenchmarkMetrics.h`

Namespace: `lightwaveos::audio`.

- Struct `AudioBenchmarkSample` (full variant): `timestamp_us`, `dcAgcLoopUs`, `rmsComputeUs`, `goertzelUs`, `chromaUs`, `controlBusUs`, `publishUs`, `totalProcessUs`, `captureReadUs`, `goertzelTriggered`, `chromaTriggered`, `_padding`. Method `clear()`.
- Constants: `BENCHMARK_RING_SIZE`, `BENCHMARK_RING_MASK`, `HOP_BUDGET_US`, `HISTOGRAM_BIN_EDGES`.
- Struct `AudioBenchmarkStats`: `hopCount`, `goertzelCount`, `avgTotalUs`, `avgDcAgcUs`, `avgGoertzelUs`, `avgChromaUs`, `peakTotalUs`, `peakGoertzelUs`, `cpuLoadPercent`, `histogramBins`. Methods `reset()`, `resetPeaks()`, `updateFromSample(const AudioBenchmarkSample& sample)`.

---

### `firmware-v3/src/audio/AudioBenchmarkRing.h`

Namespace: `lightwaveos::audio`.

- Class `AudioBenchmarkRing`: ctor; `push(const AudioBenchmarkSample& sample)`, `pop(AudioBenchmarkSample& outSample)`, `available() const`, `hasData() const`, `peekLast(AudioBenchmarkSample& outSample) const`, `peekLatest(AudioBenchmarkSample& outSample) const`, `reset()`, `totalPushed() const`. Fields: `m_samples`, `m_writeIndex`, `m_readIndex`.

---

### `firmware-v3/src/audio/AudioBenchmarkTrace.h`

No emitted symbols (header is macro-only or `#include` plumbing).

---

### `firmware-v3/src/audio/backends/esv11/EsV11Backend.{h,cpp}`

Namespace: `lightwaveos::audio::esv11`.

#### Struct `EsV11Outputs`
Vendor-style snake_case fields:
- Timing: `now_us`, `now_ms`, `sample_index`.
- Spectral: `spectrogram_smooth`, `chromagram`, `vu_level`, `novelty_norm_last`.
- Tempo: `top_bpm`, `tempo_confidence`, `phase_radians`, `beat_tick`, `beat_strength`.
- Waveform: `waveform`.

#### Class `EsV11Backend`
- Ctor: `EsV11Backend() = default`.
- Public: `init()`, `readAndProcessChunk(uint64_t now_us)`, `lastChunkTiming() const`, `getLatestOutputs(EsV11Outputs& out) const`, `tempoParams()` (non-const + const), `getSampleHistory() const`, `getSampleHistoryLength() const`.
- Nested struct `ChunkTiming` (snake_case fields): `capture_us`, `magnitudes_us`, `chroma_us`, `vu_us`, `tempo_us`, `gpu_tick_us`, `refresh_us`, `dsp_us`, `total_us`.
- Nested struct `TempoParams`: `gateBase`, `gateScale`, `gateTau`, `confFloor`, `validationThr`, `stabilityTau`, `holdUs`, `octaveRuns`, `decayFloor`, `octRatioLo`, `octRatioHi`, `wsSepFloor`, `confDecay`, `genericPersistUs`.
- Field: `m_tp` (the `TempoParams` instance — exposed publicly via accessors).
- Private fields: `m_sampleIndex`, `m_lastGpuTickUs`, `m_stableTopBin`, `m_stableBinLockedUs`, `m_octaveCandBin`, `m_octaveCandRuns`, `m_genericCandBin`, `m_genericCandFirstUs`, `m_stableBinValidated`, `m_beatPhase`, `m_lastRefreshUs`, `m_beatInBar`, `m_lastChunkTiming`, `m_latest`.
- Private methods: `tickEsGpu(float delta)`, `refreshOutputs(uint64_t now_us)`.
- Free helper in `.cpp` (esv11 namespace): `clamp01(float)`.

---

### `firmware-v3/src/audio/backends/esv11/EsBeatClock.{h,cpp}`

Namespace: `lightwaveos::audio::esv11`.

#### Class `EsBeatClock`
- Ctor: `EsBeatClock() = default`.
- Public: `reset()`, `setExternalSyncMode(bool enabled)`, `injectExternalBeat(float bpm, float phase01, bool tick, bool downbeat, uint8_t beatInBar, uint64_t now_us, uint32_t sample_rate_hz = 0)`, `tick(const ControlBusFrame& latest, bool newAudioFrame, const AudioTime& render_now)`, `snapshot() const`.
- Private fields: `m_hasBase`, `m_lastTickT` (`AudioTime`), `m_externalSync`, `m_externalPending`, `m_externalBpm`, `m_externalPhase01`, `m_externalTick`, `m_externalDownbeat`, `m_externalBeatInBar`, `m_externalT` (`AudioTime`), `m_phase01`, `m_bpm`, `m_conf`, `m_beatInBar`, `m_downbeatTick`, `m_beatTick`, `m_beatStrength`, `m_snap` (`MusicalGridSnapshot`).
- Private static method: `clamp01(float x)`.

---

### `firmware-v3/src/audio/backends/esv11/EsV11Adapter.{h,cpp}`

Namespace: `lightwaveos::audio::esv11`.

#### Class `EsV11Adapter`
- Ctor: `EsV11Adapter() = default`.
- Public: `reset()`, `buildFrame(lightwaveos::audio::ControlBusFrame& out, const EsV11Outputs& es, uint32_t hopSeq)`.
- Private adaptive normalisation fields: `m_binsMaxFollower`, `m_chromaMaxFollower`, `m_heavyBands[CONTROLBUS_NUM_BANDS]`, `m_heavyChroma[CONTROLBUS_NUM_CHROMA]`, `m_beatInBar`.
- SB parity (3.1.0 waveform) constants: `SB_WAVEFORM_POINTS`, `SB_WAVEFORM_HISTORY`. Fields: `m_sbWaveformHistory`, `m_sbWaveformHistoryIndex`, `m_sbMaxWaveformValFollower`, `m_sbWaveformPeakScaled`, `m_sbWaveformPeakScaledLast`, `m_sbNoteChroma`, `m_sbChromaMaxVal`.
- Lightweight onset: `m_prevSnareEnergy`, `m_prevHihatEnergy`.
- HF semantics (guarded `FEATURE_AUDIO_HF_SEMANTICS`): `m_hfEnergy`, `m_airEnergy`, `m_cymbalSustain`, `m_prevHfRaw`, `m_prevBrightness`, `m_hatEventAgeMs`.
- File-scope helpers in `.cpp`: anonymous-namespace constant `kDefaultControlBusFrame`; free functions `clamp01(float)`, `q15(float)`.

---

### `firmware-v3/src/audio/backends/esv11/EsV11_32kHz_Shim.h`

No exposed C++ symbols (preprocessor-only configuration shim). Sets `SAMPLES_PER_CHUNK`, `CHUNKS_PER_HOP`, etc. for the 32 kHz calibrated build.

---

### `firmware-v3/src/audio/backends/esv11/vendor/EsV11Buffers.{h,cpp}`

No surrounding C++ namespace — vendor-style C-linkage style symbols.

- Extern globals: `sample_history`, `window_lookup`, `novelty_curve`, `novelty_curve_normalized`, `vu_curve`, `vu_curve_normalized`, `tempi`, `frequencies_musical`, `spectrogram_average`, `noise_history`.
- Anonymous-namespace constants in `.cpp`: `kSpectrogramAverageSamples`, `kNoiseHistorySamples`.
- Anonymous-namespace helpers in `.cpp`: `esv11_calloc(size_t n, size_t size)`, `esv11_free(void*& ptr)`.
- Free functions: `esv11_init_buffers()`, `esv11_free_buffers()`.

---

### `firmware-v3/src/audio/backends/esv11/vendor/EsV11Shim.{h,cpp}`

No surrounding C++ namespace.

- Extern globals: `t_now_us`, `t_now_ms`.
- Free functions: `esv11_set_time(uint64_t now_us)`, `profile_function(...)`.

---

## Parameter-name patterns

Across the audio subsystem the parameter conventions hold consistently:

- **Audio sample buffers** — `const int16_t* samples` + `size_t count`; or `const int16_t* buffer` + `size_t N`; or `int16_t* buffer` for output (capture functions).
- **Output arrays** — `chromaOut`, `bandsOut`, `binsOut`, `outSample`, `out` (struct fill-by-reference).
- **Timing inputs** — `uint64_t now_us`, `uint32_t nowMs` (mixed), `uint32_t timeoutMs`, `uint32_t durationMs`, `float dtSeconds`, `float tauSeconds`.
- **Tempo / beat** — `float bpm`, `float phase01`, `bool tick`, `bool downbeat`, `uint8_t beatInBar`, `uint32_t sample_rate_hz` (vendor snake), `const AudioTime& render_now`, `bool newAudioFrame`.
- **DSP/tuning** — `float attackRate`, `float releaseRate`, `float safetyMultiplier`, `float minFloor`, `float energy`, `uint8_t refractory`, `float absFloor`.
- **Behaviour selector** — `plugins::VisualBehavior behavior`, `float priority`, `float rest`, `float build`, `float hold`, `uint16_t ms`, `const plugins::EffectContext& ctx`.
- **Style update** — `float rms`, `float flux`, `const float* bands`, `float beatConfidence`, `bool chordChanged`.
- **Trinity macros** — five-arg signature with one-word features: `float energy, float vocal, float bass, float perc, float bright`.
- **Capture** — `int16_t* buffer`, `uint32_t timeoutMs`, `int8_t gainDb`.
- **Adapter buildFrame** — `ControlBusFrame& out, const EsV11Outputs& es, uint32_t hopSeq`.
- **Control flags** — universally `bool enabled` (never `bool isEnabled` / `bool en` / `bool flag`).

## Anomalies and naming-friction points

These are surfaced as candidate review targets, not as recommendations:

1. **Typo preserved in `CaptureStats::hopsCapured`** (missing `t`) — propagates through telemetry. Verbatim across `AudioCapture.h:40` and any consumer.
2. **Vendor snake_case bleed-through.** The `esv11` namespace mixes lowerCamelCase (`tempoParams`, `m_lastChunkTiming`, `m_octaveCandBin`) with snake_case (struct fields `capture_us`, `gpu_tick_us`, `now_us`, `phase_radians`, `top_bpm`) and uses parameter `sample_rate_hz`, `render_now`. `GoertzelBin` similarly uses snake_case fields (`target_freq`, `block_size`, `coeff_q14`). Likely an artefact of the SensoryBridge / ES v1.1 vendor port. Any rename effort must decide whether to standardise or preserve vendor parity for diff cleanliness.
3. **Triple-implementation `AudioActor.cpp`.** Three full method-set duplicates (ESV11 / PIPELINECORE / GOERTZEL) means a rename of any cross-branch method (e.g. `processHop`, `captureHop`, `onTick`) needs to be applied in all three branches *and* in the helper block (`#if !FEATURE_AUDIO_BACKEND_ESV11`, lines 374–465). The header has matching tripled declarations at lines 641–1173.
4. **British vs American mix.** Source code uses American (`Initialized`, `behavior`, `analyze`, `normalize`, `color`) but comments and log strings frequently use British (`colour shift`, `initialise`, `centre`). A consistency pass would need to decide whether to remain split (per current convention) or to switch identifiers to British (large blast radius).
5. **`UNINITIALIZED` enumerator** (`AudioActorState`) — matches American spelling of `Initialized` field naming elsewhere (e.g. `m_initialized`, `isInitialized`). Internal consistency, but contrasts with British prose elsewhere in the repo.
6. **Mixed prefix styles for SensoryBridge parity buffers**: `m_sb*` is repeated 20+ times across the GOERTZEL + PIPELINECORE branches of `AudioActor` and again on `EsV11Adapter`. If parity buffers are consolidated into a class (e.g. `SbParityState`), the rename touches ~3 sites.
7. **`AudioActor::ZoneAgcSnapshot`** (nested struct) and `AudioActor::BandRatioConfig` / `BandRatioChannel` / `AudioConfidenceConfig` are inner-class types — possible targets for promotion to top-level (`audio::ZoneAgcSnapshot` etc.) if they leak into APIs.
8. **`AudioNode.h`** appears to be an empty stub (single class with no method bodies). Either a placeholder or vestigial. Worth confirming whether it has callers.
9. **`AudioMappingRegistry.h`** emits no symbols via clangd — preprocessor-only or empty file in the active configuration.
10. **Constant casing inconsistency.** `MAX_BEHAVIORS`, `MAX_BLOCK_SIZE`, `WINDOW_SIZE`, `HOP_BUDGET_US` (UPPER_SNAKE) coexist with the `kDefaultControlBusFrame`, `kSpectrogramAverageSamples`, `kNoiseHistorySamples` (Google-style lowerK prefix) used inside `.cpp` anonymous namespaces. Picking one will trigger a cosmetic-only rename pass.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-13 | agent:claude-opus-4-7 (SSA-N02) | Created — audio actor + backends naming inventory (AudioActor 3-branch monolith, AudioCapture, ChromaAnalyzer, GoertzelAnalyzer, StyleDetector, TrinityControlBusProxy, AudioBehaviorSelector, AudioTuning, EsV11Backend, EsBeatClock, EsV11Adapter, vendor shims). Extraction via clangd `get_document_symbols` + targeted Reads for parameter names. |
