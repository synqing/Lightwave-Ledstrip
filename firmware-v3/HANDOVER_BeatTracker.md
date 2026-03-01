# BeatTracker Hardening — Handover Document

**Date:** 2026-03-01
**Context:** K1-Lightwave ESP32-S3, DSP Spine v0.1, firmware-v3
**Status:** BLOCKED — tempo estimation fundamentally broken, needs new approach
**Previous best:** 6/12 coherent (comb-tooth, DESTROYED)
**Current score:** 2/12 coherent (ACF, on disk now)
**Target:** 17/17 synthetic + ≥9/12 coherent real audio drum loops

---

## 1. CRITICAL: What Happened

Over ~8 hours of work, an agent:

1. **Destroyed the 6/12 comb-tooth codebase** by overwriting `BeatTracker.cpp` with the ATOMs3 "proven" ACF algorithm, without saving a backup.
2. **Restored ACF** (autocorrelation) because claude-mem said it was "proven on ATOMs3 bringup." This was misleading — it only passed 3 easy test cases (kick_120, techhouse_full_drums, techhouse_drop) at 44.1kHz.
3. **Ran sequential single-variant tests for hours** instead of parallelizing.
4. **Failed to check claude-mem** for prior ACF failure evidence until Captain forced it.

**Net result: regression from 6/12 to 2/12. Negative progress.**

The 6/12 comb-tooth code is gone. It cannot be recovered from git (no commit was made). It would need to be reconstructed from the conversation transcript description.

---

## 2. ACF IS FUNDAMENTALLY BROKEN — Evidence

This is **confirmed across multiple sessions** in episodic memory:

| Memory ID | Date | Finding |
|-----------|------|---------|
| #35423 | Feb 17 | "Remaining 3 failures (cyberpunk_100, jazz_160, jazz_210) represent **fundamental ACF limitations** not octave ambiguity" — max 7/10 at 44.1kHz |
| #35484 | Feb 17 | "The autocorrelation approach has well-known ceiling for complex material that should not be declared a 'known limitation' and abandoned" |
| #35333 | Feb 17 | The "passed real audio validation" was ONLY 3 tests: kick_120, techhouse_full_drums, techhouse_drop — NOT comprehensive |
| #37915 | Mar 1 | "Reference pipeline shows ACF octave errors (half-tempo locks) on click tracks, confirming it is a **diagnostic tool not a production tempo estimator**" |
| #37915 | Mar 1 | "Failure modes exactly match DSP Spine Sections 6.4-6.5 predictions: slow music < 100 BPM attracts to harmonics, fast music > 160 BPM attracts to sub-harmonics" |

**Verdict: ACF cannot achieve ≥9/12 coherent. Do not attempt further ACF tuning.**

At 44.1kHz with 256-hop (172 Hz hop rate), ACF maxed at 7/10.
At 16kHz with 128-hop (125 Hz hop rate), ACF scores 2/12.
The lower sample rate and hop rate make it strictly worse.

---

## 3. The Destroyed 6/12 Approach (Comb-Tooth)

This code was overwritten and NOT saved. Reconstruct from this description:

### Algorithm (from conversation history):

```
updateTempoEstimate():
  1. Linearize OSS ring buffer (bass_onset signal)
  2. For each candidate BPM in [60..240]:
     - Convert BPM to lag in hops: lag = 60 * hopRate / bpm
     - Score = comb-tooth: sample onset signal at beat-period intervals
       - Walk backwards from newest sample at period spacing
       - At each tooth position, max-pool ±4 hops for jitter tolerance
       - Sum the max-pooled onset values at each tooth
       - Compute background mean (average of all onset values)
       - Contrast ratio = tooth_sum / (background_mean + epsilon)
     - Apply bidirectional subharmonic enhancement:
       - score += 0.5 * score_at(bpm*2) + 0.33 * score_at(bpm*3)
     - Apply log-Gaussian prior centered on tempoPriorBpm
  3. BPM density accumulator (Gaussian kernel σ=2 BPM):
     - For each candidate above threshold, add Gaussian bump at its BPM
  4. Argmax winner from density accumulator
  5. Standard confidence/lock/unlock/watchdog logic
```

### Key differences from ACF:
- **Comb-tooth** measures onset signal strength at regular beat intervals (time-domain pattern matching)
- **Max-pool ±4 hops** provides jitter tolerance (ACF has none)
- **BPM density accumulator** with Gaussian kernel prevents discrete-lag aliasing
- **Bidirectional subharmonic enhancement** (0.5/0.33) is stronger than ACF's (0.15/0.05)

### Why it scored better:
- Directly measures "are there onsets at beat positions?" rather than autocorrelation
- Max-pooling handles imprecise onset timing in complex material
- BPM-domain scoring avoids the non-uniform lag→BPM resolution problem

---

## 4. Alternative Approach: Goertzel TempoTracker

**Already exists in the codebase** at `firmware/v2/src/audio/tempo/TempoTracker.h` (memory #36092):

- 96 Goertzel resonator bins covering 48-143 BPM at 1.0 BPM resolution
- Dual-rate novelty: spectral flux at 50 Hz + VU derivative at 50 Hz
- Adaptive block sizes for frequency-dependent resolution
- Exponential decay window (newest=1.0, oldest≈0.007)
- Time-based hysteresis (200ms, 10% magnitude advantage for stability)
- Interleaved computation (2 bins/frame for CPU efficiency)
- ~28 KB memory footprint

Also vendored in ESV11 backend (memory #36832): Emotiscope v1.1 tempo pipeline, same Goertzel approach with phase synchronization.

**This is a fundamentally different algorithm** that doesn't suffer from ACF's harmonic/subharmonic confusion because each Goertzel bin is an independent resonator at a specific frequency — there's no shared correlation calculation that can mode-lock to harmonics.

**Limitation:** Current implementation covers 48-143 BPM. The acceptance test range is 60-240 BPM. Would need either:
- Extended bin count (48-240 BPM at 1 BPM = 192 bins, ~56 KB)
- Octave folding (test in BPM and BPM/2, use prior to disambiguate)

---

## 5. DSP Spine v0.1 Frozen Parameters

```
Fs        = 16000 Hz
N (window)= 512 samples
H (hop)   = 128 samples
hop_rate  = 125 Hz (16000/128)
FFT bins  = 257 (N/2+1)
bin_width = 31.25 Hz (16000/512)
```

Beat tracker lag range at these parameters:
- 60 BPM → lag = 60 * 125 / 60 = 125 hops
- 120 BPM → lag = 62.5 hops
- 240 BPM → lag = 31.25 hops
- minLag = 31, maxLag = 125 (capped by kMaxLag=256 and kOssLen/2=256)

Onset detection frozen parameters:
- fluxBinDivisor = 1.0 (no per-bin averaging)
- delta = 0.02
- onsetK = 1.5
- onsetGateRms = 0.0018
- peakPick: preMax=3, postMax=1, preAvg=10, postAvg=1, wait=8

**Onset detection is working correctly.** The 17/17 synthetic tests pass. The problem is exclusively in tempo estimation (Stage D) and beat PLL lock (Stage E).

---

## 6. Complete File Inventory

### Source files (firmware-v3):

| File | Lines | Role |
|------|-------|------|
| `src/audio/pipeline/BeatTracker.h` | 95 | Header: BeatConfig struct + BeatTracker class |
| `src/audio/pipeline/BeatTracker.cpp` | 322 | **THE FILE TO CHANGE** — tempo estimation lives here |
| `src/audio/pipeline/PipelineCore.h` | 197 | Pipeline config, FeatureFrame output struct |
| `src/audio/pipeline/PipelineCore.cpp` | 520 | Orchestration: FFT, flux, onset, calls BeatTracker |
| `src/audio/pipeline/PipelineAdapter.h` | — | Bridge to ControlBus |
| `src/audio/pipeline/PipelineAdapter.cpp` | — | Bridge to ControlBus |
| `src/audio/pipeline/FFT.h` | — | FFT abstraction (kiss_fft on ESP32, native fallback) |
| `src/audio/pipeline/FrequencyMap.h` | — | Bark/Mel band mapping |
| `src/config/features.h` | — | Feature flags including `FEATURE_SPINE16K` |

### Test files:

| File | Lines | Role |
|------|-------|------|
| `test/test_spine16k/test_spine16k_acceptance.cpp` | 570 | Main acceptance test (Unity framework) |
| `test/test_spine16k/unity_config.h` | — | Unity test config |

### Audio test fixtures:

Located at: `/Users/spectrasynq/Workspace_Management/Software/Teensy.AudioDSP_Pipeline/Tests/Audio_16k/`

**Synthetic (17 clips in `synthetic/` subdir):**
01_click_90bpm, 02_click_120bpm, 03_click_160bpm, 04_impulse_500ms, 05_noise_bursts, 06_sustained_chord, 07_sweep_20_8k, 08_silence_events_silence, 09_silence, 10_tone_440hz, 11_tone_100hz, 12_tone_4000hz, 13_silence_to_loud, 14_a_minor_chord, 15_amplitude_ramp, 16_white_noise, 17_multi_tone_bands

**Real drum loops (12 clips):**
| File | Label | Expected BPM |
|------|-------|-------------|
| hiphop_85bpm_16k.wav | hiphop_85 | 85 |
| bossa_nova_95bpm_16k.wav | bossa_95 | 95 |
| cyberpunk_100bpm_16k.wav | cyber_100 | 100 |
| kick_120bpm_16k.wav | kick_120 | 120 |
| techhouse_124bpm_full_16k.wav | tech_124 | 124 |
| hiphop_133bpm_16k.wav | hiphop_133 | 133 |
| jazz_160bpm_16k.wav | jazz_160 | 160 |
| metal_165bpm_16k.wav | metal_165 | 165 |
| jazz_210bpm_16k.wav | jazz_210 | 210 |
| techhouse_drop_16k.wav | tech_drop | 124 |
| techhouse_breakdown_16k.wav | tech_brkdn | 124 |
| techhouse_full_drums_16k.wav | tech_full | 124 |

### Reference/Historical files:

| File | Role |
|------|------|
| `ATOMs3_im69d130_bringup/esp32_audio_pipeline/BeatTracker.cpp` (229 lines) | Original ACF — DO NOT COPY, it's broken |
| `ATOMs3_im69d130_bringup/esp32_audio_pipeline/BeatTracker.h` (83 lines) | Original header |
| `firmware/v2/src/audio/tempo/TempoTracker.h` | Goertzel-based alternative (96 bins, 48-143 BPM) |
| `firmware/v2/src/audio/backends/esv11/vendor/tempo.h` | Emotiscope v1.1 Goertzel tempo (vendored) |
| `Teensy.AudioDSP_Pipeline/Tests/Test_Suite/reference_pipeline.py` (17KB) | Python offline DSP replication |

### Build command:

```bash
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware-v3
pio test -e native_test_spine16k -f test_spine16k
```

Build time: ~25s. Test runtime: ~2.5 min (processes all 12 drum loops capped at 60s each).

### platformio.ini entry:

```ini
[env:native_test_spine16k]
platform = native
build_type = test
test_framework = unity
build_flags =
    -DNATIVE_BUILD=1
    -DFEATURE_SPINE16K=1
    -std=c++17
build_src_filter =
    +<audio/pipeline/PipelineCore.cpp>
    +<audio/pipeline/BeatTracker.cpp>
```

---

## 7. Test Architecture

### test_sweep_all (GATES the build):
- Runs frozen_v0.1 config against 10 synthetic clips
- Scores: onset count range + tempo accuracy (±5 BPM or octave) + beat events
- Total: 17 criteria. **MUST be 17/17 to pass.**
- Currently: 17/17 PASSING

### test_real_audio (DIAGNOSTIC only):
- Runs 3 beat configs (frozen_v0.2, wider_prior, flat_prior) against 12 drum loops
- ACR classification: ML_DIRECT (±5 BPM), ML_DOUBLE (±8 BPM), ML_HALF (±4 BPM), ML_TRIPLE (±10 BPM), ML_THIRD (±5 BPM), ML_WRONG
- "Coherent" = any metrical relationship (not ML_WRONG)
- Reports coherent/12 and direct/12 for each config
- **Does NOT gate the build** — `TEST_ASSERT_MESSAGE(true, ...)` always passes
- Target: ≥9/12 coherent

### Key data flow:

```
PipelineCore::pushSamples(int16_t* samples, hopSize, timestamp)
  → FFT (512-pt real, Hann window)
  → Spectral flux (half-wave rectified magnitude difference)
  → Bass flux: sum of flux for bins 1-8 (31-250 Hz)
  → Onset detection (adaptive threshold: mean + K * stddev, peak picking)
  → BeatTracker::update(onset_env, bass_flux)
      → OSS stores bass_flux for tempo estimation
      → CBSS uses onset_env for beat phase tracking
      → Every ~1.5s: updateTempoEstimate() ← THIS IS THE BROKEN PART
```

---

## 8. What the BeatTracker.cpp Update Function Does

`updateTempoEstimate()` is called every `m_tempoInterval` hops (~1.5s). It must:

1. Analyze the last ~3s of onset signal history (m_oss ring buffer, kOssLen=512)
2. Estimate the dominant tempo in [60, 240] BPM
3. Convert to beat period in hops (m_beatPeriodHops)
4. Update confidence, lock/unlock state, and watchdog

The CBSS phase tracker and beat event detection in `update()` are working correctly — they just need a correct `m_beatPeriodHops` from the tempo estimator. The problem is entirely in `updateTempoEstimate()`.

---

## 9. Recommendations for Next Agent

### DO NOT:
- Use ACF (autocorrelation) for tempo estimation. It's confirmed broken.
- Overwrite files without saving backups.
- Run tests sequentially when variants are independent.
- Ignore episodic memory. Search it FIRST.

### CONSIDER:
1. **Reconstruct the comb-tooth approach** from the description in Section 3. It scored 6/12 and has room for improvement (it was the first implementation, not tuned).

2. **Port the Goertzel TempoTracker** from firmware v2. It's a fundamentally different and potentially superior approach. Needs range extension from 48-143 to 60-240 BPM.

3. **Hybrid:** Use Goertzel for coarse BPM candidates + comb-tooth for refinement/validation.

### Key insight from memory:
The DSP Spine specification (Sections 6.4-6.5) already predicted the exact failure modes being observed. The spec proposed:
- **Watchdog recovery** (implemented, but useless if the estimator itself is broken)
- **Tempo-dependent gain scheduling** (not implemented)
- **Multi-hypothesis tracking** (not implemented — this is likely needed for ≥9/12)

### Running tests:
All 3 beat configs × 12 drums run in a single `pio test` invocation (~2.5 min total). Do NOT run them one at a time. The test harness handles the sweep internally.

---

## 10. Current State of BeatTracker.cpp on Disk

The file currently contains the ACF algorithm (scoring 2/12). Key sections:

- **Lines 193-322:** `updateTempoEstimate()` — the function to replace
- **Lines 117-182:** `update()` — CBSS phase tracking, beat event detection (WORKING, do not touch)
- **Lines 55-61:** `recalcLagBounds()` — computes lag range from BPM range
- **Lines 65-113:** `setParamFloat` / `getParamFloat` — hot-reload accessors

The header (`BeatTracker.h`) has:
- `m_histogram[kMaxLag]` — lag histogram (or repurpose for new approach)
- `m_oss[kOssLen]` — onset signal ring buffer (512 floats, stores bass_onset)
- Constants: kOssLen=512, kMaxLag=256, kCbssLen=256

**Only `updateTempoEstimate()` needs to change.** Everything else works.
