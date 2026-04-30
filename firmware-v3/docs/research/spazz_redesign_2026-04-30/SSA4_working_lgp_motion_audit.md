---
abstract: "SSA-4 audit of working LGP/wave motion code (8 effects sampled, 12 inspected) versus the four spazzing effects (ChevronWaves, ChevronWavesEnhanced, SnapwaveLinear, LGPWaveCollision). Identifies the dominant working pattern (heavyBand or pre-smoothed energy → Spring → 240*dt phase, freqBase ~0.12-0.20, audioGain MULTIPLY before tanh) and the divergent practices in the broken four: amplified speed range (1.2x heavyEnergy gain), audio-coupled tanh sharpness, integer-quantised dot position (Snapwave), unsmoothed standing-wave summation (WaveCollision), and excessive spatial frequency (0.25). Recommends modelling the broken effects after LGPStarBurstEffect or LGPInterferenceScannerEffect."
---

# SSA-4: Working LGP / Wave Motion Audit

**Author:** agent:embedded-system-engineer
**Date:** 2026-04-30
**Scope:** Read-only audit of `firmware-v3/src/effects/ieffect/` to identify the dominant working motion pattern and contrast it against the four spazzing effects.

---

## Executive Summary

Eight working effects with phase-driven outward/wave motion were sampled in detail (12 inspected total). All converge on a small set of disciplined practices. The four broken effects deviate from the consensus on three to five axes each. The single highest-leverage divergence is the **broken effects feeding speed/sharpness/position from signals that have NOT been smoothed end-to-end**, while the consensus is to drive every visible-motion variable through at least one of: ControlBus pre-smoothed accessor (`heavyMid`, `heavyBass`), EMA shim, AsymmetricFollower, or critically-damped Spring.

---

## Working effects sampled

| Effect | Audio→Speed signal | Smoothing chain (speed) | Phase multiplier | freqBase | audioGain combine |
|---|---|---|---|---|---|
| `LGPInterferenceScannerEffect` | rolling avg of `heavyMid()` per-hop | rolling-avg → AsymFollower → Spring(rawDt) | `240.0f * smoothedSpeed * dt` | 0.20 / 0.35 (interfering) | `interference * audioGain` (multiply, then tanh) |
| `LGPInterferenceScannerEffectEnhanced` | rolling avg of `heavyMid()` per-hop | rolling-avg → AsymFollower → Spring(dt) | `240.0f * smoothedSpeed * dt` | 0.16-0.24 / 0.28-0.38 | same as above |
| `LGPStarBurstEffect` | `heavyBass()` per-frame, EMA(τ=50ms, rawDt) | EMA → Spring(rawDt) | `240.0f * smoothedSpeed * dt` | **0.12** (Nyquist-safe, explicit comment) | additive `star * audioGain + burstFlash * 0.8`, then tanh |
| `LGPStarBurstEffectEnhanced` | `sqrt(subBass)*1.5 + sqrt(heavyBass)*1.5` blend | AsymFollower(subBass) → mix → Spring(rawDt) | `240.0f * smoothedSpeed * dt` | 0.12 | same as StarBurst plus PLL phase correction (rawDt) |
| `LGPPhotonicCrystalEffect` (v8) | `heavyBand[1]+[2]/2` direct | Spring(rawDt) only (rolling avg + AsymFollower used for BRIGHTNESS, not speed — explicit v8 fix) | `240.0f * speedMult * dt` | integer sin8, lattice quantised | `scale8(qadd8(...))`, no float saturation |
| `BPMEffect` (v2) | `heavyBand[1]+[2]/2`, EMA(τ=50ms, rawDt) | EMA → Spring(rawDt) → `* sceneSpeedMul` | `240.0f * speedMult * dt` | 0.12 (`waveFreq`) | additive ring overlay via `qadd8` (8-bit clipped) |
| `LGPHolographicEffect` | (none — purely time-driven) | n/a | `m_phase += speedNorm * 0.02f` (per-frame, dt-free) | 0.05 / 0.15 / 0.30 / 0.60 layered | `tanhf(layerSum / numLayers)` after sum |
| `LGPMoireSilkEffect` | (none — purely time-driven) | n/a | `m_phase += 0.012 + 0.050*speedNorm` (per-frame) | 0.180 / 0.198 (close ratio for moiré) | `tanhf(g1*g2 * 2.2)` |

Additional spot-checks confirming the same pattern: `LGPSpiralVortexEffect.cpp:32` (`m_phase += speedNorm * 0.05f * 60.0f * dt`), `LGPDiamondLatticeEffect.cpp:32` (`m_phase += speedNorm * 0.02f * 60.0f * dt`), `LGPConcentricRingsEffect.cpp:32` (`m_phase += speedNorm * 0.1f * 60.0f * dt`), `LGPGravitationalWaveChirpEffect.cpp:89` (`m_phase1 += chirpFreq * 0.1f`, no audio drive on velocity).

**Files inspected:** 12 working effects + 4 broken effects = 16 source files (plus `AudioReactivePolicy.h`, `Contrast.h`, `RendererActor.cpp::computeSpeedTimeFactor`, `EffectContext.h::deltaTimeSeconds`).

---

## Pattern A — the dominant working motion pattern

The consensus implementation in seven of the working samples (Scanner, ScannerEnhanced, StarBurst, StarBurstEnhanced, PhotonicCrystal v8, BPM, and the rolling-avg variants) is:

```cpp
// 1. SOURCE: pre-smoothed audio band, NOT 12-chroma summation
float heavyEnergy = (ctx.audio.getHeavyBand(1) +
                     ctx.audio.getHeavyBand(2)) / 2.0f;        // already 80/15 ms in ControlBus
//   OR an EMA pre-smoothing pass:
//     m_smooth += (raw - m_smooth) * (1 - exp(-rawDt / 0.05))

// 2. NARROW TARGET RANGE (max ~1.4x, not 2x)
float targetSpeed = 0.6f + 0.8f * heavyEnergy;                 // 0.6..1.4 (PhotonicCrystal/Scanner)
//   ** NEVER ** 0.6f + 1.2f (which yields 0.6..1.8)

// 3. SPRING WITH rawDt (audio-coupled signal, NOT speed-scaled dt)
float smoothedSpeed = m_speedSpring.update(targetSpeed, rawDt);
if (smoothedSpeed > 1.4f) smoothedSpeed = 1.4f;
if (smoothedSpeed < 0.3f) smoothedSpeed = 0.3f;                // floor prevents stalling

// 4. PHASE INTEGRATION with scaled dt (visual motion only)
m_phase += speedNorm * 240.0f * smoothedSpeed * dt;
if (m_phase > 628.3f) m_phase -= 628.3f;                       // wrap at 100·2π

// 5. FREQ BASE: ~0.12 to ~0.20 (Nyquist-safe at 60 fps)
//    Higher than 0.20 amplifies any phase jitter into multi-wavelength visible jumps.

// 6. AUDIO GAIN MULTIPLIES the spatial pattern, THEN tanh saturates ONCE:
float pattern = waveSum * audioGain;
pattern = tanhf(pattern * 2.0f) * 0.5f + 0.5f;
//   ** NEVER ** put audioGain inside the tanh slope: tanh(x * (k0 + k1*audio))
//   That makes the saturation curve itself jitter with audio.
```

**Reference implementation (cleanest):** `LGPStarBurstEffect.cpp:75-172`. Single audio source (`heavyBass`), single EMA with explicit τ and `rawDt`-correct alpha, single Spring on `rawDt`, single phase integration on `dt`, freqBase 0.12 with explicit Nyquist comment, additive audioGain, single tanh saturation.

**The two pure-time effects** (Holographic, MoireSilk) prove the lower bound: NO audio coupling on velocity at all → glassy smooth even at the highest fade rates. They are the "ground truth" for what motion looks like with zero spazz contribution.

---

## Differential vs broken four

| Axis | Working consensus | ChevronWavesEffect | ChevronWavesEffectEnhanced | LGPWaveCollisionEffect | SnapwaveLinearEffect |
|---|---|---|---|---|---|
| Audio→Speed signal | `heavyBand[1..2]` or pre-smoothed equivalent | `heavyBand[1]+[2]/2` (OK) | `heavyBand[1]+[2]/2` (OK) | `heavyBass()` (OK) | n/a (oscillation drives position not speed) |
| Speed gain coefficient | **0.8** (range 0.6..1.4) | **1.2** (range 0.6..1.8) ⚠ | **1.2** (range 0.6..1.8) ⚠ | 0.6 (range 0.7..1.3) ✓ | n/a |
| Spring `dt` argument | `rawDt` (audio-coupled) | `rawDt` ✓ | **`dt`** (speed-scaled) ⚠ | `rawDt` ✓ | n/a (no spring) |
| Phase integration | `240.0f * smoothedSpeed * dt` | same ✓ | same ✓ | same ✓ | none — `oscillation = sum chroma * sin(timeMs)` ⚠ |
| freqBase | 0.12 to 0.20 | **0.25** ⚠ | **0.25** ⚠ | 0.15 ✓ | n/a (no spatial freq) |
| Tanh sharpness coupling | constant slope (e.g. `tanhf(pattern * 2.0)`) | **`tanhf(chevron * (2 + 4*energy + 3*snare))`** ⚠ | **same plus snare-driven boost** ⚠ | constant `tanhf(int * 2.0)` ✓ | `tanhf(osc * TANH_SCALE)` ✓ |
| Wave geometry | translating (`sin(k·x − φ)`) | translating ✓ | translating ✓ | **standing wave** `sin(kx−φ) + sin(kx+φ)` ⚠ — phase modulates amplitude not position, so phase jitter shows as flicker, not motion | dot position via `int(amp * 79)` ⚠ — integer quantisation amplifies sub-LED chroma jitter |
| PLL beat correction | only when present, `rawDt`-based (StarBurstEnh) | n/a | uses `dt` for `expf(-dt/τ)` ⚠ | n/a | n/a |
| Energy-of-chroma chain | NOT used for speed (PhotonicCrystal v8 explicit fix) | rolling avg of 12·`applyContrast(chroma)` → AsymFollower → BRIGHTNESS only | same | same | applies `applyContrast` per chroma bin in colour computation only |
| Snare/percussion path | smoothed via `dtDecay` then summed | tanh-slope multiplier ⚠ | tanh-slope multiplier + sub-bass blend | additive collision flash with `expf(-dist*0.12)` spatial decay ✓ | n/a |

Legend: ✓ = matches working consensus, ⚠ = divergent.

---

## The smoking gun

There is no single bug; each broken effect has its own dominant divergence on top of one shared anti-pattern. The shared anti-pattern is **using audio energy to modulate the slope of the per-pixel saturating non-linearity**, which the working consensus avoids unanimously.

### Per-effect dominant divergence

1. **ChevronWavesEffect** (`ChevronWavesEffect.cpp:157`)
   ```cpp
   chevron = tanhf(chevron * (tanhScale + 4.0f * energyAvgSmooth)) * 0.5f + 0.5f;
   ```
   The tanh **slope** itself ranges from 2 to 6 baseline plus 3 on snare → 5 to 9. Every pixel's brightness curve flexes with audio energy. Combined with `freqBase = 0.25f` (one wavelength every 25 LEDs) and `targetSpeed = 0.6 + 1.2 * heavyEnergy` (range 1.8x), the pattern visibly snaps each frame the audio energy crosses an inflection point of `tanh`. **This is the pure spazz signature.**

2. **ChevronWavesEffectEnhanced** (`ChevronWavesEffectEnhanced.cpp:169, 205, 225-226`)
   - Spring uses `dt` (speed-scaled) instead of `rawDt` — Spring stiffness becomes audio-rate-dependent.
   - PLL correction `correctionAlpha = 1.0f - expf(-dt / tau)` also uses speed-scaled `dt`.
   - Snare adds `m_snareSharpness * 3.0f` ON TOP of the same `tanhf(chevron * (tanhScale + 4.0f * energyAvgSmooth))` slope. Two audio-coupled slope modulators.
   - Tempo lock Schmitt trigger (0.6/0.4) toggles in/out of PLL phase correction. On lock-acquire, the phase JUMPS by `phaseError * correctionAlpha` (potentially up to half the 628.3 domain). **Visible discontinuity each lock event.**

3. **LGPWaveCollisionEffect** (`LGPWaveCollisionEffect.cpp:208-213`)
   ```cpp
   float waveOutward = sinf(distFromCenter * freqBase - m_phase);
   float waveInward  = sinf(distFromCenter * freqBase + m_phase);
   float waveSum = (waveOutward + waveInward) * 0.5f;          // = sin(k·x) · cos(φ)
   ```
   By trig identity this equals `sin(k·dist) * cos(m_phase)` — a **standing wave**. The `m_phase` does not translate the spatial pattern; it modulates its amplitude across all LEDs in lockstep. Any phase jitter (and there is plenty: speed-coupled rawDt + 240f·dt + spring transients + hi-hat `m_speedTarget = 1.6` discontinuity) appears as **whole-strip flicker**, not propagating motion. Also `m_speedTarget = m_speedTarget * 0.95 + 1.0 * 0.05` (line 137) is NOT dt-corrected — at variable frame rates the decay constant drifts.

4. **SnapwaveLinearEffect** (`SnapwaveLinearEffect.cpp:107-143, 256-267`)
   - The oscillation source is unsmoothed: `oscillation += chromaVal * sinf(timeMs * BASE_FREQUENCY * (1 + i*PHASE_SPREAD))` for each of 12 chroma bins above `NOTE_THRESHOLD`. As chroma activations cross the threshold one bin at a time, terms enter and leave the sum with a sin-magnitude wherever they happen to be — the sum can flip sign in 8 ms.
   - `tanhf(oscillation * TANH_SCALE)` smooths the saturation but not the underlying step.
   - `distInt = (uint8_t)(distance + 0.5f)` integer-quantises sub-LED jitter into hard pixel jumps. With `peakSmoothed * AMPLITUDE_MIX` near 0.5, a 0.025 oscillation wobble (1/40 of full scale) toggles `distInt` by ±2 LEDs.
   - History buffer `pushHistory(distInt, dotColor)` means each frame's quantisation noise is permanently baked into the trail. No future frame can smooth a past lie.

### The shared anti-pattern: audio modulating saturation slope

ChevronWavesEffect and ChevronWavesEffectEnhanced use the broken pattern `tanhf(value * (k0 + k1*audio))`. None of the eight working samples does this. Working effects use a fixed slope and let `audioGain` multiply the value linearly *before* the saturation:

```cpp
// WORKING: linear audio modulation, fixed saturation curve
float pattern = interference * audioGain;
pattern = tanhf(pattern * 2.0f) * 0.5f + 0.5f;

// BROKEN: audio inside the slope itself
chevron = tanhf(chevron * (2.0f + 4.0f * energyAvgSmooth)) * 0.5f + 0.5f;
```

The first form is `f(audio·x)`. The second form is `f_audio(x)` — the function shape changes per frame, so neighbouring frames render different *curves* applied to the same input. Visually that is jitter applied AFTER all upstream smoothing — there is nothing downstream to fix it.

---

## Recommended pattern to copy

**Model the four broken effects after `LGPStarBurstEffect.cpp` (the non-Enhanced).** It is the cleanest, most disciplined, audio-reactive radial wave in the codebase and the closest geometric cousin of all four broken effects:

- Single audio source (`heavyBass`), explicitly EMA-pre-smoothed with `rawDt`.
- Single Spring on `rawDt` only.
- `freqBase = 0.12f` with an explicit Nyquist-safety comment derived from the 240·dt phase rate.
- Additive snare flash with spatial `expf(-dist*0.12)` decay — no global slope perturbation.
- Single `tanhf(pattern * 2.0)` saturation with **fixed slope**.
- Phase wrap at 628.3 (100·2π).
- `dtDecay` for percussion envelopes, never raw `* 0.95f`.

When porting:

1. **ChevronWavesEffect** → drop `(tanhScale + 4.0f * energyAvgSmooth)` for fixed `tanhf(chevron * 2.0f)`; multiply by `audioGain` before the tanh; reduce `freqBase` from 0.25 to 0.15-0.18; reduce speed gain from 1.2 to 0.8.

2. **ChevronWavesEffectEnhanced** → as above, plus switch Spring and PLL `correctionAlpha` to `rawDt`; gate PLL with debounced confidence (additional hold-time, not just Schmitt) so lock-acquire doesn't apply a huge first-frame correction.

3. **LGPWaveCollisionEffect** → replace standing-wave summation with two outward waves at slightly offset frequencies (`sin(k1·x − φ)` and `sin(k2·x − φ·1.2)`) so the phase translates the pattern. Take Scanner's two-frequency formulation; it is exactly the moiré-collision aesthetic without the standing-wave amplitude trap.

4. **SnapwaveLinearEffect** → push the oscillation through an AsymmetricFollower (or EMA, τ ≈ 30 ms) before `tanh`; render with sub-pixel `enhancement::SubpixelRenderer::renderPoint(distFloat)` instead of integer `distInt`; only push to history when `|distFloat - lastDist| > 0.5` so quantisation noise can't accumulate into the trail.

For each of the four, the test is: with audio silent, the effect should be visually IDENTICAL to its no-audio fallback (a slow time-driven oscillation). With audio present, the changes should be smooth modulations of the silent baseline, not a different effect.

---

## Token-relevant references

- Broken sources: `firmware-v3/src/effects/ieffect/{ChevronWavesEffect,ChevronWavesEffectEnhanced,SnapwaveLinearEffect,LGPWaveCollisionEffect}.cpp`
- Working reference: `firmware-v3/src/effects/ieffect/LGPStarBurstEffect.cpp` (75-172)
- v8 reset doc: `firmware-v3/src/effects/ieffect/LGPPhotonicCrystalEffect.cpp` header (1-23)
- Timing semantics: `firmware-v3/src/effects/ieffect/AudioReactivePolicy.h:36-45` (`signalDt` vs `visualDt`)
- Speed-scaled dt: `firmware-v3/src/core/actors/RendererActor.cpp:87-100,1820-1854` (`computeSpeedTimeFactor`, AUTO_SPEED override)
- Contrast curve: `firmware-v3/src/effects/math/Contrast.h:39,56-67`

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-30 | agent:embedded-system-engineer | Created — SSA-4 working LGP motion audit. 16 source files inspected, dominant working pattern documented, per-effect divergence catalogue produced, smoking gun identified (audio inside tanh slope) + per-effect dominant secondary divergence (speed gain 1.2x, freqBase 0.25, standing-wave summation, integer dot quantisation), recommended port target = LGPStarBurstEffect.cpp. |
