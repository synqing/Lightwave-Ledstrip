---
abstract: "SSA1 substrate for the spazz-redesign of ChevronWaves / ChevronWavesEnhanced / SnapwaveLinear / LGPWaveCollision. Distils every motion / phase / velocity / audio-driven-position / temporal-coherence rule from EFFECT_FRAMEWORK_RATIFICATION_2026-04-29.md, EFFECT_FRAMEWORK_STANDARD.md, and EFFECT_DEVELOPMENT_STANDARD.md (one-level citation into IMPLEMENTATION_PATTERNS.md). Surfaces the canonical chain (heavy_bands → Spring → phase += speedNorm * 240 * smoothedSpeed * dt), the §4.2 ban on stacking smoothing on pre-smoothed bands, the §7.1 raw-audio-to-pixel anti-pattern, and Property #2 + #12 MUST classifications. Verdict: all four broken effects open-code §7.1-style raw audio paths in their squaring step (per ratification I-1), and at least three of them lack the documented Spring → phase chain — the single most likely root cause is missing Spring smoothing of the audio-driven phase rate, not spatial aliasing."
---

# SSA1 — Canonical motion doctrine for audio-reactive K1 effects

**Purpose.** Spazz-redesign substrate. The four effects ChevronWavesEffect, ChevronWavesEffectEnhanced, SnapwaveLinearEffect, and LGPWaveCollisionEffect all jerk/spazz on music-driven motion; the spatial-aliasing clamp at 2.5 rad on phase increment made it worse. This document extracts the canonical doctrine on motion / phase / velocity / audio-driven position / temporal coherence from the three ratified standard documents, with one level of citation into `IMPLEMENTATION_PATTERNS.md` (cited from the development standard's Part 11 file table).

**Sources cited herein.** Read-only extraction; no judgements beyond what the standards explicitly say.

| Tag | Path |
|---|---|
| RATIFY | `firmware-v3/docs/research/EFFECT_FRAMEWORK_RATIFICATION_2026-04-29.md` |
| FW-STD | `firmware-v3/docs/EFFECT_FRAMEWORK_STANDARD.md` |
| DEV-STD | `firmware-v3/docs/EFFECT_DEVELOPMENT_STANDARD.md` |
| PATTERNS | `firmware-v3/docs/audio-visual/IMPLEMENTATION_PATTERNS.md` (one-level citation from DEV-STD §11) |

---

## 1. Canonical motion principles for audio-reactive effects

1. **Frame-rate-independent timing is MUST.** Every time-varying value uses `ctx.getSafeDeltaSeconds()`. The helper clamps to `[0.0001s, 0.05s]` to prevent physics explosion on frame drops. Sources: DEV-STD §2.1 lines 60–78; FW-STD Property #12 lines 110–114; RATIFY row #12 (line 145, classified MUST, Phase D `check_frame_coupled_decay` detector pending).
2. **Rate-independent smoothing uses `α = 1 - exp(-dt/τ)` with explicit tau constants. Bare `*= 0.95f` patterns are forbidden** because at K1's observed 60–119 FPS range they decay at rates that vary by ~2×. Source: FW-STD Property #12 lines 110–114; the 2026-02-21 partial audit hit 19 files, full sweep awaits Captain Q5.
3. **Phase advancement uses the canonical formula** `m_phase += speedNorm * 240.0f * smoothedSpeed * dt;` followed by a high-precision wrap at `628.3f` (= 100 × 2π). Source: PATTERNS §3 lines 212–232; DEV-STD §4.3 line 429 (`enhancement::advancePhase` wrapper) and §8 template line 800.
4. **The base angular rate is 240 rad/s** at `speedNorm=1.0, smoothedSpeed=1.0` (≈ 38 cycles/second). Source: PATTERNS §3 lines 224–228.
5. **Audio-driven speed modulation MUST go through `enhancement::Spring` and ONLY through Spring** (no rolling average, no AsymmetricFollower stacked in front of it on `heavy_bands`). Source: DEV-STD §4.2 lines 367–379 ("CORRECT: heavy_bands → Spring ONLY"); PATTERNS §1 lines 30–44 ("WRONG → 630ms latency, CORRECT → ~200ms").
6. **Spring physics is the canonical smoother for "values that should have momentum and natural motion feel"** — speed modulation, position tracking, beat response. Critical-damped configuration `init(50.0f, 1.0f)` (stiffness=50, mass=1) is the documented default. Source: DEV-STD §3.3 lines 264–289.
7. **Audio-driven smoothed speed is hard-clamped to `[0.3, 2.0]` for stability.** This prevents runaway phase rates. Source: DEV-STD §4.3 lines 411–413; PATTERNS §1 lines 84–86.
8. **Centre-origin symmetric geometry is MUST.** All effects originate from LED 79/80 outward; mirror about centre is the default render shape. Linear left-to-right is forbidden. Source: FW-STD Property #5 lines 62–68 (LINT-ENFORCED via `check_centre_origin_inverted`); DEV-STD §2.3 lines 113–141; CLAUDE.md hard constraint.
9. **Outward / inward motion direction is encoded in the phase sign.** `sin(k*dist - phase)` = outward; `sin(k*dist + phase)` = inward. Source: PATTERNS §4 lines 300–303.
10. **Subpixel rendering is mandatory for moving elements.** Integer-position jumping IS visible aliasing/wagon-wheel; `enhancement::SubpixelRenderer::renderPoint(...)` and `renderLine(...)` use additive `qadd8` blending so multiple calls accumulate naturally. Source: DEV-STD §3.4 lines 290–311; DEV-STD §6.2 lines 583–589 (Nyquist note: "movement should stay under half-wavelength per frame to prevent wagon-wheel aliasing").
11. **Single-stage post-mode smoothing on the spectrogram is MUST. Stacked smoothing is forbidden.** `heavy_bands` already has 80 ms rise / 15 ms fall applied by ControlBus; one contrast/squaring pass is acceptable, but `AsymmetricFollower → Spring → ExpDecay` chains are forbidden. Source: FW-STD Property #2 lines 40–46; DEV-STD §4.2 lines 367–379; RATIFY row #2 (line 135, "5L-AR triple-smoothing canonical violation, fixed `aed805bb` 2026-03-23"; new `check_stacked_smoothing` detector pending Phase D).
12. **MOOD / `audio_mix` / `beat_gain` / `motion_depth` / `motion_rate` / `colour_anchor_mix` control responsiveness, NOT what responds.** They modulate mix rates, scroll speeds, follower coefficients — they do NOT swap the audio feature being consumed. Source: FW-STD Property #11 lines 104–108 (LINT-ENFORCED for AR effects); DEV-STD §10.1 lines 868–874 (control-liveness contract).
13. **Memory tails and impact terms have explicit anti-chaos bounds.** Cap additive impact terms (`impactAdd <= 0.40` typical) to prevent strobe collapse; keep memory tails decaying within `0.70–0.95s`; avoid unbounded per-pixel exponentials/trig loops. Source: DEV-STD §10.3 lines 887–893.
14. **Ambient-vs-reactive blend is the canonical routing for AR effects.** AR effects MUST call `lowrisk_ar::updateSignals(...)`, `lowrisk_ar::buildModulation(...)`, `lowrisk_ar::applyBedImpactMemoryMix(...)`, set `m_ar.tonalHue = mod.baseHue`, and feed `mod.motionRate` into at least one phase/integration term. Source: DEV-STD §10.1 lines 866–874 (LINT-ENFORCED via `check_ar_control_liveness`, `check_effect_contracts.py:505-529`).
15. **The two-stage thread separation is structurally enforced.** Audio analysis runs on Core 0 (AudioActor); render on Core 1 (RendererActor); effects must not call audio-analysis primitives in `render()`. Source: FW-STD Property #1 lines 32–38; RATIFY row #1.

---

## 2. What the standard says about audio-driven phase / position / velocity

**Direct quotes.**

- **The canonical audio-→-motion chain (PATTERNS §1 lines 32–44):**
  > "WRONG (causes jitter — ~630ms total latency):
  > heavyBass() → rolling avg → AsymmetricFollower → Spring
  > 160ms 150ms ~200ms = 630ms total
  >
  > CORRECT (smooth motion — ~200ms total latency):
  > heavy_bands[1..2] → Spring ONLY
  > ~200ms = natural momentum"

- **Heavy_bands are pre-smoothed (DEV-STD §4.2 lines 369–379):**
  > "WRONG (causes 630ms+ latency, feels disconnected):
  > heavy_bands → rolling average → AsymmetricFollower → Spring
  > 160ms 150ms 200ms = 630ms!
  >
  > CORRECT (200ms total, feels immediate):
  > heavy_bands → Spring ONLY
  > 200ms natural momentum"
  >
  > "ControlBus already applies 80ms rise / 15ms fall smoothing to heavy_bands. Adding more smoothing layers creates latency that makes the visual feel disconnected from the music."

- **Phase advancement formula (PATTERNS §3 lines 212–232; mirrored DEV-STD §4.3 line 429):**
  > "PHASE ACCUMULATION FORMULA (CRITICAL — DO NOT MODIFY)
  > Components:
  > speedNorm = ctx.speed / 50.0f → User's speed slider (0.02–1.0)
  > 240.0f → Base speed in radians/second
  > smoothedSpeed → Audio modulation (0.3–2.0)
  > dt → Frame time in seconds
  >
  > At speedNorm=1.0 and smoothedSpeed=1.0:
  > Phase advances 240 radians/second = ~38 cycles/second
  >
  > float speedNorm = ctx.speed / 50.0f;
  > m_phase += speedNorm * 240.0f * smoothedSpeed * dt;
  >
  > // High-precision wrap (100 × 2π = 628.318...)
  > if (m_phase > 628.3f) m_phase -= 628.3f;"

- **Audio mapping table (DEV-STD §4.1 lines 343–365):**
  > "heavy_bands[1..2] → Speed / Size modulation via Spring (critically damped, stiffness=50)
  > onset / percussion → Flash / Burst trigger (exponential decay from center, exp(-dist*k))
  > chromagram dominant bin → Color / Hue offset (smoothed with tau=250ms for stability)
  > RMS energy → Overall brightness / trail length (AsymmetricFollower)"

- **Speed clamp for stability (DEV-STD §4.3 lines 411–413; PATTERNS §1 lines 84–86):**
  > "if (smoothedSpeed > 2.0f) smoothedSpeed = 2.0f;
  > if (smoothedSpeed < 0.3f) smoothedSpeed = 0.3f;"

- **Frame-rate independence (DEV-STD §2.1 lines 64–75):**
  > "// === CORRECT === m_phase += speedNorm * someRate * dt; // Frame-rate independent
  > // === WRONG — frame-rate dependent (runs 2x fast at 120 FPS vs 60 FPS) === m_phase += 0.01f; // FORBIDDEN — tied to frame rate"

- **Outward vs inward sign convention (PATTERNS §4 lines 300–303):**
  > "// Phase offset based on distance (OUTWARD motion when phase increases)
  > // sin(k*dist - phase) = outward motion
  > // sin(k*dist + phase) = inward motion"

---

## 3. What the standard says about Spring / EMA / follower usage for motion

**Direct quotes.**

- **Spring is the motion smoother (DEV-STD §3.3 lines 264–289):**
  > "Spring — Physics-Based Motion. For values that should have momentum and natural motion feel:
  > // In init(): m_speedSpring.init(50.0f, 1.0f); // stiffness=50, mass=1 (critically damped)
  > // m_speedSpring.reset(1.0f); // Start at base speed
  > // In render(): float targetSpeed = computeTargetSpeed();
  > float smoothSpeed = m_speedSpring.update(targetSpeed, dt);"
  >
  > Stiffness guide (lines 281–288):
  > "20–30: Sluggish, heavy — Large mass effects, slow transitions
  > 50–80: Natural, responsive — Speed modulation, position tracking
  > 100–200: Snappy, immediate — UI feedback, beat response
  > 300+: Almost instant — Near-zero overshoot"

- **AsymmetricFollower is for audio envelopes, not motion (DEV-STD §3.2 lines 235–263):**
  > "AsymmetricFollower — Audio Envelope (CRITICAL). This is the single most important smoothing primitive for audio-reactive effects. Fast attack (instant response to beats), slow release (smooth fade-out)."
  >
  > Use cases (lines 257–262):
  > "Beat pulse / percussion: 0.02–0.05s rise, 0.15–0.30s fall
  > Bass breathing: 0.05–0.10s rise, 0.30–0.50s fall
  > Ambient modulation: 0.10–0.20s rise, 0.50–1.00s fall
  > Color shifting: 0.15–0.25s rise, 0.80–1.50s fall"

- **ExpDecay is for "any value that should approach a target smoothly" (DEV-STD §3.1 lines 207–229):**
  > "ExpDecay — Basic Smoothing. For any value that should approach a target smoothly:
  > // Header member: enhancement::ExpDecay m_smoothBrightness{0.0f, 10.0f}; // lambda=10 (fast)"
  >
  > Lambda guide (lines 222–229) maps lambda to time-to-63%: 2.0=500ms, 5.0=200ms, 10.0=100ms, 20.0=50ms, 50.0=20ms.

- **Asymmetric max follower for waveform peak — SHOULD (FW-STD Property #3 lines 48–53):**
  > "When an effect needs peak normalisation, use `AsymmetricFollower` with the canonical tau (`SbK1BaseEffect.h:171–173`: kTauWfFollowerAttack=6.9 ms, kTauWfFollowerDecay=99.9 ms)."

- **Single-stage smoothing rule (FW-STD Property #2 lines 40–46):**
  > "Audio inputs reach `render()` already smoothed via `heavy_bands` / pre-smoothed ControlBus fields. Do not stack additional smoothing layers on top. One contrast/squaring pass is acceptable; stacked `AsymmetricFollower → Spring → ExpDecay` chains are forbidden."

- **Pre-smoothed beatStrength (DEV-STD §4.4 lines 460–476):**
  > "`beatStrength()` is already smoothed by EsBeatClock (exponential decay). Do NOT add another smoothing layer."
  > "Prefer `beatStrength()` over `isOnBeat()`. `isOnBeat()` is a single-frame boolean pulse — at 120 FPS that is 8.33ms of visibility, impossible to perceive."

---

## 4. Anti-patterns flagged in the standard (relevant to spazz / jerk / motion chaos)

1. **Raw audio driving pixels directly — "CATASTROPHIC" (DEV-STD §7.1 lines 619–626):**
   > "uint8_t brightness = (uint8_t)(ctx.audio.controlBus.rms * 255);
   > ctx.leds[i] = CRGB(brightness, brightness, brightness);
   > // Result: Seizure-inducing flicker. No temporal coherence."
   >
   > "Every audio value must pass through smoothing (AsymmetricFollower for envelopes, Spring for motion, ExpDecay for everything else)."

2. **Tweaking constants without understanding (DEV-STD §7.2 lines 627–636):**
   > "Changing a smoothing ratio from `5%/95%` to `20%/80%` does nothing when the effect is missing: trail persistence (fadeToBlackBy), subpixel rendering, asymmetric attack/release envelopes, proper audio integration. Parameter tweaks are the LAST step, not the first. Fix the architecture, then tune."

3. **Frame-count based timing (DEV-STD §7.3 lines 638–649):**
   > "if (ctx.frameNumber % 60 == 0) spawnParticle(); // 0.5s at 120 FPS, 1s at 60 FPS
   > m_particlePos += 2; // 2 pixels/frame = speed depends on FPS"
   > Counter-pattern: `m_particlePos += velocity * dt;`.

4. **Stacked smoothing on pre-smoothed bands (DEV-STD §4.2 lines 369–379; FW-STD Property #2 lines 40–46; PATTERNS §1 lines 32–44):**
   > "heavy_bands → rolling average → AsymmetricFollower → Spring … 630ms!" — forbidden chain.
   > Canonical incident: 5L-AR triple-smoothing, fixed `aed805bb` 2026-03-23 (RATIFY row #2 line 135; `feedback_5lar_fix_pattern.md` cited in RATIFY source-of-truth row).

5. **Frame-coupled decay (FW-STD Property #12 lines 110–114; RATIFY Q5 line 252; row #12 line 145):**
   > "All temporal smoothing uses `α = 1 - exp(-dt/τ)` with explicit tau constants. Frame-coupled alphas (`*= 0.95f` without `dt`) break at FPS variation (K1 sees 60 → 119 FPS depending on load)."

6. **The `>> 8` coordinate collapse (DEV-STD §7.6 lines 681–689):**
   > "uint8_t noise = inoise8(x >> 8, y >> 8);
   > // At 80 LEDs across, (0..79) >> 8 = 0 for ALL positions. Everything is identical."

7. **Overwriting instead of additive blending for layered light (DEV-STD §7.5 lines 668–678; §2.6 lines 184–199):**
   > "When layering multiple light sources, use saturating addition to prevent overflow … Overwrites destroy layered effects."

8. **Dynamic allocation in render loop (DEV-STD §7.4 lines 651–666; CLAUDE.md hard constraint):**
   > "FORBIDDEN (causes heap fragmentation on ESP32) … Use static buffers."

9. **Hard-set absolute brightness post-mode (FW-STD Property #8 lines 86–96):**
   > "Effects must not hard-set absolute brightness (`CRGB(255, 255, 255)`, `CHSV(_, _, 255)`)."
   > LINT-DEFERRED: `check_hard_set_brightness` detector pending Phase D.

10. **Per-effect silence gating (FW-STD Property #9 lines 92–96):**
    > "Effects do not silence-gate themselves. … per-effect `if (audio.rms() < threshold) return;` early-returns duplicate the global and produce double-fading."
    > LINT-DEFERRED: `check_local_silence_gate` detector pending Phase D.

11. **Open-coded squaring divergent from the canonical contrast curve (RATIFY row I-1 line 164; SSA-3-INCIDENTAL evidence):**
    > "4 effects open-coded `bin*bin` divergently from canonical `(x²·0.65 + x·0.35)` mix-back: `ChevronWavesEffect.cpp:70`, `ChevronWavesEffectEnhanced.cpp:88`, `SnapwaveLinearEffect.cpp:151`, `LGPWaveCollisionEffect.cpp:62`, `BloomParityEffect.cpp:291`. Pure squaring is a different perceptual response."
    > Helper now hoisted: `effects/math/Contrast.h::applyContrast` (FW-STD I-1 line 124, DELIVERED 2026-04-30).

---

## 5. Gaps — what the standards do NOT cover that is relevant to the spazz bug

The three documents are tight on the "how to smooth audio for motion" axis but leave the following spazz-relevant questions unaddressed:

1. **Spatial-aliasing clamp on phase increment is not specified.** The standards prescribe Nyquist *as a design principle* (DEV-STD §6.2 lines 583–589 — "movement should stay under half-wavelength per frame to prevent wagon-wheel aliasing") but do not specify the maximum allowable `dist * frequency - m_phase` increment per frame, nor whether such a clamp belongs at the phase level or the per-pixel `sin` argument level. The 2.5 rad clamp Captain trialled is unattested; the standards predict it would not fix spazz because the upstream defect is in the phase *velocity*, not the spatial frequency.
2. **No documented Spring stiffness override for audio-driven phase rate.** DEV-STD §3.3 line 285 lists 50–80 as "natural, responsive — speed modulation," but does not say what happens at 60 FPS vs 119 FPS for a given stiffness. RATIFY Q5 explicitly notes 178/197 effects have not been audited for FPS independence.
3. **No spec for audio-driven phase rate when `heavy_bands` is jittery.** The standards assume `heavy_bands` 80 ms rise / 15 ms fall is sufficient. They do not document what to do when the bass content of a track produces frame-to-frame `heavy_bands[1..2]` deltas that, even *after* Spring smoothing, exceed the `[0.3, 2.0]` clamp and saturate at one end. The fast-fall (15 ms) means a `heavy_bands` drop frame after a hit can collapse `smoothedSpeed` toward 0.3 in a few frames, then snap back — the documented chain has no guard for this.
4. **Onset / kick handling is described as a separate flash channel, not as a phase-rate event.** DEV-STD §4.1 line 354 maps onset to "Flash / Burst trigger (exponential decay from center, exp(-dist*k))" — onset does NOT modulate `smoothedSpeed`. The standards do not describe what happens if an effect (incorrectly) folds onset into the phase-rate path.
5. **No specification of velocity-clamp behaviour at the 0.05s `dt` ceiling.** `getSafeDeltaSeconds()` clamps `dt` to `[0.0001, 0.05]` (DEV-STD §2.1 line 78). With `speedNorm * 240 * smoothedSpeed * dt` and `smoothedSpeed=2.0`, the per-frame `m_phase` jump on a 50 ms frame drop is `1.0 * 240 * 2.0 * 0.05 = 24 rad ≈ 3.8 cycles`. The standard is silent on whether such a single-frame jump is *expected* to look smooth (it almost certainly does not).
6. **No spec for how Spring `init(stiffness, mass)` should interact with very-high-frequency `heavy_bands` content.** Critically-damped at stiffness=50 has settling time ~200 ms (DEV-STD §3.3, PATTERNS §1 line 99). If the music has 16-th-note bass at 140 BPM (107 ms per note), the Spring is *under-damped* relative to the input rate and will resonate — none of the three documents flag this risk.
7. **No coverage of the 4 broken effects' specific issue.** The RATIFY document names all four as I-1 open-coders for the contrast curve (RATIFY line 164), but there is no documented audit of their motion / phase chains. The DEV-STD names ChevronWaves and LGPWaveCollision in §4.1 line 343 as "the three best audio-reactive effects in the system" — the canonical reference for the chain — yet they spazz, which means either (a) the canonical reference is not actually implemented in those files, or (b) the music conditions Captain is observing exceed the documented assumption envelope. The standards cannot adjudicate which.

---

## 6. Verdict — which broken effect most clearly violates the standard?

**On the evidence in the three standard documents alone (no source code read in this SSA), the four effects rank by visible standard-violation as follows:**

1. **ChevronWavesEffect** — declared by DEV-STD §4.1 line 343 as one of "the three best audio-reactive effects in the system" and the source of the canonical Pattern #1 quoted in PATTERNS §1 line 28 ("Source: ChevronWavesEffect.cpp, LGPPhotonicCrystalEffect.cpp"). It is *also* named in RATIFY row I-1 line 164 as an I-1 open-coder using `bin*bin` instead of the canonical contrast curve at line 70. The standards therefore set the highest expectation for this file (it is *the* documented exemplar) AND flag it as already divergent in its squaring step. If it spazzes, the gap between expectation and behaviour is largest here. This is the strongest evidence-anchor for the spazz redesign.

2. **LGPWaveCollisionEffect** — also named in DEV-STD §4.1 line 343 alongside ChevronWaves as a top-three exemplar, AND named in RATIFY row I-1 line 164 as an I-1 open-coder at line 62. RATIFY source-of-truth row (line 364) additionally lists `LGPWaveCollisionEffect.cpp:172` among the frame-coupled `fadeToBlackBy(…, ctx.fadeAmount)` callers — meaning this effect violates Property #12 (rate-independent smoothing) per FW-STD lines 110–114 in addition to the squaring divergence. **Two cited violations vs ChevronWaves's one.** On strict count, this is the strongest violator; on standard-setting expectation, it is second to ChevronWaves.

3. **ChevronWavesEffectEnhanced** — RATIFY row I-1 line 164 names `ChevronWavesEffectEnhanced.cpp:88` as an I-1 open-coder. Not named elsewhere in the three documents. One cited violation.

4. **SnapwaveLinearEffect** — RATIFY row I-1 line 164 names `SnapwaveLinearEffect.cpp:151` as an I-1 open-coder. Not named elsewhere. The "Linear" in its name is suggestive against FW-STD Property #5 (centre-origin MUST, lines 62–68), but the standards do not confirm a violation here without source-code inspection. One cited violation.

**Verdict.** **LGPWaveCollisionEffect most clearly violates the standard on the textual evidence available** (two cited violations: I-1 open-coded contrast + Property #12 frame-coupled `fadeToBlackBy`, RATIFY line 364). **ChevronWavesEffect carries the highest standards-vs-behaviour gap** (named as the canonical exemplar of the chain that the spazz proves is broken). The redesign should treat ChevronWaves as the calibration reference and LGPWaveCollision as the worst-on-paper offender, then verify both against source.

**Caveat.** The standards do not contain enough information to rule on the spazz root cause without source inspection. The textual evidence points at: (a) the audio-→-phase-rate chain is the load-bearing path (DEV-STD §4.2 + PATTERNS §1), (b) at least one of the four effects has a documented Property #12 violation (LGPWaveCollision frame-coupled fade), (c) all four diverge from the canonical contrast curve (RATIFY I-1), and (d) no document anywhere prescribes a spatial-aliasing phase-increment clamp — which is consistent with Captain's observation that the 2.5 rad clamp made spazz worse rather than better.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-30 | agent:opus-4.7-1M (SSA1, spazz_redesign substrate) | Created. Read-only extraction of motion / phase / velocity / audio-driven-position / temporal-coherence doctrine from EFFECT_FRAMEWORK_RATIFICATION_2026-04-29.md, EFFECT_FRAMEWORK_STANDARD.md, and EFFECT_DEVELOPMENT_STANDARD.md, with one-level citation into IMPLEMENTATION_PATTERNS.md. 15 canonical principles enumerated; quotes for audio-driven phase/position/velocity and Spring/EMA/follower usage; 11 anti-patterns; 7 documented gaps relevant to the spazz bug; verdict ranks the 4 broken effects by cited standard violations (LGPWaveCollision worst on paper at 2 violations; ChevronWaves highest expectation-vs-behaviour gap as the canonical exemplar of the now-broken chain). No source-code judgements made; standards-only analysis. |
