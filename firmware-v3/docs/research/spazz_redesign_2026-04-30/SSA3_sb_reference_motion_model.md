---
abstract: "SSA3 — Canonical motion/phase/position model extracted from SbK1Base + 6 SB reference effects, contrasted with the broken-4 audio-reactive effects (ChevronWaves, LGPInterferenceScanner, LGPWaveCollision, LGPStarBurst). Core finding: SB reference effects have NO sin/cos phase accumulator and NO frequency multiplier; motion is either pixel-domain integer/sub-pixel scroll, per-pixel waveform rendering, or pitch-mapped position. The broken-4 use `m_phase += speedNorm * 240.0f * smoothedSpeed * dt` driven by heavy-band energy with spring-smoothed speed — a non-canonical pattern that has no equivalent in the SB ancestry. Cousin recommendations included for each broken effect."
---

# SSA3 — SB Reference Motion Model vs Broken-4 Differential

**Scope:** SbK1Base + SbK1Waveform / SbK1WaveformHybrid / SbK1WaveformHarmonic / SbWaveform310Ref / SbWaveformOscilloscope / SbK1BloomV2 (representative LGP/Bloom variant).
**Compared against:** ChevronWavesEffect, LGPWaveCollisionEffect, LGPInterferenceScannerEffect, LGPStarBurstEffect (the four `m_phase += speedNorm * 240.0f * smoothedSpeed * dt` consumers).
**Read-only audit.** All citations refer to the working tree as of 2026-04-30.

---

## 1. Canonical SB phase-update equation

**There is no sin/cos phase accumulator anywhere in the SB reference family.**

This is the single most important finding. None of the seven SB reference files contain a `m_phase += rate * dt` accumulator that drives a `sinf(phase)` or `cosf(phase)` call. Motion in SB is implemented through **three distinct mechanisms**, none of which involve a "wave frequency multiplier":

### 1a. Sub-pixel scroll accumulator (the closest analogue)

Used by `SbK1WaveformEffect`, `SbK1WaveformHybridEffect`, `SbK1WaveformHarmonicEffect`, and `SbK1BloomEffect`.

```cpp
// SbK1WaveformEffect.cpp:239-244
static constexpr float kBaseScrollRate = 150.0f;
static constexpr float kSpeedMidpoint  = 10.0f;  // DEFAULT_SPEED from RendererActor
float scrollRate = kBaseScrollRate * (static_cast<float>(ctx.speed) / kSpeedMidpoint);
m_ps->scrollAccum += scrollRate * m_dt;
int pixelsToScroll = static_cast<int>(m_ps->scrollAccum);
m_ps->scrollAccum -= static_cast<float>(pixelsToScroll);
```

The accumulator units are **pixels per second**, not radians. The integer part triggers an integer scroll of `trailBuffer[]`; the fractional part carries over. Wrapping happens implicitly through the `int(...)` extraction — there is no modulo on a 2π domain.

Identical pattern, identical constants in `SbK1WaveformHybridEffect.cpp:254-259` and `SbK1WaveformHarmonicEffect.cpp:161-166`.

### 1b. Integer alternating-frame scroll (Bloom family)

```cpp
// SbK1BloomV2Effect.cpp:140-216
m_iter++;
if ((m_iter & 1) == 0) {
    // ... compute totalEnergy from chroma ...
    const bool fast = (m_mood > 0.5f);
    if (fast) {
        for (int j = kStripLen - 1; j >= (int)(kHalf + 2); --j)
            workBuf[j] = scrollBuf[j - 2];
        workBuf[kHalf]     = gray;
        workBuf[kHalf + 1] = gray;
    } else {
        for (int j = kStripLen - 1; j >= (int)(kHalf + 1); --j)
            workBuf[j] = scrollBuf[j - 1];
        workBuf[kHalf] = gray;
    }
}
```

No floating-point accumulator at all. Scroll is gated by frame parity (even frames advance, odd frames replay) and `m_mood > 0.5f` chooses between 1-pixel and 2-pixel shifts. This is K1's `bloom_algo_center_scroll` (algorithm 0) ported byte-for-byte.

### 1c. Per-pixel waveform sample render (no transport at all)

`SbWaveform310RefEffect` and `SbWaveformOscilloscopeBase` do **not** scroll. Each pixel reads from a 4-frame averaged + spatial-LPF'd waveform buffer at a fixed pixel-to-sample mapping every frame:

```cpp
// SbWaveform310RefEffect.cpp:218-231
for (uint16_t dist = 0; dist < HALF_LENGTH; ++dist) {
    uint16_t wfIndex = (uint16_t)((dist * (WAVEFORM_POINTS - 1) + (HALF_LENGTH - 1) / 2) / (HALF_LENGTH - 1));
    if (wfIndex >= WAVEFORM_POINTS) wfIndex = WAVEFORM_POINTS - 1;
    float waveform_sample = 0.0f;
    for (uint8_t s = 0; s < HISTORY_FRAMES; ++s) {
        waveform_sample += (float)m_ps->waveformHistory[z][s][wfIndex];
    }
    waveform_sample *= (1.0f / (float)HISTORY_FRAMES);
    ...
    m_ps->waveformLast[z][dist] += (input_wave_sample - m_ps->waveformLast[z][dist]) * smoothingAlpha;
    ...
}
```

"Motion" is the audio waveform itself — it changes because the underlying samples change, not because a phase variable is advancing.

### 1d. Pitch-mapped fixed position (Harmonic)

`SbK1WaveformHarmonicEffect.cpp:213-214` injects each chroma bin at a fixed pixel:
```cpp
float posF = (float)kCenterRight + ((float)c / 12.0f) * (float)(kHalfLength - 1);
```
Position is determined by pitch class (C=80, B=153). Motion comes from the strip-wide scroll accumulator (1a), not from any per-bin phase.

### 1e. Hue position (the only float wrap in the family)

`SbK1BaseEffect::processColorShift` is the only place a `[0, 1]` wrap exists, and it walks **hue**, not spatial position:

```cpp
// SbK1BaseEffect.cpp:463-466
m_huePosition += m_hueShiftSpeed * m_huePushDir;
if (m_huePosition > 1.0f) m_huePosition -= 1.0f;
if (m_huePosition < 0.0f) m_huePosition += 1.0f;
```

`m_hueShiftSpeed` is a tiny number (clamped to `0.02` max at line 449, floored at `0.0001` at line 459) modulated by the novelty-derived "speed control" of K1's 6-step hue-shift algorithm. **It is not multiplied by `dt`** — see §4 below.

---

## 2. Canonical audio-signal source for phase rate

There is no canonical "phase rate" because there is no canonical phase accumulator. There are, however, three distinct audio-driven rate sources in the SB family:

| Source | Used by | Field on ControlBus | Smoothing |
|---|---|---|---|
| `ctx.speed` (USER parameter, not audio) | All scroll variants | n/a | none — direct multiplier on `kBaseScrollRate` |
| `m_wfPeakLast` (smoothed waveform peak) | K1 Waveform / Hybrid (drives **dot position**, not motion rate) | derived from `cb.waveform[]` or `cb.sb_waveform_peak_scaled` | two-stage EMA, tau ≈ 16 ms then 23 ms |
| `noveltyNow` / `flux` | Hue position only (not spatial motion) | `cb.bins256` → `computeNovelty()`, or `ctx.audio.flux()` | none on input; output passes through K1 6-step gating |

**Critically, `heavyBands[1] + heavyBands[2]`, `bands[]`, or any "bass energy" signal is NEVER used to drive scroll speed in the SB reference family.** The scroll rate is fixed at `kBaseScrollRate = 150.0f` pixels/second and is modulated only by the user-facing `ctx.speed` knob.

The colour synthesis layer reads `m_chromaSmooth[]` (which on ESV11 is just `cb.chroma[i]` per `SbK1BaseEffect.cpp:181-183`), and `m_wfPeakLast` drives the dot's spatial position (not its motion rate) via `posF = halfLen + amp * halfLen` (`SbK1WaveformEffect.cpp:264-265`).

---

## 3. Canonical smoothing / Spring / EMA usage

**No SB reference effect uses a Spring follower for motion.** All smoothing in the SB family is one-pole EMA (or AsymmetricFollower for the waveform-peak follower). Time constants are explicit and frame-rate-independent via `alpha = 1 - exp(-dt / tau)`.

### 3a. Base class taus (SbK1BaseEffect.h:164-174)

```cpp
static constexpr float kTauMagAvg           = 0.0069f;  // mag averaging
static constexpr float kTauSpecAttack       = 0.0044f;  // spectrogram attack
static constexpr float kTauSpecDecay        = 0.0091f;  // spectrogram decay
static constexpr float kTauChromaPeakDecay  = 0.4125f;  // chroma peak decay
static constexpr float kTauChromaPeakAttack = 0.0163f;  // chroma peak attack
static constexpr float kTauHueSpeedDecay    = 0.8291f;  // hue-shift speed decay
static constexpr float kTauHueMix           = 0.8291f;  // hue mix approach
static constexpr float kTauWfFollowerAttack = 0.0069f;  // waveform follower attack
static constexpr float kTauWfFollowerDecay  = 0.0999f;  // waveform follower decay
static constexpr float kTauWfPeakLast       = 0.0234f;  // smoothed peak EMA
static constexpr float kTauVuAvg            = 0.0374f;  // VU level
```

### 3b. Two-stage waveform peak smoothing (SbK1BaseEffect.cpp:522-533)

```cpp
// Stage 1: symmetric follower (alpha=0.5 at 90 FPS) → tau ≈ 16 ms
static constexpr float kTauWfPeakStage1 = 0.016f;
float aStage1 = 1.0f - expf(-m_dt / kTauWfPeakStage1);
m_wfPeakScaled += (peakNorm - m_wfPeakScaled) * aStage1;

// Stage 2: slower EMA → tau ≈ 23 ms (used for dot position)
float aWfPk = 1.0f - expf(-m_dt / kTauWfPeakLast);
m_wfPeakLast += (m_wfPeakScaled - m_wfPeakLast) * aWfPk;
```

### 3c. Colour smoothing (SB 3.0.0 lineage)

```cpp
// SbK1WaveformHybridEffect.cpp:210-215
static constexpr float kTauColorSmooth = 0.163f;  // ~20-frame inertia at 120 FPS
float aColor = 1.0f - expf(-m_dt / kTauColorSmooth);
m_dotColorSmooth.r += (dotColor.r - m_dotColorSmooth.r) * aColor;
```

```cpp
// SbWaveform310RefEffect.cpp:186-193
static constexpr float kColourTau = 0.325f;  // SB 0.05/0.95 @ 60fps lineage
const float colourAlpha = 1.0f - expf(-dt / kColourTau);
sum_color_float[c] = m_ps->sumColourLast[z][c] + colourAlpha * (sum_color_float[c] - m_ps->sumColourLast[z][c]);
```

### 3d. AsymmetricFollower (310Ref only)

```cpp
// SbWaveform310RefEffect.cpp:71-73 (init)
m_peakFollower[z] = enhancement::AsymmetricFollower{0.0f, 0.02f, 0.30f};   // 20ms attack, 300ms release
m_maxFollower[z]  = enhancement::AsymmetricFollower{750.0f, 0.04f, 2.00f}; // 40ms attack, 2000ms release
m_rmsFollower[z]  = enhancement::AsymmetricFollower{0.0f, 0.03f, 0.25f};   // 30ms attack, 250ms release
```

These are **followers on energy/peak**, not on speed. They control trail-fade rate and brightness scaling, never phase advance.

**Spring physics are absent from the entire SB reference family.** This is a critical divergence point with the broken-4.

---

## 4. dt choice — scaled vs raw

The SB reference family has **one canonical dt**, used for all smoothing, scroll, and trail-fade computations:

```cpp
// SbK1BaseEffect.cpp:133
m_dt = ctx.getSafeDeltaSeconds();  // SPEED-scaled (visual) dt
```

SbWaveform310RefEffect uses the audio-coupled variant:

```cpp
// SbWaveform310RefEffect.cpp:91
const float dt = AudioReactivePolicy::signalDt(ctx);  // = ctx.getSafeRawDeltaSeconds()
```

Per `AudioReactivePolicy.h:36-38`:
```cpp
static inline float signalDt(const plugins::EffectContext& ctx) {
    return ctx.getSafeRawDeltaSeconds();  // unscaled by SPEED — for DSP-coupled maths
}
```

**The SB rule is: visual transport (scroll, fade) uses `getSafeDeltaSeconds()` (SPEED-scaled). Audio-coupled smoothing uses `getSafeRawDeltaSeconds()` (raw).** The K1 hue-shift code (`processColorShift` at lines 458, 485) uses `m_dt` (visual) because the hue is a visual artefact.

**Crucially, the `m_huePosition` walk does NOT use dt:**
```cpp
// SbK1BaseEffect.cpp:463
m_huePosition += m_hueShiftSpeed * m_huePushDir;  // pure per-frame increment
```
This is the ONE place `dt` is intentionally omitted. The hue speed itself decays with dt, but the position step is per-frame. This is a known K1-parity artefact (preserves the visual character at all frame rates because `m_hueShiftSpeed` is so small the per-frame error is bounded).

### 4a. Why `m_huePosition` is NOT a wave phase

The wrap on `m_huePosition` is `[0, 1]`, indexing a hue wheel — **not** a `[0, 2π]` radian domain feeding a `sinf()`. The "240.0f" multiplier in the broken-4 has no equivalent here because there is no spatial wave being phase-advanced.

---

## 5. Wrap domain and "frequency multiplier" equivalents

| SB effect | Wrap variable | Wrap domain | "Frequency multiplier" | Driver |
|---|---|---|---|---|
| SbK1Waveform | `m_ps->scrollAccum` | implicit (integer extraction) | `kBaseScrollRate = 150.0 px/s` | `ctx.speed / 10.0` (user) |
| SbK1WaveformHybrid | `m_ps->scrollAccum` | implicit | `kBaseScrollRate = 150.0 px/s` | `ctx.speed / 10.0` (user) |
| SbK1WaveformHarmonic | `m_ps->scrollAccum` | implicit | `kBaseScrollRate = 150.0 px/s` | `ctx.speed / 10.0` (user) |
| SbWaveform310Ref | none — per-pixel render every frame | n/a | n/a | hop-gated history update |
| SbWaveformOscilloscope | none — per-pixel render every frame | n/a | n/a | hop-gated history update |
| SbK1BloomV2 (algo 0) | `m_iter` (uint32) | wraps at uint32 max | 1 px/even-frame (slow) or 2 px/even-frame (fast) | `m_mood > 0.5` |
| SbK1Base hue position | `m_huePosition` | `[0, 1]` (hue wheel) | none — `m_hueShiftSpeed ≤ 0.02` clamped | K1 6-step novelty algorithm |

**There is no `240.0f` or anything close to it anywhere in the SB ancestry.** The only "rate" constant is `150.0f` (pixels per second of scroll), and it is bounded above by the LED count divided by frame time — at 120 FPS that is 80 px/frame max, which is well within the "perceptually continuous motion" envelope.

---

## 6. Differential — broken-4 vs canonical SB

### 6a. ChevronWavesEffect.cpp (lines 124-148, 163-166)

```cpp
// BROKEN — line 127-130
heavyEnergy = (ctx.audio.getHeavyBand(1) +
               ctx.audio.getHeavyBand(2)) / 2.0f;
float targetSpeed = 0.6f + 1.2f * heavyEnergy;          // line 131

// Spring physics on speed — line 134
float smoothedSpeed = m_phaseSpeedSpring.update(targetSpeed, rawDt);
if (smoothedSpeed > 2.0f) smoothedSpeed = 2.0f;
if (smoothedSpeed < 0.3f) smoothedSpeed = 0.3f;

// PHASE accumulator with frequency multiplier — line 137
m_chevronPos += speedNorm * 240.0f * smoothedSpeed * dt;

// Spatial wave from phase — line 148
float chevron = sinf(distFromCenter * freqBase - m_chevronPos);
```

Divergence from canonical SB:

1. **No `m_phase` analogue exists in SB.** SB scrolls a buffer; ChevronWaves runs `sinf()` of `(distFromCenter*freqBase - m_phase)`. This is a parametric standing-wave model, not a transport model.
2. **`heavyEnergy` (heavy bands 1+2) drives motion rate.** SB never couples bass energy to scroll speed; SB's scroll speed is a fixed `150.0f * (ctx.speed / 10.0)`.
3. **Spring physics on speed (`m_phaseSpeedSpring`).** No SB effect uses a Spring on a rate; SB uses one-pole EMA on energy/peak only.
4. **`240.0f` multiplier.** SB's only motion constant is `150.0f` (pixels/sec). The `240.0f` here is in radians (or some unitless quantity) — it has no SB precedent and was added in an earlier "fix" cycle (per the `// FIX:` comment at line 162 of LGPWaveCollisionEffect).
5. **`dt` (SPEED-scaled, visual) used for phase advance.** This means SPEED knob amplifies any rate jitter by up to 4x. SB uses raw dt for all DSP-coupled rate maths.

The "spazz" symptom is direct fallout: a Spring-smoothed multiplier on `heavyEnergy` is fed straight into `m_phase` accumulation. Heavy bands have inter-frame jitter on percussive content; Spring smoothing reduces but does not eliminate it; `240.0f` then amplifies the residual jitter into visible phase-velocity noise; the `sinf(... - m_phase)` makes that noise visible as direction reversals near the apex of the wave shape.

### 6b. LGPWaveCollisionEffect.cpp (lines 139-166)

```cpp
// BROKEN — line 140
float bassEnergy = ctx.audio.heavyBass();  // average heavy_bands[0] + [1]

// line 124, 133: percussion triggers ADD to speed/boost
if (hasAudio && ctx.audio.isSnareHit())  m_collisionBoost = 1.0f;
if (hasAudio && ctx.audio.isHihatHit())  m_speedTarget = 1.6f;

// line 151
float rawSpeedScale = (0.7f + 0.6f * bassEnergy) * m_speedTarget;

// line 156
float smoothedSpeed = m_speedSpring.update(speedTargetClamped, rawDt);

// line 164
m_phase += speedNorm * 240.0f * smoothedSpeed * dt;
if (m_phase > 628.3f) m_phase -= 628.3f;  // wrap at 100*2π
```

Divergence:

1. Identical broken pattern to ChevronWaves — `m_phase += 240.0f * spring(bassEnergy) * dt`.
2. **Discrete percussion triggers (`isSnareHit`, `isHihatHit`) jam-set `m_speedTarget = 1.6` and `m_collisionBoost = 1.0`.** SB has no analogue — SB's discrete events feed the hue-shift novelty curve at most. Setting a motion rate to a discrete value is what produces visible "kicks" in phase velocity.
3. **Wrap at `628.3f` (100·2π).** SB never wraps in a 2π domain. This is a clue that `m_phase` is conceptualised as radians, but the `240.0f` multiplier means at full speed you traverse `240` radians per second — about 38 cycles per second. That is not a wave rate; that is a strobe rate.

### 6c. LGPInterferenceScannerEffect.cpp (line 122-149, summarised)

Identical motif:
```cpp
m_scanPhase += speedNorm * 240.0f * smoothedSpeed * dt;
```
Same heavyBands-driven Spring-smoothed rate accumulator pattern. Same divergence catalogue.

### 6d. LGPStarBurstEffect.cpp (line 113-114)

```cpp
// "PROVEN PATTERN - 240.0f multiplier"
m_phase += speedNorm * 240.0f * smoothedSpeed * dt;
```

Header comment self-identifies as the "proven pattern" — which is the giveaway. This pattern is the propagation vector. The four files cite each other as authority ("Use 240.0f multiplier like ChevronWaves") rather than citing any SB reference. The pattern is endemic to the LGP/Chevron family and **has no SB ancestry whatsoever**.

---

## 7. Recommendation — visual cousin per broken effect

Rather than retain the broken phase accumulator, each broken effect should be re-derived from its closest SB visual cousin:

| Broken effect | Visual concept | Closest SB cousin | Why |
|---|---|---|---|
| **ChevronWavesEffect** | V-shaped peaks moving outward from centre | **SbK1WaveformEffect** (0x1302) | Both inject at centre and propagate outward via centre-mirror. Replace `m_phase` accumulator + `sinf(... - m_phase)` with a sub-pixel scroll accumulator (`scrollAccum += 150.0f * speedNorm * m_dt`), inject the chevron shape at LED 80, and let the scroll carry it outward. The "V" shape comes from the geometry of injection, not phase. |
| **LGPWaveCollisionEffect** | Wave packets expanding from centre, colliding | **SbK1WaveformHybridEffect** (0x1313) | Hybrid already implements centre-injection + scroll + dynamic trail fade with audio-coupled decay rate. Replace the collision logic with two scroll buffers running outward and a per-frame "collision detect" at LED 79/80 (still a static spatial event, no phase). Use Hybrid's `kTauColorSmooth = 0.163f` for the colour temporal smoothing. |
| **LGPInterferenceScannerEffect** | Two travelling waves interfering on the strip | **SbWaveformOscilloscopeBase** (0x130C) | Oscilloscope per-pixel renders a filtered waveform shape across all LEDs every frame. The "interference" pattern can be expressed as the difference between two waveform-history reads at offset positions — no phase accumulator. The 6-pass spatial LPF (`SbWaveformOscilloscopeEffect.cpp:198-213`) gives the same "wave-y" character without temporal phase noise. |
| **LGPStarBurstEffect** | Bursts radiating from centre with chroma-coloured rays | **SbK1WaveformHarmonicEffect** (0x1314) | Harmonic already injects 12 dots at fixed pitch-mapped positions and lets the scroll carry them outward as trails. Each "ray" of the starburst is one chroma bin's pitch-mapped trail. Replace radial-phase computation with Harmonic's pitch-mapped position formula: `posF = kCenterRight + (c / 12.0) * (kHalfLength - 1)`. |

### 7a. The minimal patch (if a full rewrite is not acceptable)

If the four broken effects must retain their `sinf(spatial - phase)` rendering for backward visual compatibility, the **minimum** change is:

1. **Drop the `240.0f` multiplier.** The SB scroll rate is `150.0f` pixels/sec at `ctx.speed = 10`. For a `sinf(... - m_phase)` model, the equivalent angular rate is `2π / wavelength_pixels * scroll_pixels_per_sec`. With `freqBase = 0.25` (wavelength ≈ 25 px) and 150 px/s scroll, that is `0.25 * 150 ≈ 37.5 rad/s` — an order of magnitude slower than the current `240.0f * speedNorm * smoothedSpeed`.
2. **Drop the heavyBand-driven Spring on speed.** SB does not modulate scroll rate from audio energy. If audio coupling on motion is desired, modulate **trail fade rate** (per `SbK1WaveformEffect.cpp:218-227`), **not** scroll/phase rate.
3. **Use raw dt for the phase advance.** `dt = ctx.getSafeRawDeltaSeconds()` not `ctx.getSafeDeltaSeconds()`. This decouples motion rate from the SPEED knob's amplification of jitter. (Or, accept the visual SPEED scaling of jitter by leaving it but explicitly clamp `dt` to the 33 ms WDT ceiling — `getSafeDeltaSeconds` already does this at `EffectContext.h:1101-1108`.)
4. **Drop discrete percussion-trigger rate jams.** `isSnareHit`/`isHihatHit` should drive brightness pulses or fade-rate kicks, never the rate variable feeding `m_phase`.

---

## 8. Summary — the canonical motion equation in one sentence

> **The SB reference family does not have a phase equation; it has a sub-pixel scroll accumulator (`scrollAccum += kBaseScrollRate * m_dt` with `kBaseScrollRate = 150.0 px/s`) modulated only by the user-facing `ctx.speed` knob, and audio energy is coupled to trail-fade rate or pixel brightness — never to motion rate.**

The broken-4 invented a parametric wave model (`sinf(x - phase)`), drove its phase rate from heavy-band energy through a Spring follower, multiplied by `240.0f`, and used SPEED-scaled `dt` — none of which has any SB ancestry. The "spazz" is an inevitable consequence of feeding noisy audio energy into a phase-rate accumulator with a 240× gain.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-04-30 | agent:embedded-system-engineer | Created. SSA3 motion-model audit of 7 SB reference files vs the four `240.0f`-multiplier effects (ChevronWaves, LGPInterferenceScanner, LGPWaveCollision, LGPStarBurst). |
