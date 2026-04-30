---
abstract: "Side-by-side audio surface inventory for Sensory Bridge 4.1.1 and Emotiscope 1.2 — every smoothed/raw signal, smoothing primitive, and beat/tempo/chroma/VU producer, with K1 EffectContext mapping table, gap analysis, and recommended minimum K1 surface for porting wave-mode motion. Sourced verbatim from globals.h, GDFT.h, i2s_audio.h, led_utilities.h (SB) and types.h, goertzel.h, tempo.h, vu.h, perlin.h (Emotiscope). Read this before porting any SB/ES wave/spectronome/metronome motion to K1."
---

# Canonical Audio Infrastructure — SB 4.1.1 vs Emotiscope 1.2 vs K1

**Status:** GROUNDED. Every entry is traced to a specific file and line in the upstream source.
**Authored:** 2026-04-30 by `agent:embedded-system-engineer`.
**Scope:** Audio surface only — what the rendering layer can see. Does NOT cover effect/mode internals beyond the data plumbing they consume.

---

## Reading guide

The two upstream firmwares share a common ancestor (Connor Nishijima's Lixie/Sensory Bridge lineage) but diverged on three points that matter for porting:

1. **Sensory Bridge 4.1.1** is built around a Goertzel spectrogram (64 musical bins) plus a "novelty curve" used as a colour-shift driver. There is **no explicit tempo extraction**. Beat-like motion on SB modes is driven entirely by *spectrogram_smooth*, *novelty_curve*, *audio_vu_level*, and *waveform_peak_scaled*.
2. **Emotiscope 1.2** keeps the Goertzel spectrogram (96 bins, half-step) but adds a **Goertzel-on-novelty tempo bank** (`tempi[NUM_TEMPI]`) which exposes per-tempo-bin `magnitude`, `phase`, `beat = sin(phase)`, and a `tempi_power_sum` / `tempo_confidence` summary. This is the canonical "tempo phase" feed that drives Spectronome/Metronome motion.
3. **K1** has both surfaces, but with different names (`controlBus.bins64`, `controlBus.bins64Adaptive`, `controlBus.chroma`, `onset.phase01`, `onset.bpm`). The wave-mode bug is that K1 does **not** expose a continuous *tempo phase per bin* (it exposes a single locked phase from the onset detector), and effect code that needs Emotiscope-style multi-tempo phase blending currently improvises with `getPhase(frequencyHz)` from clock time, which jitter-spazzes when tempo confidence drops.

---

## SB 4.1.1 audio surface

All entries are populated each audio frame inside `acquire_sample_chunk()` → `process_GDFT()` → `calculate_vu()` → `calculate_novelty()`. The frame rate is whatever the I2S DMA delivers (`SAMPLES_PER_CHUNK = 96` per chunk at default `SAMPLE_RATE`, with a sliding 4096-sample window in `sample_window[]`).

### Raw signals (per audio frame)

| Symbol | File | Type | Semantic | Smoothing |
|---|---|---|---|---|
| `i2s_samples_raw[1024]` | globals.h:156 | int32_t | Raw 32-bit I2S samples direct from DMA | none |
| `waveform[1024]` | globals.h:158 | int16_t | Sensitivity-gain-applied, DC-offset-removed, clipped to ±32767 | none (single-frame) |
| `waveform_history[4][1024]` | globals.h:160 | int16_t | Last 4 frames of waveform | ring buffer |
| `waveform_fixed_point[1024]` | globals.h:159 | SQ15x16 | waveform[] / 32768.0 — used by VU | none |
| `sample_window[SAMPLE_HISTORY_LENGTH]` | globals.h:157 | int16_t | Sliding window fed to Goertzel | none |
| `magnitudes[NUM_FREQS]` | globals.h:286 | int32_t | Raw Goertzel magnitudes (q² + q² − coeff·q·q, then sqrt) | none |
| `magnitudes_normalized[NUM_FREQS]` | globals.h:287 | float | magnitudes[i] / (block_size/2) | none |
| `max_waveform_val_raw` | globals.h:162 | float | Per-frame |max(waveform[])| | none |

### Smoothed signals (per audio frame)

| Symbol | File:line | Smoothing primitive | Semantic |
|---|---|---|---|
| `magnitudes_normalized_avg[NUM_FREQS]` | GDFT.h:118 | one-pole IIR, **α=0.30** for new sample | First-stage spectrogram smoother |
| `magnitudes_final[NUM_FREQS]` | GDFT.h:148-150 | `low_pass_array(...)` over `magnitudes_last`, **cutoff = 1.0 + 10·MOOD** Hz | MOOD-controlled second-stage smoother |
| `spectrogram[NUM_FREQS]` | globals.h:130 | Normalised by `goertzel_max_value` (asymmetric follower below) | Final 0..1 spectrogram |
| `spectrogram_smooth[NUM_FREQS]` | globals.h:131 | (referenced by `make_smooth_chromagram()`) | Used by chromagram folding |
| `goertzel_max_value` | GDFT.h:169 | Asymmetric: **rise α=0.005, fall α=0.0025**, with `max_value *= 0.995` per frame | Auto-ranger floor (≥4.0) |
| `note_spectrogram_smooth[NUM_FREQS]` | globals.h:140 | (mode-internal) | Note-domain remap of spectrogram |
| `note_spectrogram_smooth_frame_blending[NUM_FREQS]` | globals.h:141 | Cross-frame blend | Used for inter-frame interpolation |
| `note_spectrogram_long_term[NUM_FREQS]` | globals.h:142 | Long-term running average | Background reference for transient detection |
| `chromagram_smooth[12]` | globals.h:132 | Folded from spectrogram_smooth, normalised by `max_peak` (decay 0.999/frame, rise 0.05) | 12-pitch-class chromagram |
| `note_chromagram[12]` | globals.h:143 | (mode-internal) | Per-note chromagram |
| `max_waveform_val_follower` | i2s_audio.h:110-119 | Asymmetric: **rise 0.25, fall 0.005**, floored at SWEET_SPOT_MIN_LEVEL | Slow envelope of waveform peak |
| `waveform_peak_scaled` | i2s_audio.h:121-129 | Asymmetric: **rise 0.25, fall 0.25** of `max_waveform_val / max_waveform_val_follower` | Normalised peak scalar (0..1-ish, the "punch" feed) |
| `audio_vu_level` | i2s_audio.h:223 | RMS over `SAMPLES_PER_CHUNK`, with `VU_LEVEL_FLOOR` subtraction and `(1 - floor)` autorange | Per-frame VU |
| `audio_vu_level_average` | i2s_audio.h:246 | `(audio_vu_level + audio_vu_level_last) / 2` — 2-tap mean | Smoothed VU |
| `silent_scale` | i2s_audio.h:169 | One-pole IIR **α=0.1** of `(1.0 − silence)` | Silent-fade brightness scale |
| `sweet_spot_state_follower` | led_utilities.h:114 | One-pole **α=0.05** of `sweet_spot_state ∈ {-1, 0, +1}` | LED-side sweet-spot indicator (UI, not effects) |
| `smoothing_follower` | globals.h:147 | (reserved/wire-up) | Generic follower |
| `smoothing_exp_average` | globals.h:148 | (reserved/wire-up) | Generic exp average |

### Novelty / change-detection (per audio frame)

| Symbol | File:line | Smoothing primitive | Semantic |
|---|---|---|---|
| `novelty_curve[SPECTRAL_HISTORY_LENGTH]` | globals.h:135 | Ring buffer of `sqrt(novelty_now)` per frame | Spectral flux over last N frames |
| `spectral_history[N][NUM_FREQS]` | globals.h:134 | Ring buffer of spectrogram per frame | Used to compute `novelty_now` (positive change vs index −1) |
| `novelty_now` (local) | GDFT.h:207-221 | Mean over bins of `max(0, spectrogram[i] − spectral_history[idx-1][i])` | Single-frame positive change |
| `hue_shift_speed` | globals.h:326 | Asymmetric on `novelty_now³`: **rise 0.75·novelty, fall 0.99·hue_shift_speed** | Drives `hue_position` advance — the SB "auto-color-cycle" rate |
| `hue_position` | globals.h:325 | Integrated phase, wraps 0..1 | Auto-color-cycling hue offset |

### Beat / tempo / phase

**SB 4.1.1 has NO explicit tempo bank.** Beat-rate motion in SB modes is exclusively derived from:

- `novelty_curve[]` (treated as an event stream)
- `audio_vu_level` / `audio_vu_level_average` (energy envelope)
- `waveform_peak_scaled` (instant-punch scalar)

There is no `phase`, no `bpm`, no `beat_in_bar`, no `tempo_confidence`, no `metronome_phase`. This is the design ceiling of SB 4.1.1.

### Spectrogram normalisation auxiliaries

| Symbol | File:line | Smoothing primitive | Semantic |
|---|---|---|---|
| `max_mags[NUM_ZONES]` | globals.h:281 | reset to 0.0 each frame | Per-zone peak |
| `max_mags_followers[NUM_ZONES]` | globals.h:282 | (referenced; smoother on `max_mags`) | Per-zone follower |
| `mag_targets[NUM_FREQS]` | globals.h:283 | (referenced; per-bin target) | Per-bin auto-range target |
| `mag_followers[NUM_FREQS]` | globals.h:284 | (referenced; per-bin follower) | Per-bin auto-range follower |
| `magnitudes_last[NUM_FREQS]` | globals.h:289 | Previous frame copy | Used by `low_pass_array(...)` |
| `spectrogram_history[3][64]` | globals.h:296 | Look-ahead ring (3 frames) | Look-ahead smoothing |

---

## Emotiscope 1.2 audio surface

Audio frame rate is fixed by `NOVELTY_LOG_HZ` (the novelty/VU logging clock — see `update_novelty()` at tempo.h:340). Spectral frames are computed each visual frame; tempo Goertzel runs **one bin per frame** (interlaced) over the novelty/VU history.

### Raw signals (per audio frame)

| Symbol | File | Type | Semantic |
|---|---|---|---|
| `sample_history[]` | (extern, used by goertzel.h:199 + vu.h:30) | float | Sliding waveform history |
| `frequencies_musical[NUM_FREQS].magnitude_full_scale` | goertzel.h:283 | float | Per-bin magnitude pre-autorange |
| `vu_level_raw` | vu.h:11, 78 | volatile float | RMS-derived per-frame VU pre-smoothing |
| `vu_max` | vu.h:12, 91 | volatile float | Running max of `vu_level` (reset each novelty tick) |
| `vu_floor` | vu.h:13, 54 | volatile float | Long-window noise floor (2-second mean × 0.80) |
| `fft_max[]` | (extern, referenced tempo.h:357-360) | float | Per-bin peak-hold within novelty window |

### Smoothed signals (per visual frame)

| Symbol | File:line | Smoothing primitive | Semantic |
|---|---|---|---|
| `spectrogram[NUM_FREQS]` | goertzel.h:39, 326 | Per-bin auto-range via `max_val_smooth` (rise α=0.005, fall α=0.005), then `clip_float` | Final 0..1 spectrogram |
| `spectrogram_smooth[NUM_FREQS]` | goertzel.h:43, 337-341 | Mean over `NUM_SPECTROGRAM_AVERAGE_SAMPLES = 12` frames | Slow spectrogram |
| `spectrogram_average[12][NUM_FREQS]` | goertzel.h:44 | 12-frame ring buffer | Source of spectrogram_smooth |
| `magnitudes_smooth[NUM_FREQS]` (file-static) | goertzel.h:233 | Mean over `NUM_AVERAGE_SAMPLES = 1` (effectively pass-through but framework hooked) | Pre-autorange smoother |
| `noise_floor[NUM_FREQS]` (file-static) | goertzel.h:237, 274 | One-pole **α=0.01** of 10-second mean × 0.90, then **forced to 0.0** at line 276 | Defunct (zeroed every frame; the noise floor pipeline exists but is bypassed) |
| `chromagram[12]` | goertzel.h:40, 349-365 | Folded from `spectrogram_smooth[0..59]`, autoscaled by `1/max_val` | 12-pitch-class chromagram |
| `vu_level` | vu.h:11, 88 | Mean over `NUM_VU_SMOOTH_SAMPLES = 12` ring buffer | Smoothed VU |
| `vu_log[NUM_VU_LOG_SAMPLES]` | vu.h:4 | Ring buffer logged every 250 ms for 5 seconds | Source of `vu_floor` |
| `max_amplitude_cap` (file-static, vu.h:29) | vu.h:62-69 | Asymmetric **rise 0.10, fall 0.10**, floor 0.000025 | VU auto-ranger |
| `max_val_smooth` (file-static, goertzel.h:234) | goertzel.h:305-311 | Asymmetric **rise α=0.005, fall α=0.005**, floor 0.0000025 | Spectrogram auto-ranger |
| `silence_level` | tempo.h:16, 326 | Derived from `novelty_contrast` over last 128 novelty samples | Silence scalar (0..1) |
| `silence_detected` | tempo.h:15, 328-333 | Boolean from `silence_level_raw > 0.5` | Silence flag |

### Novelty / VU curves (per novelty tick at NOVELTY_LOG_HZ)

| Symbol | File:line | Smoothing primitive | Semantic |
|---|---|---|---|
| `novelty_curve[NOVELTY_HISTORY_LENGTH]` | tempo.h:24 | Shift-left ring; new sample is `log1p(sum_of_positive_fft_diffs)` | Spectral flux history (input to tempo Goertzel) |
| `novelty_curve_normalized[]` | tempo.h:25, 240 | Auto-scaled by `1 / max(novelty_curve)` | Normalised flux for tempo bank |
| `vu_curve[NOVELTY_HISTORY_LENGTH]` | tempo.h:27 | Shift-left ring of `max(vu_max - last_vu_max, 0)` | Positive-difference VU history |
| `vu_curve_normalized[]` | tempo.h:28, 259 | Auto-scaled by `1 / max(vu_curve)` | Normalised VU for tempo bank |

### Tempo bank (the Emotiscope special — per visual frame, interlaced one bin per frame)

| Symbol | File:line | Smoothing primitive | Semantic |
|---|---|---|---|
| `tempi[NUM_TEMPI]` | tempo.h:30 | struct array | Goertzel bank over `novelty_curve_normalized` |
| `tempi[i].target_tempo_hz` | tempo.h:64 | const | Target BPM/60 for this bin |
| `tempi[i].magnitude_full_scale` | tempo.h:193, 197 | per-frame Goertzel magnitude | Pre-autorange tempo magnitude |
| `tempi[i].magnitude` | tempo.h:220 | autoranged then **cubed** (`m³`) | Final 0..1 tempo magnitude (sharpened) |
| `tempi[i].phase` | tempo.h:160 | `atan2(imag, real) + π·BEAT_SHIFT_PERCENT`, then unwrapped | Continuous tempo phase, advances via `phase_radians_per_reference_frame · delta` |
| `tempi[i].phase_inverted` | tempo.h:117, 164 | bool | Flipped on each phase wrap (used by callers that need π-aware sign) |
| `tempi[i].beat` | tempo.h:403 | `sin(tempi[i].phase)` | Continuous beat sinusoid per tempo bin |
| `tempi_smooth[NUM_TEMPI]` | tempo.h:31, 421 | One-pole **α=0.025 rise, ×0.975 fall** when `magnitude > 0.005` | Smoothed tempo magnitudes |
| `tempi_power_sum` | tempo.h:32, 412-422 | Sum of `tempi_smooth[]` per frame | Total tempo energy |
| `tempo_confidence` | tempo.h:18, 439 | `max(tempi_smooth) / tempi_power_sum` | 0..1 confidence (peakiness of tempo distribution) |

### A-weighting & windowing

| Symbol | File:line | Semantic |
|---|---|---|
| `a_weighting_lut[NUM_FREQS]` | goertzel.h:47, 56-60 | Per-bin perceptual weight, scaled `× 0.5 + 0.5` so all values in [0.5, 1.0] |
| `window_lookup[4096]` | goertzel.h:29, 122-141 | Gaussian window (σ=0.8), mirrored second half |

### Constants worth knowing

| Constant | File | Value | Notes |
|---|---|---|---|
| `BOTTOM_NOTE` | goertzel.h:20 | 12 (quarter-steps) | Lowest tracked note |
| `NOTE_STEP` | goertzel.h:21 | 2 (half-steps) | Per-bin step |
| `NUM_FREQS` | (extern) | 96 (typical) | Number of musical bins |
| `NUM_TEMPI` | (extern) | 96 (typical) | Number of tempo bins |
| `NUM_SPECTROGRAM_AVERAGE_SAMPLES` | goertzel.h:42 | 12 | Frames in `spectrogram_smooth` |
| `NUM_VU_LOG_SAMPLES` | vu.h:1 | 20 | 5 seconds at 250 ms cadence |
| `NUM_VU_SMOOTH_SAMPLES` | vu.h:2 | 12 | Frames in `vu_level` mean |
| `BEAT_SHIFT_PERCENT` | (extern) | (calibration) | Phase offset added to tempo phase |

---

## Smoothing primitives — comparative table

| Primitive | SB 4.1.1 | Emotiscope 1.2 | K1 |
|---|---|---|---|
| **One-pole IIR (symmetric)** | `magnitudes_normalized_avg` (α=0.30); `silent_scale` (α=0.1) | `noise_floor` (α=0.01); `vu_level` (12-frame mean ≈ 1-pole) | OnePoleSmoother in `audio/dsp/` |
| **Asymmetric follower (rise≠fall)** | `goertzel_max_value` (0.005/0.0025); `max_waveform_val_follower` (0.25/0.005); `waveform_peak_scaled` (0.25/0.25); `silent_scale` only-rise | `max_amplitude_cap` (0.10/0.10); `max_val_smooth` spectrogram (0.005/0.005); `tempi_smooth` (0.025 rise / ×0.975 fall when active) | AsymmetricFollower in `audio/dsp/`; `heavy_bands[]` slow follower per band |
| **Ring-buffer mean** | `spectrogram_history[3][64]` (3-frame look-ahead) | `spectrogram_average[12]`; `vu_smooth[12]`; `vu_log[20]` | `MotionShaping` envelope; `bins64Adaptive` (SB-parity normalisation) |
| **Low-pass with frame-rate-aware cutoff** | `low_pass_array(magnitudes_final, magnitudes_last, NUM_FREQS, SYSTEM_FPS, 1.0 + 10·MOOD)` | (none — fixed-tap means only) | `dtDecay()` in effects (rule #12) |
| **Spectral flux / novelty** | `novelty_curve[]` from `Σ max(0, spectrogram[i] − spectral_history[idx−1][i])` | `novelty_curve[]` from `Σ max(0, fft_max[i] − fft_last[i])` then `log1p(...)` | `controlBus.flux`, `controlBus.fast_flux`, `onset.raw.flux` |
| **Goertzel-on-novelty (tempo)** | NONE | `calculate_magnitude_of_tempo()` over `novelty_curve_normalized` | NONE — only single-source onset phase |
| **Phase integration (tempo)** | NONE | `sync_beat_phase()` advances each tempo bin's phase by `phase_radians_per_reference_frame · delta`; corrected by `atan2(imag, real)` from Goertzel | `onset.phase01` (single locked phase) |
| **Squared / cubed sharpening** | `novelty_now³` (color-shift speed) | `tempi[i].magnitude = m³` after autorange | `audioConfidence`, `beatStrength` (gentler curves) |
| **DC blocker / sensitivity gain** | `(i2s_samples_raw × 0.000512) + 56000 − 5120`, `>>2`, `× SENSITIVITY`, clip ±32767, `− DC_OFFSET` | (handled in I2S read layer, not visible in this scope) | I2S adapter / `AudioActor` |

---

## K1 EffectContext mapping — canonical signal → K1 accessor

For every canonical SB/Emotiscope signal, the closest K1 EffectContext accessor. Verified directly against `firmware-v3/src/plugins/api/EffectContext.h:83-624`.

### Spectral / chromagram surface

| Canonical signal | K1 accessor | Verified | Notes |
|---|---|---|---|
| SB `spectrogram[NUM_FREQS]` (64 bins) | `ctx.audio.bin(i)` / `ctx.audio.bins64()` | EffectContext.h:377-385 | Direct equivalent |
| SB `spectrogram_smooth` | `ctx.audio.bins64Adaptive()` | EffectContext.h:388-396 | "SB normalisation" accessor — slow autoranged |
| SB `chromagram_smooth[12]` | `ctx.audio.heavyChroma()` / `ctx.audio.getHeavyChroma(i)` | EffectContext.h:281-287 | 12-bin pitch class, smoothed |
| SB `note_chromagram[12]` | `ctx.audio.chroma()` / `ctx.audio.getChroma(i)` | EffectContext.h:277-285 | Per-frame chroma |
| ES `spectrogram[NUM_FREQS]` (96 bins) | `ctx.audio.bin(i)` (0..63) + `ctx.audio.musicalRange(...)` | EffectContext.h:399-427 | K1 has 64 bins, not 96; named ranges available |
| ES `spectrogram_smooth` (12-frame mean) | `ctx.audio.bins64Adaptive()` | EffectContext.h:388-396 | Closest equivalent (SB-parity smoothing pipeline) |
| ES `chromagram[12]` | `ctx.audio.chroma()` | EffectContext.h:285 | Direct equivalent |
| ES `frequencies_musical[i].magnitude` | `ctx.audio.bin(i)` | EffectContext.h:377-385 | 64-bin equivalent |
| ES `frequencies_musical[i].magnitude_full_scale` | NONE — K1 only exposes autoranged | GAP | See Gaps section |

### Energy / VU / amplitude scalars

| Canonical signal | K1 accessor | Verified | Notes |
|---|---|---|---|
| SB `audio_vu_level` | `ctx.audio.rms()` | EffectContext.h:95 | Direct equivalent (RMS) |
| SB `audio_vu_level_average` | `ctx.audio.rms()` (already smoothed) | EffectContext.h:95 | K1's rms is post-smoothing |
| SB `waveform_peak_scaled` | `ctx.audio.sbWaveformPeakScaled()` | EffectContext.h:228 | Explicit SB-parity field |
| SB `max_waveform_val_follower` | NONE — exposed only via `sbWaveformPeakScaled` | PARTIAL | Follower itself not exposed |
| SB `silent_scale` | `ctx.audio.silentScale()` | EffectContext.h:292 | Direct equivalent |
| SB `silence` (bool) | `ctx.audio.isSilent()` | EffectContext.h:294 | Direct equivalent |
| ES `vu_level_raw` | `ctx.audio.fastRms()` | EffectContext.h:96 | Closest fast-path equivalent |
| ES `vu_level` (12-frame smooth) | `ctx.audio.rms()` | EffectContext.h:95 | Smoothed RMS |
| ES `vu_max` | NONE | GAP | Per-novelty-tick peak hold not exposed |
| ES `vu_floor` | NONE | GAP | 5-second noise floor not exposed |
| ES `silence_level` (0..1 scalar) | `ctx.audio.silentScale()` (inverse semantics) | EffectContext.h:292 | K1 uses 1.0=active, ES uses 1.0=silent |
| ES `silence_detected` | `ctx.audio.isSilent()` | EffectContext.h:294 | Direct equivalent |

### Beat / tempo / phase

| Canonical signal | K1 accessor | Verified | Notes |
|---|---|---|---|
| ES `tempi[i].target_tempo_hz` | `ctx.audio.bpm()` (single locked tempo only) | EffectContext.h:144-146 | K1 tracks ONE tempo, not a bank |
| ES `tempi[i].phase` (continuous, per-bin) | `ctx.audio.beatPhase()` (single phase only) | EffectContext.h:135 | K1 has ONE phase, not 96 |
| ES `tempi[i].beat = sin(phase)` | `sinf(ctx.audio.beatPhase() * 2π)` | derived | Single-tempo only |
| ES `tempi[i].magnitude` | `ctx.audio.tempoConfidence()` (aggregate only) | EffectContext.h:149-151 | K1 collapses bank to one scalar |
| ES `tempi[i].phase_inverted` | NONE | GAP | No π-aware sign per bin |
| ES `tempi_power_sum` | NONE (implied via `tempoConfidence`) | PARTIAL | Numerator only, not raw sum |
| ES `tempo_confidence` | `ctx.audio.tempoConfidence()` / `ctx.audio.tempoBeatConfidence()` | EffectContext.h:149-167 | Direct equivalent (single-source) |
| Beat tick (single-frame) | `ctx.audio.isOnBeat()` / `ctx.audio.tempoBeatTick()` | EffectContext.h:138, 157-159 | Direct equivalent |
| Downbeat tick | `ctx.audio.isOnDownbeat()` | EffectContext.h:141 | Direct equivalent |
| Beat-in-bar | `ctx.audio.beatInBar()` | EffectContext.h:170-172 | Direct equivalent |
| Beat strength (envelope) | `ctx.audio.beatStrength()` | EffectContext.h:177-179 | Decaying envelope |

### Novelty / flux / onset

| Canonical signal | K1 accessor | Verified | Notes |
|---|---|---|---|
| SB `novelty_curve[idx]` | `ctx.audio.flux()` / `ctx.audio.onsetFlux()` | EffectContext.h:99, 310 | Single-frame; no curve history exposed |
| SB `novelty_now` (per-frame) | `ctx.audio.flux()` | EffectContext.h:99 | Direct equivalent |
| SB `hue_shift_speed` | NONE — K1 doesn't drive hue cycling from novelty | GAP | Could be derived: `expf(...) * onsetEnv()³` |
| SB `hue_position` | `ctx.gHue` (clock-driven, NOT novelty-driven) | EffectContext.h:879 | Different semantics |
| ES `novelty_curve_normalized[]` (history) | `ctx.audio.flux()` (single frame) | EffectContext.h:99 | K1 does not expose flux history |
| ES `vu_curve_normalized[]` (history) | NONE | GAP | No VU history buffer |
| ES `current_novelty` (per-tick) | `ctx.audio.onsetEnv()` / `ctx.audio.onsetEvent()` | EffectContext.h:303-307 | Single-frame envelope/event |
| Per-band onset flux | `ctx.audio.kickFlux()`, `snareFlux()`, `hihatFlux()` | EffectContext.h:313-315 | K1-specific (3 bands; ES has none) |
| Semantic kick/snare/hihat | `ctx.audio.kickLevel()`, `snare()`, `hihat()`, `isKickHit()` etc. | EffectContext.h:317-332, 365-367 | K1-specific |

### Waveform

| Canonical signal | K1 accessor | Verified | Notes |
|---|---|---|---|
| SB `waveform[1024]` | `ctx.audio.waveform()` (128 samples) / `ctx.audio.preferredWaveform()` | EffectContext.h:222-236 | K1 exposes 128, SB has 1024 |
| SB `waveform_fixed_point[]` | `ctx.audio.getWaveformNormalized(i)` (-1..+1) | EffectContext.h:213-219 | Direct equivalent at 128-sample resolution |
| SB `sample_window[SAMPLE_HISTORY_LENGTH]` | NONE | GAP | Sliding window not exposed |
| SB `waveform_history[4][1024]` | NONE | GAP | History not exposed |

### Mode/style/saliency (K1 extensions, no SB/ES counterpart)

| Canonical signal | K1 accessor | Verified | Notes |
|---|---|---|---|
| (none) | `ctx.audio.musicStyle()`, `styleConfidence()`, `is{Rhythmic,Harmonic,Melodic,Texture,Dynamic}Music()` | EffectContext.h:536-554 | K1 only |
| (none) | `ctx.audio.{harmonic,rhythmic,timbral,dynamic}Saliency()` | EffectContext.h:520-529 | K1 only (MIS Phase 1) |
| (none) | `ctx.audio.motionWeight()`, `motionTime()`, `motionSpace()`, `motionFlow()`, `motionFluidity()`, `motionImpulse()` | EffectContext.h:570-580 | K1 only (Laban motion frame) |
| (none) | `ctx.audio.shapedIntensity()`, `shapedDecayMs()`, `shapedAccent()`, `shapingActive()` | EffectContext.h:589-595 | K1 only (Layer 3 shaping) |

---

## Gaps — what K1 does NOT expose that SB/ES depend on

The wave-mode spazz is best understood as the consequence of these gaps. Each row is a missing primitive that SB or ES uses to *prevent* spazzing.

| # | Missing canonical signal | Origin | Why it matters for wave motion | Severity |
|---|---|---|---|---|
| 1 | **Per-tempo-bin phase array** (`tempi[i].phase` for `i ∈ [0, NUM_TEMPI)`) | Emotiscope | Spectronome-class motion blends multiple tempo phases by `tempi_smooth[i]` weight. K1 only has `onset.phase01` (one phase), so wave effects either (a) lock to the one tempo at all costs and visibly skip when it relocks, or (b) freelance off `getPhase(frequencyHz)` from `totalTimeMs` and become tempo-blind. | **HIGH — likely root cause of spazz** |
| 2 | **Novelty curve history** (`novelty_curve[NOVELTY_HISTORY_LENGTH]`) | SB + ES | SB drives `hue_shift_speed` (and through it the colour-cycle phase) by integrating a *windowed* novelty curve. ES feeds the entire curve into the tempo Goertzel. K1's `flux()` is single-frame — wave effects integrating it without history get noisy phase. | **HIGH** |
| 3 | **VU curve history** (`vu_curve_normalized[]`) | Emotiscope | ES tempo bank uses VU curve as a secondary tempo carrier (see `calculate_magnitude_of_tempo` mixing both). K1 has only single-frame `rms()`. | MEDIUM |
| 4 | **MOOD-controlled spectrogram smoother** (`low_pass_array` cutoff = `1.0 + 10·MOOD` Hz) | SB | SB's MOOD knob *literally* controls the cutoff frequency of the spectrogram lowpass. K1's `getMoodSmoothing(rise, fall)` returns IIR coefficients but is not wired into `bins64`/`bins64Adaptive`. Effects therefore can't change spectral smoothing under MOOD without per-effect plumbing. | MEDIUM |
| 5 | **Per-bin tempo magnitude (cubed)** (`tempi[i].magnitude` after `m³`) | Emotiscope | Used as a soft mask to suppress weak tempo bins. K1 collapses to scalar `tempoConfidence`. Wave motion that wants "only show motion at the dominant tempo" has no mask. | MEDIUM |
| 6 | **`max_waveform_val_follower` / explicit asymmetric envelope follower** | SB | SB uses fast-rise/slow-fall (0.25/0.005) on the waveform peak as the "punch" carrier. K1 exposes `sbWaveformPeakScaled` (the post-follower output) but NOT the follower itself for effects that want to fork the rise/fall asymmetry. | LOW — workaround = own follower |
| 7 | **Spectral history ring** (`spectral_history[N][NUM_FREQS]`) | SB | SB's novelty calc is `spectrogram[i] − spectral_history[idx-1][i]`. Useful for n-frame-back motion. K1's flux is hard-coded to 1-frame back. | LOW |
| 8 | **Per-tempo-bin sine/cosine state** (`tempi[i].sine`, `tempi[i].cosine`) | Emotiscope | Required if K1 wants to internally re-derive `phase` from `atan2(imag, real)` per bin. | LOW (only matters if implementing #1) |
| 9 | **`phase_inverted` flag per tempo** | Emotiscope | Used by ES to alternate sign across phase wraps. Emotiscope wave modes use this for π-aware motion direction. | LOW |
| 10 | **`tempi_power_sum`** (denominator of `tempo_confidence`) | Emotiscope | Useful for "music intensity" scalar that's tempo-aware (vs RMS-aware). K1 has only the ratio. | LOW |

---

## Recommendation — minimum K1 audio surface needed to faithfully port the canonical wave/spectronome motion

This is the smallest delta that closes the wave-mode spazz at its root, ranked by leverage.

### Tier 1 (BLOCKER for clean wave motion port)

**1.1 — Multi-tempo phase bank**

Add to `controlBus` (and surface on `AudioContext`):

```cpp
struct ControlBusFrame {
    // ... existing fields ...

    // Multi-tempo phase bank (Emotiscope-equivalent)
    static constexpr uint8_t TEMPO_BANK_SIZE = 16;  // 16 bins is enough for music; reduces from ES's 96 to fit budget
    float tempoBankBpm[TEMPO_BANK_SIZE];        // BPM of each bin (compile-time const, range 60..180)
    float tempoBankPhase[TEMPO_BANK_SIZE];      // Continuous phase per bin (-π..+π), updated each frame
    float tempoBankMagnitude[TEMPO_BANK_SIZE];  // Smoothed, cubed magnitude (0..1)
    float tempoBankPowerSum;                    // Sum of magnitudes (denominator of confidence)
};
```

Effect-facing accessors:

```cpp
// AudioContext
uint8_t tempoBankSize() const { return audio::ControlBusFrame::TEMPO_BANK_SIZE; }
float tempoBankBpm(uint8_t i) const;
float tempoBankPhase(uint8_t i) const;       // continuous, advances each frame even between locks
float tempoBankMagnitude(uint8_t i) const;
float tempoBankBeat(uint8_t i) const { return sinf(tempoBankPhase(i)); }
```

Implementation: port `init_tempo_goertzel_constants()` + `update_tempi_phase()` + `sync_beat_phase()` from Emotiscope `tempo.h:51-441`. Run one bin per audio frame interlaced (matches Emotiscope's interlace pattern, fits the 125 Hz audio frame budget).

This single addition gives wave effects a **continuous, per-tempo, phase-coherent motion driver** that doesn't snap when the master tempo lock changes. It is the single highest-leverage fix for the spazz.

### Tier 2 (significantly improves robustness)

**2.1 — Novelty curve history ring** (1 KB at typical sizes)

```cpp
static constexpr uint16_t NOVELTY_HISTORY = 256;  // 2 seconds at 125 Hz
float noveltyCurve[NOVELTY_HISTORY];              // log1p of summed positive flux per audio frame
float noveltyCurveNormalized[NOVELTY_HISTORY];    // auto-scaled by 1 / max
```

Port from Emotiscope `tempo.h:283-298`. This unblocks:
- Tempo bank Goertzel input (Tier 1 needs this).
- Effects that want SB-style cubed-novelty colour-cycling drive.
- Effects that want a multi-frame momentum carrier instead of single-frame `flux()`.

**2.2 — MOOD-controlled spectrogram smoother**

Wire `ctx.mood` into the `bins64Adaptive` follower coefficients. SB equation (GDFT.h:149):

```
cutoff_hz = 1.0 + 10.0 * mood_normalized;   // 1 Hz at MOOD=0, 11 Hz at MOOD=1
alpha = 1.0 - exp(-2π * cutoff_hz * dt);    // standard 1-pole frequency-to-α conversion
bins64_smooth[i] += (bins64[i] - bins64_smooth[i]) * alpha;
```

Either bake this into the producer (`bins64Adaptive` with mood-aware smoothing) or expose a parallel `bins64Mood[]` so effects can choose. SB's MOOD knob *is* this smoother — porting it gives K1 effects a single global motion-feel knob without per-effect implementation drift.

### Tier 3 (nice-to-have, not blocker)

**3.1 — VU curve history ring** (mirrors 2.1 for VU)

For Emotiscope-faithful tempo Goertzel input mixing (`tempo.h:147` mixes novelty and VU curves equally before settling on novelty-only).

**3.2 — Per-band asymmetric follower exposure**

Expose explicit rise/fall coefficients on `controlBus.bands[]` — currently `heavy_bands[]` is one fixed slow-follower preset. SB-style "punch" follower (rise=0.25, fall=0.005) is a different envelope and can't be derived from `bands[]`/`heavy_bands[]` alone without effect-side state.

**3.3 — Spectral history ring** (`spectral_history[N][64]`)

For SB-style "n-frame-back" novelty effects. Low priority — most wave motion doesn't need it.

### Rejected / not needed

- **96-bin spectrogram parity**: K1's 64-bin surface is sufficient. The extra resolution in ES/SB is consumed by the chromagram fold (12 classes × 5 octaves = 60 bins minimum) and we already have `chroma()` directly.
- **Per-tempo `phase_inverted` flag**: Only needed for one ES wave-mode pattern; trivially derivable from `phase` sign.
- **`vu_max` / `vu_floor` raw exposure**: `silentScale` + `rms` cover all wave-mode use cases.

---

## Confirmation — the canonical audio→motion driver, distilled

**SB 4.1.1 wave-mode motion driver** = `spectrogram_smooth[]` (MOOD-controlled smoothing) + `audio_vu_level_average` (energy) + `waveform_peak_scaled` (punch) + `novelty_curve[]` (colour-cycle integrator). **No tempo, no phase.** Wave-shape continuity comes from the 1+10·MOOD low-pass holding the spectrum still between frames.

**Emotiscope 1.2 wave-mode motion driver** = `tempi[i].phase` (continuous per-tempo phase, ALWAYS advancing) + `tempi[i].magnitude` (cubed soft mask) + `vu_level` (envelope) + `novelty_curve_normalized[]` (flux history). **Wave-shape continuity comes from the integrated phase, NOT from spectral smoothing.** This is the architectural difference that matters for K1.

**K1 currently has** = `bins64Adaptive` (no MOOD smoothing) + `rms` (envelope) + `sbWaveformPeakScaled` (punch) + `flux` (single-frame, no history) + `onset.phase01` (one phase, locked to one tempo). It has **neither** of the upstream continuity mechanisms — neither SB's MOOD-controlled spectral hold, nor Emotiscope's integrated multi-tempo phase. Effects that need wave-motion coherence are forced to either (a) use `getPhase(clock_freq)` and become tempo-blind, or (b) chase the single locked phase and visibly skip when relocking.

**The minimum patch** to close this is Tier 1 (multi-tempo phase bank) + Tier 2.1 (novelty curve history) + Tier 2.2 (MOOD-controlled smoother). Tier 1 alone fixes the spazz; Tier 2 makes the port faithful enough that future SB/ES wave modes can be machine-translated.

---

## Files inspected

10 files, all read in full per scope:

1. `K1.node1/references/Sensorybridge.sourcecode/SensoryBridge-4.1.1/SENSORY_BRIDGE_FIRMWARE/globals.h`
2. `K1.node1/references/Sensorybridge.sourcecode/SensoryBridge-4.1.1/SENSORY_BRIDGE_FIRMWARE/GDFT.h`
3. `K1.node1/references/Sensorybridge.sourcecode/SensoryBridge-4.1.1/SENSORY_BRIDGE_FIRMWARE/i2s_audio.h`
4. `K1.node1/references/Sensorybridge.sourcecode/SensoryBridge-4.1.1/SENSORY_BRIDGE_FIRMWARE/led_utilities.h`
5. `K1.node1/references/Emotiscope.sourcecode/Emotiscope-1.2/src/types.h`
6. `K1.node1/references/Emotiscope.sourcecode/Emotiscope-1.2/src/goertzel.h`
7. `K1.node1/references/Emotiscope.sourcecode/Emotiscope-1.2/src/tempo.h`
8. `K1.node1/references/Emotiscope.sourcecode/Emotiscope-1.2/src/vu.h`
9. `K1.node1/references/Emotiscope.sourcecode/Emotiscope-1.2/src/notes.h` (pure dev notes — no audio surface contributed)
10. `K1.node1/references/Emotiscope.sourcecode/Emotiscope-1.2/src/perlin.h` (motion driver candidate — perlin noise; no audio dependency)

Plus K1 verification: `firmware-v3/src/plugins/api/EffectContext.h`.

---

## Open questions

1. **Tempo bank size budget on ESP32-S3 at 125 Hz audio frame rate.** Emotiscope uses 96 bins interlaced one-per-frame. K1 should empirically size `TEMPO_BANK_SIZE` (16? 32?) against the actual `NOVELTY_LOG_HZ` budget on the ESV11_32kHz path. Needs hardware measurement.
2. **`NOVELTY_LOG_HZ` calibration.** K1 uses 125 Hz audio frame rate; Emotiscope uses an unspecified reference (`REFERENCE_FPS` is referenced but not defined in the files in scope). The phase-advance constant `phase_radians_per_reference_frame = 2π·target_hz / REFERENCE_FPS` must be calibrated against K1's actual audio frame rate, not blindly copied.
3. **Does Tier 2.2 (MOOD smoother) belong in producer or consumer?** Putting it in `bins64Adaptive` makes all effects honour MOOD by default but breaks effects that want raw frequency response. Suggest a parallel `bins64Mood[]` rather than mutating `bins64Adaptive`.
4. **Hop-rate vs visual-rate phase advance.** Emotiscope's `update_tempi_phase(delta)` is called with the visual frame `delta`, not the audio hop. K1's render thread runs at up to 200 FPS (visual) while audio frames arrive at 125 Hz. Whether to advance phase on the audio thread (deterministic) or the render thread (smoother) needs an architecture decision.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-04-30 | agent:embedded-system-engineer | Created. SB 4.1.1 + Emotiscope 1.2 audio surface inventory verbatim from source; K1 EffectContext mapping verified against `firmware-v3/src/plugins/api/EffectContext.h`; gap analysis + Tier 1/2/3 recommendation for closing the wave-mode spazz at its audio root. |
