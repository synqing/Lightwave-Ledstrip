---
abstract: "Forensic substrate for Captain's ratification of EFFECT_FRAMEWORK_STANDARD.md (item 3 of the original-track 6-item pipeline, Phase A of an end-to-end multi-phase pipeline). Compiles three SSA returns (CORE / INCIDENTAL / META) into a single decision-input document. Covers: 12 LOAD-BEARING property classifications (9 MUST / 3 SHOULD recommended; 5 need new lint), 8 INCIDENTAL dispositions (2 helpers worth hoisting, 5 stay-free, 1 archive-as-deprecated), and 8 residual Captain decisions Q1–Q8 grouped by impact tier (A: low-cost / B: contained / C: strategic). TWO pre-findings dissolve original questions: the 96-bin chromagram is a category error (audio backend is already 64-bin), and the '9 violations' figure cited in source is stale (most recent audit found 0 genuine violations on 106 effects). Read this when ratifying property classifications before authoring `firmware-v3/docs/EFFECT_FRAMEWORK_STANDARD.md`. Captain reads, ratifies (verbal or inline), then standard authoring proceeds."
---

# Effect Framework Standard — Ratification Substrate

**Track D Phase 2 input. Forensic compilation, NOT policy.** This document gathers the substrate Captain needs to decide the per-property MUST/SHOULD verdicts that will produce `firmware-v3/docs/EFFECT_FRAMEWORK_STANDARD.md`. No verdicts are recorded here.

## Sources

| Source | Path |
|---|---|
| Track D Phase 1 reconstruction | `firmware-v3/docs/research/SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md` |
| Most recent audit (centre-origin / brand-voice) | `firmware-v3/docs/audit/move_0_2_centre_origin_audit_2026-04-27.md` |
| Existing standard | `firmware-v3/docs/EFFECT_DEVELOPMENT_STANDARD.md` |
| Existing lint | `firmware-v3/tools/check_effect_contracts.py` |
| Hard constraints | `CLAUDE.md` § Hard Constraints |
| Audio contract | `firmware-v3/src/audio/contracts/ControlBus.h` |
| K1 effect base | `firmware-v3/src/effects/ieffect/sensorybridge_reference/SbK1BaseEffect.h` |
| Effect registry | `firmware-v3/src/effects/CoreEffects.cpp` |
| SSA returns (this session 2026-04-29) | SSA-3-CORE, SSA-3-INCIDENTAL, SSA-3-META; high-confidence each |

---

## TL;DR

- **Pre-finding 1 dissolves Question 3.A.3:** there is no 96-bin chromagram in ControlBus. The audio backend is already 64-bin SB-parity-aligned. The "96" is an effect-private reconstruction inside `SbK1BaseEffect`, used by 4 effects only.
- **Pre-finding 2 reframes Question 3.A.4:** the "9 violations" cited is stale; the most recent audit (2026-04-27) found 0 genuine violations across 34 flags on 106 effects. The catalogue is post-2026-01 in its entirety so a date cutoff grandfathers everything (no-op).
- **3.A.1 (12 LOAD-BEARING):** SSA recommends 9 MUST + 3 SHOULD. 4 properties need new lint detectors. Several properties collapse together to avoid double-counting in the standard.
- **3.A.2 (8 INCIDENTAL):** 2 helpers worth hoisting (`applyContrast()` and `fadeToBlackByDt()`), 5 stay free, 1 (BASE_COAT) recommend archive-as-deprecated (never ported to K1).
- **3.A.4 (audit scope, reframed):** audit ALL effects, but harden the lint engine first (the move_0_2 doctrine is already in motion).
- **8 residual Captain decisions remain (Q1–Q8)**, grouped below by impact tier so they can be tackled in 5-minute / planning-session / strategic-session passes.

---

## Pre-finding 1 — The "96-bin chromagram" is a category error

### Original question (3.A.3)

`SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md:154` asks: *"Resolution divergence — K1 uses 160 LEDs and 96 semitone bins instead of SB's 128 LEDs / 64 GDFT bins. Is the 96-bin chromagram path a permanent K1 choice, or should new effects target the 64-bin SB-parity path for cross-platform fidelity?"*

### Forensic finding (SSA-3-META, high confidence)

ControlBus does NOT expose a 96-bin chromagram. The audio backend is already 64-bin SB-parity-aligned.

| Source | Evidence |
|---|---|
| `firmware-v3/src/audio/contracts/ControlBus.h:13,69` | Fields exposed: `chroma[12]` (folded chromagram), `bins64[64]` (SB-style spectrogram), `bins256[256]` (high-resolution spectrum). No `chroma[96]` field. |
| `firmware-v3/src/audio/backends/esv11/vendor/global_defines.h:6` | `NUM_FREQS = 64` (vendor constant) |
| `firmware-v3/src/audio/AudioActor.h:906,1147` | `SB_NUM_FREQS = 64` |

### What "96" actually refers to in the codebase

| Place | Purpose | Effect-facing? |
|---|---|---|
| `firmware-v3/src/audio/tempo/TempoTracker.h:20,158` | 96 BPM-probe bins (48–143 BPM at 1.0 BPM spacing). Tempo only, unrelated to chroma. | No |
| `firmware-v3/src/effects/ieffect/sensorybridge_reference/SbK1BaseEffect.h:7-8,120,219,224` | `specSmooth[96]` + `reconstructSpectrum96(const float* bins256, ...)`. Effect-private reconstruction OVER `bins256`. | Yes, but to 4 effects only. |

### Effects depending on the 96-semitone reconstruction

4 files, all in `firmware-v3/src/effects/ieffect/sensorybridge_reference/`:
- `SbK1BloomEffect.cpp`
- `SbK1BloomV2Effect.cpp`
- `SbK1BaseEffect.cpp`
- `SbSpectralBeatPulseEffect.h`

### Effects depending on canonical paths

| Field | Effects (count) |
|---|---|
| `chroma[12]` direct | 4 files |
| `bins64[64]` | 16 files |
| `bins256[256]` | 5 files |

### Implication

There is no architectural decision to make. The framework standard documents the existing tiered ControlBus contract (12 / 64 / 256). The `reconstructSpectrum96` reconstruction is INCIDENTAL to the SbK1Base family. New effects pick the field appropriate to their semantic; the standard does not need to mandate one path.

**3.A.3 dissolves.**

### Open meta-question

**Q-META-1:** was the original wording a misreading of `reconstructSpectrum96`, or did Captain genuinely intend a NEW `chroma96[96]` ControlBus contract? Presumed misreading. Confirm.

---

## Pre-finding 2 — The "9 violations" figure is stale

### Original question (3.A.4)

`SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md:156` asks: *"Existing-effect audit scope after standard lands — every existing effect, or only new effects from a cutoff date? Centre-origin audit hit 9 violations; a full 12-rule audit will likely find more."*

### Forensic finding (SSA-3-META, high confidence)

The most recent centre-origin / brand-voice audit (`firmware-v3/docs/audit/move_0_2_centre_origin_audit_2026-04-27.md`, dated yesterday) found **0 genuine violations across 34 flags on 106 effects**.

### Triage of the 34 flags

| Class | Count | Resolution |
|---|---|---|
| Rainbow-scan flags | 16 | All false positive — palette-segment use anchored to `baseHue`, not full-wheel rotation |
| Heap-in-render flags | 5 | All false positive — lint engine brace-tracking bug; flagged code is in `init()` not `render()` |
| BeatPrismOnset `[api]` raw-bus flags | 8 | Informational, not brand-voice violations |
| Centre-scan flags | 2 | 1 lint-fix (writeCentrePairDual not recognised by regex), 1 INVESTIGATE (LGPGradientFieldEffect) |
| ALLOWLIST additions | 2 | Mechanical |
| `[k1-ap-only]` flag | 1 | False positive (read-only diagnostic) |

**Headline (`move_0_2_centre_origin_audit_2026-04-27.md:15`):** *"ZERO genuine brand-voice violations were found in the effect catalogue. Highest-priority remediation is the lint engine, not the effects."*

### Catalogue cadence (git log evidence)

| Window | Commits to `firmware-v3/src/effects/` |
|---|---|
| Since 2026-04-25 | 11 |
| Since 2026-01-01 | 82 |
| Before 2026-01-01 | 0 |

100% of effect commits are post-2026-01. A cutoff-date approach grandfathers everything.

### Implication

3.A.4 reframes from "all vs cutoff date" to "lint-then-audit vs audit-now-with-broken-lint." The move_0_2 doctrine (harden lint before re-running audit) is already in motion.

### Open meta-question

**Q-META-2:** confirm "9" was either the stale figure from an earlier (pre-move_0_2) pass, OR refers to a different audit dimension. The 2026-04-27 audit is the canonical recent number.

---

## 3.A.1 — 12 LOAD-BEARING property classifications (SSA-3-CORE recommendations)

| # | Property | Recommended | Existing K1 status | Risk if MUST | New lint needed |
|---|---|---|---|---|---|
| 1 | Two-stage threads (audio core / render core) | **MUST** | Architectural; AudioActor (Core 0), RendererActor (Core 1). No effect-API hook to violate. | low (structural) | no |
| 2 | Single-stage post-mode smoothing (symmetric 75% EMA) | **MUST** | `EFFECT_DEVELOPMENT_STANDARD.md` §4.2 documents heavy_bands as pre-smoothed; not lint-gated | medium (5L-AR was canonical violation, fixed `aed805bb`; broader audit not done) | yes — stacked-smoothing detector |
| 3 | Asymmetric max follower for waveform peak | **SHOULD** | `SbK1BaseEffect.h:171-173` encodes canonical tau; `AsymmetricFollower` primitive in §3.2 | low (use-when-needed) | no |
| 4 | Audio-to-brightness via squaring SQUARE_ITER | **SHOULD** | SB-port-only via `applyContrast()`; gamma 2.2 already global via ColorCorrectionEngine | **HIGH if MUST** — would force 197-effect refactor + double-non-linearity vs gamma | conditional (only if MUST) |
| 5 | Centre-origin symmetric geometry | **MUST** | Already lint-gated (`check_centre_origin_inverted` + 120-file allowlist); CLAUDE.md hard constraint | low (already gated) | no |
| 6 | Palette/hue from chromagram (12-bin) | **MUST** | No-rainbow lint exists (`check_rainbow_inverted`); positive chromagram-use pattern not enforced | medium (older effects use `gHue + offset` palette walking) | yes — chromagram-positive detector |
| 7 | SATURATION gates chromatic vs mono | **SHOULD** | K1 has no global `chromatic_mode` bool; mechanism diverged from SB | high if MUST (would force every effect to expose chromatic/mono switch) | n/a |
| 8 | Global brightness pipeline post-mode | **MUST** | `EFFECT_DEVELOPMENT_STANDARD.md` §2.5 mandates `scale8(value, ctx.brightness)`; not lint-gated | medium (anti-pattern §7.1 documented but not gated) | yes — hard-set-brightness detector |
| 9 | Silence gating via `silent_scale` (global) | **MUST** | §7.7 Behavioral Gates documents global silence gate; per-effect silence gating duplicates | medium | yes — local-silence-gate detector |
| 10 | Post-mode global pipeline order | **MUST** | Structural (RendererActor + ColorCorrectionEngine); effects can't violate | low (structural) | no |
| 11 | MOOD knob = responsiveness, not what responds | **MUST** | `check_ar_control_liveness` for AR pack (lines 505-529) | low (already gated for AR family) | no |
| 12 | Rate-independent smoothing via tau constants | **MUST** | §2.1, §3, §7.3 documented; not lint-gated | high (frame-coupled patterns persist; 2026-02-21 audit hit 19 files) | yes — frame-coupled-decay detector |

**Net: 9 MUST + 3 SHOULD. 5 properties need new lint detectors (#2, #6, #8, #9, #12).**

### Cross-cutting collapses (recommend folding to avoid double-counting)

| Collapse | Rationale |
|---|---|
| #1 + #10 → "Pipeline structure (informational; not authorable from effect code)" | Both are structural invariants the effect API can't violate |
| #2 + #4 + #12 → "audio-to-brightness shaping chain (one canonical sequence)" | All three describe stages of the same chain; risk of stating same constraint three ways |
| #6 + #7 → "colour-domain decision (one section)" | K1 has no global chromatic_mode; #7 collapses into #6 |
| #8 + #9 → "global post-process invariants" | Both share principle: do not duplicate global post-process inside effect |

---

## 3.A.2 — 8 INCIDENTAL property recommendations (SSA-3-INCIDENTAL)

| # | Property | Recommended | Reinvention evidence | Effort |
|---|---|---|---|---|
| I-1 | SQUARE_ITER count | **provide-reference-helper** | 4 effects open-coded `bin*bin` divergently from canonical `(x²·0.65 + x·0.35)` mix-back: `ChevronWavesEffect.cpp:70`, `ChevronWavesEffectEnhanced.cpp:88`, `SnapwaveLinearEffect.cpp:151`, `LGPWaveCollisionEffect.cpp:62`, `BloomParityEffect.cpp:291` | ~10 LOC: hoist `applyContrast()` from `SbK1BaseEffect` to `effects/math/Contrast.h` |
| I-2 | Brightness→hue shift | **stay-free** | Single-effect (SB-family); no reinvention found | n/a |
| I-3 | Trail persistence mechanism | **provide-reference-helper** | 20+ frame-coupled `fadeToBlackBy(…, ctx.fadeAmount)` callers (decay rate varies 60–119 FPS) vs 17 dt-correct `dtDecay` callers. Bimodal split. | ~20 LOC: `fadeToBlackByDt(leds, n, fadePerSec, dt)` wrapping `dtDecay3` |
| I-4 | Geometry within mode | **stay-free** | Per-effect aesthetic IS its identity; no reinvention pattern | n/a |
| I-5 | PRISM_COUNT / BULB_OPACITY values | **stay-free** | Bloom V2 only consumer | n/a |
| I-6 | BASE_COAT baseline glow | **archive-as-deprecated** | ZERO references in `firmware-v3/src/`; never ported from SB | n/a (mark deprecated in standard) |
| I-7 | Temporal dithering | **stay-free** | 1 effect uses (LGPFilmPost); 1 explicitly defeats it (LGPGradientField); FastLED native dither is platform default | n/a |
| I-8 | MOOD-forced-1.0 in Bloom | **stay-free** | Mode-specific override only; no reinvention | n/a |

**Net: 2 helpers worth hoisting (I-1 `applyContrast`, I-3 `fadeToBlackByDt`), 5 stay free, 1 archive-as-deprecated (I-6 BASE_COAT).**

---

## 3.A.4 — Audit scope (reframed)

### Catalogue size

| Metric | Value | Source |
|---|---|---|
| `registerEffect` calls | 225 | `firmware-v3/src/effects/CoreEffects.cpp` |
| `.cpp` files in `ieffect/` | 179 | filesystem |
| `.h` files in `ieffect/` | 190 | filesystem |
| Audit-canonical effect class count | 106 | `move_0_2_centre_origin_audit_2026-04-27.md:11` |

### Test coverage (current)

- `firmware-v3/test/test_native/` has 5 effect-touching files: `test_effects.cpp`, `test_effect_id_limits.cpp`, `test_effect_role_flags.cpp`, `test_onset_effect_context.cpp`, `test_ws_effects_codec.cpp`
- These cover effect *infrastructure* (registry, role flags, onset context, WS codec), NOT per-effect render correctness
- Per-effect render-output unit tests: ~0% coverage

### Trade-off matrix

| Choice | Cost | Long-term debt | Captain decisions |
|---|---|---|---|
| Audit ALL 106 effects against the 12-rule standard, current lint | 7-10 SSAs × ~30K tokens × ~1 day fleet wall-clock | low (clean slate) | high (likely 20+ INVESTIGATE / LINT-FIX / ALLOWLIST given move_0_2 produced 4 such across single-rule audit) |
| Cutoff-date grandfathering | 0 SSAs | **no-op separation** (catalogue is 100% post-cutoff) | 0 |
| Audit ALL but tag non-compliant `isExperimental` instead of refactoring | same as audit-all | medium (`isExperimental` is soft kill; effects can drift) | medium |
| **Audit ALL but harden lint FIRST, then audit** | same SSA cost; lint hardening is upstream | low (lint becomes high-precision PR gate) | low (most "violations" resolve as LINT-FIX) |

### Recommendation (SSA-3-META)

**Audit ALL, harden lint first.** Per move_0_2 doctrine. SSA fleet ~7-10 audit subagents in parallel, AFTER lint-detector additions for properties #2/#6/#8/#9/#12 land. Per-effect render unit-tests: **SHOULD-not-MUST** for V1.0 (mandating MUST blocks every new effect until test-infrastructure phase).

---

## The 8 open Captain questions, grouped by impact tier

### Tier A — Low-cost decisions (5-minute scan, unblock Phase B authoring)

#### Q3 — Property #7 (SATURATION gates chromatic vs mono): preserve / drop / restate?

- **Current situation:** K1 has no global `chromatic_mode` bool. SB's mechanism diverged in K1 — the K1 palette system collapses the distinction (line 134 of K1 lineage table).
- **Proposed change (SSA recommendation):** SHOULD restated as *"if your effect supports both, gate via `palette.saturation`."*
- **Risk:** if MUST, every effect would need to expose a chromatic/mono switch — high refactor cost for low product value.
- **Source:** `SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md:64-68` (Property #7 body)

#### Q6 — I-1 `applyContrast()` hoist to `effects/math/Contrast.h`?

- **Current situation:** `applyContrast()` lives inside `SbK1BaseEffect`. Non-SB effects can't reach it without inheritance. 4 effects open-coded `bin*bin` (pure squaring) instead of canonical `(x²·0.65 + x·0.35)` mix-back. Pure squaring is a different perceptual response.
- **Proposed change:** Hoist to free function in `effects/math/Contrast.h`. ~10 LOC. The 4 open-coded callers can adopt by ~5 LOC each.
- **Risk:** ChevronWaves / Snapwave / WaveCollision were tuned against pure squaring. Adopting the canonical mix-back would change their visual feel. Same class as 5L-AR triple-smoothing — small algebra change, big visual impact.
- **Decision options:** (a) hoist + leave existing 4 effects untouched (just makes helper accessible); (b) hoist + retune all 4 to canonical curve; (c) leave as-is, document as known divergence.
- **Source:** `SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md:104` (I-1 description); SSA-3-INCIDENTAL evidence section.

#### Q7 — I-3 `fadeToBlackByDt()` substrate timing: ship now, or defer to substrate sweep?

- **Current situation:** 20+ effects use frame-coupled `fadeToBlackBy(…, ctx.fadeAmount)` (decay 2× faster at 119 FPS than at 60 FPS); 17 effects use dt-correct `dtDecay`. Living debt.
- **Proposed change:** Add ~20 LOC `fadeToBlackByDt()` wrapper to PersistenceHelpers. Migration of 20+ callers is mechanical but bulky.
- **Risk:** If shipped now, becomes part of the standard's MUST-use-this directive. If deferred, the 20+ callers continue to drift across FPS.
- **Decision options:** (a) ship wrapper now, defer migration; (b) ship wrapper + migration sweep together; (c) defer entirely to a later substrate sweep.
- **Source:** SSA-3-INCIDENTAL evidence; `firmware-v3/src/effects/PersistenceHelpers.h:57,69` (existing `dtDecay`/`dtDecay3`).

---

### Tier B — High-impact but contained (planning-session scope)

#### Q2 — Property #6 (chromagram colour): what counts as compliant?

- **Current situation:** No-rainbow lint exists (`check_rainbow_inverted`). Positive chromagram-use pattern is NOT enforced. Older effects use `ctx.palette.getColor(gHue + offset)` (palette-driven hue walking) without explicit chroma input.
- **Proposed change:** Adopt definition. Two candidates:
  - **Strict:** hue input must derive from `getChroma()` / dominant-bin / `chromaSmooth` explicitly. Lint regex detects `chroma`/`note_colors` references in render bodies.
  - **Loose:** `palette.getColor(gHue+offset)` satisfies (any palette-driven walk counts). Lint regex weaker — checks only for absence of `fill_rainbow` / `CHSV(hue,…)`.
- **Risk:** Strict definition would flag many older effects; loose definition allows the "rainbow-with-extra-steps" anti-pattern through.
- **Decision needed:** which definition is canonical for the lint detector.
- **Source:** `SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md:58-62` (Property #6); `firmware-v3/tools/check_effect_contracts.py:562-585` (existing rainbow-inverted lint).

#### Q5 — Property #12 (frame-coupled decay) audit scope: full re-audit or trust 2026-02-21 partial?

- **Current situation:** Memory entry indicates 2026-02-21 audit covered 19 files for frame-rate independence. The other 178 files in `ieffect/` are not yet audited.
- **Proposed change:** Either trust the 19-file pass (assume the rest is clean), or commission a catalogue-wide lint sweep using a new frame-coupled-decay detector (regex for `*= 0.\d+f;` patterns where operand is not `dt`-derived).
- **Risk:** If we trust the partial: silent drift on FPS variation in the 178 unaudited effects. If we full-audit: 7-10 SSA fleet cost (same scope question as Q-AUDIT generally).
- **Decision needed:** trust partial / full lint-sweep / sweep only as part of broader audit-all phase.
- **Source:** `SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md:96-100` (Property #12); memory observation #36991 (cited in SSA-3-CORE).

---

### Tier C — Strategic (multi-month engineering implication)

#### Q1 — Property #4 (squaring contrast): SB-port-only OR catalogue-wide MUST?

- **Current situation:** SB-port effects use `SbK1BaseEffect::applyContrast()`. The other ~190 effects do not. Gamma 2.2 is already applied globally via `ColorCorrectionEngine` (PWM 128 → 22% perceived per `EFFECT_DEVELOPMENT_STANDARD.md` §6.3).
- **Proposed change (SSA recommendation):** SHOULD (avoid 197-effect refactor + double-non-linearity vs gamma).
- **Risk if MUST:** 197-effect refactor; conflicts with global gamma; double-applies non-linearity for non-SB-port effects.
- **Risk if SHOULD:** SB lineage diverges from K1 mainline; effect catalogue loses the punchy SB perceptual response.
- **Strategic implication:** decides whether K1 effects are "SB-lineage by default" or "K1-native by default with SB-lineage as an opt-in family."
- **Source:** `SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md:46-50` (Property #4); `EFFECT_DEVELOPMENT_STANDARD.md` §6.3 (gamma).

#### Q4 — Property #2 (single-stage smoothing) MUST scope: all 197 or AR-only?

- **Current situation:** 5L-AR pack was the canonical violation (triple smoothing 80+200+150 ms = 430 ms latency). Fixed 2026-03-23 (commit `aed805bb`). The broader catalogue has not been audited for stacked smoothing.
- **Proposed change:** Either scope MUST to AR-tagged effects only (where audio mapping cares about latency), or broaden to all 197 (defensive against future drift).
- **Risk if AR-only:** non-AR effects can introduce stacked smoothing without lint catching it; could regress audio-reactive feel later when an effect gets retagged AR.
- **Risk if all-197:** kicks off a catalogue-wide lint sweep (same scope question as Q-AUDIT generally).
- **Strategic implication:** decides the breadth of the audit-fleet cost.
- **Source:** `SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md:32-38` (Property #2); `feedback_5lar_fix_pattern.md` (canonical example).

#### Q8 — Per-effect render unit tests: SHOULD or MUST?

- **Current situation:** ~0% of effects have render-output unit tests today. Existing test infrastructure covers registry / role flags / WS codec, not per-effect output.
- **Proposed change (SSA recommendation):** SHOULD for V1.0. Test infrastructure phase scoped separately.
- **Risk if MUST:** blocks every new effect until per-effect test infra exists. Multi-month engineering before V1.0 launch.
- **Risk if SHOULD:** technical debt accumulates; effect regressions only caught visually.
- **Strategic implication:** decides whether V1.0 launch can ship without effect-test infra OR if test infra is a launch-blocker.
- **Source:** SSA-3-META catalogue/test analysis.

---

## Decision dependencies (gating map)

```
Q1 (squaring scope) ──→ governs whether non-SB effects need refactoring on standard land
Q2 (chroma compliance) ──→ governs lint regex strictness
Q3 (SATURATION) ──→ Tier A; quick decision; resolves a property collapse
Q4 (single-stage scope) ──→ governs audit breadth (catalogue-wide or AR-only)
Q5 (frame-coupled scope) ──→ overlaps Q4 audit-breadth question
Q6 (applyContrast hoist) ──→ Tier A; affects 4 effects' tuning
Q7 (fadeToBlackByDt timing) ──→ Tier A; 20+ effects' decay behaviour over time
Q8 (unit-test mandate) ──→ governs V1.0 launch-blocker scope

Q-META-1 (96-bin premise) ──→ verify dissolves 3.A.3 (presumed answered)
Q-META-2 (9-violations stale) ──→ verify reframes 3.A.4 (presumed answered)
```

### Bottleneck identification

- **Q4** and **Q5** both gate audit breadth. If both = AR-only / partial: tighter audit fleet. If both = all-197: bigger audit fleet but cleaner outcome.
- **Q1** is the highest-blast-radius decision (197-effect refactor risk).
- **Q8** is the longest-tail decision (multi-month test-infra implication).
- **Q3, Q6, Q7** can each be decided in isolation without affecting the others.

---

## Outputs of the ratification (where decisions feed)

After Captain ratifies the decisions in this document, the following deliverables get produced (Phase B onwards):

| Deliverable | Sources from | Phase |
|---|---|---|
| `firmware-v3/docs/EFFECT_FRAMEWORK_STANDARD.md` (single canonical doc, no version suffix) | 3.A.1 verdicts + 3.A.2 verdicts + Q3 + property collapses | B |
| Lint detector additions in `firmware-v3/tools/check_effect_contracts.py` | Q2, Q5 (stacked-smoothing, chromagram-positive, hard-set-brightness, local-silence-gate, frame-coupled-decay detectors) | D |
| `effects/math/Contrast.h` (if Q6 = hoist) | Q6 + I-1 | D |
| `firmware-v3/src/effects/PersistenceHelpers.h` extension `fadeToBlackByDt` (if Q7 = ship) | Q7 + I-3 | D |
| Audit SSA fleet results (per-effect compliance scoring) | Q4 scope + lint hardening | E |
| Remediation backlog (per-effect: refactor / `isExperimental` / deregister) | E output + Captain per-effect verdicts | F |
| BASE_COAT archive note | I-6 verdict | B |

---

## Recommended sequencing for Captain

| Pass | Time | Decisions | Deliverable |
|---|---|---|---|
| **A** | ~5 min | Q3, Q6, Q7 + confirm Q-META-1 / Q-META-2 | Tier A unblocked → can begin authoring standard skeleton |
| **B** | ~30 min planning session | Q2 (chroma compliance), Q5 (frame-coupled scope) | Lint detector regexes specified |
| **C** | strategic session, can defer | Q1 (squaring scope), Q4 (single-stage scope), Q8 (unit-test mandate) | Standard finalised + audit scope locked |

Pass A unlocks Phase B authoring of `EFFECT_FRAMEWORK_STANDARD.md` skeleton (sections for properties already classified MUST/SHOULD without contention). Passes B and C populate the contentious sections.

---

## Source-of-truth pointers (verification checklist)

For each claim in this document, here is where to verify:

| Claim | Source |
|---|---|
| 12 LOAD-BEARING properties + their bodies | `SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md:24-100` |
| 8 INCIDENTAL properties | `SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md:102-111` |
| K1 lineage preservation table | `SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md:124-145` |
| Original 4 ratification questions | `SB_FRAMEWORK_RECONSTRUCTION_2026-04-26.md:146-156` |
| ControlBus chromagram contract (12/64/256) | `firmware-v3/src/audio/contracts/ControlBus.h:13,69,88-94,136-152` |
| ESV11 NUM_FREQS = 64 | `firmware-v3/src/audio/backends/esv11/vendor/global_defines.h:6` |
| `reconstructSpectrum96` (4-effect-private) | `firmware-v3/src/effects/ieffect/sensorybridge_reference/SbK1BaseEffect.h:7-8,120,219,224` |
| Most recent audit result (0/34 genuine) | `firmware-v3/docs/audit/move_0_2_centre_origin_audit_2026-04-27.md:11,15` |
| Existing lint (`check_effect_contracts.py`) | `firmware-v3/tools/check_effect_contracts.py:84-224, 505-529, 536-559, 562-585` |
| Existing standard | `firmware-v3/docs/EFFECT_DEVELOPMENT_STANDARD.md` (full doc) |
| Hard constraints | `CLAUDE.md` § Hard Constraints |
| Catalogue cadence | `git log --since="2026-01-01" --oneline -- firmware-v3/src/effects/` |
| 5L-AR triple-smoothing canonical incident | `~/.claude/projects/.../memory/feedback_5lar_fix_pattern.md` |
| `applyContrast` open-coded reinventions | `ChevronWavesEffect.cpp:70`, `ChevronWavesEffectEnhanced.cpp:88`, `SnapwaveLinearEffect.cpp:151`, `LGPWaveCollisionEffect.cpp:62`, `BloomParityEffect.cpp:291` |
| `fadeToBlackBy` frame-coupled callers | `LGPNeuralNetworkEffect.cpp:38`, `LGPFresnelCausticSweepEffect.cpp:204`, `LGPGravitationalWaveChirpEffect.cpp:71`, `LGPWaveCollisionEffect.cpp:172`, `LGPPerlinInterferenceWeaveEffect.cpp:96`, `LGPExperimentalAudioPack.cpp` (×20+) |
| `dtDecay` dt-correct callers | `firmware-v3/src/effects/PersistenceHelpers.h:57,69` (helper); 17 LGP/Audio* call-sites |
| BASE_COAT zero-references | `grep BASE_COAT firmware-v3/src/` returns nothing |

---

## Status

| Phase | Status | Owner | Output |
|---|---|---|---|
| A — SSA forensic substrate gathering | DONE 2026-04-29 | Agent | This document |
| B — Captain ratification | NOT STARTED | Captain | Verdicts on Q1–Q8 + property table |
| C — `EFFECT_FRAMEWORK_STANDARD.md` authoring | BLOCKED on B | Agent | Single canonical standard doc |
| D — Lint detector additions | BLOCKED on C | Agent | Updated `check_effect_contracts.py` |
| E — Catalogue audit (SSA fleet) | BLOCKED on D | Agent (SSA fleet) | Per-effect compliance scoring |
| F — Remediation backlog | BLOCKED on E | Captain + Agent | Per-effect: refactor / `isExperimental` / deregister |

This document is the deliverable of Phase A. Phase B is Captain's read + ratify. Phase C onwards is downstream agent code work.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-29 | agent:opus-4.7-1M (synthesised from 3-SSA forensic swarm: SSA-3-CORE / SSA-3-INCIDENTAL / SSA-3-META, all high-confidence returns) | Created. Forensic substrate for Captain's ratification of `EFFECT_FRAMEWORK_STANDARD.md`. Surfaces two pre-findings that dissolve/reframe original questions 3.A.3 and 3.A.4: (1) the 96-bin chromagram is a category error — backend is already 64-bin SB-parity-aligned; (2) the "9 violations" figure is stale — most recent audit found 0 genuine on 106 effects. Compiles 12 LOAD-BEARING property classifications (9 MUST + 3 SHOULD recommended), 8 INCIDENTAL recommendations (2 helpers / 5 stay-free / 1 archive-deprecated), and 8 residual Captain decisions Q1–Q8 grouped by impact tier (A 5-min / B planning-session / C strategic). All claims cite source file:line. No verdicts recorded — Captain reads, ratifies, then Phase B (standard authoring) begins. |
