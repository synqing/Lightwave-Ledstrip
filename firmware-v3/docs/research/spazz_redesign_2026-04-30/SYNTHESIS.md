---
abstract: "Synthesis of 10 parallel SSA investigations into the music-driven spazz/jerk bug afflicting ChevronWavesEffect, ChevronWavesEffectEnhanced, SnapwaveLinearEffect, LGPWaveCollisionEffect. Identifies convergent root causes (triple-multiplication of audio on phase rate; audio-modulated tanh slope; binary-trigger step changes; per-effect secondary bugs). Surfaces a doctrine conflict (K1 written doctrine endorses the broken pattern; SB historical reference disagrees). Presents redesign decision points for Captain."
---

# Spazz Redesign — Synthesis

**Date**: 2026-04-30
**Branch**: `feature/synergy-topology-phase-0-1`
**Bug**: ChevronWavesEffect, ChevronWavesEffectEnhanced, SnapwaveLinearEffect, LGPWaveCollisionEffect — all spazz/jerk during music playback. Symptom: "randomly accelerates forward (edge to centre) and uncontrollably jerks back and forth." Music-only; silence is fine. A spatial-aliasing clamp at 2.5 rad on phase increment was tried and made it worse — ruling out aliasing.
**Source SSAs**: SSA1–SSA10 in this directory.

---

## §1 Convergent root causes (the SHARED bug)

Eight independent SSAs converge on overlapping mechanisms. Confidence high.

| # | Mechanism | SSAs | Severity |
|---|---|---|---|
| 1 | **Triple-multiplication of audio on phase rate** — `speedNorm × dt × smoothedSpeed` all carry audio modulation, compounding bass jitter into phase | SSA-10, SSA-4 | Critical |
| 2 | **Audio-modulated tanh slope** — broken effects do `tanhf(v × (k0 + k1·audio))`; working effects do `tanhf(v × k_fixed) × audioGain`. Slope change ≡ whole-strip flicker | SSA-4 | Critical |
| 3 | **Percussion triggers (1-frame binary pulses) drive continuous parameters as step changes** — 1-frame whole-strip jumps in `tanhScale 2→5`, `m_speedTarget = 1.6`, `m_collisionBoost = 1.0` | SSA-8 | Critical |
| 4 | **Frame-rate-dependent decay in LGPWaveCollision:137** — `m_speedTarget *= 0.95 + 1.0·0.05` is fixed-per-frame, not dt-corrected. Sustained hi-hats sawtooth `m_speedTarget` between 1.40 and 1.60 every 7–8 frames | SSA-8 | Critical |
| 5 | **Spring transient velocity spikes 7× steady-state** — Spring(50,1) doesn't overshoot in position, but its velocity spike on bass kicks injects discontinuities into anything reading or differentiating it | SSA-6 | High |
| 6 | **`heavy_bands` shallow smoothing (~80ms τ); the deeper `UpdateFromHop` envelope is dead code on K1** — documented-vs-actual asymmetry that has been silent since ESV11 landed | SSA-5 | High |
| 7 | **Inappropriate audio signal for motion rate** — SB historical (SSA-3) and audio API design (SSA-9) both indicate `motionFlow`/`beatPhase`/time should drive RATE, not bass. Bass should drive amplitude only | SSA-3, SSA-9 | High |
| 8 | **dt scaled by speedFactor on Spring path in ChevronWavesEnhanced:169** — Spring is detuned 3–25× by `ctx.speed` setting. Other 3 effects use `rawDt` correctly | SSA-7 | Medium (Enhanced-specific) |
| 9 | **AUTO_SPEED rewrites `ctx.speed` per-frame from `liveliness`** at RendererActor:1813-1828 — adds another audio-modulated knob into the multiplication chain. SSA-5 claims `liveliness=0` on K1 (LWLS-only signal); SSA-4 sees the rewrite happening. **Open conflict — see §3.** | SSA-4, SSA-10, SSA-5 | High if active |

---

## §2 Per-effect dominant secondary bugs (SSA-4 + SSA-2 + SSA-10)

Each broken effect carries its OWN problem on top of the shared mechanisms.

### ChevronWavesEffect
- `freqBase = 0.25` (working effects use 0.12–0.20). Spatial frequency too high for the strip.
- Speed gain 1.2× in `0.6 + 1.2·heavyEnergy` (working norm 0.6–0.8×).
- `isSnareHit()` called **per-pixel** inside 160-LED loop (line 153), flipping `tanhScale 2.0→5.0` for one frame = whole-strip step change.

### ChevronWavesEffectEnhanced
- Spring uses **scaled `dt`** at line 169 — should be `rawDt`. At default speed the Spring is 3× softer than tuned.
- PLL tempo-lock acquisition snaps phase across the full domain when `tempoConfidence` crosses the 0.6 Schmitt boundary.
- All other ChevronWaves issues inherited.

### SnapwaveLinearEffect
- Position formula is structurally broken: `tanh(Σ chroma_i · sin(t · multi_freq_i) × 3) × rms × 79`.
- Multi-frequency oscillator with amplitudes (`chroma_i`) updating every audio hop = non-Lipschitz.
- Dot can teleport ±79 LEDs in one frame.
- Hard `rms < threshold` step gate (line 115) instead of smooth `silentScale`.
- Integer-quantised to `distInt` then baked into history trail = quantisation jitter persists in trail.
- **Needs structural redesign, not tuning.**

### LGPWaveCollisionEffect
- `sinf(k·x − φ) + sinf(k·x + φ)` is by trig identity `2·cos(k·x)·cos(φ)` — a **STANDING WAVE**. Phase modulates whole-strip amplitude rather than translating the pattern. Phase jitter shows as flicker, not motion.
- Frame-rate-dependent `m_speedTarget` decay (line 137) — **the documented sustained-hi-hat sawtooth bug** quantified by SSA-8.
- All other shared mechanisms inherited.

---

## §3 Doctrine conflict — Captain decision point

**SSA-2 says**: K1/LightwaveOS written doctrine across 4 docs (TROUBLESHOOTING, IMPLEMENTATION_PATTERNS, AUDIO_REACTIVE_EFFECTS_ANALYSIS, audio-visual-semantic-mapping) **declares ChevronWavesEffect.cpp:111-125 IS the canonical reference** for the `m_phase += speedNorm × 240 × smoothedSpeed × dt` pattern. ChevronWaves "conforms to written doctrine."

**SSA-3 says**: This pattern has **zero SB ancestry**. Canonical SB motion is `scrollAccum += 150.0f × dt` modulated **only by user `ctx.speed`, never by audio energy**. Audio drives appearance (trail-fade, brightness), not motion. SB references contain NO sin/cos phase accumulator anywhere.

**Resolution**: K1 doctrine codifies the broken pattern as canon by **circular self-citation** — ChevronWaves cites itself as the reference for the design. The doctrine drifted away from SB historical without cross-checking the upstream architecture it claims to derive from.

**TROUBLESHOOTING.md §1 itself names "Spastic / Jerky Motion"** as the doctrine-violation symptom. The doctrine knows this failure mode exists.

**Decision needed**: which doctrine wins?
- **(A)** Honour written K1 doctrine, treat the bug as an implementation issue inside the doctrinal pattern, fix per-effect secondary issues only.
- **(B)** Honour SB historical principle ("time drives motion, audio drives appearance"), redesign the 4 effects from scratch around time-driven phase + audio-driven amplitude/colour.
- **(C)** Hybrid — keep the phase accumulator pattern but eliminate the audio multiplications on rate; let audio drive ONLY the visual envelope (brightness/trail/colour). This is what SSA-10 recommends.

---

## §4 The redesign sketch (SSA-10 + cross-references)

If Captain picks (B) or (C), the canonical pattern becomes:

```
// ── Per frame ─────────────────────────────────────────────
const float rawDt = enhancement::getSafeDeltaSeconds(ctx.rawDeltaTimeSeconds);
const float speedNorm = ctx.speed / 50.0f;  // user knob only

// Audio amplitude — SINGLE smoother, ONE source
float audioAmp = ctx.audio.heavyBass();              // 0..1
audioAmp = m_ampFollower.update(audioAmp, rawDt);    // AsymmetricFollower(rise=0.04, fall=0.20)

// Phase rate — TIME-DRIVEN, user-knob modulated, NOT audio-modulated
m_phase += BASE_RATE * speedNorm * rawDt;            // BASE_RATE = 240 (or per-effect)
m_phase = fmodf(m_phase, TWO_PI);

// Per-pixel ─────────────────────────────────────────────
const float WAVE = sinf(distFromCentre * freqBase - m_phase);
float v = tanhf(WAVE * FIXED_SLOPE) * 0.5f + 0.5f;   // FIXED slope, not audio-modulated
uint8_t brightness = (uint8_t)(v * 255.0f * intensityNorm * (0.2f + 0.8f * audioAmp));
                                                     // audio multiplies LINEARLY after tanh
```

Plus per-effect work:

| Effect | Specific change |
|---|---|
| ChevronWavesEffect | freqBase 0.25 → 0.18, kill audio-modulated tanh slope, snare → hold-and-decay env, port from LGPStarBurstEffect template |
| ChevronWavesEffectEnhanced | Spring dt → rawDt, PLL clamp on lock-acquire delta, otherwise as basic |
| LGPWaveCollisionEffect | Replace `sin(kx−φ) + sin(kx+φ)` standing-wave formula with traveling-wave model, dt-correct line 137 decay, snare/hihat → enveloped, not stepped |
| SnapwaveLinearEffect | Drop `tanh(Σ chroma·sin)` formula entirely. Replace with chroma-centroid position follower per SSA-10. Smooth `silentScale` gate. Linear trail-fade. |

Reference port target: **`LGPStarBurstEffect.cpp`** (SSA-4 finding) — disciplined radial wave with all the right patterns.

---

## §5 Open questions Captain must answer before code touches tree

1. **Doctrine** (§3): which option (A/B/C)? Recommendation: **(C) hybrid** — preserves visual identity, breaks the multiplication chain, doesn't fight ratified doctrine.

2. **`liveliness` conflict** (SSA-5 vs SSA-4): is `liveliness` actually 0.0 on K1, or does AUTO_SPEED have an alternative driver? If liveliness is 0, the AUTO_SPEED path doesn't compound audio modulation on K1 (one less mechanism). Need source-level confirmation before final implementation. Want me to dispatch an SSA-11 to nail this down?

3. **`ctx.speed` as user knob vs auto-driven**: should `ctx.speed` continue to be auto-rewritten by RendererActor at all, or should AUTO_SPEED be opt-out for these 4 effects? If audio rate-modulation is forbidden, AUTO_SPEED contradicts that.

4. **Snapwave intent**: the original SB `light_mode_snapwave` semantic vs SSA-10's chroma-centroid follower vs alternative — what visual identity should Snapwave have post-redesign?

5. **`is*Hit()` envelope time constants**: SSA-8 recommends τ_attack ≈ 12ms, τ_decay ≈ 180–300ms for the hold-and-decay pattern. Captain hardware-A/B before merge per `feedback_test_them_all.md` — yes/no?

6. **Implementation order**: fix all 4 in one pass, or fix one (most likely to convince) first and iterate? Recommendation: fix `LGPWaveCollisionEffect` first — it has the cleanest single-cause bug (line 137 frame-rate-dependent decay) and the standing-wave fix is mathematically obvious.

---

## §6 What is NOT in scope for this fix

- Fixing the K1 doctrine documents themselves (SSA-2 found doctrine endorses the broken pattern). Doctrine update is a separate task; should wait until Captain ratifies the new motion model from this redesign.
- Adding `heavy_bands_xsmooth[]` to ControlBus (SSA-5 recommendation 3) — broader blast radius, separate task.
- Replacing `Spring` class globally with `AsymmetricFollower` (SSA-6 recommendation) — the 4 broken effects can adopt locally without touching the shared class.
- Resolving the `liveliness=0` LWLS-only signal architectural issue (SSA-5 open question).
- The `audioConfidence/liveliness` implications for OTHER effects on K1 (SSA-5 flagged: "several effects read these and may misbehave").

---

## §7 Hardware test plan (when implementation lands)

Per `feedback_test_them_all.md` and `feedback_hardware_test_before_commit.md`:

1. Build new firmware, flash K1v1 (`/dev/cu.usbmodem1101`).
2. Test order: LGPWaveCollision (cleanest fix) → ChevronWaves → ChevronWavesEnhanced → Snapwave.
3. Per effect: bass-heavy track, sustained hi-hat track, vocal-driven track. Confirm no spazz, no jerk, no flicker.
4. Compare K1v1 (new) vs K1v2 (still bin*bin pure squaring + old motion) for I-1 contrast preservation.
5. If any effect still spazzes, do NOT commit — iterate.

---

## §8 SSA index (this directory)

| SSA | Topic | File |
|---|---|---|
| 1 | Canonical motion doctrine | `SSA1_canonical_motion_doctrine.md` |
| 2 | Audio→visual semantic mapping for motion | `SSA2_audio_visual_motion_mapping.md` |
| 3 | SensoryBridge reference motion model | `SSA3_sb_reference_motion_model.md` |
| 4 | Working LGP wave effects audit | `SSA4_working_lgp_motion_audit.md` |
| 5 | ControlBus / AudioActor signal smoothness | `SSA5_audio_signal_smoothness.md` |
| 6 | Spring class numerical analysis | `SSA6_spring_numerical_analysis.md` |
| 7 | dt path and frame timing | `SSA7_dt_path_and_frame_timing.md` |
| 8 | Percussion trigger semantics | `SSA8_percussion_trigger_semantics.md` |
| 9 | EffectContext audio API enumeration | `SSA9_effect_context_audio_api.md` |
| 10 | Cross-effect differential diagnosis + redesign | `SSA10_differential_diagnosis_and_redesign.md` |

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-30 | Claude (claude-sonnet-4-6) | Created — synthesised 10 parallel SSA returns into convergent root-cause list, doctrine-conflict flag, redesign sketch, and Captain decision points. |
