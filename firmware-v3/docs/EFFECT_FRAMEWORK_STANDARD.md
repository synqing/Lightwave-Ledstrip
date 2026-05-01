---
abstract: "Canonical effect-authoring rules for K1 LightwaveOS firmware. Codifies the 12 LOAD-BEARING properties identified in Track D (SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md) as 9 MUST + 3 SHOULD per the SSA-3-CORE recommendations recorded in EFFECT_FRAMEWORK_RATIFICATION_2026-04-29.md. Read alongside EFFECT_DEVELOPMENT_STANDARD.md (the developer how-to); this document is the WHAT and WHY. Captain may override any classification — edit the relevant section and add a changelog entry. Lint enforcement is staged: properties marked LINT-ENFORCED are gated by check_effect_contracts.py at PR time; properties marked LINT-DEFERRED have detector skeletons in tools/ pending Phase D implementation."
---

# Effect Framework Standard

**Status: ratified default classifications per SSA-3-CORE / SSA-3-INCIDENTAL / SSA-3-META forensic recommendations. Captain has not formally ratified — these are the agent-recommended defaults pending verbal override on Q1–Q8 in `EFFECT_FRAMEWORK_RATIFICATION_2026-04-29.md`. Lint enforcement is staged.**

This document complements `EFFECT_DEVELOPMENT_STANDARD.md`. The development standard is the how-to (write effects this way). This standard is the rulebook (the rules effects must follow + the rationale + the enforcement mechanism). Where they overlap, this document is authoritative on classification; the development standard is authoritative on idiom.

## Key

- **MUST** — violation produces visible/audible/structural failure. Lint-enforced where mechanically detectable. Hardware-test-required where not.
- **SHOULD** — recommended pattern. Use the canonical primitive; deviation is per-effect choice and must be justified in PR description.
- **INCIDENTAL** — variation acceptable across the catalogue. Optionally provided as a reusable helper if reinvention has been observed.

## Source-of-truth pointers

| Source | Path |
|---|---|
| Track D Phase 1 reconstruction | `firmware-v3/docs/research/SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md` |
| Ratification substrate (SSA returns + Q1–Q8) | `firmware-v3/docs/research/EFFECT_FRAMEWORK_RATIFICATION_2026-04-29.md` |
| Hard constraints (CLAUDE.md) | `CLAUDE.md` § Hard Constraints |
| Existing dev standard | `firmware-v3/docs/EFFECT_DEVELOPMENT_STANDARD.md` |
| Existing lint engine | `firmware-v3/tools/check_effect_contracts.py` |
| Golden-frame regression gate | `firmware-v3/test/test_golden/` |

---

## The 12 LOAD-BEARING properties

### 1. Two-stage thread separation — **MUST** (structural)

Audio analysis runs on Core 0 (AudioActor); render runs on Core 1 (RendererActor). Effects must not call audio-analysis primitives in `render()`.

**Enforcement:** structural — the effect API exposes no hooks to violate. Lint-deferred.

**Rationale:** violation breaks the 2.0 ms render budget and creates cross-core data races.

### 2. Single-stage post-mode smoothing on the spectrogram — **MUST**

Audio inputs reach `render()` already smoothed via `heavy_bands` / pre-smoothed ControlBus fields. **Do not stack additional smoothing layers on top.** One contrast/squaring pass is acceptable; stacked `AsymmetricFollower → Spring → ExpDecay` chains are forbidden.

**Enforcement:** **LINT-DEFERRED** — `check_stacked_smoothing` detector skeleton in `tools/check_effect_contracts.py`. Phase D will scan effect class member declarations for >1 follower/decay primitive operating on the same audio scalar.

**Rationale:** the canonical violation was the 5L-AR pack triple-smoothing (commit `aed805bb`, fixed 2026-03-23) — 80+200+150 ms latency made the pack feel disconnected from music.

### 3. Asymmetric max follower for waveform peak — **SHOULD**

When an effect needs peak normalisation, use `AsymmetricFollower` with the canonical tau (`SbK1BaseEffect.h:171-173`: kTauWfFollowerAttack=6.9 ms, kTauWfFollowerDecay=99.9 ms).

**Enforcement:** undetectable mechanically. Per-effect choice.

### 4. Audio-to-brightness via squaring (SQUARE_ITER) — **SHOULD**

The SB squaring contrast curve `(x²·0.65 + x·0.35)` is the SB-port-family canonical. Non-SB effects use the global gamma 2.2 (applied post-render by `ColorCorrectionEngine`).

**Enforcement:** undetectable. Captain may override Q1 to MUST → 197-effect refactor + helper hoist required (see INCIDENTAL I-1 below).

**Rationale:** mandating MUST would force 197-effect refactor AND double-apply non-linearity over global gamma. Conservative SHOULD pending Captain verdict.

### 5. Centre-origin symmetric geometry — **MUST**

All effects originate from LED 79/80 outward. No linear sweeps. Mirror about the centre is the default render shape.

**Enforcement:** **LINT-ENFORCED** — `check_centre_origin_inverted` in `check_effect_contracts.py:536-559`, with allowlist of ~120 legitimately-linear files. Already gated.

**Cross-reference:** CLAUDE.md hard constraint.

### 6. Palette/hue from chromagram — **MUST**

Audio-reactive colour drives from `chroma[12]` / `note_colors[12]` / dominant-bin / `chromaSmooth`, NOT from `gHue` walking through the full hue wheel.

**Enforcement:**
- Negative: **LINT-ENFORCED** — `check_rainbow_inverted` (`check_effect_contracts.py:562-585`) catches `fill_rainbow`, `CHSV(hue, ...)`, `hue+=`, `hue++`.
- Positive: **LINT-DEFERRED** — `check_chromagram_positive` detector skeleton awaits Captain Q2 verdict on whether `palette.getColor(gHue+offset)` satisfies (loose) or explicit `getChroma()`/`chromaSmooth` reference is required (strict).

**Cross-reference:** CLAUDE.md hard constraint (no rainbows).

### 7. SATURATION gates chromatic vs monochromatic — **SHOULD**

If an effect supports both chromatic (per-pitch hue) and monochromatic (single hue + intensity) modes, gate via `palette.saturation`. K1 has no global `chromatic_mode` bool; the SB mechanism diverged.

**Enforcement:** undetectable. Captain may override Q3 to drop entirely.

### 8. Global brightness pipeline post-mode — **MUST**

Effects write normalised values; the global brightness scale (`brightness × silent_scale × auto-exposure × gamma`) is applied once at output by `RendererActor::showLeds()` + `ColorCorrectionEngine`. Effects must not hard-set absolute brightness (`CRGB(255, 255, 255)`, `CHSV(_, _, 255)`).

**Enforcement:** **LINT-DEFERRED** — `check_hard_set_brightness` detector skeleton in `tools/check_effect_contracts.py`. Phase D will grep render bodies for `CRGB(255, 255, 255)`, `CRGB::White`, `CHSV(_, _, 255)`.

### 9. Silence gating via `silent_scale` is a global post-process — **MUST**

Effects do not silence-gate themselves. The `ctx.audio.available = false` state and `silentScale` global are applied by RendererActor; per-effect `if (audio.rms() < threshold) return;` early-returns duplicate the global and produce double-fading.

**Enforcement:** **LINT-DEFERRED** — `check_local_silence_gate` detector skeleton. Phase D will detect `audio.available` early-return + `audio.rms() <` patterns inside render bodies.

### 10. Post-mode global pipeline order — **MUST** (structural)

PRISM → BULB → incandescent → BASE_COAT → scale → dither, applied by `RendererActor` + `ColorCorrectionEngine`. Effects cannot reorder.

**Enforcement:** structural — the effect API offers no hook to insert into the post-pipeline.

### 11. MOOD = responsiveness — **MUST** for AR effects

`MOOD`, `audio_mix`, `beat_gain`, `motion_depth`, `motion_rate`, `colour_anchor_mix` control responsiveness (mix rates, scroll speeds, follower coefficients). They do NOT change what the effect responds to.

**Enforcement:** **LINT-ENFORCED for AR effects** — `check_ar_control_liveness` (`check_effect_contracts.py:505-529`) verifies `*AREffect.cpp` files invoke `updateSignals`, `buildModulation`, `applyBedImpactMemoryMix`, `mod.motionRate`, `m_ar.tonalHue`, spectral usage. Non-AR effects: undetectable.

### 12. Rate-independent smoothing via tau constants — **MUST**

All temporal smoothing uses `α = 1 - exp(-dt/τ)` with explicit tau constants. Frame-coupled alphas (`*= 0.95f` without `dt`) break at FPS variation (K1 sees 60 → 119 FPS depending on load).

**Enforcement:** **LINT-DEFERRED** — `check_frame_coupled_decay` detector skeleton. Phase D will scan effect render bodies for bare `*= 0.\d+f;` patterns where the operand is not `dt`-derived. The 2026-02-21 partial audit covered 19 files; full sweep awaits Captain Q5 verdict.

---

## INCIDENTAL property dispositions

(Source: SSA-3-INCIDENTAL recommendations. Reinvention evidence cited where the recommendation is `provide-reference-helper`.)

| # | Property | Disposition | Helper |
|---|---|---|---|
| I-1 | SQUARE_ITER count + 0.65/0.35 mix-back | **provide-reference-helper** ✓ DELIVERED | `effects/math/Contrast.h` — free function `applyContrast(bin, squareIter)` with `kSbK1SquareIter = 0.65f`. Identical to `SbK1BaseEffect::applyContrast`. 4 open-coders (`ChevronWaves`, `SnapwaveLinear`, `LGPWaveCollision`, `BloomParity`) may adopt; migration is optional, existing code unchanged. |
| I-2 | Brightness→hue shift | **stay-free** | — |
| I-3 | Trail persistence mechanism | **provide-reference-helper** ✓ DELIVERED | `PersistenceHelpers.h::fadeToBlackByDt(leds, n, rate60fps, dt)` — wraps `dtDecay3`. Replaces frame-coupled `fadeToBlackBy(…, ctx.fadeAmount)` with dt-correct decay. 20+ callers may migrate; migration optional, existing code unchanged. |
| I-4 | Geometry within mode | **stay-free** | Per-effect aesthetic IS its identity. |
| I-5 | PRISM_COUNT / BULB_OPACITY | **stay-free** | Bloom V2 only consumer. |
| I-6 | BASE_COAT baseline glow | **archive-as-deprecated** | ZERO references in K1; never ported from SB. |
| I-7 | Temporal dithering | **stay-free** | FastLED native dither is the platform default. |
| I-8 | MOOD-forced-1.0 in Bloom | **stay-free** | Mode-specific override. |

---

## Meta-decisions

### M-RES — Audio resolution path

**Decision:** the existing tiered ControlBus contract — `chroma[12]` / `bins64[64]` / `bins256[256]` — is canonical. The 96-semitone `reconstructSpectrum96` reconstruction inside `SbK1BaseEffect` is INCIDENTAL to that family and used by 4 effects only. New effects pick the field appropriate to their semantic.

**Rationale:** the audio backend is already 64-bin SB-parity-aligned (vendor `NUM_FREQS = 64`). The original Q3.A.3 question rested on a category error (no `chroma[96]` field exists). See `EFFECT_FRAMEWORK_RATIFICATION_2026-04-29.md` Pre-finding 1.

### M-AUDIT — Existing-effect audit scope

**Decision:** audit ALL effects, BUT harden the lint engine FIRST. Per the move_0_2 doctrine. After the LINT-DEFERRED detectors above land in Phase D, dispatch the audit fleet.

**Rationale:** the catalogue is 100% post-2026-01 (82 of 82 effect commits this year), so a date-cutoff grandfathering is a no-op. The most recent audit (2026-04-27) found 0 genuine violations across 34 flags on 106 effects — the move_0_2 lesson is that mechanical lint at current precision produces ~94% false-positive rate. Tightening the lint comes first.

### M-TEST — Per-effect render unit-tests

**Decision:** **SHOULD-not-MUST** for V1.0. Test infrastructure (the golden-frame harness in `firmware-v3/test/test_golden/`) is the gating mechanism, not per-effect bespoke tests. Future Captain verdict on Q8 may promote to MUST once the harness covers the full catalogue (G5).

**Rationale:** mandating MUST would block every new effect until per-effect render-output tests exist — multi-month engineering before V1.0 launch. The golden-frame harness gates regression at PR time; that's the appropriate enforcement layer.

---

## Captain override protocol

Any classification above can be overridden by Captain. Process:

1. Edit this document. Change the property's classification (MUST ↔ SHOULD ↔ INCIDENTAL).
2. Add a changelog entry citing the override + rationale.
3. If the change affects a LINT-ENFORCED detector, update `check_effect_contracts.py` accordingly.
4. If the change introduces a new MUST that the catalogue currently violates, file a remediation backlog.

---

## Outputs (continuation surface)

After this standard lands, the following deliverables follow:

| Phase | Deliverable | Status |
|---|---|---|
| **B** | This document | **DONE 2026-04-30** |
| **D** | Lint detectors `check_stacked_smoothing`, `check_chromagram_positive`, `check_hard_set_brightness`, `check_local_silence_gate`, `check_frame_coupled_decay` | Skeletons in `tools/check_effect_contracts.py` (this commit); full implementation deferred |
| **D** | I-1 helper hoist (`effects/math/Contrast.h`) | **DONE 2026-04-30** |
| **D** | I-3 helper add (`PersistenceHelpers::fadeToBlackByDt`) | **DONE 2026-04-30** |
| **E** | Catalogue audit fleet (~7-10 SSAs) | Blocked on D detectors completing |
| **F** | Per-effect remediation backlog | Blocked on E |
| — | Golden-frame harness G5 (extend factory to ~106 effects) | Blocked on SbK1Base stub additions |

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-30 | agent:opus-4.7-1M | Created. Codifies SSA-3-CORE / SSA-3-INCIDENTAL / SSA-3-META recommendations as defaults: 9 MUST + 3 SHOULD across 12 LOAD-BEARING; 2 helper-hoist + 5 stay-free + 1 archive-deprecated across 8 INCIDENTAL; 96-bin path declared INCIDENTAL to SbK1Base family (M-RES); audit-all-with-lint-first (M-AUDIT); per-effect tests SHOULD-not-MUST (M-TEST). Captain has not formally ratified Q1–Q8 from `EFFECT_FRAMEWORK_RATIFICATION_2026-04-29.md`; this document records the agent-recommended defaults, Captain may override per protocol above. |
| 2026-04-30 | agent:sonnet-4.6 | I-1 and I-3 helpers delivered: `effects/math/Contrast.h` (applyContrast + kSbK1SquareIter) and `PersistenceHelpers::fadeToBlackByDt`. Updated INCIDENTAL table and Outputs table to DONE. |
