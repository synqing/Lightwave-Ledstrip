---
abstract: "Canonical doctrine for what audio signals drive WAVE PHASE / POSITION (motion, not colour or brightness) on K1, distilled from firmware-v3/docs/audio-visual/. Establishes: heavy_bands -> Spring is the only sanctioned speed driver, the proven phase accumulation formula (m_phase += speedNorm * 240 * smoothedSpeed * dt; wrap at 628.3), the PLL-style beat-locking pattern (free-run oscillator + tempo-confidence-gated phase correction), the rule that percussion triggers MUST decay (never instant phase jumps), and that chroma is colour, not motion. Read when designing or fixing audio-reactive motion in any wave/ripple/scanner effect. Written 2026-04-30 as part of the spazz redesign for ChevronWaves, ChevronWavesEnhanced, SnapwaveLinear, LGPWaveCollision."
---

# SSA-2: Canonical Audio -> Visual Motion Mapping (Spazz Redesign)

**Date:** 2026-04-30
**Author:** claude:opus-4-7 (1M context)
**Scope:** Motion only. Colour, brightness, and texture are out of scope.
**Status:** Doctrine extracted from existing audio-visual reference docs. No new prescription.
**Sources:**
- `firmware-v3/docs/audio-visual/audio-visual-semantic-mapping.md` v2.1.0
- `firmware-v3/docs/audio-visual/audio-visual-contract-surface.md` v1.2.0
- `firmware-v3/docs/audio-visual/MUSICAL_LOGIC_CANONICAL_MODEL.md` (2026-04-28)
- `firmware-v3/docs/audio-visual/IMPLEMENTATION_PATTERNS.md` v1.0.0
- `firmware-v3/docs/audio-visual/VISUAL_PIPELINE_MECHANICS.md`
- `firmware-v3/docs/audio-visual/AUDIO_OUTPUT_SPECIFICATIONS.md`
- `firmware-v3/docs/audio-visual/TROUBLESHOOTING.md`
- `firmware-v3/docs/audio-visual/ADR_2026-03-25_FIRST_CLASS_ONSET_SURFACE.md`

---

## 1. Confidence Statement

Confidence is **HIGH** that the canonical doctrine for motion exists, is consistent across docs, and is unambiguous. The doctrine is concentrated in `IMPLEMENTATION_PATTERNS.md` Patterns 1, 3, 9, with `TROUBLESHOOTING.md` § 1 explicitly diagnosing the "Spastic / Jerky Motion" symptom as a doctrine violation. Multiple files cross-reference the same `m_phase += speedNorm * 240.0f * smoothedSpeed * dt;` formula and cite ChevronWavesEffect.cpp lines 111-125 as the reference implementation.

---

## 2. Canonical Audio-Signal -> Motion-Parameter Mapping

The doctrine is **layered**: a free-running phase oscillator is the only sanctioned source of wave motion. Audio modulates the *speed* of that oscillator, never the phase value directly except in one narrow PLL-style correction case.

### 2.1 Mapping table (motion only)

| Motion parameter | Canonical audio source | Smoothing path | Pattern reference |
|---|---|---|---|
| Phase advance rate (`speedMult`) | `heavy_bands[1]` + `heavy_bands[2]` mean (300-1200 Hz, rhythmic bass + low-mids) | Direct `heavy_bands` -> `Spring` (stiffness 50, mass 1, critically damped). NO extra smoothing. | IMPL §1; VPM §4 |
| Phase value (`m_phase`) | None directly. Phase is a **free-running accumulator** advanced by `speedMult`. | Wrap at 628.3 (= 100 * 2π) for float-precision retention. | IMPL §3; VPM §4 |
| Phase correction toward beat (optional, when tempoConfidence high) | `beatPhase()` (== `onset.phase01`, == `MusicalGridSnapshot.beat_phase01`) | Gated PLL-style P-only correction with τ ≈ 100 ms; ONLY engaged when tempo lock is held with hysteresis (lock >0.6, unlock <0.4) | ChevronWavesEffectEnhanced.cpp:173-207 |
| Wave direction sign | None (deterministic structural choice). `sin(k*dist - phase)` = outward; `sin(k*dist + phase)` = inward. | N/A | TROUBLESHOOTING §2; IMPL §4 |
| One-shot perturbations to motion (collision flash, edge boost, ripple spawn radius=0) | `isSnareHit()`, `isKickHit()`, `isHihatHit()`, `hasOnsetEvent()` (single-frame `fired` flags) | **Decay envelope only** — set state variable to 1.0 on `fired`, multiply by ~0.88 per frame (≈80 ms decay). Never directly add to `m_phase`. | IMPL §9; ADR onset §Surface Definition |
| Beat-locked oscillation (size pulse, brightness pulse — NOT phase) | `beatPhase()` (0..1 within beat) | Use as input to `sinf(beatPhase * 2π)` or `1 - beatPhase`. | IMPL §9 |
| Phase jitter / direct phase modulation | **Forbidden** as a doctrine. No reference effect modulates `m_phase` per-frame from raw audio. | N/A | TROUBLESHOOTING §1 |

### 2.2 Why heavy_bands and not bands or chroma

> "`heavy_bands[]` are already smoothed by ControlBus (80ms rise / 15ms fall). The Spring adds natural momentum. Total latency: ~200ms." — `TROUBLESHOOTING.md` §1

> "Speed modulation, ambient movement, slow breathing -> heavy_bands[]" / "Beat-synced visuals, flash triggers, percussion response -> bands[]" — `AUDIO_OUTPUT_SPECIFICATIONS.md` §6 table

`bands[]` are responsive (attack 64 ms, release 528 ms); `heavy_bands[]` are heavily damped (attack 144 ms, release 1056 ms). The doctrine is that *motion speed must be a slow-changing variable* — heavy_bands feed Spring, Spring feeds the phase accumulator, and that whole chain has a single ~200 ms response time. Using `bands[]` (or worse, raw chroma) as a speed driver violates the doctrine because the chain becomes too reactive — every transient becomes a velocity spike — and that is what users perceive as "spazzing".

### 2.3 The proven phase formula

`firmware-v3/docs/audio-visual/IMPLEMENTATION_PATTERNS.md` §3:

```cpp
// PHASE ACCUMULATION FORMULA (CRITICAL - DO NOT MODIFY)
float speedNorm = ctx.speed / 50.0f;
m_phase += speedNorm * 240.0f * smoothedSpeed * dt;
if (m_phase > 628.3f) m_phase -= 628.3f;
```

Constants:
- `240.0f` rad/s base rate at speedNorm=smoothedSpeed=1.0 (≈38 cycles/sec full speed).
- `628.3f` = 100 × 2π (wrap point — preserves float precision over long runs).
- `0.408f` = 256/628.3 (only when converting `m_phase` to `sin8()` integer form).

The header comment "(CRITICAL - DO NOT MODIFY)" is the doctrine's verbatim wording. Adding extra audio terms to this expression — for example a per-frame `+ kickPulse * something` — is not part of the doctrine and is not present in any reference implementation.

---

## 3. Beat-Locking Doctrine

### 3.1 Default mode: free-run

The doctrine's default is a **free-running oscillator with audio-modulated speed**. Beat tracking is NOT used to set `m_phase`. From `TROUBLESHOOTING.md` §4:

> "**Symptom: Beat Sync Drifting Over Time** ... **Root Cause:** Using raw audio timing instead of K1 beat tracker. **Solution:** Use `ctx.audio.beatPhase()` for beat-locked animation." (Quote: lines 252-267)

Note that the cited fix is for *beat-locked animation* (e.g. brightness pulse), not for *wave phase*. Wave phase is free-run by default — see ChevronWavesEffect.cpp lines 47-48 and 137 — and the beat tracker is consulted only optionally as a phase-correction hint.

### 3.2 PLL-style correction (optional, conditional)

`ChevronWavesEffectEnhanced.cpp:173-207` is the single reference implementation of PLL-style beat-lock for wave phase. The pattern is:

1. **Always advance `m_phase` from the free-run oscillator** (`m_phase += speedNorm * 240 * smoothedSpeed * dt`).
2. **Test tempo confidence with Schmitt trigger hysteresis**: lock when `tempoConfidence > 0.6`, release when `< 0.4`. This prevents lock chatter near the threshold.
3. **Drop the lock immediately when audio drops** (`!hasAudio` -> `m_tempoLocked = false`) to avoid "ghost lock" — phase drifting toward a stale beatPhase value when the music stops.
4. **When locked, apply P-only correction** with τ ≈ 100 ms, computed once per frame (not per-pixel):
   ```cpp
   float phaseError = targetPhase - m_chevronPos;
   if (phaseError > HALF_DOMAIN) phaseError -= PHASE_DOMAIN;   // wrapped error
   if (phaseError < -HALF_DOMAIN) phaseError += PHASE_DOMAIN;
   const float correctionAlpha = 1.0f - expf(-dt / 0.1f);
   m_chevronPos += phaseError * correctionAlpha;
   ```
5. **Wrap `m_phase` AFTER correction** (handles both negative and overflow).

`onset.beat.reliable` and `onset.downbeat.reliable` are documented as requiring "live audio plus tempo confidence >= 0.25" (`ADR_2026-03-25` § Reliability Rules). Effects "should treat `reliable=false` as a hint to fall back to tempo or lower-risk behaviour" (`audio-visual-contract-surface.md` §3 Onset reliability contract). For motion specifically, "lower-risk behaviour" means: drop the lock and continue free-running, never snap.

### 3.3 What NOT to do for beat-lock

The doctrine implies (by absence) that the following are NOT canonical:
- Setting `m_phase = beatPhase * PHASE_DOMAIN` directly (instant snap — produces visible jumps).
- Using `beatPhase` without a confidence gate (low-confidence audio produces erratic motion).
- Applying PLL correction when `audio.available == false` (drift toward zero or last-known phase).
- Per-pixel computation of `correctionAlpha` (`ChevronWavesEnhanced` comment "Compute ONCE per frame, not per pixel").

---

## 4. Percussion-Triggered Motion

### 4.1 Doctrine: decay envelope, never instant phase jump

The first-class onset surface (`ADR_2026-03-25_FIRST_CLASS_ONSET_SURFACE.md`) defines six channels — `beat`, `downbeat`, `transient`, `kick`, `snare`, `hihat` — each with `fired`, `strength01`, `level01`, `ageMs`, `intervalMs`, `sequence`, `reliable`. `fired` is a *single-frame* pulse flag. `level01` is the *continuous held/decayed level*.

The doctrine for using these in motion is `IMPLEMENTATION_PATTERNS.md` §9 and the template at §10:

```cpp
if (ctx.audio.isSnareHit()) {
    m_collisionFlash = 1.0f;     // instant attack
}
m_collisionFlash *= 0.88f;       // ~80 ms decay at 60 fps
```

**The `fired` flag sets a state variable to 1.0; the state variable decays each frame; the state variable is then read by other code (brightness gain, edge sharpness, ripple spawn).** It is never added to `m_phase`. There is no reference implementation in the audio-visual docs that modulates wave phase directly from a percussion trigger.

### 4.2 Permitted percussion -> motion couplings (per the docs)

| Use | Source | Doctrinal example |
|---|---|---|
| Spawn an event (e.g. ripple, particle) at radius=0 | `isKickHit() / isSnareHit() / isHihatHit() / hasOnsetEvent()` | IMPL §5 ripple spawn; §9 |
| Boost edge sharpness / pattern frequency for ~80 ms | decay-envelope variable, e.g. `m_edgeBoost` | IMPL §9 line 723 |
| Trigger collision flash / brightness boost | decay-envelope variable, e.g. `m_collisionFlash` | IMPL §10 lines 881-884 |
| Bump `targetSpeed` upward briefly | feed into Spring as `+ snareLevel * gain`, NOT directly into m_phase | LGPWaveCollisionEffect.cpp:133-145 |

### 4.3 Doctrinal relationship between `level01` and `fired`

`fired` is the trigger; `level01` is the envelope. Effects that need a *continuous* percussion signal should read `level01` (which is held/decayed by the renderer-side onset surface) rather than re-implementing the decay locally. Effects that need an *event* should read `fired`. Either way, the audio side does not produce raw phase or position values — it produces magnitudes that the effect must turn into an envelope before mixing into motion.

### 4.4 Reliability gate

Per `audio-visual-contract-surface.md` §3 Onset reliability contract:

- `snare` and `hihat` "may still carry fallback transport values, but must report `reliable=false` when they are not backed by the native detector path."
- Effects "should treat `reliable=false` as a hint to fall back to tempo or lower-risk behaviour."

For motion, the "lower-risk behaviour" interpretation is: **do not let an unreliable percussion trigger perturb motion at all**. Wave phase is the primary visual driver — it cannot afford spurious kicks.

---

## 5. Chroma / Tonal Information -> Motion

### 5.1 Doctrine: chroma drives COLOUR, not motion

The doctrine across all four reference documents is unambiguous: chroma is a *colour* / *palette-offset* signal, never a motion signal. From `IMPLEMENTATION_PATTERNS.md` §7 ("Chroma-Based Color Modulation"):

> "Use the 12-bin chroma analysis to drive color changes based on musical pitch."

`chroma[]` and `heavy_chroma[]` appear in the canonical mapping table (`audio-visual-semantic-mapping.md` §ControlBus Signal Inventory) under Harmonic Signals with the visual implication "Color/mood changes" (Quick Reference table line 444-452).

`MUSICAL_LOGIC_CANONICAL_MODEL.md` §5 confirms the empirical usage: 44 effects consume `chroma()`, 19 consume `heavyChroma`, 10 consume `rootNote()`, 10 consume `chordConfidence()`. These are colour and palette consumers, not motion consumers.

### 5.2 The one apparent counter-example, and why it is the bug being fixed

`SnapwaveLinearEffect.cpp:107-143` does drive position from chroma:
```cpp
oscillation += chromaVal * sinf(timeMs * BASE_FREQUENCY * freqMult);
```
Each of 12 chroma bins contributes a `sin(t * f_i)` term where `f_i = BASE_FREQUENCY * (1 + 0.5 * i)` (i.e. 12 incommensurate pure tones added in real time, then `tanh`-limited). This is the *opposite* of the doctrine — it makes motion a real-time deterministic function of chord content, with no smoothing layer between chroma and `oscillation`. The chroma values themselves are autorange-normalised at audio rate (per `MUSICAL_LOGIC_CANONICAL_MODEL.md` §2.4 — autoranger floor 0.0025, plus chromagram fold of 60 detectors), so any chord change produces a discrete jump in the bin weights, which produces a discrete jump in motion. This is exactly the mechanism `audio-visual-semantic-mapping.md` §The Binding Trap warns about and exactly the symptom described in `TROUBLESHOOTING.md` §1.

### 5.3 If chord content must affect motion at all

The only doctrinal couplings between harmony and motion that appear in the docs are *behaviour-level*, not value-level:
- `shouldDriftWithHarmony()` accessor (contract surface §3) selects a *behaviour* (`DRIFT_WITH_HARMONY`) at the layer-4 selection stage of the adaptive architecture (semantic mapping §Adaptive Response Architecture).
- "Major / minor chord detection for warm/cool color shifts" — colour, not motion (IMPL §7).
- Chord-change events qualify as Harmonic novelty, which the saliency framework treats as a *colour/mood* mover (§Musical Saliency Analysis table), not a motion mover.

There is no doctrinal pattern where `chroma[i]` directly multiplies a `sinf()` whose phase advances at audio rate. The closest sanctioned pattern is using a *smoothed* dominant-bin index (250 ms time constant — IMPL §7) as a colour offset, which is value-level but applied to hue, not phase.

---

## 6. Anti-Patterns Specific to Motion (verbatim)

### 6.1 The smoothing-stack jitter (TROUBLESHOOTING §1)

> **Symptom: Spastic/Jerky Motion**
>
> **Description:** Effect motion appears choppy, jittery, or nervous even with music that should produce smooth motion.
>
> **Root Cause:** Too many smoothing layers stacked together, creating accumulated latency and phase conflicts.
>
> ```
> heavyBass() -> rolling avg -> AsymmetricFollower -> Spring
>                    ~160ms          ~150ms              ~200ms = 630ms TOTAL LATENCY
>
> The smoothing layers fight each other, causing oscillation and jitter.
> ```
>
> **Solution:** Use `heavy_bands` directly into Spring - NO extra smoothing.

### 6.2 The catch-up-lurch (TROUBLESHOOTING §1)

> **Symptom: Motion "Catches Up" in Lurches**
> ...
> **Root Cause:** Excessive smoothing latency (600ms+) causes phase to accumulate, then release suddenly.

### 6.3 The bass-lurch (TROUBLESHOOTING §1)

> **Symptom: Motion Lurches on Bass Hits**
> ...
> **Root Cause:** Using raw audio bands without Spring physics.

### 6.4 Beat-sync drift (TROUBLESHOOTING §4)

> **Symptom: Beat Sync Drifting Over Time**
> ...
> **Root Cause:** Using raw audio timing instead of K1 beat tracker.

### 6.5 Frame-rate dependence (TROUBLESHOOTING §4)

> **Symptom: Effect Runs at Wrong Speed on Different Frame Rates**
> ...
> **Root Cause:** Not using delta time for animation.
> ```cpp
> // WRONG - frame-rate dependent
> m_phase += 0.1f;
> // CORRECT - frame-rate independent
> m_phase += speedNorm * 240.0f * smoothedSpeed * dt;
> ```

### 6.6 Rigid frequency -> visual binding (semantic-mapping §The Binding Trap)

> "WRONG APPROACH:
> bass -> expansion
> treble -> shimmer
> chord -> hue
> snare -> burst
>
> This is just another cage. Same input = same output. Forever. Predictable."

For motion specifically, the cage manifests as: "every kick produces the exact same speed bump" or "every chord change produces the exact same position jump". The doctrinal answer is hysteresis, novelty-gating, and saliency-driven behaviour selection (semantic-mapping §5 Non-Deterministic Variation).

### 6.7 The rigid `bass -> expansion` example, applied to motion

From `audio-visual-semantic-mapping.md` Quick Reference §DO NOT:

> - Create rigid frequency -> visual bindings
> - Assume bass/beat always present
> - Use same response for all music types
> - Map instantaneously without temporal context
> - Make deterministic input->output functions

"Map instantaneously without temporal context" is the motion-specific killer: any per-frame deterministic function from a noisy audio signal to position will look like spazzing.

### 6.8 Direct chroma into geometry (no doctrinal precedent)

There is no doctrinal anti-pattern *named* "chroma drives motion" in the docs, because the docs do not anticipate that anyone would do it. The absence is itself the doctrine — every reference effect uses chroma for hue/palette only.

---

## 7. Verdict: How the Four Broken Effects Compare to Doctrine

Source-grep evidence in `firmware-v3/src/effects/ieffect/`. Each effect is judged against §2-§6 above.

### 7.1 ChevronWavesEffect.cpp — **CONFORMS** (textbook canonical)

- Spring(50, 1) initialised at line 47-48. ✓ (matches IMPL §1)
- `heavyEnergy = (heavy_bands[1] + heavy_bands[2]) / 2.0f` — matches IMPL §1 verbatim. ✓
- `targetSpeed = 0.6f + 1.2f * heavyEnergy` — within doctrinal range "Conservative 0.6x to 1.4x". The 1.2 factor pushes it to 1.8 max; clamped to 2.0 at line 135. Marginally aggressive but not a violation. ✓
- `m_chevronPos += speedNorm * 240.0f * smoothedSpeed * dt;` — canonical phase formula. ✓
- `isSnareHit()` used at line 153 (likely as a colour/flash trigger, not phase). ✓
- **No direct phase modulation from raw audio.** ✓

This is the *reference implementation* the docs cite (lines 111-125). If it is spazzing on real music, the cause must be either (a) `heavy_bands` itself behaving differently than the docs assume (audio pipeline issue, not effect issue), or (b) something OUTSIDE the §2.1 mapping is perturbing motion. Hypotheses for SSA team: check whether `targetSpeed` is being replaced at audio rate by something not visible in this grep, or whether the renderer's `dt` is irregular.

### 7.2 ChevronWavesEffectEnhanced.cpp — **CONFORMS, plus optional PLL** (textbook PLL doctrine)

- Free-run oscillator: `m_chevronPos += speedNorm * 240.0f * smoothedSpeed * dt` at line 190. ✓
- Hysteresis Schmitt trigger on `tempoConfidence` (lock 0.6, unlock 0.4) lines 184-186. ✓
- Lock cleared on audio drop (line 180). ✓ (avoids "ghost lock")
- P-only correction with τ=100 ms when locked, lines 192-207. ✓
- Wrapped phase error computed correctly (lines 197-200). ✓
- `correctionAlpha` computed once per frame, not per pixel (line 205, comment line 203). ✓
- `isSnareHit()` at line 139 used as event trigger. ✓

This is the canonical PLL pattern. Same caveat as 7.1 — if motion still feels wrong, the bug is upstream of this code or in a path not yet inspected.

### 7.3 LGPWaveCollisionEffect.cpp — **CONFORMS to motion doctrine** (audio drives speed via Spring)

- Spring(50, 1) initialised at line 39-40. ✓
- `bassEnergy = ctx.audio.heavyBass()` at line 140 (averages heavy_bands[0]+[1]). ✓ heavy_bands per doctrine.
- Spring update at line 156. ✓
- `m_phase += speedNorm * 240.0f * smoothedSpeed * dt` at line 164. ✓ canonical formula.
- Phase wrap at line 165. ✓
- `isSnareHit()` at line 123, `isHihatHit()` at line 133 — used as triggers, not direct phase mods. ✓
- `VALIDATION_PHASE` and `VALIDATION_SPEED` macros at lines 170-171 — telemetry only.

The motion driver itself looks doctrinal. If the effect spazzes, the cause is likely in the trigger path (lines 123-145) — i.e. how the snare/hihat triggers feed back into `speedTarget` before the Spring. Specifically: line 150 comment says "modulated by bassEnergy ... and speedTarget (from hi-hat)" — if the hi-hat trigger is being added to `speedTarget` *before* the Spring (rather than as a separate decay envelope), that is the violation: the Spring then has to chase a discontinuous target.

### 7.4 SnapwaveLinearEffect.cpp — **VIOLATES doctrine** (chroma drives motion at audio rate)

- `oscillation += chromaVal * sinf(timeMs * BASE_FREQUENCY * freqMult)` for each of 12 chroma bins (lines 124-130). **VIOLATION** of §5.1: chroma drives position directly, with no smoothing between chroma and the geometric output.
- Each of 12 incommensurate sinusoids summed -> non-periodic motion that re-shapes whenever any `chroma[i]` changes weight, which on K1 is every 8 ms hop period.
- `tanh(oscillation * TANH_SCALE)` at line 140 limits amplitude but **does not smooth time-derivatives** — the position can still change arbitrarily fast at any instant.
- No Spring. No phase accumulator. No `heavy_bands`. No `dt` in the motion path (only in the per-zone RMS follower at line 220, which is for brightness, not motion).
- `BASE_FREQUENCY` and `PHASE_SPREAD` are configured constants — the violation is structural, not a tuning problem.
- Energy gate at line 115 (`rms < ENERGY_GATE_THRESHOLD -> return 0`) is a step function that produces a hard discontinuity at the threshold crossing. Doctrine says use `silentScale` (continuous fade with τ ≈ 190 ms) — see `MUSICAL_LOGIC_CANONICAL_MODEL.md` §4.6 — not a binary gate.

This is the textbook anti-pattern. The fix per doctrine is to: (a) introduce a free-run phase accumulator using the canonical formula, (b) drive its `speedMult` via Spring from `heavy_bands`, (c) optionally use chroma's *dominant bin* (smoothed at 250 ms — IMPL §7) as a colour or hue offset, NOT as a position term. The "snap" character is allowed to come from `tanh`-shaped envelope on a single phase signal, not from summing 12 audio-rate sines.

### 7.5 Summary table

| Effect | Speed driver | Phase driver | Percussion handling | Verdict |
|---|---|---|---|---|
| ChevronWavesEffect | heavy_bands[1..2] -> Spring | canonical free-run | trigger flag, decay envelope | **CONFORMS** |
| ChevronWavesEffectEnhanced | heavy_bands[1..2] -> Spring | free-run + PLL (gated) | trigger flag, decay envelope | **CONFORMS** |
| LGPWaveCollisionEffect | heavyBass -> Spring | canonical free-run | trigger flag (likely OK; verify it does not feed Spring target as discontinuity) | **CONFORMS, audit trigger path** |
| SnapwaveLinearEffect | (none) | `sum(chroma[i]*sin(t*f_i))` direct | hard `rms` gate, no decay | **VIOLATES** §5.1, §6.1, §6.6, §6.8 |

---

## 8. Open Questions

1. **ChevronWaves and LGPWaveCollision conform on the surface but reportedly spazz.** Either (a) `heavy_bands` themselves are no longer behaving per the doctrinal "80 ms rise / 15 ms fall" because of upstream pipeline changes, or (b) something between Spring output and the LED render is introducing a discontinuity, or (c) the user-reported "spazz" is actually the percussion trigger envelope decay being too aggressive (0.88/frame at 120 Hz is ~30 ms half-life, much shorter than the 60 fps `~80 ms` the doctrine quotes — frame-rate dependence smuggled in via the constant).
2. Constant `0.88` per frame: doctrine says "~80ms decay at 60fps". At 120 fps, `0.88^120 ≈ 2e-7` over 1 sec, so half-life is ~5.4 frames ≈ 45 ms, not 80 ms. Effects that adopted this from the docs verbatim are running 1.78x faster decay than the doctrine intends. This is not a doctrine violation but a doctrine-execution bug.
3. SnapwaveLinear has no precedent in the audio-visual docs for what *should* drive its motion. The doctrinal answer is to redesign — there is no "fix the existing chroma->position math" path that conforms.

---

## 9. References

- `firmware-v3/docs/audio-visual/IMPLEMENTATION_PATTERNS.md` §1 (Audio-Reactive Speed Pattern), §3 (Phase Accumulation), §9 (Beat-Synchronized Effects), §10 (Complete Effect Template)
- `firmware-v3/docs/audio-visual/VISUAL_PIPELINE_MECHANICS.md` §4 (Phase Accumulation Formula), §6 (Smoothing Engines)
- `firmware-v3/docs/audio-visual/AUDIO_OUTPUT_SPECIFICATIONS.md` §6 (Heavy Bands vs Regular Bands), §7 (Effect Context Access Methods)
- `firmware-v3/docs/audio-visual/TROUBLESHOOTING.md` §1 (Motion Jitter), §2 (Direction), §4 (Timing/Sync)
- `firmware-v3/docs/audio-visual/audio-visual-semantic-mapping.md` §The Binding Trap, §Musical Intelligence Principles, §Quick Reference DO NOT
- `firmware-v3/docs/audio-visual/audio-visual-contract-surface.md` §3 (Audio access in effects, First-class onset semantics, Onset reliability contract), §6 (Contract Rules for Effect Authors)
- `firmware-v3/docs/audio-visual/MUSICAL_LOGIC_CANONICAL_MODEL.md` §4.2 (Tempo and beat fields), §4.6 (silentScale), §5 (Effect consumer surface)
- `firmware-v3/docs/audio-visual/ADR_2026-03-25_FIRST_CLASS_ONSET_SURFACE.md` (Surface Definition, Reliability Rules)

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-30 | claude:opus-4-7 (1M context) | Created. SSA-2 deliverable for spazz_redesign_2026-04-30. Distilled canonical motion mapping from firmware-v3/docs/audio-visual/ — heavy_bands -> Spring is the only sanctioned speed driver, canonical phase formula `m_phase += speedNorm*240*smoothedSpeed*dt`, PLL-style beat-lock pattern with hysteresis, percussion-as-decay-envelope rule, chroma-is-colour-not-motion rule. Verdict on the four broken effects: ChevronWaves and ChevronWavesEnhanced conform; LGPWaveCollision conforms with a flagged audit on the trigger path; SnapwaveLinear violates by driving position directly from chroma at audio rate (textbook anti-pattern from semantic-mapping §The Binding Trap and TROUBLESHOOTING §1). |
