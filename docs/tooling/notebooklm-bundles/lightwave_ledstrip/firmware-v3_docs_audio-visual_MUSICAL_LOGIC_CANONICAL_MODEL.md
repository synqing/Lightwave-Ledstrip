---
abstract: "Descriptive map of K1's audio pipeline as currently implemented in firmware-v3/. Documents the audio chain, ESV11 backend output (Goertzel bank, chromagram, waveform, tempo), linear-FFT path, complete ControlBusFrame field inventory with each field's populator and derivation, EffectContext consumer surface with observed call-site counts, and the meaning of K1 audio terminology defined by pointing at the source location where each term lives. Single source of truth for what the audio pipeline produces today. Pure observation, no external framework, no prescription."
---

# Musical Logic Canonical Model

A descriptive map of K1's audio pipeline as currently implemented in `firmware-v3/`. The terms below mean what they point at in source. The numbers below are what the source produces.

---

## 1. Audio chain

```
Microphone
  → I2S DMA (Core 0)
  → AudioActor::audioTask  (firmware-v3/src/audio/AudioActor.cpp)
  → EsV11Backend::readAndProcessChunk  (vendored DSP loop)
  → EsV11Adapter::buildFrame           (vendor → ControlBusFrame)
  → AudioActor post-stages             (linear FFT, OnsetDetector, BR detector, derived features, scene translation)
  → ControlBus::Publish                (snapshot buffer)
  → RendererActor (Core 1) reads snapshot
  → effects via EffectContext::AudioContext
  → FastLED → RMT → WS2812
```

The canonical build path is selected at compile time by `FEATURE_AUDIO_BACKEND_ESV11_32KHZ` (set by the K1 V2 production env). This forces `FEATURE_AUDIO_BACKEND_ESV11=1`. The mutually exclusive `FEATURE_AUDIO_BACKEND_PIPELINECORE` branch is not compiled in canonical builds; its populators (`ControlBus::UpdateFromHop`, parts of `applyDerivedFeatures`) do not run on the canonical path.

Source: `firmware-v3/src/audio/AudioActor.cpp`, `firmware-v3/src/audio/backends/esv11/`, `firmware-v3/src/audio/contracts/ControlBus.h`, `firmware-v3/src/plugins/api/EffectContext.h`, `firmware-v3/src/config/features.h`.

---

## 2. ESV11 backend output

The ESV11 backend is vendored from Emotiscope v1.1_320 in `firmware-v3/src/audio/backends/esv11/vendor/`. The vendor identifier is verbatim in the source headers (`vendor/goertzel.h`, `vendor/global_defines.h`).

### 2.1 Goertzel detector bank

- 64 detectors. (`NUM_FREQS = 64` in `vendor/global_defines.h`.)
- Each detector targets a specific frequency from a 192-entry master table (`vendor/goertzel.h`).
- The master table holds 24 entries per octave — quarter-tone spacing, 55 Hz at index 24, doubling every 24 entries.
- The Goertzel iterator picks every 2nd entry: `note = BOTTOM_NOTE + i*NOTE_STEP` with `BOTTOM_NOTE = 12` and `NOTE_STEP = 2`. Picking every 2nd entry of a quarter-tone table yields one detector per semitone.
- Lowest detector frequency: ≈77.78 Hz (master-table index 12).
- Highest detector frequency: ≈2876.89 Hz (master-table index 138).
- Detector bandwidth: 4 × neighbour-distance in Hz; block size = SAMPLE_RATE / bandwidth.

### 2.2 Chromagram

- 12 elements, one per pitch-class slot.
- Built by modulo-12 sum of detector indices 0..59 — five complete octaves at 12 detectors each.
- Detectors 60..63 contribute to the spectrum but are not folded into the chromagram. (`get_chromagram` in `vendor/goertzel.h`.)

### 2.3 Other ESV11 output

- Waveform window of 128 samples.
- `vu_level` (RMS-like scalar from raw PCM).
- Novelty / flux scalars from the tempo / onset machinery.
- Tempo: BPM (`top_bpm`), beat tick, beat strength, phase (radians), downbeat tick, tempo confidence. (`vendor/tempo.h`.)

### 2.4 ESV11 internal normalisation and smoothing

- Per-frame autoranger inside `vendor/goertzel.h` tracks the smoothed maximum and divides the spectrogram by it; smoothed max is floored at 0.0025.
- Adaptive noise-floor subtraction via slow EMA (`vendor/goertzel.h`).
- Two-stage temporal averaging: ring buffers of 2 then 12 samples produce `spectrogram_smooth[NUM_FREQS]`.

### 2.5 K1 32 kHz shim

`firmware-v3/src/audio/backends/esv11/EsV11_32kHz_Shim.h` overrides:

- `SAMPLE_RATE`: 12 800 → 32 000.
- `CHUNK_SIZE`: 64 → 128.
- `SAMPLE_HISTORY_LENGTH`: 4 096 → 10 240.
- `NOVELTY_LOG_HZ`: 50.

The shim does not override `NUM_FREQS`, `BOTTOM_NOTE`, `NOTE_STEP`, or the chromagram fold span. The Goertzel target frequencies and the structural musical constants are unchanged from the vendor.

Frame rate at 32 kHz canonical: 125 Hz hop publish (2 chunks of 128 samples per hop, 8 ms hop period).

---

## 3. Linear FFT path

Independently of the Goertzel bank, `AudioActor::audioTask` runs a 512-point real FFT over a rolling 512-sample window of the raw PCM history.

- 256 magnitude bins.
- Linear spacing in Hz. At 32 kHz canonical, bin width is 62.5 Hz (`binHz = SAMPLE_RATE / kStmFftSize`, `kStmFftSize = 512`).
- Coverage: 0 Hz to 16 kHz (Nyquist).
- Output array: `bins256[256]` in `ControlBusFrame`.
- The linear-FFT bins do not coincide with the Goertzel detector frequencies.

The FFT also feeds the STM (spectral-temporal modulation) extractor: `stmTemporal[16]` and `stmSpectral[42]`, plus `stmTemporalEnergy`, `stmSpectralEnergy`, `stmReady`.

---

## 4. ControlBusFrame field inventory

`ControlBusFrame` is the snapshot struct effects consume by value via `SnapshotBuffer<ControlBusFrame>`. Defined in `firmware-v3/src/audio/contracts/ControlBus.h`. The struct is bounded by `static_assert(sizeof(ControlBusFrame) <= 5120)`.

Fields are listed by category. For each: name, what it carries, who populates it. All numerical ranges and indices are observed from current source.

### 4.1 Spectrum and chroma

| Field | Carries | Populator |
|---|---|---|
| `bands[8]` | Octave-grouped means; band b = mean of `bins64[b*8 .. b*8+7]` | `EsV11Adapter::buildFrame` |
| `chroma[12]` | ES chromagram, autorange-normalised by max-follower | `EsV11Adapter::buildFrame` |
| `heavy_bands[8]` | Slow EMA of `bands[]` | `EsV11Adapter::buildFrame` |
| `heavy_chroma[12]` | Slow EMA of `chroma[]` | `EsV11Adapter::buildFrame` |
| `bins64[64]` | ES `spectrogram_smooth` scaled by autorange max-follower | `EsV11Adapter::buildFrame` |
| `bins64Adaptive[64]` | Currently written from the same expression as `bins64`; the two arrays carry identical values in the present code path | `EsV11Adapter::buildFrame` |
| `bins256[256]` | 512-point real FFT magnitudes, peak-normalised | `AudioActor` (ESV11 path) |
| `binHz` | `SAMPLE_RATE / 512` (62.5 Hz at 32 kHz) | `AudioActor` |
| `waveform[128]` | Time-domain waveform copy (int16) | `EsV11Adapter::buildFrame` |
| `sb_waveform[128]` | Per-sample time-domain copy plus 4-frame ring history | `EsV11Adapter::buildFrame` |
| `sb_waveform_peak_scaled` | Two-stage peak follower (raw – 750 sweet spot, scaled by SB max-follower) | `EsV11Adapter::buildFrame` |
| `sb_waveform_peak_scaled_last` | Slow EMA of `sb_waveform_peak_scaled` | `EsV11Adapter::buildFrame` |
| `sb_note_chromagram[12]` | Sum of `bins64Adaptive[note + 12*octave]` across 6 octaves, capped at 1.0 | `EsV11Adapter::buildFrame` |
| `sb_chromagram_max_val` | Max over `sb_note_chromagram[]` | `EsV11Adapter::buildFrame` |
| `sb_spectrogram[64]`, `sb_spectrogram_smooth[64]`, `sb_chromagram_smooth[12]`, `sb_hue_position`, `sb_hue_shifting_mix` | Sensory Bridge 4.1.1 sidecar fields for SB-parity rendering | `AudioActor::publishFrame` (SB sidecar helpers) |
| `es_vu_level_raw` | `vu_level` direct from ES (pre-AGC) | `EsV11Adapter::buildFrame` |
| `es_bins64_raw[64]` | ES `spectrogram_smooth` pre-AGC | `EsV11Adapter::buildFrame` |
| `es_chroma_raw[12]` | ES `chromagram` pre-AGC | `EsV11Adapter::buildFrame` |

### 4.2 Tempo and beat

| Field | Carries | Populator |
|---|---|---|
| `tempoLocked` | `es_tempo_confidence > 0.5` | `AudioActor` ES bridge |
| `tempoConfidence` | `es_tempo_confidence` pass-through | `AudioActor` ES bridge |
| `tempoBeatTick` | `es_beat_tick` pass-through | `AudioActor` ES bridge |
| `tempoDownbeatTick` | `es_downbeat_tick` pass-through | `AudioActor` ES bridge |
| `tempoBpm` | `es_bpm` pass-through (default 120.0) | `AudioActor` ES bridge |
| `tempoBeatStrength` | `es_beat_strength` pass-through | `AudioActor` ES bridge |
| `es_bpm`, `es_tempo_confidence`, `es_beat_tick`, `es_beat_strength` | Raw ES tempo fields | `EsV11Adapter::buildFrame` |
| `es_phase01_at_audio_t` | `(phase_radians + π) / 2π`, wrapped to [0,1) | `EsV11Adapter::buildFrame` |
| `es_beat_in_bar` | Modulo-4 counter, increments on each `es_beat_tick` | `EsV11Adapter::buildFrame` |
| `es_downbeat_tick` | `es_beat_tick && (m_beatInBar == 0)` | `EsV11Adapter::buildFrame` |

### 4.3 Transient and onset (FFT-based, OnsetDetector)

| Field | Carries | Populator |
|---|---|---|
| `onsetFlux` | Spectral flux from `OnsetDetector::process` | `AudioActor::audioTask` |
| `onsetEnv` | Thresholded onset envelope | `AudioActor::audioTask` |
| `onsetEvent` | OnsetDetector raw event; subsequently overwritten by the BR detector if any band fires | `AudioActor::audioTask` |
| `onsetBassFlux`, `onsetMidFlux`, `onsetHighFlux` | Per-band spectral flux (low / mid / high) | `AudioActor::audioTask` |
| `onsetProcessUs` | Detector self-time in microseconds | `AudioActor::audioTask` |

### 4.4 Percussion triggers (band-ratio detector)

| Field | Carries | Populator |
|---|---|---|
| `kickTrigger` | Band-ratio detector on `bands[0]+bands[1]` | `AudioActor::audioTask` |
| `snareTrigger` | Band-ratio detector on `bands[2]+bands[3]` | `AudioActor::audioTask` |
| `hihatTrigger` | Band-ratio detector on `bands[5]+bands[6]+bands[7]` | `AudioActor::audioTask` |
| `snareEnergy` | `clamp01(sum(bins64[5..10]) / 6)` | `EsV11Adapter::buildFrame` |
| `hihatEnergy` | `clamp01(sum(bins64[50..60]) / 11)` | `EsV11Adapter::buildFrame` |

The ESV11 vendor's onset-derived snare and hihat triggers are present in source but hard-disabled in `EsV11Adapter`; the band-ratio detector in `AudioActor` is the live source. FFT-onset triggers feed `onsetEvent` for telemetry but do not drive percussion booleans.

### 4.5 Semantic high-level fields

Gated by `FEATURE_AUDIO_HF_SEMANTICS`, which follows `FEATURE_AUDIO_SYNC` (= 1 in production envs). Primary populator is `EsV11Adapter::buildFrame`; a fallback in `ControlBus::applyDerivedFeatures` exists but does not run on the canonical path because the EsV11Adapter writes first and sets a populated flag.

| Field | Carries | Source indices |
|---|---|---|
| `hfEnergy` | EMA of `clamp01(sum(bins64Adaptive[50..63]) / 14)` | bins50..63 ≈1865 Hz – 2877 Hz |
| `hfFlux` | `clamp01((hfRaw - prevHfRaw) * 4)` | derived from `hfEnergy` raw |
| `hatEvent` (`AudioEventQ15` — strength, confidence, ageMs, flags) | Refractory-gated event: fires when (refractoryDone[~70 ms] AND `hfFlux > 0.16` AND `hfRatio > 0.85` AND `hfRaw > 0.08`) | derived from HF chain |
| `cymbalSustain` | EMA of `(hfRaw*0.65 + airRaw*0.35)`, slow attack/release | derived from HF + air |
| `airEnergy` | EMA of `clamp01(sum(bins64Adaptive[58..63]) / 6)` | bins58..63 ≈2715 Hz – 2877 Hz |
| `spectralBrightness` | Weighted centroid `weighted/(energy*63)` over `bins64Adaptive[]` | derived from full bins64Adaptive |
| `spectralBrightnessDelta` | Per-frame change in `spectralBrightness`, clamped to [-1, 1] | derived from spectralBrightness |

### 4.6 Backend-agnostic derived features

| Field | Carries | Populator |
|---|---|---|
| `chordState` (root, type, confidence, root/third/fifth strengths) | Triad detection from `chroma[]`: root = dominant pitch class, third = +3 or +4 semitones, fifth = +6/+7/+8 | `ControlBus::detectChord` |
| `liveliness` | EMA of `clamp01(tempoConfidence*0.6 + fast_flux*0.4)` | `ControlBus::applyDerivedFeatures` |
| `saliency` (`MusicalSaliencyFrame`) | Harmonic, rhythmic, timbral, dynamic novelty (raw + smooth) from chord, flux derivatives, RMS deltas | `ControlBus::computeSaliency` (when `FEATURE_MUSICAL_SALIENCY`, default 1) |
| `currentStyle`, `styleConfidence` | Music-style estimate from `m_styleDetector` | `AudioActor::publishFrame` (when `FEATURE_STYLE_DETECTION`) |
| `silentScale`, `isSilent` | Hysteresis on `clamp01(rmsUngated) < silenceThreshold`; once `silence_hysteresis_ms` exceeded, fade to 0 with EMA τ ≈ 0.19 s; instant snap to 1.0 on first non-silent frame | `ControlBus::applyDerivedFeatures` |
| `audioConfidence` | Instant attack to 1.0 when (`rawHopRms > 0.004` AND `spectralNovelty > 0.01`); 37-frame hold; exponential release α = 0.005 | `AudioActor::audioTask` |
| `spectralNovelty` | Sum of `|bands[i] – prevBands[i]|` frame-to-frame | `AudioActor::audioTask` |
| `timing_jitter` | Coefficient of variation of inter-onset intervals | `ControlBus::updateTimingJitter` |
| `syncopation_level` | Onset-to-beat-phase deviation; uses `frame.es_phase01_at_audio_t` | `ControlBus::updateSyncopation` |
| `pitch_contour_dir` | Smoothed delta of approximate spectral centroid (8-band weighted average) | `ControlBus::updatePitchContour` |
| `scene` (`SceneParameters`) | Output of TranslationEngine | `AudioActor::audioTask` (when `FEATURE_TRANSLATION_ENGINE`, default = `FEATURE_AUDIO_SYNC`) |
| `t` (`AudioTime`) | End-of-hop timestamp from `(es.sample_index, SAMPLE_RATE, now_us)` | `AudioActor::audioTask` |
| `hop_seq` | Monotonic hop sequence number | `AudioActor::audioTask` |
| `rms`, `flux` | `clamp01(sqrt(es.vu_level)*1.25)` and `clamp01(es.novelty_norm_last)` | `EsV11Adapter::buildFrame` |
| `fast_rms`, `fast_flux` | On the ESV11 path, identical to `rms` / `flux`; on the PipelineCore path these are separately-EMA'd | `EsV11Adapter::buildFrame` |

### 4.7 STM (linear-FFT consumer)

| Field | Carries | Populator |
|---|---|---|
| `stmTemporal[16]` | Temporal modulation per mel band | `AudioActor::audioTask` (`m_stmExtractor.process`) |
| `stmSpectral[42]` | Spectral modulation | `AudioActor::audioTask` (`m_stmExtractor.process`) |
| `stmTemporalEnergy` | Mean of `stmTemporal[]` | `AudioActor::audioTask` |
| `stmSpectralEnergy` | Mean of `stmSpectral[]` | `AudioActor::audioTask` |
| `stmReady` | True once history is warm | `AudioActor::audioTask` |

---

## 5. Effect consumer surface

Effects access audio through `EffectContext::AudioContext`, defined in `firmware-v3/src/plugins/api/EffectContext.h`. Helper accessors and observed call-site counts across `firmware-v3/src/effects/`:

| Accessor | Files using | Notes |
|---|---|---|
| `audio.available` | 108 | Gate. |
| `audio.beatStrength()` | 46 | |
| `audio.rms()` | 45 | |
| `audio.chroma()` (12-pointer iter) | 44 | |
| `audio.hopSequence()` | 36 | |
| `audio.bass()` (bands[0..1] sum) | 28 | |
| `audio.isSnareHit()` | 28 | |
| `audio.isOnBeat()` | 24 | |
| `audio.silentScale()` | 24 | |
| `audio.treble()` (bands[6..7] sum) | 21 | |
| `audio.tempoConfidence()` | 19 | |
| `audio.getHeavyChroma(N)` | 19 | |
| `audio.getBand(N)` | 16 | Literal indices observed: 0, 5, 6, 7. |
| `audio.beatPhase()` | 15 | |
| `audio.isHihatHit()` | 13 | |
| `audio.mid()` | 12 | |
| `audio.flux()` | 11 | |
| `audio.fastFlux()` | 11 | |
| `audio.rootNote()` | 10 | |
| `audio.chordConfidence()` | 10 | |
| `audio.bands()` (8-pointer iter) | 7 | |
| `audio.bins64Adaptive()` (pointer iter) | 4 | LGPSpectrumDetail, LGPSpectrumDetailEnhanced, JuggleEffect, SbRawWaveformScope |
| `audio.bins64()` (pointer iter) | 2 | |
| Direct `controlBus.bins256[]` | 5 | All spectrum / scope reference effects |
| `audio.bin(i)`, `audio.binAdaptive(i)`, `audio.musicalBin(i)`, `audio.musicalRange(lo,hi)` | 0 each | Helpers exist on the surface; no current effect calls them. |
| `audio.hfEnergy()`, `audio.hfFlux()`, `audio.hatEvent()`, `audio.cymbalSustain()`, `audio.airEnergy()`, `audio.brightness()`, `audio.spectralBrightnessDelta()` | 0 each | Helpers exist on the surface; no current effect calls them. |

The five most-consumed audio fields cover the bulk of audio-reactive effects: `rms`, `beatStrength`, `chroma`, the `bass()` convenience, and one onset trigger (`isSnareHit` or `isOnBeat`).

`bins256` is referenced directly by 5 effect files, all of which are spectrum-detail or scope reference effects.

---

## 6. Terminology in K1's source

Each term means what it points at in source.

- **musical bin** — a single Goertzel detector inside the 64-detector bank in `firmware-v3/src/audio/backends/esv11/vendor/goertzel.h`. Indexed 0..63. K1's helper accessors `audio.musicalBin(i)` and `audio.musicalRange(lo, hi)` live on `EffectContext::AudioContext`; both currently have zero effect consumers.
- **bins64 / bins64[64]** — the array in `ControlBusFrame` populated from the 64-Goertzel-detector spectrogram via `EsV11Adapter::buildFrame`. Range ≈78 Hz – ≈2877 Hz, semitone-spaced.
- **bins64Adaptive / bins64Adaptive[64]** — a separately-named array in `ControlBusFrame` currently written from the same expression as `bins64` in `EsV11Adapter::buildFrame`. Identical values in the present code.
- **bins256 / bins256[256]** — the linear-FFT output, 62.5 Hz / bin at 32 kHz, populated by `AudioActor::audioTask`.
- **bands / bands[8]** — octave-grouped means of `bins64`; band b = mean(bins64[b*8 .. b*8+7]).
- **chroma / chroma[12] / chromagram** — the 12-element pitch-class array, populated by `EsV11Adapter::buildFrame` from the ES vendor's modulo-12 fold of the bottom 60 detectors of `spectrogram_smooth`. Cardinality 12; pre-AGC version available as `es_chroma_raw`.
- **heavy_chroma[12], heavy_bands[8]** — slow-EMA-smoothed copies of `chroma` / `bands`.
- **sb_note_chromagram[12]** — a separate octave-folding of `bins64Adaptive` maintained for Sensory Bridge sidecar parity, distinct from `chroma[12]`.
- **hfEnergy, hfFlux, hatEvent, cymbalSustain, airEnergy, spectralBrightness, spectralBrightnessDelta** — scalar / event / envelope fields populated by `EsV11Adapter::buildFrame` under `FEATURE_AUDIO_HF_SEMANTICS`. Each field's source data is `bins64Adaptive[]` indices in the 50..63 range (≈1865 Hz – ≈2877 Hz) plus per-field smoothing as listed in §4.5. Helper accessors exist on `EffectContext::AudioContext`; current effect consumers: zero.
- **stmTemporal[16], stmSpectral[42], stmTemporalEnergy, stmSpectralEnergy, stmReady** — spectral-temporal-modulation fields populated by `AudioActor::audioTask` from `bins256`.
- **kickTrigger, snareTrigger, hihatTrigger** — booleans set by the band-ratio detector in `AudioActor::audioTask` over `bands[]`. The ESV11 vendor's onset-derived equivalents are present in source but disabled in `EsV11Adapter`; the BR detector is the live source.
- **silentScale, isSilent** — hysteresis-gated silence indicators in `ControlBus::applyDerivedFeatures`.
- **audioConfidence** — a "music actively present" scalar in `AudioActor::audioTask`; distinct from `silentScale` in attack and release dynamics.
- **chordState, liveliness, saliency, currentStyle, styleConfidence, timing_jitter, syncopation_level, pitch_contour_dir** — backend-agnostic derived features populated by `ControlBus::applyDerivedFeatures` and `AudioActor`; sources and gates listed in §4.6.
- **scene (SceneParameters)** — structured output of `translation_get_parameters`, populated when `FEATURE_TRANSLATION_ENGINE` is enabled.
- **t (AudioTime)** — end-of-hop timestamp tuple `(sample_index, SAMPLE_RATE, now_us)`.
- **hop_seq** — monotonic hop sequence number; gap-detection performed downstream.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-28 | claude:opus-4-7 | Created. Descriptive map of K1's audio pipeline as currently implemented in `firmware-v3/`: audio chain, ESV11 backend output (Goertzel bank, chromagram, waveform, tempo machinery, K1 32 kHz shim), linear-FFT path, complete `ControlBusFrame` field inventory with each field's populator and derivation, `EffectContext::AudioContext` consumer surface with observed call-site counts, and K1 audio terminology defined by source pointers. Source citations: `firmware-v3/src/audio/AudioActor.cpp`, `firmware-v3/src/audio/backends/esv11/EsV11Adapter.cpp`, `firmware-v3/src/audio/backends/esv11/vendor/goertzel.h`, `firmware-v3/src/audio/backends/esv11/vendor/global_defines.h`, `firmware-v3/src/audio/backends/esv11/EsV11_32kHz_Shim.h`, `firmware-v3/src/audio/contracts/ControlBus.h`, `firmware-v3/src/audio/contracts/ControlBus.cpp`, `firmware-v3/src/plugins/api/EffectContext.h`. |
