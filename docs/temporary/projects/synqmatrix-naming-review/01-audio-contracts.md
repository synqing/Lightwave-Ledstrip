---
abstract: "Exhaustive symbol inventory of firmware-v3/src/audio/contracts/ — class/struct names, public/private methods, all data members, free functions, enums and constants, plus recurring parameter-name patterns. Read-only extraction for SynqMatrix migration naming-convention review."
---

# Subsystem: Audio Contracts

> Scope: firmware-v3/src/audio/contracts/
> Files inspected: 13 (.h x 10, .cpp x 3 — AudioTime, ControlBus, MusicalGrid, OnsetSemantics + AudioEffectMapping)
> Generated 2026-05-13 for SynqMatrix migration naming review.

## Convention snapshot
- Namespace pattern: `lightwaveos::audio` (most files); `lightwaveos { namespace audio { ... } }` form in MusicalSaliency.h, StyleDetector.h (mixed style), AudioEffectMapping.h. SnapshotBuffer also lives in `lightwaveos::audio`.
- File naming: PascalCase (e.g. `ControlBus.h`, `MusicalGrid.h`, `OnsetSemantics.cpp`).
- Class/struct case: PascalCase (e.g. `ControlBus`, `MusicalGrid`, `AudioParameterMapping`, `MotionShaper`).
- Method case: **mixed** — PascalCase for top-level lifecycle methods (`Reset`, `UpdateFromHop`, `Publish`, `ReadLatest`, `Tick`, `GetFrame`, `OnTempoEstimate`, `SetTimeSignature`) and camelCase for setters/getters and post-2025 additions (`setSmoothing`, `getAlphaFast`, `setMoodSmoothing`, `applyDerivedFeatures`, `updateFromK1`, `injectExternalBeat`, `isSilent`, `hasOverride`, `applyCurve`, `updateSmoothed`). No file is internally consistent — this is the dominant inconsistency to flag for Captain.
- Free-function case: PascalCase with snake_case suffix (`AudioTime_SamplesBetween`, `AudioTime_SecondsBetween`); also `resetOnsetSemanticTracker`, `updateOnsetContext`, `getMusicStyleName` use camelCase.
- Member variable prefix: `m_` everywhere (e.g. `m_frame`, `m_alpha_fast`, `m_zones`, `m_beat_float`, `m_lookahead_bands`).
- Member variable internal case: **mixed** — snake_case dominates inside `ControlBus`/`MusicalGrid` (`m_alpha_fast`, `m_band_attack`, `m_bpm_smoothed`, `m_beat_float`, `m_pending_beat_t`), camelCase appears in newer files (`m_lastBeatStrength`, `m_lastHopSeq`, `m_lastTriggerMs`, `m_prevOnset`, `m_externalBpm`, `m_lastApplyMicros`).
- Constant prefix: **mixed** — `k`-prefix is not used in this subsystem. Public constants use SCREAMING_SNAKE_CASE (`CONTROLBUS_NUM_BANDS`, `CONTROLBUS_NUM_CHROMA`, `LOOKAHEAD_FRAMES`, `BINS_64_COUNT`, `STM_MEL_BANDS`, `MSEM_CONF_HIGH`, `MSEM_BIT_WEIGHT`, `JITTER_WINDOW`, `SYNCOPATION_EMA_ALPHA`, `MAX_MAPPINGS_PER_EFFECT`, `MAX_EFFECTS`, `VERSION`) — all `static constexpr`.
- Field-naming style: snake_case in older audio-fact fields (`hop_seq`, `fast_rms`, `bar_phase01`, `beats_per_bar`, `beat_in_bar`, `beat_index`, `bar_index`, `sample_index`, `sample_rate_hz`, `monotonic_us`, `es_bpm`, `es_beat_tick`, `sb_waveform`, `timing_jitter`, `pitch_contour_dir`, `syncopation_level`) vs camelCase in newer additions (`tempoLocked`, `tempoBpm`, `kickTrigger`, `snareTrigger`, `onsetFlux`, `audioConfidence`, `silentScale`, `stmReady`, `binHz`, `bins64`, `bins256`, `bins64Adaptive`, `currentStyle`, `styleConfidence`, `chordState`, `cymbalSustain`, `hfFlux`, `spectralBrightnessDelta`). Mixed within the same struct (`ControlBusFrame`).
- Enum class style: PascalCase types, SCREAMING_SNAKE_CASE values (`ChordType::MAJOR`, `MusicStyle::RHYTHMIC_DRIVEN`, `SaliencyType::HARMONIC`, `AudioSource::BASS`, `VisualTarget::BRIGHTNESS`, `MappingCurve::LINEAR`) — but enumerator names also include numeric suffixes (`BAND_0`..`BAND_7`).
- Sentinel enum value: `NONE = 0xFF` used for "disabled" in `AudioSource`, `VisualTarget`; `Count = 6` used in `OnsetSemanticChannelIndex`.

---

## lightwaveos::audio::AudioTime
File: `firmware-v3/src/audio/contracts/AudioTime.h` / `.cpp`

### Members (fields)
- `sample_index` — `uint64_t`, monotonic ADC sample count
- `sample_rate_hz` — `uint32_t`, hop sample rate (default 12800)
- `monotonic_us` — `uint64_t`, `esp_timer_get_time()` snapshot

### Constructors
- `AudioTime()` default
- `AudioTime(uint64_t idx, uint32_t sr, uint64_t us)`

### Free functions (namespace-level)
- `int64_t AudioTime_SamplesBetween(const AudioTime& a, const AudioTime& b)`
- `float AudioTime_SecondsBetween(const AudioTime& a, const AudioTime& b)`

### Notable parameter-name patterns
- `a`, `b`, `idx`, `sr`, `us`

---

## lightwaveos::audio::ChordType (enum class)
File: `firmware-v3/src/audio/contracts/ControlBus.h`

### Enum values
- `NONE = 0`, `MAJOR = 1`, `MINOR = 2`, `DIMINISHED = 3`, `AUGMENTED = 4`

---

## lightwaveos::audio::ChordState (struct)
File: `firmware-v3/src/audio/contracts/ControlBus.h`

### Members
- `rootNote` — `uint8_t` (0-11, C=0..B=11)
- `type` — `ChordType`
- `confidence` — `float` (0..1 triad energy ratio)
- `rootStrength` — `float`
- `thirdStrength` — `float`
- `fifthStrength` — `float`

---

## lightwaveos::audio::AudioEventQ15 (struct)
File: `firmware-v3/src/audio/contracts/ControlBus.h`

### Members
- `strength` — `uint16_t` (Q15 0..65535)
- `confidence` — `uint16_t`
- `ageMs` — `uint16_t` (saturated)
- `flags` — `uint16_t` (bit0 active, bit1 valid, bit2 degraded, bit3 clipped-source)

---

## lightwaveos::audio::ControlBusRawInput (struct)
File: `firmware-v3/src/audio/contracts/ControlBus.h`

### Constants
- `BINS_64_COUNT = 64` (`static constexpr uint8_t`)
- `BINS_256_COUNT = 256` (`static constexpr uint16_t`)
- `STM_MEL_BANDS = 16` (`static constexpr uint8_t`)
- `STM_SPECTRAL_BINS = 42` (`static constexpr uint8_t`)

### Members
- `rms`, `rmsUngated`, `flux` — `float`
- `bands[CONTROLBUS_NUM_BANDS]` — `float[8]`
- `chroma[CONTROLBUS_NUM_CHROMA]` — `float[12]`
- `waveform[CONTROLBUS_WAVEFORM_N]` — `int16_t[128]`
- `snareEnergy`, `hihatEnergy` — `float`
- `snareTrigger`, `hihatTrigger` — `bool`
- `onsetFlux`, `onsetEnv`, `onsetEvent`, `onsetBassFlux`, `onsetMidFlux`, `onsetHighFlux` — `float`
- `kickTrigger` — `bool`
- `bins64[BINS_64_COUNT]`, `bins64Adaptive[BINS_64_COUNT]` — `float[64]`
- `bins256[BINS_256_COUNT]` — `float[256]`
- `binHz` — `float`
- `stmTemporal[STM_MEL_BANDS]`, `stmSpectral[STM_SPECTRAL_BINS]` — `float`
- `stmTemporalEnergy`, `stmSpectralEnergy` — `float`
- `stmReady` — `bool`
- `tempoLocked`, `tempoBeatTick`, `tempoDownbeatTick` — `bool`
- `tempoConfidence`, `tempoBpm`, `tempoBeatStrength` — `float`

---

## lightwaveos::audio::ControlBusFrame (struct)
File: `firmware-v3/src/audio/contracts/ControlBus.h`

### Constants
- `BINS_64_COUNT`, `BINS_256_COUNT`, `STM_MEL_BANDS`, `STM_SPECTRAL_BINS` (aliases of `ControlBusRawInput::*`)

### Members (canonical order)
- `t` — `AudioTime`
- `hop_seq` — `uint32_t`
- `rms`, `flux`, `fast_rms`, `fast_flux` — `float`
- `liveliness` — `float`
- `scene` — `SceneParameters` (perceptual translation output)
- `bands[8]`, `chroma[12]`, `heavy_bands[8]`, `heavy_chroma[12]` — `float`
- `waveform[128]` — `int16_t`
- `sb_waveform[128]` — `int16_t` (Sensory Bridge parity)
- `sb_waveform_peak_scaled`, `sb_waveform_peak_scaled_last` — `float`
- `sb_note_chromagram[12]`, `sb_chromagram_smooth[12]` — `float`
- `sb_chromagram_max_val`, `sb_hue_position`, `sb_hue_shifting_mix` — `float`
- `sb_spectrogram[64]`, `sb_spectrogram_smooth[64]` — `float`
- `chordState` — `ChordState`
- `saliency` — `MusicalSaliencyFrame`
- `currentStyle` — `MusicStyle`
- `styleConfidence` — `float`
- `snareEnergy`, `hihatEnergy` — `float`
- `snareTrigger`, `hihatTrigger` — `bool`
- `onsetFlux`, `onsetEnv`, `onsetEvent`, `onsetBassFlux`, `onsetMidFlux`, `onsetHighFlux` — `float`
- `kickTrigger` — `bool`
- `onsetProcessUs` — `uint16_t`
- (FEATURE_AUDIO_HF_SEMANTICS) `hfEnergy`, `hfFlux`, `cymbalSustain`, `airEnergy`, `spectralBrightness`, `spectralBrightnessDelta` — `float`
- (FEATURE_AUDIO_HF_SEMANTICS) `hatEvent` — `AudioEventQ15`
- `bins64[64]`, `bins64Adaptive[64]`, `bins256[256]` — `float`
- `binHz` — `float`
- `stmTemporal[16]`, `stmSpectral[42]`, `stmTemporalEnergy`, `stmSpectralEnergy` — `float`
- `stmReady` — `bool`
- `tempoLocked`, `tempoBeatTick`, `tempoDownbeatTick` — `bool`
- `tempoConfidence`, `tempoBpm`, `tempoBeatStrength` — `float`
- `es_bpm`, `es_tempo_confidence`, `es_beat_strength`, `es_phase01_at_audio_t` — `float`
- `es_beat_tick`, `es_downbeat_tick` — `bool`
- `es_beat_in_bar` — `uint8_t`
- `es_vu_level_raw` — `float`
- `es_bins64_raw[64]`, `es_chroma_raw[12]` — `float`
- `silentScale`, `audioConfidence`, `spectralNovelty` — `float`
- `isSilent` — `bool`
- `timing_jitter`, `syncopation_level`, `pitch_contour_dir` — `float`

---

## lightwaveos::audio::LookaheadBuffer (struct)
File: `firmware-v3/src/audio/contracts/ControlBus.h`

### Members
- `history[LOOKAHEAD_FRAMES][LOOKAHEAD_MAX_BANDS]` — `float`
- `current_frame`, `num_bands`, `frames_filled` — `size_t`
- `enabled` — `bool`

### Public methods (inline)
- `void init(size_t bands)`
- `void reset()`

---

## lightwaveos::audio::ZoneAGC (struct)
File: `firmware-v3/src/audio/contracts/ControlBus.h`

### Members
- `max_mag`, `max_mag_follower`, `attack_rate`, `release_rate`, `min_floor` — `float`

### Public methods (inline)
- `void reset()`

---

## lightwaveos::audio::SpikeDetectionStats (struct)
File: `firmware-v3/src/audio/contracts/ControlBus.h`

### Members
- `totalFrames`, `spikesDetectedBands`, `spikesDetectedChroma`, `spikesCorrected` — `uint32_t`
- `totalEnergyRemoved`, `avgSpikesPerFrame`, `avgCorrectionMagnitude` — `float`

### Public methods
- `void reset()`

---

## lightwaveos::audio::ControlBus (class)
File: `firmware-v3/src/audio/contracts/ControlBus.h` (impl `.cpp`)

### Public API
- `ControlBus()` ctor
- `void Reset()`
- `void UpdateFromHop(const AudioTime& now, const ControlBusRawInput& raw)`
- `const ControlBusFrame& GetFrameRef() const`
- `ControlBusFrame GetFrame() const`
- `void setSmoothing(float alphaFast, float alphaSlow)`
- `void setAttackRelease(float bandAttack, float bandRelease, float heavyBandAttack, float heavyBandRelease)`
- `float getAlphaFast() const`
- `float getAlphaSlow() const`
- `void setMoodSmoothing(uint8_t mood)`
- `uint8_t getMood() const`
- `void setLookaheadEnabled(bool enabled)`
- `bool getLookaheadEnabled() const`
- `void setZoneAGCEnabled(bool enabled)`
- `bool getZoneAGCEnabled() const`
- `void setZoneAGCRates(float attack, float release)`
- `void setZoneMinFloor(float floor)`
- `void setChromaZoneAGCEnabled(bool enabled)`
- `bool getChromaZoneAGCEnabled() const`
- `void setChromaZoneAGCRates(float attack, float release)`
- `void setBenchAudioToggles(bool lookahead, bool zoneAgc, bool chromaZoneAgc)`
- `float getZoneFollower(uint8_t zone) const`
- `float getZoneMaxMag(uint8_t zone) const`
- `float getChromaZoneFollower(uint8_t zone) const`
- `float getChromaZoneMaxMag(uint8_t zone) const`
- `const SpikeDetectionStats& getSpikeStats() const`
- `void resetSpikeStats()`
- `void setChordDetectionEnabled(bool enabled)`
- `bool getChordDetectionEnabled() const`
- `const ChordState& getChordState() const`
- `void setSilenceParameters(float threshold, float hysteresisMs)`
- `float getSilenceThreshold() const`
- `float getSilenceHysteresisMs() const`
- `bool isSilenceEnabled() const`
- `void applyDerivedFeatures(ControlBusFrame& frame, float dt, float rmsUngated)`
- `void applyStmSmoothing(ControlBusFrame& frame)`

### Private methods
- `void updateTimingJitter(uint32_t now_ms, bool onsetDetected)` (inline)
- `void updateSyncopation(float beatPhase, bool onsetDetected)` (inline)
- `void updatePitchContour(float centroid, float dt)` (inline)
- `void detectAndRemoveSpikes(LookaheadBuffer& buffer, const float* input, float* output, size_t num_bands, bool isBands, bool benchEnabled)`
- `void detectChord(const float* chroma, ChordState& outChord)`
- `void computeSaliency(ControlBusFrame& frame)`

### Members (fields)
- `m_frame` — `ControlBusFrame`
- `m_liveliness_s` — `float`
- `m_last_time` — `AudioTime`
- `m_time_valid` — `bool`
- `m_rms_s`, `m_flux_s` — `float`
- `m_bands_s[8]`, `m_chroma_s[12]`, `m_heavy_bands_s[8]`, `m_heavy_chroma_s[12]` — `float`
- `m_stm_temporal_s[16]`, `m_stm_spectral_s[42]` — `float`
- `m_stm_temporal_energy_s`, `m_stm_spectral_energy_s` — `float`
- `m_lookahead_bands`, `m_lookahead_chroma` — `LookaheadBuffer`
- `m_bands_despiked[8]`, `m_chroma_despiked[12]` — `float`
- `m_alpha_fast`, `m_alpha_slow` — `float`
- `m_band_attack`, `m_band_release`, `m_heavy_band_attack`, `m_heavy_band_release` — `float`
- `m_zone_agc_enabled`, `m_chroma_zone_agc_enabled` — `bool`
- `m_zones[CONTROLBUS_NUM_ZONES]`, `m_chroma_zones[CONTROLBUS_NUM_ZONES]` — `ZoneAGC`
- `m_bench_lookahead_enabled`, `m_bench_zone_agc_enabled`, `m_bench_chroma_zone_agc_enabled` — `bool`
- `m_spikeStats` — `SpikeDetectionStats`
- `m_chord_detection_enabled` — `bool`
- `m_saliencyTuning` — `SaliencyTuning`
- `m_mood` — `uint8_t`
- `m_silent_scale_smoothed` — `float`
- `m_silence_start_ms` — `uint32_t`
- `m_silence_triggered` — `bool`
- `m_silence_threshold`, `m_silence_hysteresis_ms` — `float`
- `m_clamped_bands[8]`, `m_clamped_chroma[12]` — `float`
- `m_ioi_buffer[JITTER_WINDOW]` — `float[16]`
- `m_last_onset_ms` — `uint32_t`
- `m_ioi_head`, `m_ioi_count` — `uint8_t`
- `m_syncopation_ema` — `float`
- `m_prev_centroid`, `m_pitch_contour_smooth` — `float`
- (HF semantics) `m_hf_energy_s`, `m_air_energy_s`, `m_cymbal_sustain_s`, `m_prev_hf_energy`, `m_prev_spectral_brightness` — `float`
- (HF semantics) `m_hat_event_age_ms` — `uint16_t`

### Constants
- `JITTER_WINDOW = 16`, `SYNCOPATION_EMA_ALPHA = 0.15f` (private `static constexpr`)
- (Namespace-scope) `CONTROLBUS_NUM_BANDS = 8`, `CONTROLBUS_NUM_CHROMA = 12`, `CONTROLBUS_WAVEFORM_N = 128`, `LOOKAHEAD_FRAMES = 3`, `LOOKAHEAD_MAX_BANDS = 64`, `CONTROLBUS_NUM_ZONES = 4`

### Notable parameter-name patterns observed
- `now`, `raw`, `frame`, `dt`, `rmsUngated`, `alphaFast`, `alphaSlow`, `bandAttack`, `bandRelease`, `heavyBandAttack`, `heavyBandRelease`, `mood`, `enabled`, `attack`, `release`, `floor`, `lookahead`, `zoneAgc`, `chromaZoneAgc`, `zone`, `threshold`, `hysteresisMs`, `now_ms`, `onsetDetected`, `beatPhase`, `centroid`, `buffer`, `input`, `output`, `num_bands`, `isBands`, `benchEnabled`, `chroma`, `outChord`

---

## lightwaveos::audio::MusicalGridSnapshot (struct)
File: `firmware-v3/src/audio/contracts/MusicalGrid.h`

### Members
- `t` — `AudioTime`
- `bpm_smoothed`, `tempo_confidence`, `beat_phase01`, `bar_phase01` — `float`
- `beat_tick`, `downbeat_tick` — `bool`
- `beat_index`, `bar_index` — `uint64_t`
- `beats_per_bar`, `beat_unit`, `beat_in_bar` — `uint8_t`
- `beat_strength` — `float`

---

## lightwaveos::audio::MusicalGridTuning (struct)
File: `firmware-v3/src/audio/contracts/MusicalGrid.h`

### Members
- `bpmMin`, `bpmMax`, `bpmTau`, `confidenceTau`, `phaseCorrectionGain`, `barCorrectionGain` — `float`

---

## lightwaveos::audio::MusicalGrid (class)
File: `firmware-v3/src/audio/contracts/MusicalGrid.h` / `.cpp`

### Public API
- `MusicalGrid()` ctor
- `void Reset()`
- `void SetTimeSignature(uint8_t beats_per_bar, uint8_t beat_unit)`
- `void setTuning(const MusicalGridTuning& tuning)`
- `MusicalGridTuning getTuning() const`
- `void OnTempoEstimate(const AudioTime& t, float bpm, float confidence01)`
- `void OnBeatObservation(const AudioTime& t, float strength01, bool is_downbeat)`
- `void updateFromK1(float bpm, float confidence, bool is_locked)`
- `void onK1Beat(int beat_in_bar, bool is_downbeat, float strength)`
- `void injectExternalBeat(float bpm, float phase01, bool isTick, bool isDownbeat, int beatInBar)`
- `void setExternalSyncMode(bool enabled)`
- `bool isExternalSyncMode() const`
- `void Tick(const AudioTime& render_now)`
- `uint32_t ReadLatest(MusicalGridSnapshot& out) const`

### Private static helpers
- `static float clamp01(float x)`
- `static float wrapHalf(float phase01)`

### Members (fields)
- `m_snap` — `SnapshotBuffer<MusicalGridSnapshot>`
- `m_has_tick` — `bool`
- `m_last_tick_t` — `AudioTime`
- `m_bpm_target`, `m_bpm_smoothed`, `m_conf` — `float`
- `m_beat_float` — `double`
- `m_prev_beat_index` — `uint64_t`
- `m_beats_per_bar`, `m_beat_unit` — `uint8_t`
- `m_pending_beat` — `bool`
- `m_pending_beat_t` — `AudioTime`
- `m_pending_strength` — `float`
- `m_pending_is_downbeat` — `bool`
- `m_lastBeatStrength` — `float`
- `m_tuning` — `MusicalGridTuning`
- `m_externalSyncMode`, `m_externalBeatTick`, `m_externalDownbeatTick` — `bool`
- `m_externalBpm`, `m_externalPhase01` — `float`
- `m_externalBeatInBar` — `int`

### Free function (translation unit-local)
- `static inline double fract(double x)` (anonymous-static, MusicalGrid.cpp)

### Notable parameter-name patterns
- `beats_per_bar`, `beat_unit`, `tuning`, `t`, `bpm`, `confidence01`, `strength01`, `is_downbeat`, `confidence`, `is_locked`, `beat_in_bar`, `strength`, `phase01`, `isTick`, `isDownbeat`, `beatInBar`, `enabled`, `render_now`, `out`, `x`

---

## lightwaveos::audio::OnsetSemanticChannelIndex (enum class)
File: `firmware-v3/src/audio/contracts/OnsetSemantics.h`

### Enum values
- `Beat = 0`, `Downbeat = 1`, `Transient = 2`, `Kick = 3`, `Snare = 4`, `Hihat = 5`, `Count = 6`

---

## lightwaveos::audio::OnsetSemanticTrackerEntry (struct)
File: `firmware-v3/src/audio/contracts/OnsetSemantics.h`

### Members
- `lastFireMs`, `previousFireMs`, `sequence` — `uint32_t`
- `heldLevel01` — `float`

---

## lightwaveos::audio::OnsetSemanticTrackerState (struct)
File: `firmware-v3/src/audio/contracts/OnsetSemantics.h`

### Members
- `channels[Count]` — `OnsetSemanticTrackerEntry[6]`

---

## lightwaveos::audio::OnsetSemanticInputs (struct)
File: `firmware-v3/src/audio/contracts/OnsetSemantics.h`

### Members
- `controlBus` — `const ControlBusFrame&`
- `musicalGrid` — `const MusicalGridSnapshot&`
- `audioAvailable`, `trinityActive` — `bool`
- `nowMs` — `uint32_t`
- `dtSeconds` — `float`

### Free functions (namespace-level)
- `void resetOnsetSemanticTracker(OnsetSemanticTrackerState& state)`
- `void updateOnsetContext(const OnsetSemanticInputs& inputs, OnsetSemanticTrackerState& state, plugins::OnsetContext& out)`

### Free functions (translation unit-local, anonymous namespace, OnsetSemantics.cpp)
- `static float clampUnit(float value)`
- `static void populateChannel(plugins::OnsetChannel& channel, OnsetSemanticTrackerState& state, OnsetSemanticChannelIndex index, bool fired, float strength01, float level01, bool reliable, uint32_t nowMs)`

### Notable parameter-name patterns
- `inputs`, `state`, `out`, `value`, `channel`, `index`, `fired`, `strength01`, `level01`, `reliable`, `nowMs`, `tracker`, `heldLevel`, `candidate01`

---

## lightwaveos::audio MotionSemantics constants
File: `firmware-v3/src/audio/contracts/MotionSemantics.h`

### Constants (namespace-scope, `static constexpr uint8_t`)
- `MSEM_CONF_HIGH = 255`, `MSEM_CONF_MEDIUM = 128`, `MSEM_CONF_LOW = 64`
- `MSEM_BIT_WEIGHT = 0`, `MSEM_BIT_TIME = 1`, `MSEM_BIT_SPACE = 2`, `MSEM_BIT_FLOW = 3`, `MSEM_BIT_FLUIDITY = 4`, `MSEM_BIT_IMPULSE = 5`
- Macro: `MOTIONSEMANTICS_FLUIDITY_DEFAULT = 0.5f` (preprocessor)
- Feature macro: `CONTROLBUS_HAS_TIMING_JITTER` (gating)

---

## lightwaveos::audio::MotionSemanticFrame (struct)
File: `firmware-v3/src/audio/contracts/MotionSemantics.h`

### Members
- `weight`, `time_quality`, `space`, `flow` — `float` (Laban Effort 4D)
- `fluidity`, `impulse_strength` — `float`
- `inferred_mask` — `uint8_t` (bitmask)
- `confidence_min` — `uint8_t`

---

## lightwaveos::audio::MotionAuthorOverrides (struct)
File: `firmware-v3/src/audio/contracts/MotionSemantics.h`

### Members (all `float`, NAN sentinel for "no override")
- `weight`, `time_quality`, `space`, `flow`, `fluidity`, `impulse_strength`

### Public methods
- `bool hasOverride(uint8_t bit) const`
- `void clear()`

---

## lightwaveos::audio::MotionSemanticEngine (class)
File: `firmware-v3/src/audio/contracts/MotionSemantics.h` (header-only)

### Public API
- `MotionSemanticEngine()` ctor (initialises 6 `AsymmetricFollower` members)
- `void update(const ControlBusFrame& bus, float dt, const MotionAuthorOverrides* overrides = nullptr)`
- `const MotionSemanticFrame& frame() const`

### Private static helpers
- `static float clampf(float v, float lo, float hi)`
- `static void applyOverride(const MotionAuthorOverrides* overrides, float& raw, uint8_t bit, uint8_t conf, uint8_t& mask, uint8_t& conf_min)`
- `static float getOverrideValue(const MotionAuthorOverrides* o, uint8_t bit)`

### Members (fields)
- `m_frame` — `MotionSemanticFrame`
- `m_smooth_weight`, `m_smooth_time_quality`, `m_smooth_space`, `m_smooth_flow`, `m_smooth_fluidity`, `m_smooth_impulse` — `lightwaveos::effects::enhancement::AsymmetricFollower`

### Notable parameter-name patterns
- `bus`, `dt`, `overrides`, `raw`, `bit`, `conf`, `mask`, `conf_min`, `o`, `v`, `lo`, `hi`

---

## lightwaveos::audio::MotionShaping (struct)
File: `firmware-v3/src/audio/contracts/MotionShaper.h`

### Members
- `intensity`, `decayMs`, `accentScale` — `float`
- `envType` — `uint8_t` (0=none, 1=impact, 2=standard, 3=dramatic)
- `active` — `bool`

---

## lightwaveos::audio::MotionShaper (class)
File: `firmware-v3/src/audio/contracts/MotionShaper.h` (header-only)

### Public API
- `void update(const ControlBusFrame& bus, const MotionSemanticFrame& motion, uint32_t now_ms)`
- `const MotionShaping& shaping() const`

### Private methods
- `void selectAndTrigger(const MotionSemanticFrame& motion, float syncopation, uint32_t now_ms)`

### Members (fields)
- `m_out` — `MotionShaping`
- `m_envelope` — `effects::enhancement::TemporalEnvelope`
- `m_lastHopSeq`, `m_lastTriggerMs` — `uint32_t`
- `m_prevOnset` — `bool`

### Notable parameter-name patterns
- `bus`, `motion`, `now_ms`, `syncopation`, `newHop`, `onset`, `risingEdge`, `rawEnv`

---

## lightwaveos::audio::SaliencyType (enum class)
File: `firmware-v3/src/audio/contracts/MusicalSaliency.h`

### Enum values
- `HARMONIC = 0`, `RHYTHMIC = 1`, `TIMBRAL = 2`, `DYNAMIC = 3`

---

## lightwaveos::audio::MusicalSaliencyFrame (struct)
File: `firmware-v3/src/audio/contracts/MusicalSaliency.h`

### Members
- Novelty: `harmonicNovelty`, `rhythmicNovelty`, `timbralNovelty`, `dynamicNovelty` — `float`
- Composite: `overallSaliency` — `float`; `dominantType` — `uint8_t`
- History: `prevChordRoot`, `prevChordType` — `uint8_t`; `prevFlux`, `prevRms` — `float`; `beatIntervalHistory[4]` — `float`; `beatIntervalIdx` — `uint8_t`; `lastBeatTimeMs` — `float`
- Smoothing: `harmonicNoveltySmooth`, `rhythmicNoveltySmooth`, `timbralNoveltySmooth`, `dynamicNoveltySmooth` — `float`

### Public methods (inline)
- `SaliencyType getDominantType() const`
- `bool isSalient(SaliencyType type, float threshold = 0.3f) const`
- `float getNovelty(SaliencyType type) const`

---

## lightwaveos::audio::SaliencyTuning (struct)
File: `firmware-v3/src/audio/contracts/MusicalSaliency.h`

### Members
- Time constants: `harmonicRiseTime`, `harmonicFallTime`, `rhythmicRiseTime`, `rhythmicFallTime`, `timbralRiseTime`, `timbralFallTime`, `dynamicRiseTime`, `dynamicFallTime` — `float`
- Thresholds: `harmonicChangeThreshold`, `fluxDerivativeThreshold`, `rmsDerivativeThreshold`, `beatVarianceThreshold` — `float`
- Weights: `harmonicWeight`, `rhythmicWeight`, `timbralWeight`, `dynamicWeight` — `float`

---

## lightwaveos::audio::MusicStyle (enum class)
File: `firmware-v3/src/audio/contracts/StyleDetector.h`

### Enum values
- `UNKNOWN = 0`, `RHYTHMIC_DRIVEN = 1`, `HARMONIC_DRIVEN = 2`, `MELODIC_DRIVEN = 3`, `TEXTURE_DRIVEN = 4`, `DYNAMIC_DRIVEN = 5`

### Free function (inline namespace-level)
- `inline const char* getMusicStyleName(MusicStyle style)`

---

## lightwaveos::audio::SnapshotBuffer<T> (template class, final)
File: `firmware-v3/src/audio/contracts/SnapshotBuffer.h`

### Public API
- `SnapshotBuffer()` default ctor; copy/move ctors and operators `delete`d
- `void Publish(const T& v)`
- `uint32_t ReadLatest(T& out) const`
- `uint32_t Sequence() const`
- `uint32_t RetryCount() const`
- `const void* StorageAddressForDiagnostics() const`
- `size_t PayloadBytesForDiagnostics() const`
- `size_t ObjectBytesForDiagnostics() const`

### Members (fields)
- `m_buf[2]` — `T` (double-buffered, `alignas(T)`)
- `m_active` — `std::atomic<uint32_t>` (mutable)
- `m_seq` — `std::atomic<uint32_t>` (mutable)
- `m_retryCount` — `std::atomic<uint32_t>` (mutable)

### Notable parameter-name patterns
- `v`, `out`, `cur`, `safe_cur`, `nxt`, `s0`, `s1`, `idx`, `seqChanged`

---

## lightwaveos::audio::InternalSnapshotBufferOwner<T> (template class, final)
File: `firmware-v3/src/audio/contracts/SnapshotBuffer.h`

### Typedefs / using
- `using Buffer = SnapshotBuffer<T>`

### Public API
- `InternalSnapshotBufferOwner()` ctor (allocates via `heap_caps_malloc(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)`)
- `~InternalSnapshotBufferOwner()` dtor
- Copy/move ctors and operators `delete`d
- `bool IsReady() const`
- `Buffer* get()`
- `const Buffer* get() const`
- `Buffer& operator*()` / `const Buffer& operator*() const`
- `Buffer* operator->()` / `const Buffer* operator->() const`

### Members (fields)
- `m_buffer` — `Buffer*`
- `m_storage` — `void*` (or `alignas(Buffer) unsigned char[sizeof(Buffer)]` on NATIVE_BUILD)

---

## lightwaveos::audio::AudioSource (enum class)
File: `firmware-v3/src/audio/contracts/AudioEffectMapping.h`

### Enum values (39 sources + sentinel)
- Energy: `RMS=0`, `FAST_RMS=1`, `FLUX=2`, `FAST_FLUX=3`
- Bands: `BAND_0=4`..`BAND_7=11`
- Aggregates: `BASS=12`, `MID=13`, `TREBLE=14`, `HEAVY_BASS=15`
- Timing: `BEAT_PHASE=16`, `BPM=17`, `TEMPO_CONFIDENCE=18`
- Render-side reuse: `HEAVY_MID=19`, `HEAVY_TREBLE=20`, `AUDIO_CONFIDENCE=21`, `LIVELINESS=22`, `SILENT_SCALE=23`
- Onset semantic: `ONSET_EVENT=24`, `KICK_LEVEL=25`, `SNARE_LEVEL=26`, `HIHAT_LEVEL=27`
- Harmonic: `CHROMA_MAX=28`, `CHORD_CONFIDENCE=29`
- Saliency: `OVERALL_SALIENCY=30`, `HARMONIC_SALIENCY=31`, `RHYTHMIC_SALIENCY=32`, `TIMBRAL_SALIENCY=33`, `DYNAMIC_SALIENCY=34`
- Scene: `BEAT_PULSE=35`, `PHRASE_PROGRESS=36`, `TENSION=37`, `SPECTRAL_BRIGHTNESS=38`
- Sentinel: `NONE=0xFF`

---

## lightwaveos::audio::VisualTarget (enum class)
File: `firmware-v3/src/audio/contracts/AudioEffectMapping.h`

### Enum values
- `BRIGHTNESS=0`, `SPEED=1`, `INTENSITY=2`, `SATURATION=3`, `COMPLEXITY=4`, `VARIATION=5`, `HUE=6`, `NONE=0xFF`

---

## lightwaveos::audio::MappingCurve (enum class)
File: `firmware-v3/src/audio/contracts/AudioEffectMapping.h`

### Enum values
- `LINEAR=0`, `SQUARED=1`, `SQRT=2`, `LOG=3`, `EXP=4`, `INVERTED=5`

---

## lightwaveos::audio::AudioParameterMapping (struct)
File: `firmware-v3/src/audio/contracts/AudioEffectMapping.h`

### Members
- `source` — `AudioSource`
- `target` — `VisualTarget`
- `curve` — `MappingCurve`
- `inputMin`, `inputMax`, `outputMin`, `outputMax`, `smoothingAlpha`, `tauSeconds`, `gain` — `float`
- `enabled`, `additive` — `bool`
- `smoothedValue` — `float` (runtime)

### Public methods (defined in `.cpp`)
- `float applyCurve(float normalizedInput) const`
- `float apply(float rawInput) const`
- `void updateSmoothed(float rawInput, float dtSeconds)`
- `float getSmoothedOutput() const`

---

## lightwaveos::audio::EffectAudioMapping (struct)
File: `firmware-v3/src/audio/contracts/AudioEffectMapping.h`

### Constants
- `MAX_MAPPINGS_PER_EFFECT = 8`
- `VERSION = 1`

### Members
- `version` — `uint8_t`
- `effectId` — `EffectId`
- `globalEnabled` — `bool`
- `mappingCount` — `uint8_t`
- `mappings[MAX_MAPPINGS_PER_EFFECT]` — `AudioParameterMapping[8]`
- `reserved[8]` — `uint8_t`
- `checksum` — `uint32_t`

### Public methods (defined in `.cpp`)
- `void calculateChecksum()`
- `bool isValid() const`
- `const AudioParameterMapping* findMapping(VisualTarget target) const`
- `AudioParameterMapping* findMapping(VisualTarget target)`
- `const AudioParameterMapping* findMappingBySourceTarget(AudioSource source, VisualTarget target) const`
- `AudioParameterMapping* findMappingBySourceTarget(AudioSource source, VisualTarget target)`
- `bool addMapping(const AudioParameterMapping& mapping)`
- `bool removeMapping(VisualTarget target)`
- `void clearMappings()`

---

## lightwaveos::audio::AudioMappingRegistry (class, singleton)
File: `firmware-v3/src/audio/contracts/AudioEffectMapping.h` / `.cpp`

### Constants
- `MAX_EFFECTS = limits::MAX_EFFECTS` (static constexpr)

### Public API
- `static AudioMappingRegistry& instance()`
- `bool begin()`
- `static void setTestAllocator(void* (*allocFn)(size_t count, size_t size))` (NATIVE_BUILD only)
- Copy ctor and assignment `delete`d
- `const EffectAudioMapping* getMapping(EffectId effectId) const`
- `EffectAudioMapping* getMapping(EffectId effectId)`
- `bool setMapping(EffectId effectId, const EffectAudioMapping& config)`
- `void setEffectMappingEnabled(EffectId effectId, bool enabled)`
- `bool hasActiveMappings(EffectId effectId) const`
- `uint16_t getActiveEffectCount() const`
- `uint16_t getTotalMappingCount() const`
- `void applyMappings(EffectId effectId, const ControlBusFrame& bus, const MusicalGridSnapshot& grid, bool audioAvailable, float dtSeconds, uint8_t& brightness, uint8_t& speed, uint8_t& intensity, uint8_t& saturation, uint8_t& complexity, uint8_t& variation, uint8_t& hue)`
- `static const char* getSourceName(AudioSource source)`
- `static const char* getTargetName(VisualTarget target)`
- `static const char* getCurveName(MappingCurve curve)`
- `static float getAudioValue(AudioSource source, const ControlBusFrame& bus, const MusicalGridSnapshot& grid)`
- `static AudioSource parseSource(const char* name)`
- `static VisualTarget parseTarget(const char* name)`
- `static MappingCurve parseCurve(const char* name)`
- `uint32_t getLastApplyMicros() const`
- `uint32_t getApplyCount() const`
- `uint32_t getMaxApplyMicros() const`
- `void resetStats()`

### Private methods
- `AudioMappingRegistry()` default ctor
- `int16_t findSlot(EffectId effectId) const`
- `int16_t findOrClaimSlot(EffectId effectId)`
- `void applySingleMapping(AudioParameterMapping& mapping, float audioValue, float dtSeconds, uint8_t& targetValue, uint8_t minVal, uint8_t maxVal)`

### Members (fields)
- `m_mappings` — `EffectAudioMapping*` (PSRAM-allocated)
- `m_ready`, `m_allocFailureLogged` — `bool`
- `m_applyCount`, `m_lastApplyMicros`, `m_maxApplyMicros` — `uint32_t`
- `m_totalApplyMicros` — `uint64_t`

### Notable parameter-name patterns
- `effectId`, `config`, `bus`, `grid`, `audioAvailable`, `dtSeconds`, `brightness`, `speed`, `intensity`, `saturation`, `complexity`, `variation`, `hue`, `source`, `target`, `curve`, `name`, `mapping`, `audioValue`, `targetValue`, `minVal`, `maxVal`, `normalizedInput`, `rawInput`, `allocFn`

---

## Free functions / namespace-level (roll-up)

- `lightwaveos::audio::AudioTime_SamplesBetween(const AudioTime&, const AudioTime&)`
- `lightwaveos::audio::AudioTime_SecondsBetween(const AudioTime&, const AudioTime&)`
- `lightwaveos::audio::resetOnsetSemanticTracker(OnsetSemanticTrackerState&)`
- `lightwaveos::audio::updateOnsetContext(const OnsetSemanticInputs&, OnsetSemanticTrackerState&, plugins::OnsetContext&)`
- `lightwaveos::audio::getMusicStyleName(MusicStyle)` (inline)
- `lightwaveos::audio::MusicalGrid::clamp01`, `wrapHalf` (private static)
- (Anonymous-namespace, `.cpp`-local) `clampUnit`, `populateChannel` (OnsetSemantics.cpp); `fract` (MusicalGrid.cpp)

---

## Notable parameter-name patterns observed (across whole subsystem)

Recurring across the subsystem (rank by frequency):
- Time/cadence: `dt`, `dtSeconds`, `now`, `now_ms`, `nowMs`, `render_now`, `t`, `monotonic_us`, `sample_index`, `sample_rate_hz`
- Audio signal scalars: `bpm`, `confidence`, `confidence01`, `strength`, `strength01`, `level01`, `phase01`, `beatPhase`, `centroid`, `rms`, `rmsUngated`, `flux`
- Frame/state passing: `frame`, `bus`, `grid`, `raw`, `out`, `state`, `inputs`, `tuning`, `overrides`, `motion`, `tracker`
- Flags: `enabled`, `isTick`, `isDownbeat`, `is_locked`, `is_downbeat`, `audioAvailable`, `trinityActive`, `reliable`, `fired`, `onsetDetected`
- Identifiers: `effectId`, `source`, `target`, `curve`, `name`, `bit`, `mask`, `index`, `zone`
- Limits/clamps: `lo`, `hi`, `min_floor`, `inputMin`, `inputMax`, `outputMin`, `outputMax`, `threshold`, `hysteresisMs`, `minVal`, `maxVal`
- Tuning gains: `gain`, `alphaFast`, `alphaSlow`, `bandAttack`, `bandRelease`, `tauSeconds`, `attack`, `release`, `mood`
- Numeric/counters: `idx`, `cur`, `nxt`, `s0`, `s1`, `safe_cur`, `num_bands`, `count`

Naming inconsistencies (Captain-flag):
- snake_case vs camelCase parameter style mixes within the same translation unit (e.g. `MusicalGrid.h` declares `beats_per_bar` and `beat_unit` next to `isTick` and `isDownbeat`).
- Method case PascalCase (`Reset`, `Tick`, `Publish`, `ReadLatest`, `UpdateFromHop`, `GetFrame`) coexists with camelCase (`setSmoothing`, `getMood`, `applyDerivedFeatures`, `updateFromK1`).
- Field case mismatch inside `ControlBusFrame`: `hop_seq`, `fast_rms`, `es_bpm`, `es_beat_tick`, `timing_jitter` (snake) versus `tempoLocked`, `tempoBpm`, `kickTrigger`, `audioConfidence`, `silentScale`, `binHz`, `bins64Adaptive`, `chordState` (camel).
- Suffix `01` denotes "value in [0,1)" range — used inconsistently (`beat_phase01`, `bar_phase01`, `phase01`, `confidence01`, `strength01`, `level01`, `heldLevel01`) but not applied to e.g. `tempoConfidence`, `audioConfidence`, `silentScale`, `intensity`.
- `m_` prefix is universal for members, but suffix `_s` (smoothed) is used in some places (`m_rms_s`, `m_flux_s`, `m_bands_s`) and `Smooth`/`smoothed` camel suffix elsewhere (`harmonicNoveltySmooth`, `smoothedValue`, `m_pitch_contour_smooth`, `m_silent_scale_smoothed`).
- Constants split between `static constexpr` PascalCase-with-underscores (none here), `SCREAMING_SNAKE_CASE` (`CONTROLBUS_NUM_BANDS`, `MAX_MAPPINGS_PER_EFFECT`, `JITTER_WINDOW`, `MSEM_*`), and macro defines (`MOTIONSEMANTICS_FLUIDITY_DEFAULT`, `CONTROLBUS_HAS_TIMING_JITTER`).
- No `k`-prefix convention is used anywhere in this subsystem (relevant for SynqMatrix style alignment).

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-13 | agent:subagent (audio-contracts-extractor) | Created exhaustive symbol inventory for audio contracts subsystem under SynqMatrix migration naming review. |
