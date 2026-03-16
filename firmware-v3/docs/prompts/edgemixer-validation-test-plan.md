# EdgeMixer v2 Validation Test Plan

**Date:** 2026-03-14
**Scope:** Validate EdgeMixer v2 fixes (maxC < 2, temporal floor 15) and all 5 colour modes against K1 Waveform (0x1302) and baseline effects. Confirm AudioActor coefficients are correct (5c9f4f74 calibration — DO NOT MODIFY).
**Devices:** Both available devices (V1 and K1v2)
**Frames per capture:** 900 minimum (7.5s at 120 FPS)

---

## Pre-Flight Checks (MANDATORY before any test)

Run these before the first capture. If any check fails, STOP and report.

### PF-1: AudioActor Coefficient Verification

Confirm the following values are present in `src/audio/AudioActor.cpp` in the ESV11 backend section (`#if FEATURE_AUDIO_BACKEND_ESVADC11`). These are the correct v3-calibrated values. If ANY value differs, the codebase is in a bad state — do not proceed.

| Parameter | Expected Value |
|-----------|---------------|
| Waveform follower attack | `delta * 0.6f` |
| Waveform follower decay | `delta * 0.03f` |
| Mood EMA smoothing rate | `25.0f + (5.0f * moodNorm)` |
| Spectrogram follower (attack + decay) | `distance * 0.95f` |
| Chroma peak decay | `*= 0.98f` |
| Chroma peak attack | `+= distance * 0.5f` |
| Waveform peak scaled (attack + decay) | `0.7f` both |
| Waveform peak last weights | `0.3f` new / `0.7f` old |

**Pass:** All 8 parameters match in the ESV11 backend section.
**Fail:** Any mismatch. Do not test — report which values differ.

### PF-2: EdgeMixer Fix Verification

Confirm in `src/effects/enhancement/EdgeMixer.h`:

| Fix | Expected |
|-----|----------|
| Near-black threshold | `maxC < 2` (not 6) |
| Temporal floor | `kTemporalFloor = 15` (or equivalent) |
| All 5 modes present | MIRROR, ANALOGOUS, COMPLEMENTARY, SPLIT_COMPLEMENTARY, SATURATION_VEIL |

**Pass:** All three confirmed.
**Fail:** Any mismatch. Do not test — report.

### PF-3: Build Verification

```bash
cd firmware-v3
pio run -e esp32dev_audio_esv11_32khz 2>&1 | tail -5
pio run -e esp32dev_audio_esv11_k1v2_32khz 2>&1 | tail -5
```

**Pass:** Both builds complete with zero errors.
**Fail:** Any build error. Do not flash — report.

**CRITICAL:** These are the ONLY two build environments used in this project. `esp32dev_audio_esv11_32khz` targets the V1 device. `esp32dev_audio_esv11_k1v2_32khz` targets the K1v2 device. Do NOT build or reference `esp32dev_audio_pipelinecore` — it is not used.

### PF-4: NVS State Reset

Before testing, reset EdgeMixer to known state via serial JSON or WebSocket:

```json
{"cmd": "setEdgeMixer", "mode": 0, "spread": 30, "strength": 200, "spatial": 0, "temporal": 0}
{"cmd": "saveEdgeMixer"}
```

Then verify:
```json
{"cmd": "getEdgeMixer"}
```

**Pass:** Response shows mode=0 (MIRROR), spread=30, strength=200, spatial=0 (UNIFORM), temporal=0 (STATIC).
**Fail:** Any field doesn't match. Reset NVS namespace and retry.

---

## Test Suite

### Phase 1: Baseline Capture (MIRROR mode — EdgeMixer disabled)

**Purpose:** Establish reference metrics for K1 Waveform without any EdgeMixer processing. All subsequent tests compare against these baselines.

#### T1.1: K1 Waveform Baseline — Music Input

- **EdgeMixer config:** mode=0 (MIRROR), strength=200, spatial=0, temporal=0
- **Effect:** 0x1302 (K1 Waveform)
- **Audio:** Consistent music source (same track, same volume, same starting point for every capture)
- **Capture:** 900 frames on both devices
- **Metrics to record:**

| Metric | Layer | Expected Range | Hard Fail |
|--------|-------|---------------|-----------|
| Black frame ratio | L0 | < 0.05 | > 0.15 |
| Clipped frame ratio | L0 | < 0.02 | > 0.10 |
| Mean brightness | L0 | 0.15–0.99 | < 0.05 or > 0.995 |
| Stability (LSS) | L1 | > 0.60 | < 0.40 |
| Flicker (elaTCSF) | L1 | < 0.35 | > 0.60 |
| Frequency consistency (VMCR) | L1 | > 0.50 | < 0.30 |
| Energy correlation | L2 | > 0.30 | < 0.10 |
| Beat alignment | L2 | > 0.20 | < 0.05 |
| Onset response | L2 | > 1.20 | < 0.80 |
| Composite score | All | > 0.45 | < 0.30 |

**Pass criteria:** All metrics within expected range on both devices. No hard fails.
**Store as:** `eval_results/baseline/T1.1_waveform_mirror_{device}.json`

#### T1.2: K1 Waveform Baseline — Silence

- **EdgeMixer config:** mode=0 (MIRROR), strength=200, spatial=0, temporal=0
- **Effect:** 0x1302 (K1 Waveform)
- **Audio:** No input (silence / mic disconnected)
- **Capture:** 300 frames (2.5s sufficient for silence characterisation)
- **Metrics to record:**

| Metric | Expected | Hard Fail |
|--------|----------|-----------|
| Black frame ratio | > 0.80 | < 0.50 |
| Mean brightness | < 0.05 | > 0.15 |
| L2 divergence (strip1 vs strip2) | < 5.0 | > 20.0 |

**Purpose:** Confirms both strips fade to black identically in silence. This is the symmetric decay baseline.
**Store as:** `eval_results/baseline/T1.2_waveform_silence_{device}.json`

---

### Phase 2: EdgeMixer Mode Validation (all 5 modes with K1 Waveform)

**Purpose:** Verify each EdgeMixer mode produces expected colour differentiation WITHOUT corruption. Compare every metric against T1.1 baseline.

**Common config for all Phase 2 tests:**
- Effect: 0x1302 (K1 Waveform)
- Audio: Same music source as T1.1 (identical track, volume, start point)
- Strength: 200
- Spatial: 0 (UNIFORM)
- Temporal: 0 (STATIC)
- Capture: 900 frames on both devices

#### T2.1: MIRROR (mode=0) — Control

Repeat of T1.1. Results must match T1.1 within ±5% on all metrics. This validates capture reproducibility.

**Pass:** All metrics within ±5% of T1.1 values.
**Fail:** Any metric differs by >10% from T1.1. Indicates non-determinism in capture pipeline.

#### T2.2: ANALOGOUS (mode=1)

- **EdgeMixer config:** mode=1, spread=30, strength=200

**Acceptance criteria (compared to T1.1 baseline):**

| Metric | Acceptable Degradation from T1.1 | Hard Fail Threshold |
|--------|----------------------------------|---------------------|
| Stability (LSS) | ≤ 0.10 drop | > 0.20 drop |
| Flicker (elaTCSF) | ≤ 0.08 increase | > 0.15 increase |
| Energy correlation | ≤ 0.10 drop | > 0.25 drop |
| Onset response | ≤ 0.15 drop | > 0.30 drop |
| Composite score | ≤ 0.08 drop | > 0.15 drop |
| L2 divergence (strip1 vs strip2) | > T1.1 baseline (must increase — EdgeMixer is doing something) | ≤ T1.1 baseline (EdgeMixer had no effect) |
| Black frame ratio | ≤ T1.1 + 0.02 | > T1.1 + 0.10 |

**Additional check — near-black leak:** Count frames where any pixel on strip 2 has `maxC ∈ [2, 6]` AND was transformed (colour differs from strip 1 equivalent). Must be zero. This validates the maxC < 2 threshold isn't leaking garbage hue conversions on dim pixels.

#### T2.3: COMPLEMENTARY (mode=2)

- **EdgeMixer config:** mode=2, spread=30, strength=200

**Same acceptance criteria table as T2.2, PLUS:**

| Additional Metric | Acceptance | Hard Fail |
|-------------------|------------|-----------|
| Saturation preservation | Strip 2 mean saturation ≥ 60% of strip 1 mean saturation | < 45% |
| Hue offset verification | Mean hue delta between strips = 128 ± 15 (FastLED units) | < 100 or > 156 |

**Why saturation preservation matters:** The D(0.851) desaturation matrix intentionally retains 85% saturation in linear RGB. Measured in HSV analysis on captured frames, this reads as ~62% due to the luminance-preserving blend-toward-grey. The 60% floor allows for measurement variance while catching genuine compounding loss.

#### T2.4: SPLIT_COMPLEMENTARY (mode=3)

- **EdgeMixer config:** mode=3, spread=30, strength=200

**Same acceptance criteria as T2.2.** No additional special metrics.

#### T2.5: SATURATION_VEIL (mode=4)

- **EdgeMixer config:** mode=4, spread=30, strength=200

**Same acceptance criteria as T2.2, PLUS:**

| Additional Metric | Acceptance | Hard Fail |
|-------------------|------------|-----------|
| Hue preservation | Mean hue delta between strips < 5 (FastLED units) | > 15 |
| Saturation reduction | Strip 2 mean saturation < strip 1 mean saturation | Strip 2 sat ≥ strip 1 sat |

**Why:** SATURATION_VEIL should desaturate strip 2 without shifting hue. If hue is moving, the `rgb2hsv_approximate` noise on near-white pixels is leaking through.

---

### Phase 3: Spatial Mode Validation

**Purpose:** Verify CENTRE_GRADIENT spatial mode produces centre-weighted EdgeMixer effect.

#### T3.1: COMPLEMENTARY + CENTRE_GRADIENT

- **EdgeMixer config:** mode=2, spread=30, strength=200, spatial=1 (CENTRE_GRADIENT), temporal=0
- **Effect:** 0x1302 (K1 Waveform)
- **Audio:** Same music source
- **Capture:** 900 frames

**Acceptance criteria:**

| Metric | Acceptance | Hard Fail |
|--------|------------|-----------|
| All T2.3 metrics | Must pass T2.3 thresholds | Any T2.3 hard fail |
| Centre pixel divergence | Strip 2 pixels [70–86] should have L2 divergence < 50% of edge pixels [0–15] | Centre ≥ edge divergence |
| Gradient monotonicity | L2 divergence should increase monotonically from centre to edge (±10% noise tolerance) | Non-monotonic by > 20% |

---

### Phase 4: Temporal Mode Validation

**Purpose:** Verify RMS_GATE temporal mode attenuates during silence without hard-killing strip 2.

#### T4.1: RMS_GATE — Music→Silence→Music Transition

- **EdgeMixer config:** mode=2, spread=30, strength=200, spatial=0, temporal=1 (RMS_GATE)
- **Effect:** 0x1302 (K1 Waveform)
- **Audio sequence:** 3s music → 3s silence → 3s music (total 9s = 1080 frames, capture all)
- **Capture:** 1080 frames minimum

**Acceptance criteria:**

| Phase | Metric | Acceptance | Hard Fail |
|-------|--------|------------|-----------|
| Music (frames 0–360) | L2 divergence | ≥ 80% of T2.3 divergence | < 50% of T2.3 |
| Silence transition (frames 360–400) | L2 divergence decay | Should decrease smoothly over 20–40 frames | Step function (instant drop to 0) |
| Silence (frames 400–720) | L2 divergence | Should reach minimum > 0 (temporal floor prevents zero) | Exactly 0 for > 10 consecutive frames |
| Music resume (frames 720–760) | L2 divergence rise | Should increase within 30 frames | No response after 60 frames |
| Music steady (frames 760–1080) | All T2.3 metrics | Must pass T2.3 thresholds | Any T2.3 hard fail |

**Critical check — symmetric decay:** During silence phase, measure strip 1 and strip 2 brightness independently. Both should decay toward black. Strip 2 must NOT freeze (old bug: RMS_GATE early return left strip 2 unprocessed). Acceptable asymmetry: strip 2 brightness within 20% of strip 1 brightness during decay.

---

### Phase 5: Cross-Effect Validation

**Purpose:** Confirm EdgeMixer doesn't corrupt effects OTHER than K1 Waveform. Run a representative effect from each rendering category.

#### T5.1: Simple palette effect (non-audio-reactive)

- **Effect:** Any basic palette-cycling effect (NOT K1 family)
- **EdgeMixer config:** mode=2, spread=30, strength=200, spatial=0, temporal=0
- **Capture:** 300 frames, both devices

**Pass:** Stability (LSS) > 0.70, flicker (elaTCSF) < 0.25. No black frame spikes.

#### T5.2: Audio-reactive non-K1 effect

- **Effect:** Any audio-reactive effect that is NOT a K1/SB port
- **EdgeMixer config:** mode=2, spread=30, strength=200, spatial=0, temporal=0
- **Audio:** Same music source
- **Capture:** 900 frames, both devices

**Pass:** Onset response > 1.0, composite > 0.40. Energy correlation threshold relaxed to > 0.01 — LGP Holographic uses spatial audio coupling not captured by brightness-RMS correlation.

---

### Phase 6: Regression Gate

**Purpose:** Final pass/fail decision for the EdgeMixer v2 release.

#### Regression Criteria (ALL must pass):

1. **No metric regression > 15%** from MIRROR baseline (T1.1) across any EdgeMixer mode on any device
2. **Zero near-black leaks** (maxC ∈ [2, 6] transformed pixels) across all tests
3. **Zero hard-kill events** (10+ consecutive frames of zero L2 divergence during music) across all temporal mode tests
4. **Cross-device consistency** — V1 and K1v2 composite scores within 10% of each other for the same test
5. **Both device builds clean** — zero errors on `esp32dev_audio_esv11_32khz` (V1) and `esp32dev_audio_esv11_k1v2_32khz` (K1v2)

#### Regression Failure Protocol:

If any criterion fails:
1. Record the failing metric, test ID, device, and captured value
2. Do NOT attempt to fix coefficients in AudioActor.cpp (they are correct — see `CORRECTION-audioactor-revert-cancelled.md`)
3. The issue is in EdgeMixer.h or the test configuration — investigate there
4. Report findings before making any code changes

---

## Output Format

For each test, produce a results file containing:

```json
{
  "test_id": "T2.3",
  "device": "k1v2",
  "effect": "0x1302",
  "edgemixer": {
    "mode": 2,
    "spread": 30,
    "strength": 200,
    "spatial": 0,
    "temporal": 0
  },
  "frames_captured": 900,
  "metrics": {
    "L0": { "black_ratio": 0.0, "clip_ratio": 0.0, "mean_brightness": 0.35 },
    "L1": { "stability_lss": 0.72, "flicker_elatcsf": 0.18, "vmcr": 0.65 },
    "L2": { "energy_corr": 0.45, "beat_align": 0.32, "onset_response": 1.45 },
    "composite": 0.52,
    "strip_divergence_l2": 89.3,
    "near_black_leaks": 0
  },
  "pass": true,
  "notes": ""
}
```

Store all results in `eval_results/edgemixer_v2_validation/`.

---

## What You Must NOT Do

1. **Do NOT modify AudioActor.cpp.** The smoothing coefficients are correct v3 pipeline calibration. Read `docs/prompts/CORRECTION-audioactor-revert-cancelled.md` for full context.
2. **Do NOT modify SbK1BaseEffect.h or SbK1BaseEffect.cpp.** The tau constants are correct.
3. **Do NOT modify EdgeMixer.h** unless a test fails AND you can identify the specific line causing the failure. Even then, report before changing.
4. **Do NOT modify any effect source files.**
5. **Do NOT change the audio source between captures within the same test phase.** Reproducibility depends on identical input.
6. **Do NOT skip pre-flight checks.** If PF-1 through PF-4 don't all pass, the test results are meaningless.
