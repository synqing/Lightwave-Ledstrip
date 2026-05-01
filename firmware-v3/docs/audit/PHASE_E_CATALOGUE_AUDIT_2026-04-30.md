---
abstract: "Phase E catalogue-wide compliance audit against 12 LOAD-BEARING rules in EFFECT_FRAMEWORK_STANDARD.md. Three violation classes found: I-3 frame-coupled fadeToBlackBy (97 files, 139 sites), I-1 open-coded squaring (4 files), Rule #9 local silence gates (6 files). All other 9 rules pass clean. Includes migration manifest and priority order."
---

# Phase E — Catalogue Compliance Audit
**Date:** 2026-04-30
**Scope:** All effect source files against the 12 LOAD-BEARING rules in `firmware-v3/docs/EFFECT_FRAMEWORK_STANDARD.md`
**Basis:** Phase D lint detectors passing clean; this audit covers the remaining rule classes not covered by automated lint.

---

## Executive Summary

Three violation classes were identified across the full catalogue. Nine rule classes pass with zero violations.

| Class | Rule | Files | Sites | Severity |
|-------|------|-------|-------|----------|
| I-3 | Frame-coupled `fadeToBlackBy` (Rule #12 adjacent) | 97 | 139 | High — frame-rate-dependent visual behaviour |
| I-1 | Open-coded squaring instead of `applyContrast` (Rule #3 adjacent) | 4 | 4 | Medium — visual output differs from canonical K1 curve |
| #9 | Local silence early-return (Rule #9) | 6 | 6 | Low — double-fade artefact under silence |

No violations in: centre-origin (Rule #5), no-rainbow (Rule #6), hard brightness (Rule #8), AR liveness (Rule #11), Rule #12 lint band [0.80–0.99], K1 AP-only, raw `bins256` containment, tempo-bank, GEO-kill patterns, stacked smoothing.

---

## Rule-by-Rule Results

### PASS — Rule #5: Centre Origin
**0 violations across 179 files.**
All effects originate from LED 79/80 outward. No linear sweeps detected.

### PASS — Rule #6: No Rainbow
**0 violations.**
No full hue-wheel sweep patterns (`fill_rainbow`, rainbow cycling) detected in any effect source.

### PASS — Rule #8: Hard Brightness Cap
**0 violations** (1 test-rig exception correctly allowlisted).

### PASS — Rule #11: AR Liveness
**0 violations.**
All audio-reactive effects use `controlBus` fields correctly. No dead audio paths detected.

### PASS — Rule #12 Lint Band [0.80–0.99]
**0 violations.**
Phase D lint gate is clean. No bare `fadeToBlackBy` constants in the lint-targeted band.

### PASS — K1 AP-Only
**0 violations.**
No STA-mode or AP+STA concurrent configuration in any effect or network file.

### PASS — Raw `bins256` Containment
**0 violations.**
No direct `bins256[]` access outside the approved adapter layer.

### PASS — Tempo-Bank
**0 violations.**

### PASS — GEO-Kill Patterns
**0 violations.**

### PASS — Stacked Smoothing
**0 violations.**

---

### FAIL — I-3: Frame-Coupled `fadeToBlackBy` (Rule #12 Adjacent)

**97 files, 139 call sites.**

`FastLED::fadeToBlackBy(leds, n, fadeBy)` applies a fixed fractional decay per render call. At 119 FPS the strip decays approximately 2× faster than at 60 FPS, making persistence a function of frame rate rather than wall time. This is a visual correctness defect.

**Replacement:** `fadeToBlackByDt(leds, n, fadeBy, ctx.dt)` from `PersistenceHelpers.h`. Drop-in; same integer `fadeBy` semantics, compensated for elapsed time. No include change required if `PersistenceHelpers.h` is already transitively included; otherwise add `#include "effects/helpers/PersistenceHelpers.h"`.

#### Subclass A — Hardcoded constant `fadeBy` (most critical — deterministic mismatch)

| File | `fadeBy` constant |
|------|-------------------|
| `AttackOnlyPitchVelocityFieldEffect.cpp` | 30 |
| `AudioBloomEffect.cpp` | 25 |
| `BreathingEffect.cpp` | 15 |
| `ConfettiEffect.cpp` | 20 |
| `LGPBeatPrismOnsetEffect.cpp` (×5 variants) | 30 each |
| `LGPBeatPulseEffect.cpp` | 35 |
| `LGPColorTemperatureEffect.cpp` | 8 |
| `LGPExperimentalAudioPack.cpp` (×14 internal effects) | 30 |
| `LGPGravitationalLensingEffect.cpp` | 20 |
| `LGPPerlinCausticsAmbientEffect.cpp` | 10 |
| `LGPPerlinInterferenceWeaveAmbientEffect.cpp` | 10 |
| `LGPPerlinShocklinesAmbientEffect.cpp` | 12 |
| `LGPPerlinVeilAmbientEffect.cpp` | 10 |
| `LGPPerceptualBlendEffect.cpp` | 8 |
| `LGPRGBPrismEffect.cpp` | 10 |
| `PulseEffect.cpp` | 30 |
| `WaveAmbientEffect.cpp` | 12 |
| `WaveEffect.cpp` | 12 |
| `WaveReactiveEffect.cpp` | 12 |

#### Subclass B — Dynamic `ctx.fadeAmount` (frame-coupled; value is parameter-driven but uncompensated)

BPMEnhancedEffect.cpp, BreathingEnhancedEffect.cpp, ChevronWavesEffectEnhanced.cpp, HeartbeatEffect.cpp, HeartbeatEsTunedEffect.cpp, JuggleEffect.cpp, LGPAuroraBorealisEffect.cpp, LGPBioluminescentWavesEffect.cpp, LGPBoxWaveEffect.cpp, LGPChladniHarmonicsEffect.cpp, LGPColorAcceleratorEffect.cpp, LGPFresnelCausticSweepEffect.cpp, LGPGoldCodeSpeckleEffect.cpp, LGPGravitationalWaveChirpEffect.cpp, LGPKdVSolitonPairEffect.cpp, LGPMeshNetworkEffect.cpp, LGPMycelialNetworkEffect.cpp, LGPNeuralNetworkEffect.cpp, LGPNeuralNetworkRadialEffect.cpp, LGPPerlinCausticsEffect.cpp, LGPPerlinInterferenceWeaveEffect.cpp, LGPPerlinShocklinesEffect.cpp, LGPPerlinVeilEffect.cpp, LGPQuantumEntanglementEffect.cpp, LGPQuantumTunnelingEffect.cpp, LGPQuasicrystalLatticeEffect.cpp, LGPSolitonWavesEffect.cpp, LGPSolitonWavesRadialEffect.cpp, LGPStarBurstEffect.cpp, LGPStarBurstEffectEnhanced.cpp, LGPStarBurstNarrativeEffect.cpp, LGPWaveCollisionEffect.cpp, LGPWaveCollisionEffectEnhanced.cpp, RippleEnhancedEffect.cpp, SinelonEffect.cpp, plus the `sensorybridge_reference/` family.

---

### FAIL — I-1: Open-Coded Squaring (Rule #3 Adjacent)

**4 files, 4 sites.**

These files use `float bright = bin * bin` (pure x²) rather than `applyContrast(bin, kSbK1SquareIter)`, which computes `bin² × 0.65 + bin × 0.35` — the canonical K1 mix-back that preserves low-energy bin visibility. The open-coded form produces brighter peaks but crushes low-energy bins relative to the canonical curve. Visual output differs noticeably on sparse material.

| File | Line | Expression |
|------|------|------------|
| `ChevronWavesEffect.cpp` | 70 | `bin * bin` |
| `ChevronWavesEffectEnhanced.cpp` | 88 | `bin * bin` |
| `SnapwaveLinearEffect.cpp` | 151 | `bin * bin` |
| `LGPWaveCollisionEffect.cpp` | 62 | `bin * bin` |

**Fix:** Add `#include "effects/math/Contrast.h"` if not already present. Replace `bin * bin` with `applyContrast(bin, kSbK1SquareIter)`.

**Caution:** This is a visible visual change. Captain hardware A/B test is required before committing. Do not batch with I-3 remediation.

---

### FAIL — Rule #9: Local Silence Early-Return (6 files — allowlisted, Phase E remediation)

**6 files, 6 sites.**

| File | Line | Pattern |
|------|------|---------|
| `AudioWaveformEffect.cpp` | 137 | `if (!ctx.audio.available) return;` |
| `AudioBloomEffect.cpp` | 143 | `if (!ctx.audio.available) return;` |
| `LGPSpectrumDetailEffect.cpp` | 69 | `if (!ctx.audio.available) return;` |
| `LGPSpectrumDetailEnhancedEffect.cpp` | 114 | `if (!ctx.audio.available) return;` |
| `TrinityTestEffect.cpp` | 76 | `if (!ctx.audio.available) return;` |
| `WaveformParityEffect.cpp` | 63 | `if (!ctx.audio.available) return;` |

The global `silent_scale` in `RendererActor` already attenuates all effect output during silence. An additional early-return in the effect layer causes the strip to hard-cut to black rather than fade gracefully, doubling the decay artefact.

**Fix:** Remove the `if (!ctx.audio.available) return;` guard in each file. No replacement needed — the global handler covers the case.

---

## Migration Manifest — I-3 `fadeBy` → `rate60fps` Conversion

The `fadeToBlackByDt` function signature is:

```cpp
fadeToBlackByDt(CRGB* leds, uint16_t count, uint8_t fadeBy, float dt);
```

`dt` is `ctx.dt` (seconds per frame). The function internally computes:

```
decayFactor = pow(1.0f - fadeBy / 256.0f, dt * 60.0f)
```

The integer `fadeBy` value already encodes a 60 FPS decay target. No constant conversion is needed — pass the original `fadeBy` value unchanged. The only mechanical change per call site is:

```cpp
// Before
fadeToBlackBy(leds, NUM_LEDS, 30);

// After
fadeToBlackByDt(leds, NUM_LEDS, 30, ctx.dt);
```

### Common constant reference

| `fadeBy` value | Approximate half-life at 60 FPS | Used by |
|---|---|---|
| 8 | ~21 frames (~350 ms) | `LGPColorTemperatureEffect`, `LGPPerceptualBlendEffect` |
| 10 | ~17 frames (~280 ms) | `LGPPerlinCausticsAmbientEffect`, `LGPPerlinVeilAmbientEffect`, `LGPRGBPrismEffect` |
| 12 | ~14 frames (~230 ms) | `LGPPerlinShocklinesAmbientEffect`, `WaveAmbientEffect`, `WaveEffect`, `WaveReactiveEffect` |
| 15 | ~11 frames (~185 ms) | `BreathingEffect` |
| 20 | ~8 frames (~135 ms) | `ConfettiEffect`, `LGPGravitationalLensingEffect` |
| 25 | ~7 frames (~110 ms) | `AudioBloomEffect` |
| 30 | ~5 frames (~87 ms) | `AttackOnlyPitchVelocityFieldEffect`, `LGPBeatPrismOnset*`, `LGPExperimentalAudioPack`, `PulseEffect` |
| 35 | ~4 frames (~72 ms) | `LGPBeatPulseEffect` |

Half-life values are informational only. The `fadeToBlackByDt` function preserves these exact timings at any frame rate by compensating with `dt`.

---

## Remediation Priority Order

### Priority 1 — I-3 Subclass A (hardcoded constants) — no Captain gate required

Mechanical substitution, no visual tuning involved. Safe to batch as a single commit per logical group.

1. Include `PersistenceHelpers.h` where not already present.
2. Replace all Subclass A call sites: `fadeToBlackBy(leds, n, K)` → `fadeToBlackByDt(leds, n, K, ctx.dt)`.
3. Build and flash. Verify no frame-timing regression (target: sub-2.0 ms per frame).

### Priority 2 — Rule #9 silence gates — no Captain gate required

Remove six early-return guards. Low risk; global handler already covers the behaviour.

### Priority 3 — I-3 Subclass B (dynamic `ctx.fadeAmount`) — parameter audit first

Before converting, verify whether any `ctx.fadeAmount` parameters are already calibrated by the user against a specific frame rate. If calibrated at 60 FPS, conversion is still the correct fix. If calibrated at 119 FPS, converted values will appear to have longer persistence — Captain should be informed before merge.

### Priority 4 — I-1 open-coded squaring — Captain A/B test required

Do not merge without hardware sign-off. Submit as a runtime-switchable A/B before committing, per the TEST-THEM-ALL rule in `MEMORY.md`.

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-04-30 | agent:claude-sonnet-4-6 | Created — Phase E catalogue audit, 3 violation classes, 9 rule passes, migration manifest and priority order |
