---
abstract: "16 firmware spec changes from HUNT Wave 1 research (60 web searches, 6 vectors). Tier 1 critical: vibrato-suppressed onset (SuperFlux), asymmetric smoothing, K-weighted RMS. Tiers 2-4 cover refractory, weights, hue caps, motion variation."
---

# K1 Translation Engine — HUNT Wave 1 Spec Change Document

**Date:** 2026-03-09
**Source:** HUNT mode research pipeline — 12 cycles, ~60 web searches, 6 research vectors
**Author:** Automated ArXiv Hunt Cycle (scheduled task)
**Status:** HUNT WAVE 1 COMPLETE

---

## Executive Summary

This document consolidates all firmware v3 spec changes recommended by the HUNT Wave 1 research cycle. The research validated the Translation Engine's multi-feature architecture as fundamentally sound while identifying 16 specific improvements across 4 tiers of priority. Three changes are critical bug-class fixes (vibrato suppression, asymmetric smoothing, K-weighted RMS). The remainder are evidence-based refinements to weights, formulas, and perceptual constraints.

**Overall finding:** The firmware's approach is architecturally correct. No published alternative (neural, learned, or otherwise) is viable on ESP32 hardware for real-time audio-visual translation. The specific weights, thresholds, and formulas are the weak points — most are bespoke and unvalidated, but can be improved using the evidence gathered here.

---

## Tier 1 — CRITICAL (must implement for v1)

### 1. Vibrato-Suppressed Onset Detection
**Vector:** V1 | **Confidence:** HIGH

**Current:** Raw spectral flux in `change_score` formula.
**Problem:** [FACT] Raw spectral flux produces documented false positives on vibrato, tremolo, and sustained strings (Böck & Widmer, DAFx 2013). Spectral flux fails on sustained-string instruments (hardest instrument group) and repeated notes with insufficient magnitude increase.
**Change:** Replace raw spectral flux with **SuperFlux** (maximum-filtered spectral flux) or **CGD** (chirp group delay smoothened) onset detection.

- SuperFlux: battle-tested in madmom, Essentia, librosa. Online mode viable. +4% recall on vibrato-heavy music.
- CGD: 300% more computationally efficient (3.2ms vs 9.4ms total). Same F1 scores. 2024 paper, no production deployment yet.

**Recommendation:** Ship v1 with SuperFlux (lower risk). Evaluate CGD for v1.1 firmware update.
**Impact:** Eliminates a class of false boundary detections in change_score.
**Effort:** Medium — algorithm swap in spectral processing pipeline.

---

### 2. Asymmetric Exponential Smoothing
**Vector:** V4 | **Confidence:** HIGH

**Current:** Symmetric smoothing (single alpha).
**Problem:** Symmetric smoothing makes LEDs either sluggish (low alpha) or jittery (high alpha). Cannot be both responsive and smooth.
**Change:** Implement separate attack and decay constants.

```
attack_alpha ≥ 0.95  (responds within 1-2 frames at 60fps)
decay_alpha  ≈ 0.05–0.15  (300-600ms decay to 37%)
```

**Evidence (3 independent streams converging):**
- [FACT] Perceptual science: ramped sounds (slow attack, fast decay) are perceived as louder/longer. Humans detect audio-leading-visual errors more readily than visual-leading-audio. (Vatakis & Spence 2006)
- [FACT] Open-source practice: scottlawsonbc (most-forked LED visualiser) uses rise=0.99, decay=0.01–0.5. LedFx uses same pattern. (github.com/scottlawsonbc/audio-reactive-led-strip)
- [FACT] IEC 61672-1 Impulse mode: 35ms attack / 1500ms decay. Industry reference for perceptual amplitude tracking.

**Recommendation:** This is the single highest-leverage perceptual improvement.
**Impact:** Transforms perceived responsiveness without adding visual noise.
**Effort:** Low — modify smoothing filter to accept two alpha parameters.

---

### 3. K-Weighted RMS
**Vector:** V3 | **Confidence:** HIGH

**Current:** Linear (unweighted) RMS.
**Problem:** [FACT] Linear RMS does not correlate well with perceived loudness. Misses the human hearing system's frequency-dependent sensitivity.
**Change:** Replace linear RMS with K-weighted RMS via 2-stage IIR biquad filter.

- K-weighting: pre-filter (+4dB shelf above 3kHz) + high-pass rolloff below 100Hz.
- [FACT] Implementable as 2 cascaded biquad filters. <5ms on ESP32 at 240MHz.
- [FACT] Single-precision float IIR filters may fail below 40Hz — use .32 fixed-point.
- Fallback: A-weighting (single biquad) if K-weighting budget is too tight.

**Recommendation:** Medium-effort change with high perceptual impact. Aligns RMS with ITU-R BS.1770 (LUFS) standard used universally in broadcast and music production.
**Impact:** Improves loudness tracking accuracy across all frequency content.
**Effort:** Medium — IIR filter implementation + fixed-point arithmetic.

---

## Tier 2 — HIGH (strong evidence, should implement for v1)

### 4. Continuous Refractory Formula
**Vector:** V4 | **Confidence:** MEDIUM-HIGH

**Current:** 3-tier step function: 150ms / 250ms / 350ms by tempo range.
**Change:** `refractory_ms = 80 + (4200 / BPM)`

| BPM | Old (step) | New (continuous) |
|-----|-----------|-----------------|
| 60  | 350ms     | 150ms           |
| 90  | 250ms     | 127ms           |
| 120 | 150ms     | 115ms           |
| 150 | 150ms     | 108ms           |
| 180 | 150ms     | 103ms           |

**Evidence:** [FACT] Temporal Binding Window narrows with tempo: ~200ms at 60 BPM, ~150ms at 120 BPM (Zamfira, British Journal of Psychology 2026). The continuous formula matches this perceptual data and eliminates tier boundary discontinuities.
**Effort:** Low — single formula change.

---

### 5. Revised Tension/Energy Weights
**Vector:** V3 | **Confidence:** MEDIUM

**Current:** `0.30 × RMS + 0.25 × flux + 0.25 × onset_density + 0.20 × harmonic_complexity`
**Change:** `0.35 × RMS + 0.30 × flux + 0.20 × onset_density + 0.15 × harmonic_complexity`

**Evidence:**
- [FACT] TenseMusic: loudness dominates predictions. Onset frequency has small/negative weight. (PLOS ONE, PMC10798497)
- [FACT] Gingras et al.: spectral flux alone explains R²=0.65 of arousal variance. (QJEP 2014)
- [INFERENCE] RMS weight increase supported by loudness-dominant finding. Flux increase supported by independent arousal prediction capacity. Onset_density decrease supported by near-zero TenseMusic weight (but moderated — the firmware targets energy/arousal, not tension, where onset contributes more). Harmonic_complexity is the least validated component.

**Important note:** These values should be parameterised as configurable constants for A/B testing. Treat as starting points, not final values.
**Effort:** Low — constant changes + configuration infrastructure.

---

### 6. Hue Change Rate Cap at 8 Hz
**Vector:** V5 | **Confidence:** HIGH

**Current:** No upper bound on hue_shift_speed.
**Change:** Hard-clamp: `min(hue_shift_speed, 8.0)`

**Evidence:** [FACT] Chromatic flicker fusion at ~10 Hz (isoluminant) to ~25 Hz (with luminance differences). Chromatic temporal contrast sensitivity drops monotonically with frequency — best at slow changes, progressively worse at fast. (Jiang et al., Journal of Neuroscience 2007; PMC1193381)
**Effort:** Trivial — single clamp operation.

---

### 7. Per-Feature Asymmetric Smoothing
**Vector:** V3 | **Confidence:** MEDIUM

**Current:** Single global smoothing alpha.
**Change:** Per-feature attack/decay constants:

| Feature | Attack α | Decay α | Rationale |
|---------|----------|---------|-----------|
| RMS | 0.95 | 0.15 | Fast energy response, moderate persistence |
| Flux | 0.90 | 0.20 | Rapid spectral change detection, smooth release |
| Onset density | 0.85 | 0.10 | Rhythmic drive capture, short persistence |
| Harmonic complexity | 0.70 | 0.05 | Slow harmonic evolution, long persistence |

**Evidence:** [FACT] Farbood (2012): optimal attentional windows vary by feature — 3s for most, 7s for tonal tension. [FACT] TenseMusic: feature-specific sliding windows with asymmetric memory decay. [FACT] IEC 61672-1 Fast = 125ms.
**Effort:** Medium — separate smoothing instances per feature.

---

## Tier 3 — MEDIUM (defensible, recommended for v1)

### 8. Parameterise change_score Weights
**Vector:** V1 | **Confidence:** MEDIUM

**Current:** Hard-coded 0.45 / 0.30 / 0.25 (energy / flux / harmonic).
**Change:** Expose as runtime-configurable constants. Keep current values as defaults.
**Rationale:** No published paper validates these specific weights. They need empirical calibration via A/B testing against annotated music with known phrase boundaries.
**Effort:** Low.

---

### 9. Tempo-Relative Boundary Threshold
**Vector:** V1 | **Confidence:** MEDIUM

**Current:** Fixed threshold 0.18.
**Change:** `effective_threshold = 0.18 × (120 / current_BPM)^0.3`

At 60 BPM → 0.21, at 120 BPM → 0.18, at 180 BPM → 0.16. Gentle scaling — lower threshold at high tempos catches smaller change deltas; higher threshold at low tempos prevents false positives.
**Effort:** Low.

---

### 10. Beat-Anchored Hue Change Rate
**Vector:** V5 | **Confidence:** MEDIUM

**Current:** `hue_shift_speed = base_rate × (0.5 + 0.5 × harmonic_complexity) × tempo_scale`
**Change:** `hue_shift_speed = clamp((BPM / 60) × (0.5 + 0.5 × harmonic_complexity), 0.5, 8.0)`

Anchors to beat rate (one hue transition per beat), modulated by harmonic complexity (0.5× to 1× of beat rate), hard-capped at 8 Hz, floored at 0.5 Hz.
**Effort:** Low — formula replacement.

---

### 11. Saturation Modulation from Harmonic Complexity
**Vector:** V5 | **Confidence:** MEDIUM

**Current:** Harmonic complexity affects hue speed only.
**Change:** Add: harmonic_complexity also modulates saturation. High complexity → reduced saturation (matching consonance/dissonance perceptual research).

**Evidence:** [FACT] Ciuha et al. (ACM MM 2010): consonance maps to high saturation, dissonance maps to low saturation.
**Effort:** Low — add one multiplication to saturation calculation.

---

### 12. Motion Primitive Variation Factor
**Vector:** V2 | **Confidence:** MEDIUM

**Current:** Deterministic primitive activation.
**Change:** Add variation factor (0.0–0.3) introducing controlled randomness in animation parameters per primitive activation. Scale inversely with selection confidence.

**Evidence:** [FACT] "Glow with the Flow" (ArXiv 2602.08838, 2026) user study: AI-generated lighting was rated "repetitive" and "too explicit" compared to hand-authored designs. Direct, deterministic mapping feels mechanical.
**Effort:** Low-medium — random offset generation + confidence-based scaling.

---

## Tier 4 — DEFERRED (v1.1 or v2)

### 13. Two-Tier Onset Priority (v1.1)
Beat-aligned onsets at 60% refractory; non-beat onsets at standard. Neurological justification via auditory alpha-phase-reset in visual cortex.

### 14. Jitter Minimisation (v1.1)
Target <5ms σ of frame processing time. Jitter is more perceptually damaging than fixed latency offset.

### 15. Primitive Transition Crossfade (v1.1)
50–100ms interpolation buffer between primitive transitions to prevent discontinuities.

### 16. Telemetry-First Learned Models (v2 roadmap)
No learned models for v1. For v2: implement telemetry logging of (audio_features, selected_primitive, visual_params, user_engagement_proxy). Accumulate 10K+ tuples across beta users before training any model. The rule-based system IS the teacher model.

---

## Framing Corrections

Two important framing changes, not formula changes:

1. **Rename "tension" to "energy"** — The firmware computes visual energy reactivity, not perceived musical tension. Even the best academic tension models (TenseMusic, r=0.68) explain only ~46% of variance. The firmware should use `energy_score` not `tension_score` in variable names and documentation.

2. **Document the 6-primitive taxonomy as a design choice, not an empirical finding** — No published taxonomy validates DRIFT/FLOW/BLOOM/RECOIL/LOCK/DECAY. The taxonomy is purpose-built for LED motion patterns. Laban's 8 Effort Actions are the nearest academic analogue but were designed for human body movement, not LED animation. User study validation is the only path to empirical support.

---

## Research Gaps Identified (Engineering/Product Tasks)

These cannot be resolved by further literature search:

1. **A/B testing framework** for weight calibration (change_score weights, energy weights) — engineering task
2. **Perceptual distinctiveness testing** for 6 motion primitives — product/user-research task (can users distinguish DRIFT from FLOW? BLOOM from RECOIL?)
3. **Genre-specific parameter profiles** — all referenced research uses Western classical or pop music. EDM, jazz, ambient, hip-hop may need different parameter sets. Engineering + user testing.
4. **LED-specific perceptual testing** — most referenced research uses screens, headphones, or VR. LED strip perception characteristics (spatial resolution, viewing angle, ambient light interaction) are under-researched.

---

## Source Summary

Key papers referenced across all vectors:

- Müller et al. (2024) — Novelty functions for music signal processing (TISMIR)
- Böck & Widmer (2013) — SuperFlux vibrato suppression (DAFx)
- Barchet et al. (2024) — TenseMusic feature weights and temporal windows (PLOS ONE)
- Gingras et al. (2014) — Spectral flux predicts 65% arousal variance (QJEP)
- Herremans & Chew (2020) — Tonal tension model, r=0.750 (Entropy)
- Zamfira (2026) — Temporal Binding Window narrows with tempo (BJP)
- Ciuha et al. (2010) — Circle-of-thirds harmonic-colour mapping (ACM MM)
- Jiang et al. (2007) — Chromatic flicker fusion (Journal of Neuroscience)
- Spence (2011) — Crossmodal correspondences tutorial review (APP)
- ArXiv 2408.13734 (2024) — CGD onset detection, 300% more efficient than SuperFlux
- ArXiv 2602.08838 (2026) — "Glow with the Flow" LED lighting user study
- IEC 61672-1 — Sound Level Meter exponential averaging standards
- scottlawsonbc/audio-reactive-led-strip — Open-source LED smoothing reference implementation
