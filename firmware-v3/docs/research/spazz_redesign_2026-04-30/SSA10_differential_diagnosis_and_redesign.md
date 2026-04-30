---
abstract: "SSA-10 cross-effect synthesis. Differential diagnosis of the music-only spazz across ChevronWaves, ChevronWavesEnhanced, SnapwaveLinear, LGPWaveCollision. Identifies the shared root cause as DOUBLE-MULTIPLICATION OF AUDIO-DRIVEN SPEED into the phase increment: with FEATURE_AUTO_SPEED enabled, ctx.speed is rewritten every frame from liveliness, then computeSpeedTimeFactor(ctx.speed) scales ctx.deltaTimeSeconds, then the effect re-uses (ctx.speed/50.0) as a multiplier on top of dt — so phase advances proportional to liveliness². Snapwave is exempt from the multiplication but suffers the same liveliness fluctuation through ctx.audio.rms() and chroma. Proposes a canonical motion-update pattern: rawDt for phase, fixed nominal rate, audio-modulated speed clamped to a small ratio (e.g. 0.85..1.30), aggressive critical-damped Spring on the modulator, and a single source-of-truth audio-energy follower per effect. Read when authoring the redesign for the four spazz effects or when designing any future audio-reactive phase-accumulator effect."
---

# SSA-10 — Differential Diagnosis and Redesign of the Music-Only Spazz

**Scope:** ChevronWavesEffect, ChevronWavesEnhancedEffect, SnapwaveLinearEffect, LGPWaveCollisionEffect.

**Confidence:** HIGH on the shared root cause for the three phase-accumulator effects; HIGH on a separate but parallel mechanism for Snapwave; HIGH on the redesign being tractable inside hard-constraints (no heap in render, centre origin preserved, British English, dt-correct).

**Method:** Read all four .h + .cpp end-to-end. Verified Spring / AsymmetricFollower / `getSafeDeltaSeconds` semantics in `effects/enhancement/SmoothingEngine.h`. Verified dt source-of-truth in `core/actors/RendererActor.cpp:1808-1856` and the speed-curve in `RendererActor.cpp:87-100`. Verified `signalDt` / `visualDt` contract in `effects/ieffect/AudioReactivePolicy.h`. Cross-referenced ratification doctrine in `docs/research/EFFECT_FRAMEWORK_RATIFICATION_2026-04-29.md`.

---

## 1 — Per-effect motion-path map

Reading direction for each table: top is the rawest audio source; bottom is what writes into the phase / position accumulator that the human eye sees as motion.

### 1.1 ChevronWavesEffect (id 18) — `m_chevronPos`

| Stage | Source | Smoother | Used in |
|---|---|---|---|
| Audio-energy aggregator | `ctx.audio.getChroma(0..11)` summed via `applyContrast` per hop (line 70-95) | sliding window length 4 (`m_chromaEnergyHist`) | `m_energyAvg`, `m_energyDelta` |
| Energy follower | `m_energyAvg`, `m_energyDelta` | `AsymmetricFollower 200/500ms` and `250/400ms`, `updateWithMood(rawDt, mood)` (line 112-113) | `energyAvgSmooth` (only used in `tanhScale` and `audioGain`, NOT in motion) |
| Phase-speed source | `ctx.audio.getHeavyBand(1) + getHeavyBand(2) / 2` (line 127-129) | `targetSpeed = 0.6f + 1.2f * heavyEnergy` (line 131) | `m_phaseSpeedSpring.update(targetSpeed, rawDt)` |
| Phase-speed smoother | `m_phaseSpeedSpring` `init(50, 1)` critically damped | clamp `[0.3, 2.0]` (line 135-136) | `smoothedSpeed` |
| **Phase update (motion)** | **`m_chevronPos += speedNorm * 240.0f * smoothedSpeed * dt`** (line 137) | none — direct accumulation | LED waveform via `sinf(distFromCenter * 0.25f - m_chevronPos)` |
| Trigger | `ctx.audio.isSnareHit()` | n/a | `tanhScale = 5.0f` for one frame (visual sharpness only, not motion) |

Observation — motion enters the eye through THREE multiplicative inputs all driven by audio: `speedNorm` (auto-speed-derived `ctx.speed`), `smoothedSpeed` (heavy_bands-derived), and `dt` (which is `rawDt * speedFactor(ctx.speed)`).

### 1.2 ChevronWavesEnhancedEffect (id 90) — `m_chevronPos`

| Stage | Source | Smoother | Used in |
|---|---|---|---|
| Chromagram smoothing | `ctx.audio.getHeavyChroma(i)` (per-hop targets, 12 followers, line 82-83, 131-134) | `AsymmetricFollower` array 12-deep, `updateWithMood(rawDt, mood)` | `m_chromaSmoothed[]` |
| Energy aggregator | sum `applyContrast(m_chromaSmoothed[i])` (line 90-101) | sliding window `m_chromaEnergyHist[4]` | `m_energyAvg`, `m_energyDelta` |
| Energy follower | `m_energyAvg`, `m_energyDelta` | `AsymmetricFollower 200/500ms` and `250/400ms` (line 147-148) | `energyAvgSmooth` (visual-only) |
| Phase-speed source | `ctx.audio.getHeavyBand(1) + getHeavyBand(2) / 2` (line 162) | `targetSpeed = 0.6f + 1.2f * heavyEnergy` | `m_phaseSpeedSpring.update(targetSpeed, dt)` (line 169 — uses **scaled** `dt`, NOT `rawDt`) |
| Phase-speed smoother | `m_phaseSpeedSpring` `init(50, 1)` critically damped | clamp `[0.3, 2.0]` (line 170-171) | `smoothedSpeed` |
| **Phase update (free-run)** | **`m_chevronPos += speedNorm * 240.0f * smoothedSpeed * dt`** (line 190) | none | LED waveform |
| **Phase correction (PLL)** | `ctx.audio.beatPhase()` × `PHASE_DOMAIN` if `tempoLocked` (Schmitt 0.6/0.4) (line 193-207) | first-order correction, `tau = 0.1s` | additive nudge to `m_chevronPos` once per frame |
| Trigger | `ctx.audio.isSnareHit()` (line 139) | exponential decay `pow(0.90, rawDt*60)` | `m_snareSharpness` (visual only) |

Observation — same triple-multiplicative motion as 1.1 PLUS a hard PLL correction. With Schmitt hysteresis, when `tempoConfidence` crosses 0.6 the `m_chevronPos` SNAPS toward `beatPhase * 628.3f`. If `beatPhase` itself is noisy, the snap is the jerk.

### 1.3 SnapwaveLinearEffect (id 98) — `distance` (recomputed every frame, NOT accumulated)

| Stage | Source | Smoother | Used in |
|---|---|---|---|
| Energy gate | `ctx.audio.rms()` (line 114) | none — instantaneous | early-return guard (silence = 0) |
| Oscillation source | `ctx.audio.getChroma(0..11)` × `sinf(rawTotalTimeMs * 0.001 * (1 + i*0.5))` (line 124-130) | none — instantaneous | `oscillation` |
| Snap shaping | `tanhf(oscillation * 3.0f)` (line 140) | none | `oscillation ∈ [-1, 1]` |
| Peak amplitude | `ctx.audio.rms()` (line 234) | `AsymmetricFollower 20/200ms`, `update(dt)` where `dt = signalDt(ctx) = rawDt` (line 245) | `peakSmoothed` |
| RMS for fade | `ctx.audio.rms()` (line 217) | `AsymmetricFollower 30/250ms`, `update(rawDt)` (line 220) | dynamic `fadeAmount = 20 + 40*(1-smoothRms)` |
| **Position update (motion)** | **`amp = oscillation * peakSmoothed * 0.7f`** (line 256) → `distance = abs(amp) * 79` (line 265) | none — distance recomputed each frame | LED dot position |
| History | push `distInt` per frame into ring buffer of 40 entries | quadratic age-falloff render | trail (renders previous 40 distances) |

Observation — Snapwave does NOT use a phase accumulator. Its motion is `tanh(weighted_sum_of_chroma_with_per_note_oscillator) * smoothed_rms`. There is **no Spring**, no `ctx.speed` multiplication, no `ctx.deltaTimeSeconds` (it uses `signalDt = rawDt`). The "spazz" mechanism here is structurally different from the other three.

### 1.4 LGPWaveCollisionEffect (id 17) — `m_phase`

| Stage | Source | Smoother | Used in |
|---|---|---|---|
| Chroma energy aggregator | `ctx.audio.getChroma(0..11)` per hop (line 65-70) | window 4 (`m_chromaEnergyHist`) | `m_energyAvg`, `m_energyDelta` |
| `energyDelta` second smoother | `m_energyDelta` | EMA `tau=50ms` `m_energyDeltaEMASmooth` (line 98-107) | `energyDeltaForSmoothing` |
| Energy follower | `m_energyAvg`, `m_energyDeltaForSmoothing` | `AsymmetricFollower 200/500ms` and `250/400ms` `updateWithMood(rawDt, mood)` (line 114-115) | `energyAvgSmooth`, `energyDeltaSmooth` |
| Snare collision boost | `ctx.audio.isSnareHit()` (line 123) sets `m_collisionBoost=1.0` | `dtDecay(0.88, rawDt)` | spatial flash at centre |
| Hi-hat speed boost | `ctx.audio.isHihatHit()` (line 133) sets `m_speedTarget=1.6` | linear EMA `*0.95 + 0.05` per frame (NOT dt-correct) (line 137) | `m_speedTarget` |
| Speed source | `ctx.audio.heavyBass()` (line 140) | `rawSpeedScale = (0.7 + 0.6*bassEnergy) * m_speedTarget` (line 151) clamp `≤1.6` | `speedTargetClamped` |
| Speed smoother | `m_speedSpring` `init(50, 1)` critically damped | clamp `[0.3, 1.6]` (line 156-158) | `smoothedSpeed` |
| **Phase update (motion)** | **`m_phase += speedNorm * 240.0f * smoothedSpeed * dt`** (line 164) | none | wave packet collision via `sinf(d*0.15 - m_phase) + sinf(d*0.15 + m_phase)` |

Observation — same triple-multiplicative motion as 1.1 / 1.2. Plus an **additional non-dt-correct linear EMA** on `m_speedTarget` (line 137: `*0.95 + 0.05` per frame), which is a frame-rate-dependent bug independent of the main spazz.

---

## 2 — Shared construct(s)

The three phase-accumulator effects all write the IDENTICAL update line, modulo variable name:

| Effect | File:line | Update line |
|---|---|---|
| ChevronWaves | `ChevronWavesEffect.cpp:137` | `m_chevronPos += speedNorm * 240.0f * smoothedSpeed * dt;` |
| ChevronWavesEnhanced | `ChevronWavesEffectEnhanced.cpp:190` | `m_chevronPos += speedNorm * 240.0f * smoothedSpeed * dt;` |
| LGPWaveCollision | `LGPWaveCollisionEffect.cpp:164` | `m_phase += speedNorm * 240.0f * smoothedSpeed * dt;` |

Where in all three:
- `speedNorm = ctx.speed / 50.0f` (effect-level computation, line 58 / 70 / 52)
- `dt = enhancement::getSafeDeltaSeconds(ctx.deltaTimeSeconds)` (line 108 / 126 / 95)
- `ctx.deltaTimeSeconds` is computed in `RendererActor.cpp:1834-1852` as:
  ```
  speedFactor = computeSpeedTimeFactor(ctx.speed)        // sqrt-curve, 0.04..1.0
  ctx.deltaTimeSeconds = rawDeltaSeconds * speedFactor
  ```
- `ctx.speed` itself is REWRITTEN every frame in `RendererActor.cpp:1808-1828` from audio:
  ```
  liveliness = m_lastControlBus.liveliness         // [0..1] audio-derived, fluctuates
  autoBase = 10.0f + 30.0f * liveliness            // [10..40]
  ctx.speed = clamp(autoBase * userTrim, 1, 50)    // userTrim 0.7..1.3
  ```
- `smoothedSpeed` is the Spring output, target = `0.6 + 1.2 * heavyBass` or equivalent — also audio-derived, also fluctuates, also clamped.

Snapwave does not contain this construct. Snapwave's motion is `tanh(audio) * smoothed(rms) * 79` — a direct position rather than a phase accumulation. Its spazz mechanism is described in §3 hypothesis H4.

---

## 3 — Hypothesis elimination matrix

I evaluate the leading candidate explanations for "spazz forward + jerk back during music, fine in silence" against each effect's actual code.

Legend: A = construct present and matches symptom; P = construct present but does not match symptom; N = construct absent. The "verdict" column is read from the row, not column-by-column.

| # | Hypothesis | Chevron | Chevron Enh | Snapwave | WaveCollision | Verdict |
|---|---|---|---|---|---|---|
| H1 | Spring under-damped or noisy → speed oscillation drives wave forward then backward | A (spring stiffness 50, target driven by `heavyBands`) | A | N (no Spring) | A (spring stiffness 50, target driven by `heavyBass`) | **Explains 3/4 but NOT Snapwave.** Cannot be the SOLE cause given the user reports identical symptom across all four. Spring is critically damped by construction (`damping = 2*sqrt(k*m)`); cannot itself oscillate. CO-CAUSAL but not root. |
| H2 | dt clamp `[0.0001, 0.05]` causes jitter at frame-rate spikes | A — `getSafeDeltaSeconds` used | A | A — same | A | **Plausible at most as a contributing factor, NOT root.** A 50ms clamp produces at most a single-frame velocity spike, not a sustained "spazz forward + jerk back." Clamp is symmetric, behaviour unchanged in silence (silence still has dt). |
| H3 | Phase wraparound `> 628.3f` causes a discontinuity that reads as jerk-back | N (Chevron has no wrap) | A (line 210-211 — wraps, no aliasing because computed inside sin) | N | A (line 165 — wraps, same) | **Eliminated.** Wrap is invisible because `sin(x) == sin(x - 2πN)`. Verified by reading Chevron (no wrap, same symptom). |
| H4 | Audio energy fluctuates ANY high-rate signal feeding the position; in music it modulates wildly, in silence it's near-zero so nothing moves | A — heavyBass + auto-speed | A — heavyBass + auto-speed + beatPhase | A — chroma sum + rms (oscillation built from chroma directly) | A — heavyBass + hi-hat boost + auto-speed | **PRESENT IN ALL FOUR.** Matches "fine in silence" because all audio-driven multipliers collapse to base/zero in silence. CANDIDATE ROOT. |
| H5 | PLL correction in Enhanced snaps `m_chevronPos` toward noisy `beatPhase` when Schmitt fires → jerk-back | N | A (line 193-207) | N | N | **Eliminated as root.** Only Enhanced has it; user reports identical symptom across all four. Real but a SECONDARY problem on top of root. |
| H6 | snare/hi-hat triggers spike `m_speedTarget` / `m_collisionBoost` | A — `tanhScale` only (visual not motion) | A — `m_snareSharpness` (visual not motion) | N — no snare/hi-hat trigger | A — `m_speedTarget=1.6` jumps motion (line 134) | **Partial.** WaveCollision uniquely has a hi-hat motion trigger; could explain extra jerk in WaveCollision specifically. Not present in the other three's MOTION path → not root. |
| H7 | Frame-coupled (non-dt-correct) decay on hi-hat speedTarget multiplies the hi-hat jolt across slow frames | N | N | N | A (line 137 — `m_speedTarget = m_speedTarget*0.95 + 1.0*0.05` is frame-coupled, NOT dt-corrected) | **Eliminated as shared root.** WaveCollision-only. Genuine secondary bug worth fixing while we're in there. |
| H8 | Spatial-aliasing on phase increment (already tested by Captain) | n/a — already ruled out | n/a | n/a | n/a | **Eliminated by Captain's test.** |
| H9 | TRIPLE-MULTIPLICATION of `ctx.speed` into the phase increment: once via `speedNorm`, once via `dt = rawDt * speedFactor(ctx.speed)`, and once via `smoothedSpeed` (Spring target also audio-derived) | A — line 58 + 108 + 134-137 | A — line 70 + 126 + 169-190 | N (does not use ctx.speed in motion) | A — line 52 + 95 + 156-164 | **PRESENT IN ALL THREE PHASE-ACCUMULATOR EFFECTS, MATCHES SYMPTOM.** With `FEATURE_AUTO_SPEED`, `ctx.speed = clamp(10 + 30*liveliness, 1, 50)`; effects multiply `(ctx.speed/50)` × `speedFactor(ctx.speed)` × `0.6+1.2*heavyBass` × `dt`. The first three terms are all audio-modulated and they ALL pull in the same direction. **CANDIDATE ROOT for 3/4.** |
| H10 | Snapwave-specific: chroma-driven oscillation `Σ chroma_i * sin(t * (1+0.5i))` is itself wildly non-stationary in music; multiplying by `tanh(× 3)` then by `peakSmoothed` produces large-amplitude fast-direction-change for the dot | N | N | A — exactly the structure in `computeOscillation()` line 124-140 | N | **EXPLAINS SNAPWAVE.** `oscillation` is a sum of 12 sinusoids at incommensurate frequencies whose AMPLITUDES change every audio hop. tanh× 3 saturates the output so direction reversals are sharp. Multiplied by `rms`, in music the dot position can flip ±79 LEDs in a single frame. In silence, the function returns 0 immediately at line 116. |

### Hypothesis ranking

| Effect | Root | Co-causal | Eliminated |
|---|---|---|---|
| ChevronWaves | H9 (triple mul) | H4 (audio modulation) | H3, H5, H6, H7, H8, H1-as-sole-cause |
| ChevronWaves Enhanced | H9 (triple mul) + H5 (PLL on noisy beatPhase) | H4 | H3, H6, H7, H8 |
| Snapwave | H10 (oscillation structure) | H4 | H1, H3, H5, H6, H7, H8, H9 |
| WaveCollision | H9 (triple mul) | H4, H6 (hi-hat boost), H7 (frame-coupled hi-hat decay) | H3, H5, H8 |

H9 + H10 is the **cross-effect synthesis** the parent agent asked for: not one bug but two adjacent bugs sharing a single deeper failure mode. Both are *un-bounded modulation of a motion variable by an audio-derived envelope*. The chevrons/collision do it via `(ctx.speed/50) * speedFactor(ctx.speed) * (0.6+1.2*heavyBass)`. Snapwave does it via `tanh(Σ chroma * sin(...))`.

---

## 4 — The shared bug (single paragraph)

In music, all four effects let the audio envelope drive the **derivative** of the visible position (phase rate, or for Snapwave the position itself) through more than one independent multiplicative path simultaneously. For ChevronWaves / ChevronWavesEnhanced / LGPWaveCollision, the derivative is `dx/dt = speedNorm × 240 × smoothedSpeed × dt`, and `speedNorm`, `smoothedSpeed`, AND `dt` are EACH audio-modulated (via `liveliness` → `ctx.speed`, via `heavyBass` → spring target, via `computeSpeedTimeFactor(ctx.speed)`). The product is approximately `liveliness × bassEnergy × sqrt(liveliness)`, i.e. the visible motion rate scales roughly with `liveliness¹·⁵ × bassEnergy` rather than with bassEnergy alone. When the music has a transient, those three audio quantities transiently align and reinforce, and `m_chevronPos` jumps; when they fall, the spring can produce a (critically-damped, but still finite-tau) recovery that briefly drops `smoothedSpeed` below 1.0 while `liveliness` is still recovering, so the visual stalls or appears to lurch back. Snapwave's failure is structurally analogous: `distance = |tanh(Σ chroma × time_oscillator) × rms × 0.7| × 79`, where the inner sum is a non-stationary multi-frequency oscillator whose amplitudes change every hop. There is no temporal coupling — the dot position can teleport ±79 LEDs in one frame because the oscillation function itself is non-Lipschitz under audio change. Silence kills everything because every multiplicative path collapses to zero at its source. **The shared invariant being violated is "audio modulates ONE thing — usually intensity or saturation — and motion advances at a bounded, audio-trimmed rate."**

---

## 5 — Redesign principles

These are numbered so the redesign sketch in §6 can cite them.

1. **Single audio modulator per motion variable.** Phase rate is modulated by exactly ONE audio quantity, and that quantity passes through exactly ONE smoother. The auto-speed `ctx.speed` rewrite, the `computeSpeedTimeFactor`-scaled `dt`, and the in-effect Spring of `heavyBass` form THREE compounding modulators. Pick one. *Rationale:* the user-visible spazz is a multiplicative product of audio-correlated signals; the only way to make that product behave like its weakest factor is to delete the others. Cited from the SB doctrine in EFFECT_FRAMEWORK_RATIFICATION_2026-04-29.md §3.A.1 #2 ("Single-stage post-mode smoothing").

2. **Phase advances on RAW dt, never on speed-scaled dt.** Visual motion that accumulates a phase MUST use `signalDt(ctx)` / `getSafeRawDeltaSeconds()`, NOT `getSafeDeltaSeconds()`. `ctx.deltaTimeSeconds` is documented in `AudioReactivePolicy.h` and `EffectContext.h:899` as user-trim-scaled; it is appropriate for VISUAL-only quantities like trail-fade rate, NOT for phase increment when `ctx.speed` is itself being read by the effect. *Rationale:* this is what Snapwave already does (`signalDt`). It is the single change that ELIMINATES the triple-multiplication in the three phase effects.

3. **`ctx.speed` enters the phase update at most ONCE.** Either via the dt path (let the Renderer scale dt and don't multiply by speedNorm in the effect), OR via an explicit speedNorm multiplier on a raw-dt phase update (and then ALSO disable auto-speed-on-this-effect — see #4). Never both. *Rationale:* the auto-speed system in `RendererActor.cpp:1808-1828` is a SECOND control loop on top of the effect's internal audio modulation. Two control loops driving the same variable is the textbook recipe for instability. The four spazz effects pre-date the auto-speed feature being live and were not redesigned for it.

4. **Audio modulator clamped to a small ratio.** The motion rate scalar applied to a nominal phase rate must clamp to a TIGHT band — recommend `[0.85, 1.30]` — not the current `[0.3, 2.0]` (Chevrons) or `[0.3, 1.6]` (WaveCollision). *Rationale:* a 6.7× ratio between min and max means the perceived velocity can change by a factor of nearly 7 over one beat. Brain reads that as "spazz." A 1.5× ratio reads as "musically responsive." (Cf. EFFECT_DEVELOPMENT_STANDARD.md §4.2: heavy_bands should pre-smooth audio for visual consumption; effect should not multiply that further.)

5. **Spring stiffness sized for a target settle time, not an arbitrary 50.** All three phase effects share `init(50.0f, 1.0f)`. With critical damping, the time constant is roughly `2 / sqrt(k/m) = 2 / sqrt(50) ≈ 283 ms` to reach steady state. That is too slow on bass kicks (perceived stall) and too fast for sustained energy (perceived flutter). Recommend explicit per-effect tuning: ~150 ms for Chevrons (k≈350), ~80 ms for WaveCollision (k≈1250). *Rationale:* tau-based authoring is mandated in EFFECT_DEVELOPMENT_STANDARD.md §7.3.

6. **Triggers do NOT drive motion.** Snare and hi-hat events should drive VISUAL parameters (sharpness, brightness flash, hue offset) but NOT the phase rate. WaveCollision currently lets `isHihatHit()` boost `m_speedTarget` to 1.6× — this is a per-trigger speed lurch on top of the continuous bass modulation. Remove it from the motion path. *Rationale:* triggers are by nature impulsive and irregular; allowing them to push a momentum-based system (Spring) injects step changes that the Spring then has to smooth — reading visually as exactly the "jerk back" the user reports.

7. **PLL phase corrections, if used at all, must operate on the absolute phase target NOT the current accumulator.** ChevronWavesEnhanced applies a P-only correction `m_chevronPos += phaseError * (1 - exp(-dt/0.1))` whenever `tempoLocked`. If `beatPhase` is itself jittery (it is — beat-tracking confidence Schmitt-trips at 0.6 per memory `firmware_stage1_d14_r3_milestone.md`), this nudge IS the jerk on locked frames. Either the PLL should run a true 2nd-order loop (proportional + integral on velocity) OR be removed and motion left as free-run. *Rationale:* the simplest robust loop is the one that doesn't fight the free-run integrator.

8. **Snapwave specifically: replace per-frame `oscillation = tanh(Σ chroma × sin(t × multi-freq))` with a position smoother.** The current formula is non-Lipschitz under chroma change. Change to: a TARGET position computed from the chroma-energy centroid (one number), then smooth the target with a single `AsymmetricFollower`-on-position. *Rationale:* eliminates the multi-frequency interference that produces ±79-LED frame-to-frame teleports.

---

## 6 — Concrete redesign sketch (canonical motion update)

This is the pseudo-code that ChevronWaves / ChevronWavesEnhanced / LGPWaveCollision should adopt. It is dt-correct, single-modulator, and bounded.

```cpp
// === Per-effect state (header) ============================================
//   float m_phase = 0.0f;
//   enhancement::AsymmetricFollower m_motionEnergyFollower{0.0f, 0.060f, 0.220f};
//                                                   // 60ms attack, 220ms release
//   // No more m_speedSpring -- replaced by AsymmetricFollower above.
//   // No more m_speedTarget hi-hat boost -- triggers do not drive motion (#6).
//
// Recommended constants (per-effect adaptation in §7):
//   constexpr float kNominalPhasePerSec = 240.0f;     // unchanged radians/sec
//   constexpr float kAudioRateMin       = 0.85f;       // (#4)
//   constexpr float kAudioRateMax       = 1.30f;       // (#4)
//   constexpr float kPhaseDomain        = 100.0f * 6.2831853f;  // wrap point
// ==========================================================================

void render(plugins::EffectContext& ctx) {
    // -- Time -------------------------------------------------------------
    // Phase advances on RAW dt -- never on speed-scaled dt (#2).
    const float rawDt = AudioReactivePolicy::signalDt(ctx);
    // ctx.deltaTimeSeconds (visualDt) is reserved for trail-fade and other
    // user-speed-scaled visual rates, not for phase.

    // -- Audio modulator (single source, single smoother) (#1) -----------
    // Choose ONE audio quantity per effect (see §7 for per-effect choice).
    // Here the canonical example uses heavyBass.
    float audioEnergy01 = 0.0f;
    if (ctx.audio.available) {
        audioEnergy01 = ctx.audio.heavyBass();          // already pre-smoothed in ControlBus
        if (audioEnergy01 < 0.0f) audioEnergy01 = 0.0f;
        if (audioEnergy01 > 1.0f) audioEnergy01 = 1.0f;
    }
    // Mood-aware exp follower (replaces Spring; predictable tau, no momentum)
    const float moodNorm = ctx.getMoodNormalized();
    const float energySmoothed = m_motionEnergyFollower.updateWithMood(
        audioEnergy01, rawDt, moodNorm);

    // -- Bounded rate (#4) ------------------------------------------------
    // audioRate ∈ [kAudioRateMin, kAudioRateMax]. NO ctx.speed multiplication
    // here (#3). User trim is applied by the auto-speed system rewriting
    // ctx.speed -- the effect ignores ctx.speed for motion. If the effect
    // wants direct user-speed control, it should disable auto-speed via
    // its EffectMetadata role flag and consume ctx.speed exactly once.
    const float audioRate = kAudioRateMin
                          + (kAudioRateMax - kAudioRateMin) * energySmoothed;

    // -- Phase update (single multiplication path) (#1, #2, #3) ----------
    m_phase += kNominalPhasePerSec * audioRate * rawDt;
    while (m_phase >= kPhaseDomain) m_phase -= kPhaseDomain;
    while (m_phase <  0.0f)         m_phase += kPhaseDomain;

    // -- Triggers drive VISUAL only (#6) ---------------------------------
    float visualSharpness = 2.0f;                       // base
    if (ctx.audio.available && ctx.audio.isSnareHit()) {
        visualSharpness = 5.0f;                         // visual only -- NOT phase
    }
    // (Hi-hat / kick may drive brightness flash, hue offset, etc. -- never m_phase.)

    // -- Trail fade uses VISUAL dt (intentional) -------------------------
    fadeToBlackByDt(ctx.leds, ctx.ledCount, ctx.fadeAmount,
                    ctx.getSafeDeltaSeconds());         // visualDt OK here

    // -- Render loop (centre-origin invariant unchanged) -----------------
    for (uint16_t i = 0; i < ctx.ledCount && i < STRIP_LENGTH; ++i) {
        float distFromCenter = (float)centerPairDistance(i);
        float wave = sinf(distFromCenter * kFreqBase - m_phase);
        wave = tanhf(wave * visualSharpness) * 0.5f + 0.5f;
        // ... existing colour / brightness mapping, unchanged ...
    }
}
```

**Why this fixes the spazz, mechanically:**

- ONE audio multiplier on rate (`audioRate`), bounded `[0.85, 1.30]`. Visual rate now varies by 1.5× across the dynamic range — perceptually "musical breathing," not "spazz."
- Phase increment is `kNominalPhasePerSec × audioRate × rawDt`. None of the three terms is influenced by `ctx.speed` or by `computeSpeedTimeFactor`. The double-multiplication is structurally impossible.
- AsymmetricFollower (60/220 ms) replaces Spring. Critically damped Spring is fine in principle but its tau was un-tuned (see #5). AsymmetricFollower has explicit named time constants — easier to author against EFFECT_DEVELOPMENT_STANDARD.md §7.3.
- Triggers drive visual properties (sharpness, brightness, hue) but not motion. The "uncontrollable jerk back and forth" cannot occur because there is no per-trigger step input to the motion path.
- In silence, `audioEnergy01 = 0` → `audioRate = 0.85` → motion proceeds at 85% nominal. This means **the effect still moves in silence**, which is preferable to the current freeze-on-silence (which masks the bug rather than fixing it). If freeze-on-silence is desired by Captain, set `kAudioRateMin = 0` instead.

---

## 7 — Per-effect adaptation notes

The redesign in §6 is the canonical pattern. Each effect needs minor adaptations to preserve its visual identity. Here is the per-effect mapping; each cell preserves the effect's identifying VISUAL property without touching the motion path.

### 7.1 ChevronWavesEffect — V-shaped centre-out propagation

| Element | Current | Redesigned |
|---|---|---|
| Audio modulator | `(getHeavyBand(1) + getHeavyBand(2)) / 2` | `ctx.audio.heavyMid()` (already a heavy_bands average; use the helper from EffectContext.h:117) |
| Smoother | Spring(50, 1) with target `0.6 + 1.2*heavyMid` | AsymmetricFollower(60ms / 220ms) on `heavyMid` |
| Rate band | `[0.3, 2.0]` × `speedNorm` × `dt`-scaled | `[0.85, 1.30]` × `rawDt` × `kNominalPhasePerSec` |
| Snare effect | `tanhScale = 5` for one frame (visual) | UNCHANGED — keeps the chevron-snap aesthetic |
| Spatial freq | `freqBase = 0.25` (~25 LED wavelength) | UNCHANGED |
| Hue | `gHue + chromaHue + dist*2 + chevronPos*0.5` | UNCHANGED — the visual identity is "V chevrons of varying hue traveling outward" |

Visual identity preserved: V-shape from `tanh(sin(dist*0.25 - phase))`, hue rotation per LED, snare-driven sharpness.

### 7.2 ChevronWavesEnhancedEffect — Enhanced V chevrons with snare sharpness

Same as 7.1 PLUS:

| Element | Current | Redesigned |
|---|---|---|
| Beat-phase PLL | P-only correction (line 193-207) | **REMOVE** for spazz fix. Keep ChevronWavesEnhanced as "free-run + snare-sharper". A future PLL can be reintroduced as a 2nd-order loop with explicit damping (#7), authored into a separate effect family. |
| Sub-bass tracking | `m_subBassFollower` — currently target NEVER UPDATED (`m_targetSubBass` is initialised to 0 in `init()` and never written elsewhere; line 73, line 60, line 136 reads stale 0) — DEAD CODE | Remove the dead path. If sub-bass differentiation is wanted, add `m_targetSubBass = ctx.audio.heavyBass()` inside the hop block. |
| Snare sharpness | `m_snareSharpness` decays via `pow(0.90, rawDt*60)` with snare reset to 1.0 | UNCHANGED — already correct (visual only, dt-correct) |
| Tempo-lock Schmitt | 0.6/0.4 hysteresis on `tempoConfidence` | DEAD with PLL removal — also remove the Schmitt state |

Visual identity preserved: same chevrons, sharpness still pumps on snare via `m_snareSharpness`. The "Enhanced" qualifier earns its name through the snare sharpness pop, not through the PLL (which was visually indistinguishable from free-run except during jerks).

### 7.3 SnapwaveLinearEffect — Bouncing dot with history trail

Snapwave does NOT fit the §6 phase-accumulator pattern. Apply principle #8 instead.

| Element | Current | Redesigned |
|---|---|---|
| Position formula | `tanh(Σ chroma × sin(t × multi-freq) × 3) × peakSmoothed × 0.7 × 79` | `target_distance = chroma_centroid_signed × peakSmoothed × 0.7 × 79`; `m_distance = positionFollower.update(target_distance, rawDt)`. Position follower e.g. AsymmetricFollower(40ms / 100ms). |
| Chroma centroid | not computed | `centroid = (Σ i*chroma_i) / (Σ chroma_i) - 5.5` → in `[-5.5, +5.5]`, normalise to `[-1, +1]` by `/5.5`. Single-number representation of "where the music is in chromagram." |
| Snap shape | `tanh(... × 3)` saturating at ±1 | UNCHANGED on the OUTPUT side: clamp `m_distance` to ±79; the "snap" comes from the AsymmetricFollower's fast-attack. |
| Time oscillator | `sin(timeMs * 0.001 * (1 + i*0.5))` per chroma bin | **REMOVE.** This is the source of spazz. The dot oscillates because of the sin's, not because of the music. |
| RMS gate | `if rms < 0.05: return 0` | UNCHANGED |
| History trail | 40-frame ring buffer, quadratic age fade | UNCHANGED |

Visual identity preserved: bouncing dot with snap, fading tail, mirrored about centre. The "bounce" used to come from the SUM of 12 phase-shifted sin's at incommensurate freqs (which is a chaotic oscillator); after redesign the bounce comes from the AsymmetricFollower's attack/release behaviour as the chroma centroid moves with the music. Empirically this is what humans expect to see — the dot tracks the dominant note.

### 7.4 LGPWaveCollisionEffect — Counter-propagating wave packets

Same as 7.1 PLUS:

| Element | Current | Redesigned |
|---|---|---|
| Audio modulator | `ctx.audio.heavyBass()` | UNCHANGED (this is appropriate for a wave-collision aesthetic) |
| Hi-hat motion boost | `if isHihatHit: m_speedTarget = 1.6f` (line 133-135) → folds into smoothedSpeed | **REMOVE FROM MOTION PATH** (#6). If hi-hat-driven brightness flash is wanted, route it to a separate visual variable analogous to `m_collisionBoost`. |
| Frame-coupled `m_speedTarget` decay | `m_speedTarget = m_speedTarget*0.95 + 0.05` per frame (line 137) | DEAD with hi-hat removal |
| EMA smoothing on `m_energyDelta` | `tau = 50ms`, line 98-107 | UNCHANGED — already dt-correct, addresses energyDelta jitter |
| Snare collision flash | `m_collisionBoost` set on snare, decays via `dtDecay(0.88, rawDt)` | UNCHANGED — visual only, dt-correct, very nice already |
| Spatial freq | `freqBase = 0.15` (~42 LED wavelength) | UNCHANGED |
| Wave structure | `sin(d*k - phase) + sin(d*k + phase)` (counter-propagating) | UNCHANGED |
| nblend factor | `nblend(leds[i], newColor, 180)` (~70/30) | UNCHANGED |

Visual identity preserved: standing-wave nodes from interference, snare-driven centre flash, counter-propagating wave fronts. The "collision" event reads on snare hits (visual) rather than as a discontinuous motion event (hi-hat).

---

## 8 — Implementation order suggestion (non-binding)

If the parent agent wants to land this in stages without a single big-bang commit:

| Stage | Change | Validation |
|---|---|---|
| 1 | Replace `dt` with `signalDt(ctx)` (i.e. `rawDt`) in the three phase update lines. Remove `speedNorm` from those lines. | If H9 is correct, spazz drops dramatically by this change alone. Single commit, easy revert. |
| 2 | Replace Spring with AsymmetricFollower on the audio modulator; tighten clamps to `[0.85, 1.30]`. | Removes residual flutter. |
| 3 | Remove ChevronWavesEnhanced PLL correction; remove WaveCollision hi-hat motion boost. | Removes the per-effect secondary jerks. |
| 4 | Snapwave redesign per §7.3 — chroma-centroid + position follower. | Snapwave-specific; independent of the phase-accumulator fix. |
| 5 | Hardware A/B test all four effects on K1v2 with the same music corpus. Use TEST.THEM.ALL. matrix per memory `feedback_test_them_all.md`. | Captain decides. |

Stage 1 is the highest-leverage single change. If the user-perceived spazz survives stage 1, the ranking of H9 vs H4 should be re-examined with runtime data (e.g. trace `m_phase` velocity vs `ctx.audio.heavyBass()` vs `ctx.speed` per frame).

---

## 9 — Source-of-truth pointers (verification checklist)

| Claim | Source |
|---|---|
| `ctx.deltaTimeSeconds = rawDeltaSeconds * speedFactor` | `firmware-v3/src/core/actors/RendererActor.cpp:1834-1852` |
| `ctx.speed = clamp(autoBase * userTrim)` rewrite per frame | `firmware-v3/src/core/actors/RendererActor.cpp:1808-1828` |
| `computeSpeedTimeFactor` sqrt-curve, range `[0.04, 1.0]` | `firmware-v3/src/core/actors/RendererActor.cpp:87-100` |
| `signalDt = rawDt`, `visualDt = scaled dt` contract | `firmware-v3/src/effects/ieffect/AudioReactivePolicy.h:36-45` |
| `Spring` is critically damped by construction | `firmware-v3/src/effects/enhancement/SmoothingEngine.h:102-107` |
| `AsymmetricFollower` true-exp formula | `firmware-v3/src/effects/enhancement/SmoothingEngine.h:162-191` |
| `getSafeDeltaSeconds` clamp `[0.0001, 0.05]` | `firmware-v3/src/effects/enhancement/SmoothingEngine.h:304-309` |
| Triple-mul in ChevronWaves | `ChevronWavesEffect.cpp:58, 108, 134, 137` |
| Triple-mul in ChevronWavesEnhanced | `ChevronWavesEffectEnhanced.cpp:70, 126, 169, 190` |
| Triple-mul in LGPWaveCollision | `LGPWaveCollisionEffect.cpp:52, 95, 156, 164` |
| Snapwave non-Lipschitz oscillation | `SnapwaveLinearEffect.cpp:124-140` |
| WaveCollision hi-hat motion boost | `LGPWaveCollisionEffect.cpp:133-138` |
| WaveCollision frame-coupled decay | `LGPWaveCollisionEffect.cpp:137` |
| ChevronWavesEnhanced PLL | `ChevronWavesEffectEnhanced.cpp:193-207` |
| ChevronWavesEnhanced dead `m_targetSubBass` | `ChevronWavesEffectEnhanced.h:73`, `.cpp:60`, `.cpp:136` (no producer) |
| Audio centroid / heavy_bands semantics | `EffectContext.h:106-131` |
| EFFECT_DEVELOPMENT_STANDARD §4.2, §7.3 (referenced) | `firmware-v3/docs/EFFECT_DEVELOPMENT_STANDARD.md` |
| Ratification doctrine #2 (single-stage smoothing) | `firmware-v3/docs/research/EFFECT_FRAMEWORK_RATIFICATION_2026-04-29.md` §3.A.1 row 2 |

---

## 10 — Open questions (for parent agent / Captain)

1. **Auto-speed interaction.** `RendererActor.cpp:1808-1828` rewrites `ctx.speed` for ALL effects when `FEATURE_AUTO_SPEED` is on. The redesign in §6 ignores `ctx.speed` for motion, which means user SPEED knob behaviour for these four effects becomes a no-op for motion (visual fade still scales). Acceptable? Or should the redesign read `ctx.speed` ONCE as a `userRate` multiplier (separately from the audio modulator) so the SPEED knob still works?
2. **Silence behaviour.** `kAudioRateMin = 0.85` means motion continues in silence at 85% nominal. User reported "music-only" spazz; silence behaviour may want to be a deliberate freeze. Captain decision per `feedback_test_them_all.md`.
3. **Snapwave identity.** §7.3 changes the dot's underlying motion model from "12 incommensurate sinusoids of music-modulated amplitude" to "chroma centroid follower." This is a substantive change to the algorithm's character even though the visual surface (bouncing dot with trail) is preserved. Worth a hardware A/B against the original SB `light_mode_snapwave()` behaviour to confirm Captain's intent.
4. **Other phase-accumulator effects.** This SSA covered four. The Effects directory has ~106 effects per EFFECT_FRAMEWORK_RATIFICATION_2026-04-29.md. If H9 is the correct root cause, other effects with the same `m_x += speedNorm * X * smoothedSpeed * dt` construct will exhibit the same symptom under sufficiently aggressive music. A grep audit for the construct is appropriate as a follow-on SSA.
5. **The `dt` clamp at 50 ms (`getSafeDeltaSeconds` MAX 0.05).** At 20 FPS or slower, this clamp triggers and produces a step-function in dt — visible as a single-frame jerk. Currently masks itself in normal operation but if the audio-driven path is fixed and the spazz persists at low frame rates, suspect this. Out of scope here.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-30 | agent:opus-4.7-1M (SSA-10 cross-effect synthesis) | Created. Read all four effects end-to-end (Chevron / ChevronEnhanced / Snapwave / WaveCollision), the canonical motion-update doctrine in `EFFECT_FRAMEWORK_RATIFICATION_2026-04-29.md`, and the dt-source-of-truth (`RendererActor.cpp:1808-1856`, `AudioReactivePolicy.h`, `SmoothingEngine.h`). Diagnosed shared root cause as triple-multiplication of `ctx.speed` into the phase increment for the three accumulator effects (H9), and a separate non-Lipschitz oscillation structure for Snapwave (H10). Both share the deeper failure mode of un-bounded multiplicative audio coupling on a motion variable. Proposed canonical motion-update pattern using `signalDt` (rawDt), single AsymmetricFollower-smoothed audio modulator, tight `[0.85, 1.30]` rate clamp, and triggers routed to visual-only paths. Per-effect adaptations preserve V-chevron / counter-wave / bouncing-dot identities. |
