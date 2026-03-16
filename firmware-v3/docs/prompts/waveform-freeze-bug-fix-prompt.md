# K1 Waveform Freeze Bug Fix — CC Agent Prompt

## Context

You are working in `firmware-v3/` of an ESP32-S3 LED controller (LightwaveOS). Build environments:

- `esp32dev_audio_esv11_32khz` — V1 device
- `esp32dev_audio_esv11_k1v2_32khz` — K1V2 device

Do NOT build or reference `esp32dev_audio_pipelinecore`. It is not used.

### The Bug

Effect 0x1302 (K1 Waveform / `SbK1WaveformEffect`) freezes to a solid colour during quiet audio passages. Both strips fill with a static colour — no motion, no decay. The effect snaps back instantly when audio returns. Both devices exhibit identical behaviour after clean NVS erase + reflash.

### Root Cause

Two coupled mechanisms in `SbK1WaveformEffect.cpp`:

**Mechanism 1 — Trail fade collapses to 1.0 (no decay):**

Line 187:
```cpp
float fade = 1.0f - 0.10f * absAmp;
```

When `absAmp ≈ 0` (quiet audio), `fade = 1.0`. The trail buffer is multiplied by 1.0 every frame — nothing decays. Every pixel retains its colour permanently. The shift operation pushes this stale colour outward, and the mirror copy fills both halves. Result: solid colour fill within ~0.67s (80 frames at 120 FPS).

**Mechanism 2 — Dot position locks to centre:**

Lines 204-205:
```cpp
float amp = m_wfPeakLast;
if (fabsf(amp) < 0.05f) amp = 0.0f;
```

Hard gate: any `m_wfPeakLast` below 0.05 is forced to exactly zero. With `amp = 0`:

```cpp
float posF = halfLen + amp * halfLen;  // = 80.0 (centre, every frame)
```

The dot is pinned at pixel 80 forever. Combined with fade=1.0, the dot colour propagates outward without decay.

**Why the transition is binary:** The 750-count noise floor subtraction in the audio chain produces `peak = 0` exactly (not "near zero") when mic signal is below the floor. There is no gradual transition. Recovery is equally instant — when audio returns above the noise floor, `m_wfPeakLast` crosses 0.05 within one EMA time constant (~23ms) and the effect snaps back.

## Exact Changes Required

### File: `src/effects/ieffect/sensorybridge_reference/SbK1WaveformEffect.cpp`

This is the ONLY file you modify. Nothing else.

#### Fix 1: Add minimum fade floor

Find line 187 (the fade calculation):

```cpp
float fade = 1.0f - 0.10f * absAmp;
```

Replace with:

```cpp
// Minimum 2% per-frame decay prevents permanent trail retention during
// silence. At 120 FPS, 2%/frame = full decay from 255→0 in ~1.9s.
// During active playback (absAmp=0.5), fade=0.95 — unchanged behaviour.
static constexpr float kMinFade = 0.02f;
float fade = 1.0f - std::max(kMinFade, 0.10f * absAmp);
```

**Verification:** With `absAmp = 0.0`, fade should now be `1.0 - 0.02 = 0.98` (not 1.0). With `absAmp = 0.5`, fade should be `1.0 - 0.05 = 0.95` (unchanged from current). With `absAmp = 1.0`, fade should be `1.0 - 0.10 = 0.90` (unchanged from current).

#### Fix 2: Remove the hard amplitude gate

Find lines 204-205 (the amplitude gate):

```cpp
float amp = m_wfPeakLast;
if (fabsf(amp) < 0.05f) amp = 0.0f;
```

Replace with:

```cpp
float amp = m_wfPeakLast;
// Hard gate removed. m_wfPeakLast is already EMA-smoothed (tau=23ms)
// so it doesn't jitter near zero. With the fade floor, a near-zero dot
// at centre fades naturally instead of painting a permanent stripe.
```

That is: remove the `if` statement entirely. Keep `float amp = m_wfPeakLast;` as-is. The two comment lines replace the removed gate to explain why it was removed.

#### Ensure std::max is available

Check the includes at the top of the file. If `<algorithm>` or `<cmath>` is not already included, add:

```cpp
#include <algorithm>
```

`std::max` requires `<algorithm>`. If the file already uses `std::max` elsewhere or includes `<algorithm>`, no change needed.

## What You Must NOT Do

1. **Do NOT modify AudioActor.cpp.** The smoothing coefficients are correct v3 pipeline calibration.
2. **Do NOT modify EdgeMixer.h.** The RGB matrix refactor is complete and validated.
3. **Do NOT modify SbK1BaseEffect.h or SbK1BaseEffect.cpp.** The tau constants and base class are correct.
4. **Do NOT modify any other effect files.**
5. **Do NOT modify the noise floor (750), the EMA time constant (tau=23ms), or the waveform follower in AudioActor.** These are calibrated for the v3 pipeline.
6. **Do NOT add a "silence detection" or "freeze detection" mechanism.** The fix is simpler: ensure trails always decay and the dot position is not hard-clamped.
7. **Do NOT change the shift or mirror operations.** They work correctly — the bug is in the fade and amplitude gate, not in the buffer management.

## Verification

### 1. Build Check

```bash
cd firmware-v3
pio run -e esp32dev_audio_esv11_32khz 2>&1 | tail -5
pio run -e esp32dev_audio_esv11_k1v2_32khz 2>&1 | tail -5
```

Both must compile with zero errors.

### 2. Code Review

Run `git diff src/effects/ieffect/sensorybridge_reference/SbK1WaveformEffect.cpp` and verify:

- Line 187: `std::max(kMinFade, 0.10f * absAmp)` replaces `0.10f * absAmp`
- `kMinFade = 0.02f` is defined as `static constexpr float`
- Lines 204-205: The `if (fabsf(amp) < 0.05f) amp = 0.0f;` statement is removed
- No other lines changed
- `<algorithm>` included if not already present

### 3. Behavioural Verification

After flashing both devices, test with K1 Waveform (0x1302):

**Test A — Silence transition:**
1. Play music for 5 seconds (waveform animating normally)
2. Cut audio to silence
3. **Expected:** Waveform trails fade gracefully to black over ~2 seconds. No solid colour freeze. No binary snap.
4. **Fail:** Any solid colour fill lasting more than 2 seconds. Any pixel staying at constant brightness for more than 240 frames (2s).

**Test B — Silence recovery:**
1. From silence (strips should be dark/black after fade)
2. Resume music
3. **Expected:** Waveform resumes within 100ms (12 frames). Dot position reflects audio amplitude immediately.
4. **Fail:** Delay > 500ms before waveform resumes. Or waveform resumes but trails don't decay (fade still stuck at 1.0).

**Test C — Quiet passage:**
1. Play music with a quiet passage (not silence — low volume, ambient)
2. **Expected:** Waveform amplitude decreases, dot moves slowly near centre, trails fade slowly. Continuous animation at reduced intensity. No freeze.
3. **Fail:** Effect freezes to solid colour during the quiet passage.

**Test D — Capture metrics (900 frames with music):**

Compare against the pre-fix baseline:

| Metric | Pre-Fix Baseline (V1/K1V2) | Post-Fix Expected | Hard Fail |
|--------|---------------------------|-------------------|-----------|
| flicker | 0.564 / 0.629 | < 0.40 | > 0.50 |
| near_black_leaks | 332 / 180 | 0 | > 10 |
| VMCR | 0.377 / 0.342 | > 0.45 | < 0.30 |
| energy_corr | -0.329 / -0.101 | > 0.00 | < -0.20 |
| composite | 0.287 / 0.403 | > 0.40 | < 0.25 |

**Why these thresholds:**
- Flicker should drop significantly — no more freeze→unfreeze oscillation
- Near-black leaks should reach zero — no more freeze transition dim pixels
- VMCR should improve — stable animation instead of frozen spatial pattern
- Energy correlation should go positive — audio now continuously drives the visual (no dead periods)
- Composite should improve as all sub-metrics improve

### 4. EdgeMixer Interaction Check

After confirming the freeze fix works in MIRROR mode, set EdgeMixer to COMPLEMENTARY (mode=2, spread=30, strength=200) and repeat Test A. Both strips should fade to black during silence — strip 2 with the hue-rotated colours. No asymmetric decay. No solid colour fill.

## Pass/Fail Criteria

- **PASS:** Both builds clean. Hard gate removed. Fade floor added. Tests A-D pass on both devices. Near-black leaks = 0. Flicker < 0.40. Only SbK1WaveformEffect.cpp modified.
- **FAIL:** Any build error. Fade floor missing or wrong value. Hard gate still present. Any test fails. Any file other than SbK1WaveformEffect.cpp modified. Flicker still > 0.50.

## Deliverables

1. The edited `src/effects/ieffect/sensorybridge_reference/SbK1WaveformEffect.cpp`
2. Build output confirming both `esp32dev_audio_esv11_32khz` and `esp32dev_audio_esv11_k1v2_32khz` compile clean
3. `git diff` output for review
4. Test A-D results (visual observation + 900-frame capture metrics) on both devices
