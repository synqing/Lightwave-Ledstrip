---
abstract: "SSA-N03 — read-only name enumeration for the audio pipeline + onset + tempo + translation subsystems on branch feature/synqmatrix-rename-2026-05-13. Signatures and identifiers only (no bodies). Covers firmware-v3 src/audio/pipeline/* (PipelineAdapter, PipelineCore, BeatTracker, FrequencyMap, FFT; STMExtractor deferred to N01), src/audio/onset/OnsetDetector, src/audio/tempo/TempoTracker (excluded from production build), and the top-level translation/proxy modules TranslationEngine, StyleDetector and TrinityControlBusProxy. Excludes contracts/, backends/esv11/, AudioActor*, AudioCapture, ChromaAnalyzer, GoertzelAnalyzer per task scope. Use this artefact when scoping a rename / contract refactor across audio analysis modules."
---

# SSA-N03 — Audio Pipeline + Onset + Tempo + Translation Naming Inventory

**Branch:** `feature/synqmatrix-rename-2026-05-13`
**Date:** 2026-05-13
**Mode:** Read-only name extraction. No bodies, no rewrites.
**Source-truth verification:** clangd not consulted (output already at signature level); identifiers cross-checked by direct `Read` of each header and selective `Read` of each .cpp.

## Compile-out / Build-Gate Notes (Load-Bearing)

- `firmware-v3/src/audio/tempo/TempoTracker.cpp` is **excluded from the canonical K1 production build** via `platformio.ini` `build_src_filter`:
  - Line 169: `-<audio/tempo/TempoTracker.cpp>` (ESV11 / `esp32dev_audio_esv11_k1v2_32khz` envs)
  - Line 469: `-<audio/tempo/TempoTracker.cpp>` (PipelineCore env, comment: "Exclude Goertzel/ChromaAnalyzer/TempoTracker (replaced by PipelineCore)")
  - Its header `TempoTracker.h` IS compiled in because `PipelineCore.h` and `PipelineCore.cpp` reference `lightwaveos::audio::TempoTracker`, `TempoTrackerOutput`, `NUM_FREQS` in fields/members. So in production, the header's types are reachable but the implementation file is dropped. Any rename touching `TempoTracker` symbols must update both the excluded `.cpp` and the included header callers.
- `PipelineCore.cpp` embeds a "compat" path that owns a `TempoTracker m_tempoCompat` and feeds it via 64-bin FFT-derived novelty. This means the **TempoTracker class object code is still linked** through `PipelineCore.o` even though `TempoTracker.cpp` is excluded — i.e. the methods called on `m_tempoCompat` (init, updateNovelty, updateTempo, advancePhase, getOutput) must resolve. Verify build status before assuming TempoTracker is dead code.
- `FFT.h` is a single-header inline implementation (`namespace fft`, `namespace fft::detail`) — no `.cpp`. Renames must update header in place.
- `FrequencyMap.h` is header-only (all inline). No `.cpp` counterpart.
- `OnsetDetector.cpp` contains `#ifdef NATIVE_BUILD` block with file-scope anonymous-namespace helpers `fft_complex_inplace` and `rfft_native` — these are **host-test only** (replaced by `esp-dsp` calls on target). Rename scope differs by build flag.
- `TranslationEngine.cpp` carries a private file-scope state pool `TranslationScratch g_scratch[kScratchCapacity]` keyed on `TranslationState*`. This is **mutable global state**; renames that change the engine's ABI must account for it.

## Files Inspected (11)

| File | Lines | Role |
|---|---:|---|
| `firmware-v3/src/audio/pipeline/PipelineAdapter.h` | 190 | FeatureFrame → ControlBusRawInput bridge |
| `firmware-v3/src/audio/pipeline/PipelineAdapter.cpp` | 309 | Adapter implementation |
| `firmware-v3/src/audio/pipeline/PipelineCore.h` | 207 | DSP pipeline (FFT/bands/chroma/onset/beat) |
| `firmware-v3/src/audio/pipeline/PipelineCore.cpp` | 570 | Pipeline implementation |
| `firmware-v3/src/audio/pipeline/BeatTracker.h` | 95 | Comb-filter / CBSS beat tracker (pipeline-local) |
| `firmware-v3/src/audio/pipeline/BeatTracker.cpp` | 375 | Beat tracker implementation |
| `firmware-v3/src/audio/pipeline/FrequencyMap.h` | 265 | Frequency-semantic bin query (header-only) |
| `firmware-v3/src/audio/pipeline/FFT.h` | 192 | Radix-2 real FFT helper (header-only) |
| `firmware-v3/src/audio/onset/OnsetDetector.h` | 219 | 1024-pt FFT log-flux onset detector |
| `firmware-v3/src/audio/onset/OnsetDetector.cpp` | 425+ | Onset detector implementation |
| `firmware-v3/src/audio/tempo/TempoTracker.h` | 360 | Goertzel tempo bank (excluded from prod build) |
| `firmware-v3/src/audio/tempo/TempoTracker.cpp` | 700+ | Tempo tracker implementation |
| `firmware-v3/src/audio/TranslationEngine.h` | 105 | Features → SceneParameters perceptual translator |
| `firmware-v3/src/audio/TranslationEngine.cpp` | 574 | Translation implementation + g_scratch pool |
| `firmware-v3/src/audio/StyleDetector.h` | 180 | Music style classifier (feeds AudioFeatures) |
| `firmware-v3/src/audio/StyleDetector.cpp` | (paired) | Style classifier implementation |
| `firmware-v3/src/audio/TrinityControlBusProxy.h` | 78 | Trinity ML macro → ControlBusFrame proxy |
| `firmware-v3/src/audio/TrinityControlBusProxy.cpp` | (paired) | Proxy implementation |

`STMExtractor.{h,cpp}` deliberately not enumerated here — covered by SSA-N01 (contracts).

---

## 1. `audio/pipeline/PipelineAdapter.h/.cpp`

**Namespace:** `lightwaveos::audio`

### File-scope constants
- `static constexpr uint16_t BINS64_LEGACY_COUNT = 64;`
- `static constexpr uint16_t BINS256_COUNT = 256;`

### `PipelineAdapter.cpp` file-scope `using`
- `using ::audio::NamedBand;`

### `class PipelineAdapter`
- Nested `struct PipelineAdapter::Config`
  - `float sampleRate`
  - `uint16_t fftSize`
  - `float snareOnsetThreshold`
  - `float hihatOnsetThreshold`
  - `float onsetEnvGate`
  - `float fluxScale`
  - `float silenceRmsGateOpen`
  - `float silenceRmsGateClose`
  - `uint16_t silenceGateHoldHops`
  - `float silenceRmsGate` (legacy/unused)
- Public methods
  - `PipelineAdapter() = default;`
  - `void init(const Config& cfg);`
  - `const ::audio::FrequencyMap& frequencyMap() const;`
  - `void adapt(const FeatureFrame& frame, const float* magSpectrum, const int16_t* hopBuffer, ControlBusRawInput& out);`
  - `const float* bins256() const;`
- Private methods
  - `void buildBins64Shim(const float* normSpectrum, float* bins64Out);`
  - `void derivePercussion(const float* normSpectrum, float onsetEnv, float& snareEnergyOut, float& hihatEnergyOut, bool& snareTriggerOut, bool& hihatTriggerOut);`
  - `void normaliseMagnitudes(const float* magIn, float* normOut, uint16_t count);`
- Private fields
  - `Config m_config;`
  - `::audio::FrequencyMap m_freqMap;`
  - `float m_bins256[BINS256_COUNT];`
  - `float m_prevSnareEnergy;`
  - `float m_prevHihatEnergy;`
  - `bool m_spectrumGateOpen;`
  - `uint16_t m_silenceGateHoldCounter;`
  - `uint16_t m_minHoldCounter;`

---

## 2. `audio/pipeline/PipelineCore.h/.cpp`

**No enclosing namespace** (lives at root). Internally references `lightwaveos::audio::TempoTracker`.

### File-scope helpers (.cpp, anonymous / `static`)
- `static float clamp01(float x);` (file-scope; later overridden by `BeatTracker.cpp` anonymous-namespace duplicate — separate translation unit)
- `static constexpr float kChromaFreqs[PipelineCore::kChroma * PipelineCore::kOctaves];`
- `static constexpr float kBandEdges[PipelineCore::kBands + 1];`

### `struct StageFlags`
- `bool enableDc;`
- `bool enableBands;`
- `bool enableChroma;`
- `bool enableRms;`
- `bool enableWhitening;`

### `struct PeakPickConfig`
- `uint16_t preMax;`
- `uint16_t postMax;`
- `uint16_t preAvg;`
- `uint16_t postAvg;`
- `float delta;`
- `uint16_t wait;`

### `struct PipelineConfig`
- `uint32_t sampleRate;`
- `uint16_t hopSize;`
- `uint16_t windowSize;`
- `float dcAlpha;`
- `float onsetMeanAlpha;`
- `float onsetVarAlpha;`
- `float onsetK;`
- `float onsetGateRms;`
- `float fluxBinDivisor;`
- `float whitenDecay;`
- `float whitenFloor;`
- `PeakPickConfig peakPick;`
- `BeatConfig beat;`
- `StageFlags stages;`

### `struct FeatureFrame`
- `uint32_t seq;`
- `uint32_t timestamp_us;`
- `float rms;`
- `float rms_ungated;`
- `float peak;`
- `float bands[8];`
- `float chroma[12];`
- `float flux;`
- `float onset_env;`
- `float onset_event;`
- `float tempo_bpm;`
- `float tempo_confidence;`
- `float tempo_locked;`
- `float beat_phase;`
- `float beat_event;`
- `uint32_t process_us;`
- `uint32_t max_process_us;`
- `uint32_t dropped_blocks_total;`
- `uint32_t dropped_blocks_now;`

### `struct BandDef`
- `uint16_t binLo;`
- `uint16_t binHi;`

### `class PipelineCore`
- Public static constants
  - `static constexpr size_t kBands = 8;`
  - `static constexpr size_t kChroma = 12;`
  - `static constexpr size_t kOctaves = 4;`
  - `static constexpr size_t kMaxHop = 256;`
  - `static constexpr size_t kMaxWindow = 512;`
  - `static constexpr size_t kNumBins = kMaxWindow / 2;`
  - `static constexpr size_t kPeakBufSize = 32;`
- Public methods
  - `PipelineCore();`
  - `void reset();`
  - `void setConfig(const PipelineConfig& cfg);`
  - `const PipelineConfig& getConfig() const;`
  - `bool pushSamples(const int16_t* samples, size_t count, uint32_t timestamp_us);`
  - `bool pullFrame(FeatureFrame& out);`
  - `void setLastProcessUs(uint32_t us);`
  - `void addDroppedFrame();`
  - `bool setParamFloat(const char* name, float value);`
  - `bool getParamFloat(const char* name, float* out) const;`
  - `const float* getMagnitudeSpectrum() const;`
  - `const int16_t* getHopBuffer() const;`
- Private methods
  - `void initBinMapping();`
  - `void processHop(uint32_t timestamp_us);`
  - `float computeRms(const int16_t* samples, size_t count) const;`
  - `void buildWindow();`
  - `void computeMagnitudeSpectrum();`
  - `void extractBands(float* bands) const;`
  - `void extractChroma(float* chroma) const;`
  - `float computeLogFlux();`
  - `float computeOnsetEnv(float flux);`
  - `float peakPickUpdate(float onsetEnv);`
- Private fields
  - `PipelineConfig m_cfg;`
  - `FeatureFrame m_frame;`
  - `bool m_frameReady;`
  - `int16_t m_hopBuffer[kMaxHop];`
  - `size_t m_hopIndex;`
  - `float m_dcEstimate;`
  - `int16_t m_windowBuffer[kMaxWindow];`
  - `size_t m_windowIndex;`
  - `bool m_windowFilled;`
  - `float m_windowTemp[kMaxWindow];`
  - `float m_windowCoeffs[kMaxWindow];`
  - `arm_rfft_fast_instance_f32 m_rfftInst;` (guarded by `__ARM_ARCH_7EM__`)
  - `float m_fftOut[kMaxWindow];`
  - `float m_magSpectrum[kNumBins];`
  - `BandDef m_bandDefs[kBands];`
  - `uint16_t m_chromaBins[kChroma * kOctaves];`
  - `float m_prevLogMag[kNumBins];`
  - `bool m_hasPrevMag;`
  - `float m_onsetMean;`
  - `float m_onsetVar;`
  - `float m_whitenPeak[kNumBins];`
  - `float m_peakBuf[kPeakBufSize];`
  - `size_t m_peakWriteIdx;`
  - `uint32_t m_peakFrameCounter;`
  - `uint32_t m_peakLastEventFrame;`
  - `bool m_peakHasEvent;`
  - `uint32_t m_seq;`
  - `BeatTracker m_beatTracker;`
  - `float m_lastBassFlux;`
  - `lightwaveos::audio::TempoTracker m_tempoCompat;`
  - `lightwaveos::audio::TempoTrackerOutput m_tempoCompatOut;`
  - `float m_tempoBins64[lightwaveos::audio::NUM_FREQS];`
  - `uint8_t m_tempoCompatHopDiv;`

### Hot-reload parameter name namespaces (string keys passed to `setParamFloat` / `getParamFloat`)
- Prefix `peak.`: `preMax`, `postMax`, `preAvg`, `postAvg`, `delta`, `wait`
- Prefix `onset.`: `k`, `meanAlpha`, `varAlpha`, `gateRms`
- Prefix `dc.`: `alpha`
- Prefix `beat.`: delegated to `BeatTracker::setParamFloat` (see §3)

---

## 3. `audio/pipeline/BeatTracker.h/.cpp`

**No enclosing namespace** (root). Distinct from `firmware-v3/src/audio/onset/...` BeatTracker / `audio/tempo` — this is the **pipeline-internal** comb/CBSS tracker.

### File-scope helpers (.cpp, anonymous namespace)
- `inline float clamp01(float x);` (in `namespace {}`)

### `struct BeatConfig`
- `float tempoMinBpm;`
- `float tempoMaxBpm;`
- `float tempoPriorBpm;`
- `float tempoPriorWidth;`
- `float tempoDecay;`
- `float cbssAlpha;`
- `float minBeatFactor;`
- `uint8_t watchdogCycles;`
- `float watchdogThresh;`

### `class BeatTracker`
- Public static constants
  - `static constexpr size_t kOssLen = 512;`
  - `static constexpr size_t kMaxLag = 256;`
  - `static constexpr size_t kCbssLen = 256;`
- Public methods
  - `BeatTracker();`
  - `void reset();`
  - `void setConfig(const BeatConfig& cfg, uint32_t sampleRate, uint16_t hopSize);`
  - `void update(float onset_env, float bass_onset = -1.0f);`
  - `bool setParamFloat(const char* name, float value);`
  - `bool getParamFloat(const char* name, float* out) const;`
  - `float tempoBpm() const;`
  - `float beatPhase() const;`
  - `bool beatEvent() const;`
  - `float tempoConfidence() const;`
  - `bool tempoLocked() const;`
- Private methods
  - `void updateTempoEstimate();`
  - `void recalcLagBounds();`
- Private fields
  - `BeatConfig m_cfg;`
  - `float m_hopRate;`
  - `float m_oss[kOssLen];`
  - `size_t m_ossIdx;`
  - `float m_histogram[kMaxLag];`
  - `int m_minLag;`
  - `int m_maxLag;`
  - `float m_cbss[kCbssLen];`
  - `size_t m_cbssIdx;`
  - `float m_cbssPrev;`
  - `float m_cbssPrevPrev;`
  - `float m_tempoBpm;`
  - `int m_beatPeriodHops;`
  - `float m_beatPhase;`
  - `bool m_beatEvent;`
  - `float m_tempoConfidence;`
  - `bool m_tempoLocked;`
  - `uint32_t m_beatEventCount;`
  - `uint32_t m_totalHops;`
  - `uint32_t m_lastBeatHop;`
  - `uint32_t m_hopsSinceBeat;`
  - `float m_ossMean;`
  - `uint8_t m_watchdogCount;`
  - `uint32_t m_hopCount;`
  - `uint32_t m_tempoInterval;`

### `setParamFloat` / `getParamFloat` accepted name keys
- `minBeatFactor`, `cbssAlpha`, `tempoPriorBpm`, `tempoPriorWidth`, `tempoDecay`, `tempoMinBpm`, `tempoMaxBpm`, `watchdogThresh`

---

## 4. `audio/pipeline/FrequencyMap.h` (header-only)

**Namespace:** `audio` (note: bare `audio`, NOT `lightwaveos::audio`).

### Types
- `struct FrequencyBand`
  - `uint16_t binLo;`
  - `uint16_t binHi;`
  - `float freqLo;`
  - `float freqHi;`
- `enum class NamedBand : uint8_t { SUB_BASS = 0, KICK, LOW_MID, MID, SHIMMER, SNARE, HIHAT, AIR, COUNT };`
- `struct BandDefinition`
  - `float freqLo;`
  - `float freqHi;`

### File-scope
- `static constexpr BandDefinition kNamedBandDefs[static_cast<size_t>(NamedBand::COUNT)];`

### `class FrequencyMap`
- Public static constant
  - `static constexpr size_t kMaxBins = 256;`
- Public methods (all inline in header)
  - `void init(float sampleRate, uint16_t fftSize);`
  - `float bandEnergy(const float* spectrum, NamedBand band) const;`
  - `float bandMeanEnergy(const float* spectrum, NamedBand band) const;`
  - `float energyInRange(const float* spectrum, float freqLo, float freqHi) const;`
  - `float meanEnergyInRange(const float* spectrum, float freqLo, float freqHi) const;`
  - `float binHz() const;`
  - `uint16_t numBins() const;`
  - `float sampleRate() const;`
  - `uint16_t fftSize() const;`
  - `bool ready() const;`
  - `const FrequencyBand& namedBand(NamedBand band) const;`
  - `uint16_t freqToBin(float freqHz) const;`
  - `float binToFreq(uint16_t bin) const;`
- Private methods
  - `FrequencyBand computeBand(float freqLo, float freqHi) const;`
  - `static float sumBins(const float* spectrum, uint16_t binLo, uint16_t binHi);`
- Private fields
  - `float m_sampleRate;`
  - `uint16_t m_fftSize;`
  - `uint16_t m_numBins;`
  - `float m_binHz;`
  - `bool m_initialised;`
  - `FrequencyBand m_namedBands[static_cast<size_t>(NamedBand::COUNT)];`

---

## 5. `audio/pipeline/FFT.h` (header-only)

**Namespaces:** `fft`, `fft::detail`.

### `namespace fft::detail`
- `constexpr size_t kMaxFftSize = 512;`
- `constexpr size_t kMaxUnpackBins = kMaxFftSize / 4;`
- `constexpr size_t kMaxStages = 10;`
- `inline void ensureTwiddles(size_t N, float* stageRe, float* stageIm, float* unpackRe, float* unpackIm);`
- Static cache: `static size_t s_cachedN` (inside `ensureTwiddles`)

### `namespace fft`
- `inline void bitReverse(float* buf, size_t N);`
- `inline void rfft(float* buf, size_t N);`
  - Static cached LUTs inside body: `s_stageRe`, `s_stageIm`, `s_unpackRe`, `s_unpackIm`
- `inline void magnitudes(const float* buf, float* mag, size_t N);`

---

## 6. `audio/onset/OnsetDetector.h/.cpp`

**Namespace:** `lightwaveos::audio`.

### File-scope helpers (.cpp, `NATIVE_BUILD` only, anonymous namespace)
- `void fft_complex_inplace(float* data, size_t N);`
- `void rfft_native(float* data, size_t N);`

### `enum OnsetGateFlags : uint8_t`
- `ONSET_GATE_NONE = 0`
- `ONSET_GATE_ABS_RMS = 1u << 0`
- `ONSET_GATE_ACTIVITY = 1u << 1`
- `ONSET_GATE_NO_PREV = 1u << 2`
- `ONSET_GATE_WARMUP = 1u << 3`

### `struct OnsetResult`
- `float flux;`
- `float onset_env;`
- `float onset_event;`
- `float bass_flux;`
- `float mid_flux;`
- `float high_flux;`
- `bool kick_trigger;`
- `bool snare_trigger;`
- `bool hihat_trigger;`
- `float input_rms;`
- `float noise_floor;`
- `float activity;`
- `uint8_t gate_flags;`
- `uint16_t process_us;`
- `uint16_t fft_frontend_us;`
- `uint16_t decision_us;`
- `uint16_t flux_us;`

### `class OnsetDetector`
- Nested `struct OnsetDetector::Config`
  - `uint32_t warmupFrames;`
  - `uint16_t fftSize;`
  - `uint16_t musicalBinLo;`
  - `uint16_t musicalBinHi;`
  - `uint8_t thresholdFrames;`
  - `float thresholdMultiplier;`
  - `float thresholdOffset;`
  - `float thresholdFloor;`
  - `float rmsGate;`
  - `float activityFloor;`
  - `float noiseFloorRise;`
  - `float noiseFloorFall;`
  - `uint16_t activityHoldFrames;`
  - `float noiseAdaptK;`
  - `float activityGateK;`
  - `float activityRangeK;`
  - `float activityRangeMin;`
  - `float activityCutoff;`
  - `uint8_t preMaxFrames;`
  - `uint8_t peakWait;`
  - `uint16_t bassBinLo;`
  - `uint16_t bassBinHi;`
  - `uint16_t snareBinLo;`
  - `uint16_t snareBinHi;`
  - `uint16_t hihatBinLo;`
  - `uint16_t hihatBinHi;`
  - `float kickThresholdK;`
  - `float snareThresholdK;`
  - `float hihatThresholdK;`
  - `float kickEmaAlpha;`
  - `float snareEmaAlpha;`
  - `float hihatEmaAlpha;`
  - `uint8_t kickRefractory;`
  - `uint8_t snareRefractory;`
  - `uint8_t hihatRefractory;`
- Public methods
  - `void init();`
  - `void init(const Config& cfg);`
  - `void reset();`
  - `OnsetResult process(const float* samples, float currentRms);`
- Private static constants
  - `static constexpr uint16_t MAX_FFT_SIZE = 1024;`
  - `static constexpr uint16_t MAX_BINS = MAX_FFT_SIZE / 2;`
  - `static constexpr uint8_t FLUX_RING_SIZE = 16;`
  - `static constexpr uint8_t ENV_RING_SIZE = 32;`
- Private nested type
  - `struct BandState`
    - `float fluxMean;`
    - `float prevFlux;`
    - `float prev2Flux;`
    - `uint32_t lastTriggerFrame;`
- Private methods
  - `void computeHannLut();`
  - `void computeFFT();`
  - `void computeMagnitudes();`
  - `float computeBandFlux(uint16_t binLo, uint16_t binHi) const;`
  - `float computeActivity(float currentRms);`
  - `float applyAdaptiveThreshold(float flux);`
  - `float peakPick(float env, bool emitEnabled);`
  - `bool bandTrigger(BandState& state, float flux, float thresholdK, float emaAlpha, uint8_t refractoryFrames, bool emitEnabled);`
  - `static float computeMedian(const float* ring, uint8_t ringSize, uint8_t count);`
  - `static uint32_t getTimeUs();`
- Private fields
  - `Config m_cfg;`
  - `float m_hannLut[MAX_FFT_SIZE];`
  - `float m_fftBuf[MAX_FFT_SIZE];`
  - `float m_magnitude[MAX_BINS];`
  - `float m_prevMagnitude[MAX_BINS];`
  - `bool m_hasPrevMag;`
  - `bool m_initialised;`
  - `float m_rmsNoiseFloor;`
  - `uint16_t m_activityHoldCounter;`
  - `float m_fluxRing[FLUX_RING_SIZE];`
  - `uint8_t m_fluxWriteIdx;`
  - `uint8_t m_fluxCount;`
  - `float m_envRing[ENV_RING_SIZE];`
  - `uint8_t m_envWriteIdx;`
  - `uint32_t m_lastEventFrame;`
  - `uint32_t m_frameCount;`
  - `BandState m_bassState;`
  - `BandState m_midState;`
  - `BandState m_highState;`
  - `uint32_t m_warmupFrames;`

---

## 7. `audio/tempo/TempoTracker.h/.cpp`  (compile-out gate: `TempoTracker.cpp` excluded from ESV11 + PipelineCore production envs)

**Namespace:** `lightwaveos::audio`.

### File-scope constants (header)
- `constexpr uint16_t NUM_TEMPI = 96;`
- `constexpr uint16_t NUM_FREQS = 64;`
- `constexpr float TEMPO_LOW = 48.0f;`
- `constexpr float TEMPO_HIGH = TEMPO_LOW + static_cast<float>(NUM_TEMPI - 1);`
- `constexpr float SPECTRAL_LOG_HZ = 50.0f;`
- `constexpr float VU_LOG_HZ = 50.0f;`
- `constexpr uint16_t SPECTRAL_HISTORY_LENGTH = 1024;`
- `constexpr uint16_t VU_HISTORY_LENGTH = 512;`
- `constexpr float BEAT_SHIFT_PERCENT = 0.08f;`
- `constexpr float REFERENCE_FPS = 100.0f;`
- `constexpr float NOVELTY_DECAY = 0.999f;`
- `constexpr float HYSTERESIS_TIME_MS = 200.0f;`
- `constexpr int HYSTERESIS_FRAMES = static_cast<int>(HYSTERESIS_TIME_MS * SPECTRAL_LOG_HZ / 1000.0f);`
- `constexpr float WINDOW_DECAY_RATE = 5.0f;`

### `struct TempoBin`
- `float target_bpm;`
- `float target_hz;`
- `float coeff;`
- `float sine;`
- `float cosine;`
- `uint32_t block_size;`
- `float phase;`
- `bool phase_inverted;`
- `float phase_radians_per_frame;`
- `float magnitude;`
- `float magnitude_raw;`
- `float beat;`

### `struct TempoTrackerOutput`
- `float bpm;`
- `float phase01;`
- `float confidence;`
- `bool beat_tick;`
- `bool locked;`
- `float beat_strength;`

### `class TempoTracker`
- Public methods
  - `void init();`
  - `void updateNovelty(const float* bins, uint16_t num_bins, float rms, bool bins_ready);`
  - `void updateTempo(float delta_sec);`
  - `void advancePhase(float delta_sec);`
  - `TempoTrackerOutput getOutput() const;`
  - `const TempoBin* getBins() const;`
  - `const float* getSmoothed() const;`
  - `uint16_t getWinnerBin() const;`
  - `const float* getSpectralHistory() const;`
  - `const float* getVuHistory() const;`
  - `uint16_t getSpectralIndex() const;`
  - `uint16_t getVuIndex() const;`
- Private methods
  - `void initWindowLut();`
  - `float computeMagnitude(uint16_t bin);`
  - `void normalizeBuffer(float* buffer, float* normalized, float& scale, float tau, uint16_t length);`
  - `void updateWinner();`
  - `uint16_t validateWinnerBin() const;`
  - `void checkSilence();`
- Private fields
  - `TempoBin tempi_[NUM_TEMPI];`
  - `float tempi_smooth_[NUM_TEMPI];`
  - `float spectral_curve_[SPECTRAL_HISTORY_LENGTH];`
  - `uint16_t spectral_index_;`
  - `float bins_last_[NUM_FREQS];`
  - `float window_lut_[SPECTRAL_HISTORY_LENGTH];`
  - `float vu_curve_[VU_HISTORY_LENGTH];`
  - `uint16_t vu_index_;`
  - `float rms_last_;`
  - `float vu_accum_;`
  - `uint8_t vu_accum_count_;`
  - `float novelty_scale_;`
  - `float vu_scale_;`
  - `uint8_t spectral_scale_count_;`
  - `uint8_t vu_scale_count_;`
  - `uint16_t calc_bin_;`
  - `uint16_t winner_bin_;`
  - `uint16_t candidate_bin_;`
  - `uint8_t candidate_frames_;`
  - `float power_sum_;`
  - `float confidence_;`
  - `float current_phase_;`
  - `bool beat_tick_;`
  - `uint32_t last_tick_ms_;`
  - `uint32_t time_ms_;`
  - `bool silence_detected_;`
  - `float silence_level_;`

---

## 8. `audio/TranslationEngine.h/.cpp`

**Namespace:** `lightwaveos::audio`. Free-function ABI (no class), with mutable file-scope scratch pool.

### Forward declaration
- `enum class ChordType : uint8_t;`

### `enum class MotionPrimitive : uint8_t`
- `DRIFT, FLOW, BLOOM, RECOIL, LOCK, DECAY`

### `struct AudioFeatures`
- `uint32_t hop_sequence;`
- `float rms;`
- `float fast_rms;`
- `float flux;`
- `float fast_flux;`
- `float bass;`
- `float mid;`
- `float treble;`
- `float harmonic_saliency;`
- `float rhythmic_saliency;`
- `float timbral_saliency;`
- `float dynamic_saliency;`
- `float liveliness;`
- `float tempo_bpm;`
- `float tempo_confidence;`
- `float beat_strength;`
- `bool beat_tick;`
- `bool downbeat_tick;`
- `bool snare_trigger;`
- `bool hihat_trigger;`
- `bool is_silent;`
- `float silent_scale;`
- `audio::MusicStyle style;`
- `float style_confidence;`
- `uint8_t chord_root;`
- `audio::ChordType chord_type;`
- `float chord_confidence;`

### `struct SceneParameters`
- `MotionPrimitive motion_type;`
- `float brightness_scale;`
- `float motion_rate;`
- `float motion_depth;`
- `float hue_shift_speed;`
- `float diffusion;`
- `float beat_pulse;`
- `float tension;`
- `float phrase_progress;`
- `float transition_progress;`
- `bool phrase_boundary;`
- `bool timing_reliable;`

### `struct TranslationState`
- `MotionPrimitive current_motion;`
- `MotionPrimitive target_motion;`
- `float transition_progress;`
- `float brightness_scale;`
- `float motion_rate;`
- `float motion_depth;`
- `float hue_shift_speed;`
- `float diffusion;`
- `float beat_pulse;`
- `float tension;`
- `float phrase_progress;`
- `float phrase_energy_accum;`
- `float phrase_flux_accum;`
- `float phrase_harmonic_accum;`
- `float previous_phrase_energy;`
- `float previous_phrase_flux;`
- `float previous_phrase_harmonic;`
- `float time_since_last_onset_s;`
- `float time_since_last_beat_s;`
- `float held_bpm;`
- `float held_bpm_age_s;`
- `uint8_t beat_in_bar;`
- `uint8_t bars_in_phrase;`
- `uint8_t bar_in_phrase;`
- `bool phrase_boundary;`
- `bool timing_reliable;`

### File-scope inline constant
- `inline constexpr SceneParameters kDefaultSceneParameters = { ... };`

### Public free functions
- `void translation_init(TranslationState* state);`
- `void translation_update(const AudioFeatures* features, float delta_time, TranslationState* state);`
- `void translation_get_parameters(const TranslationState* state, SceneParameters* params);`

### `.cpp` file-scope (anonymous namespace) — internal API surface

Constants (all `constexpr float` / `constexpr uint8_t` / `constexpr size_t`):
- `kDtMin`, `kDtMax`
- `kTempoConfidenceReliable`, `kTempoConsecutiveHopsRequired`, `kTempoHoldSeconds`, `kTempoMinBpm`, `kTempoMaxBpm`, `kFallbackBpm`
- `kBeatPulseTau`, `kMotionTau`, `kDriftTau`
- `kHardBoundaryScore`, `kChordBoundaryConfidence`, `kStyleRhythmicConfidence`
- `kSceneBrightnessMin`, `kSceneBrightnessMax`, `kSceneRateMin`, `kSceneRateMax`, `kSceneDepthMin`, `kSceneDepthMax`
- `kScratchCapacity = 8`

Inline math helpers:
- `inline float clampf(float v, float lo, float hi);`
- `inline float clamp01(float v);`
- `inline float lerpf(float a, float b, float t);`
- `inline float expAlpha(float dt, float tau);`
- `inline float smoothExp(float current, float target, float dt, float tau);`
- `inline float max3(float a, float b, float c);`

Internal type:
- `struct TranslationScratch`
  - `const TranslationState* key;`
  - `MotionPrimitive pending_winner;`
  - `uint8_t pending_count;`
  - `uint8_t low_rms_hops;`
  - `uint16_t phrase_hops;`
  - `uint8_t phrase_chord_root;`
  - `float prev_rms;`
  - `float negative_slope_s;`
  - `float high_energy_age_s;`
  - `uint8_t consecutive_reliable_hops;`
  - `float recoil_remaining_s;`
  - `float nominal_hop_s;`
  - `float last_change_score;`
  - `bool skip_phrase_delta_next;`

File-scope mutable state pool:
- `TranslationScratch g_scratch[kScratchCapacity];` (load-bearing global)

Internal helpers:
- `TranslationScratch& scratchFor(const TranslationState* state);`
- `void resetScratch(const TranslationState* state);`
- `float hueShiftTarget(float effectiveBpm, float harmonicSaliency, float chordConfidence);`
- `float computeTensionRaw(float rms, float flux, float rhythmic, float harmonic);`
- `void clampStateOutputs(TranslationState* state);`

---

## 9. `audio/StyleDetector.h/.cpp`

**Namespace:** `lightwaveos::audio`. (Note: `MusicStyle` enum itself lives in `contracts/StyleDetector.h` per N01 scope; this file only consumes it.)

### `struct StyleClassification`
- `MusicStyle detected;`
- `float confidence;`
- `float styleWeights[5];`
- `uint32_t framesAnalyzed;`
- `float getWeight(MusicStyle style) const;`

### `struct StyleDetectorTuning`
- `uint32_t analysisWindowHops;`
- `uint32_t minHopsForClassification;`
- `float beatConfidenceThreshold;`
- `float bassRatioThreshold;`
- `float trebleRatioThreshold;`
- `float dynamicRangeThreshold;`
- `float fluxVarianceThreshold;`
- `float chordChangeRateThreshold;`
- `float styleAlpha;`
- `float hysteresisThreshold;`

### `struct StyleFeatures`
- `float beatConfidenceAvg;`
- `float beatConfidenceVar;`
- `float bassRatio;`
- `float midRatio;`
- `float trebleRatio;`
- `float rmsMin;`
- `float rmsMax;`
- `float dynamicRange;`
- `float chordChangeRate;`
- `uint32_t chordChanges;`
- `float fluxMean;`
- `float fluxVariance;`
- `void reset();`

### `class StyleDetector`
- Public methods
  - `StyleDetector();`
  - `void update(float rms, float flux, const float* bands, float beatConfidence, bool chordChanged);`
  - `const StyleClassification& getClassification() const;`
  - `MusicStyle getStyle() const;`
  - `float getConfidence() const;`
  - `const StyleFeatures& getFeatures() const;`
  - `void reset();`
  - `void setTuning(const StyleDetectorTuning& tuning);`
- Private methods
  - `void computeStyleWeights();`
  - `void selectDominantStyle();`
- Private fields
  - `StyleClassification m_classification;`
  - `StyleFeatures m_features;`
  - `StyleDetectorTuning m_tuning;`
  - `uint32_t m_hopCount;`
  - `float m_windowStartTimeS;`
  - `uint8_t m_prevChordRoot;`
  - `float m_prevFlux;`
  - `float m_rmsSum;`
  - `float m_fluxSum;`
  - `float m_fluxSqSum;`
  - `float m_beatConfSum;`
  - `float m_beatConfSqSum;`
  - `float m_bandSums[8];`

---

## 10. `audio/TrinityControlBusProxy.h/.cpp`

**Namespace:** `lightwaveos::audio`.

### `class TrinityControlBusProxy`
- Public methods
  - `TrinityControlBusProxy();`
  - `void setMacros(float energy, float vocal, float bass, float perc, float bright);`
  - `const ControlBusFrame& getFrame() const;`
  - `bool isActive() const;`
  - `void markActive();`
  - `void reset();`
- Private fields
  - `ControlBusFrame m_frame;`
  - `uint64_t m_lastUpdate;`
- Private static constant
  - `static constexpr uint64_t STALENESS_TIMEOUT_US = 250000;` (250 ms staleness window)

---

## Cross-Subsystem Type Dependencies (rename blast radius)

| Type / Symbol | Defined in | Referenced from (N03 surface) |
|---|---|---|
| `FeatureFrame` | `PipelineCore.h` | `PipelineAdapter.h`, `PipelineAdapter.cpp` |
| `ControlBusRawInput` | `contracts/ControlBus.h` (N01) | `PipelineAdapter.{h,cpp}` |
| `ControlBusFrame` | `contracts/ControlBus.h` (N01) | `TrinityControlBusProxy.{h,cpp}` |
| `BeatConfig` | `pipeline/BeatTracker.h` | `pipeline/PipelineCore.h` (`PipelineConfig::beat`) |
| `BeatTracker` | `pipeline/BeatTracker.h` | `pipeline/PipelineCore.h` (member `m_beatTracker`) |
| `lightwaveos::audio::TempoTracker` | `tempo/TempoTracker.h` | `pipeline/PipelineCore.h` (member `m_tempoCompat`) — **header symbol still required even though `TempoTracker.cpp` is excluded from build** |
| `lightwaveos::audio::TempoTrackerOutput` | `tempo/TempoTracker.h` | `pipeline/PipelineCore.h` (member `m_tempoCompatOut`) |
| `lightwaveos::audio::NUM_FREQS` | `tempo/TempoTracker.h` | `pipeline/PipelineCore.{h,cpp}` (sizing `m_tempoBins64`) |
| `::audio::FrequencyMap` | `pipeline/FrequencyMap.h` | `pipeline/PipelineAdapter.{h,cpp}` (member + accessor) |
| `::audio::NamedBand` | `pipeline/FrequencyMap.h` | `pipeline/PipelineAdapter.cpp` (`derivePercussion`) |
| `audio::MusicStyle` | `contracts/StyleDetector.h` (N01) | `TranslationEngine.h` (`AudioFeatures::style`), `StyleDetector.h` |
| `audio::ChordType` | `contracts/...` (N01) | `TranslationEngine.h` (`AudioFeatures::chord_type`) |
| `OnsetResult` | `onset/OnsetDetector.h` | (Consumed by AudioActor — N02 surface) |
| `fft::rfft`, `fft::magnitudes`, `fft::bitReverse` | `pipeline/FFT.h` | `pipeline/PipelineCore.cpp` (ESP_PLATFORM branch) |

## Naming Anomalies (flag for rename discussion)

1. **`audio` vs `lightwaveos::audio` namespace split.** `FrequencyMap.h` and `FFT.h` declare a bare `audio::` / `fft::` namespace, while every other file in `audio/` lives in `lightwaveos::audio`. `PipelineAdapter.cpp` adds `using ::audio::NamedBand;` to bridge. Recommend collapsing to `lightwaveos::audio` to remove the namespace duality.
2. **Two unrelated `BeatTracker` types in the source tree.** `audio/pipeline/BeatTracker.{h,cpp}` (no namespace) is comb/CBSS based. There is also the older Goertzel-era beat path in `audio/tempo/TempoTracker.{h,cpp}` (`lightwaveos::audio`) — distinct contract, distinct field names. The pipeline `BeatTracker` is owned by `PipelineCore::m_beatTracker` but `processHop()` in `PipelineCore.cpp` does NOT call `m_beatTracker.update()` — it drives `m_tempoCompat` (the `TempoTracker`) instead. The `BeatTracker` member therefore appears to be dead-but-linked in PipelineCore; worth a rename-time prune decision.
3. **`pipeline/BeatTracker.h` lives at root namespace.** No `lightwaveos::audio` enclosure; collides conceptually with the also-root `StageFlags`, `PeakPickConfig`, `PipelineConfig`, `FeatureFrame`, `BandDef`, `PipelineCore` symbols.
4. **`FeatureFrame` field naming uses snake_case** (`tempo_bpm`, `onset_env`, `beat_event`, `dropped_blocks_total`) whereas every adjacent struct (`ControlBusRawInput`, `OnsetResult` partially) mixes camelCase and snake_case. `OnsetResult` itself is fully snake_case for results but its `Config` is camelCase. Naming convention is not consistent across the subsystem.
5. **`OnsetGateFlags` uses `enum` (unscoped) with `ONSET_GATE_*` prefix**, whereas `NamedBand` and `MotionPrimitive` are `enum class` without prefixes. Pick one style.
6. **`OnsetDetector::Config::peakWait` vs `PeakPickConfig::wait`.** Both name the inter-event refractory period; one is fully qualified, the other terse. Easy to confuse during rename.
7. **`BeatTracker::setParamFloat("watchdogThresh")` and `BeatTracker::setParamFloat("watchdogCycles")` mismatch** — only `watchdogThresh` is hot-reload accessible; `watchdogCycles` is set only via `BeatConfig` at config-load time. The setter table is incomplete versus the config struct.
8. **`TempoTracker` is in `audio/tempo/` but its `.cpp` is excluded from the production build** (`build_src_filter -<audio/tempo/TempoTracker.cpp>` lines 169 & 469). Header types stay reachable because `PipelineCore` embeds the class as a member. Any rename touching `TempoTracker` must update the excluded `.cpp` AND every `PipelineCore` call site, even though no current build links the implementation directly from `tempo/`.
9. **`TranslationEngine` uses C-style free functions with mutable file-scope `g_scratch[8]`** keyed on `TranslationState*`. This is the only file in scope with shared mutable globals at function-scope-equivalent visibility. Rename must preserve this lookup-by-pointer behaviour or restructure intentionally.
10. **`AudioFeatures::is_silent` + `silent_scale` vs `OnsetResult::activity` vs `ControlBusRawInput.tempoLocked`** — three different idioms for "music is/isn't playing strongly enough." Consider unifying.
11. **`PipelineCore::kBands == 8` but `BandDef m_bandDefs[kBands]` and `kNumBins == 256`**, whereas `FrequencyMap::kMaxBins == 256` and `FrequencyMap::numBins() == fftSize / 2`. Both modules independently assert the 256-bin assumption — no shared constant. Worth a single canonical `kFftBinCount` in N01 (contracts).
12. **`SPECTRAL_LOG_HZ`, `VU_LOG_HZ`, `REFERENCE_FPS`** in `TempoTracker.h` are ALL_CAPS `constexpr`; `kPi`, `kBands`, `kNumBins`, `kScratchCapacity` elsewhere are camelCase `constexpr`. Style is inconsistent within the subsystem.

---

**Files inspected:** 18 (11 .h, 7 .cpp — STMExtractor deferred to N01).
**Output path:** `docs/temporary/projects/synqmatrix-naming-review/03-audio-pipeline-onset-tempo.md`

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-13 | agent:claude-opus-4-7 | Created SSA-N03 naming inventory for audio pipeline + onset + tempo + translation surfaces on branch feature/synqmatrix-rename-2026-05-13. Documents compile-out gate for TempoTracker.cpp and flags 12 cross-cutting naming anomalies. |
