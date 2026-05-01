---
abstract: "Architecture reform proposal grounded in upstream canonical patterns (SB 3.1.0/4.1.0/4.1.1, Emotiscope 1.2). Diagnoses K1's visual pipeline as having FOUR responsibilities (smoothing, motion rate, persistence, audio→visual mapping) wrongly placed inside each effect. Proposes a 5-layer model that puts each responsibility OUTSIDE the effect. Recommends a 3-tier scope: spazz fix immediately, primitives kernel concurrently, full audio-layer reform as separate milestone. The principle: K1 effects should be DUMB; smart goes UPSTREAM."
---

# K1 Visual Pipeline Reform

**Date**: 2026-04-30
**Source evidence**: 18 SSAs (10 internal + 8 against canonical upstream sources) summarised in `PORT_PLAN.md` and `SYNTHESIS.md`.
**Status**: Proposal. No code changes implied by this document. Captain decision required on scope before any implementation.

---

## §1 The diagnostic principle

> The canonical pipelines (SB 3.1.0, SB 4.1.0/4.1.1, Emotiscope 1.2) put **smoothing, motion rate, persistence, and audio→visual mapping OUTSIDE the effect**. K1 puts all four INSIDE every effect. That is why every effect has its own bugs and every redesign is a per-effect war.

Eight SSAs cross-confirm the same architectural divergence:

| Responsibility | Canonical placement | K1 current placement |
|---|---|---|
| **Audio smoothing** | Producer-side (SB `low_pass_array(... MOOD)`; Emotiscope `*_smooth[]` arrays). One smoothing chain per signal, MOOD-controlled. | Each effect builds its own — `AsymmetricFollower` here, `Spring` there, `EMAImpl` somewhere else, all on overlapping inputs. |
| **Motion rate** | Time-driven, scroll-fixed, or `tempi[i].phase`-coherent. NEVER audio-amplitude. | `m_phase += speedNorm × 240 × smoothedSpeed × dt` — three audio-modulated terms multiplying. |
| **Persistence** | Single global pass: `apply_frame_blending()` whole-image IIR (Emotiscope), or shift-register scroll on a previous-frame buffer (SB), or no decay at all (SB 3.1.0 redraws clean). | Every effect calls `fadeToBlackByDt(K)` itself. K varies per-effect. Trail length is amplitude-coupled, non-deterministic. |
| **Audio→visual mapping** | ONE law per mode: amplitude→position, OR scroll-N-per-frame, OR `sin(tempi.phase)`. Never combined. | Compounded — Snapwave does `tanh(Σ chroma·sin(t·multifreq)) × rms × 79`; Chevron does Spring on bass × tanh slope on snare × phase × dt × speed. |

The K1 effect layer is FAT because each effect re-invents an entire mini-pipeline. Canonical effect layers are THIN because the heavy lifting happens elsewhere.

This explains every spazz mode. It explains why fixes for one effect break others. It explains why doctrine drifted into circular self-citation — there was no single architectural source-of-truth to cite, only a collection of bespoke per-effect implementations.

---

## §2 The 5-layer canonical model

### Layer 1 — Audio signal conditioning (producer-side)

Smoothing happens ONCE, at the source, before publication.

| Signal | Canonical taxonomy | Recommended K1 source |
|---|---|---|
| Spectrogram smoothed | `magnitudes_smooth[]` low-passed at `1.0 + 10·MOOD` Hz cutoff (SB 4.1.0 GDFT.h:149) | `bins64Adaptive[]` after MOOD-controlled IIR (currently MOOD only affects `getMoodSmoothing` coefficients, NOT wired into bins) |
| Chromagram smoothed | `chromagram_smooth[12]` per-bin EMA (SB 4.1.0 led_utilities.h:1140+) | `chroma[]` already on ControlBus; verify deep smoothing is alive on K1 path |
| VU level | `vu_level_smooth` one-pole + `audio_vu_level_average` longer EMA (Emotiscope vu.h) | `rms` exists; need explicit `vu_smooth_50ms` and `vu_smooth_200ms` siblings |
| **Tempi phase bank** | `tempi[NUM_TEMPI]` — Goertzel on novelty curve at 16 BPM bins, each with `phase`/`magnitude`/`beat=sin(phase)` (Emotiscope tempo.h) | **Does not exist on K1.** Single locked `tempoBPM` + `beatPhase`. This is the structural gap. |
| Beat oscillation scalar | LightwaveOS-internal canonical: single `[-1, +1]` scalar derived upstream from spectrogram + chromagram + tempo + onset, smoothed via `1 - exp(-dt × 10Hz)` (SNAPWAVE_BEAT_DETECTION_API.md) | **Does not exist on K1.** Effects that want it must roll their own. |
| Novelty curve ring | `novelty_curve[N]` — column-summed positive change in spectrogram (SB GDFT.h) | **Does not exist on K1.** Prerequisite for tempi[] bank. |

K1's `liveliness` and `audioConfidence` are populated only on the LWLS path — on K1 they're constants (1.0 and 0.0 respectively per SSA-5). That's a Layer 1 silent bug.

### Layer 2 — ControlBus publishing contract

The contract effects consume.

**Naming convention** (proposed):
- `field_raw` — what the producer wrote at audio rate, no smoothing
- `field_fast` — ~20-50ms tau (kicks visible)
- `field_smooth` — ~150-300ms tau (perceptual amplitude)
- `field_slow` — ~500-1000ms tau (musical-passage envelope)

Each smoothed field carries its tau as a comment in `ControlBus.h`. Effects pick the smoothing they want by FIELD CHOICE, not by adding a smoother.

**Field audit todos** (out of scope for spazz fix):
- Mark which fields are LWLS-only-alive vs K1-alive
- Document tau for every smoothed field
- Identify dead fields (`UpdateFromHop` envelope per SSA-5 finding)
- Add `tempi[]`, `beatOsc`, `noveltyCurve[]` per Layer 1 gaps

### Layer 3 — Effect base classes (motion law inheritance)

Three motion laws, one base class each. Effects inherit one and override only `drawContent()`.

```cpp
class AmplitudeEffect : public IEffect {
    // Canonical: position = f(amplitudeSmooth) at fixed mapping curve.
    // Base provides: smoothed-amplitude pickup, motion-blur trail via shared buffer.
    // Override: drawContent(zoneCtx, position, palette, magnitude).
};

class ScrollEffect : public IEffect {
    // Canonical: scroll a content buffer N pixels per frame outward from centre.
    // Base provides: scroll accumulator, multiplicative-decay trail (alpha-blend),
    //                centre-origin scroll, dual-strip mirror.
    // Override: drawContentAtCentre(zoneCtx, palette, magnitude).
};

class RhythmicEffect : public IEffect {
    // Canonical: position or brightness driven by sin(tempi[i].phase).
    // Base provides: tempi[] bank pickup, phase-to-position mapping.
    // Override: drawAtPhase(zoneCtx, phase, palette, magnitude).
};

class SpectralEffect : public IEffect {
    // Canonical: direct per-LED fill from spectrogram.
    // Base provides: bins-to-LED mapping, palette lookup.
    // Override: contentForBin(bin) optionally; default uses palette directly.
};
```

The 4 broken effects map to:
- ChevronWaves → ScrollEffect (kaleidoscope-style outward scroll)
- ChevronWavesEnhanced → ScrollEffect (bloom-style sub-pixel scroll)
- LGPWaveCollision → ScrollEffect (two-pass mirrored scroll)
- SnapwaveLinear → AmplitudeEffect (vu_dot-style centred dot)

The other 195+ effects in the tree mostly map cleanly into these four base classes too. Most current effect code becomes a 30-line `drawContent()` override.

### Layer 4 — Render primitives library

Three primitives, K1-flavoured. Build them once; reuse forever.

```cpp
namespace lightwaveos::render {

// Auto motion-blur from per-slot position memory. Mirror of Emotiscope draw_dot.
void drawDot(RenderContext& ctx, uint8_t slotId, float position01,
             uint8_t hue, uint8_t brightness);

// Sub-pixel scroll of a previous-frame buffer with multiplicative decay.
// Mirror of Emotiscope draw_sprite. Centre-origin, outward-scroll variant.
void drawSpriteScrolled(RenderContext& ctx, const CRGB* prevFrame,
                        float alpha, float scrollPxPerFrame);

// Direct spectral fill with palette lookup. Mirror of SB gdft mode.
void fillFromBins(RenderContext& ctx, const float* bins, uint8_t binCount,
                  const Palette& palette);

}  // namespace
```

All primitives:
- Centre-origin (LED 79/80) by construction
- Dual-strip mirror handled internally
- dt-correct via `getSafeDeltaSeconds`
- Bounds-checked, no heap, < 0.5ms each at 320 LEDs

### Layer 5 — Frame post-process

Whole-image one-pole IIR after all effects render. Mirror of SB `apply_frame_blending()`.

```cpp
// Called by RendererActor AFTER all effects + zone composition complete,
// BEFORE FastLED.show(). One pass over the 320-LED frame buffer.
void applyFrameBlending(CRGB* leds, size_t n, float persistence);
```

Persistence comes from `ctx.mood`:
- mood=0 (reactive): persistence 0.0 (no blend, every frame fresh) — sharp/punchy
- mood=255 (smooth): persistence 0.92 — soft/floaty trails

This is THE place trail behaviour is decided. Effects no longer call `fadeToBlackByDt`. The lint detector for Rule #12 (frame-coupled decay) becomes meaningful because there's a canonical alternative.

---

## §3 K1 has-vs-needs (current state)

| Layer | What K1 has today | Gap |
|---|---|---|
| 1: Audio conditioning | Some smoothed bands; `UpdateFromHop` deeper envelope is **dead code on K1** (SSA-5); `liveliness=0` and `audioConfidence=1` constants on K1 path; no tempi[]; no novelty curve; MOOD not wired into spectrogram α | Major — Layer 1 is half-built |
| 2: ControlBus publishing | Mixed raw + smoothed fields; no tau metadata; no naming convention | Minor — naming convention can be added without breaking effects |
| 3: Effect base classes | Single `IEffect` base; everything bespoke | Major — no motion-law inheritance exists |
| 4: Render primitives | None — every effect has its own per-pixel loop | Major — primitives library does not exist |
| 5: Frame post-process | None — every effect calls `fadeToBlackByDt` itself | Major — global blending doesn't exist; rule #12 lint has no alternative to recommend |

This is the real picture. K1 is half-baked across all 5 layers. The 4 broken effects are the most visible symptom but the architectural debt is the whole pipeline.

---

## §4 Three scope options

### Option A — Tier 1 fix only (1–3 days)

Fix the 4 broken effects per `PORT_PLAN.md` Tier 1 sketches. Per-effect rewrites mirroring SB 4.1.0 patterns by mechanism. No new infrastructure. No primitives library. No base classes.

**Win**: spazz dies, ships fast.
**Loss**: the 195+ other effects retain the same architectural debt. Future effects keep being half-baked. The canonical patterns we just identified become research artefacts, not lived practice.

### Option B — Tier 1 fix + Layer 4 primitives (1–2 weeks) — **RECOMMENDED**

Build the 3-primitive render library (Layer 4). Port the 4 broken effects to USE those primitives — proving the kernel works. No base class hierarchy yet. No Layer 1/2/5 changes.

After this lands:
- 4 broken effects fixed AND clean architecture
- Primitives kernel proven on hardware
- Future effects (and the legacy 195) can adopt the kernel incrementally
- Layer 3 base classes can be added later by wrapping the same primitives
- Layer 1/5 audio reform becomes a separate, schedulable milestone

**Win**: ship-fast spazz fix + a foundation that compounds. Every future effect uses the kernel for free.
**Loss**: marginal — 3-5 extra days vs Option A. Audio layer still half-built but that's a separate decision.

### Option C — Full 5-layer reform (multi-week milestone)

All 5 layers in one branch. Layer 1 producer changes, Layer 2 ControlBus contract update, Layer 3 base classes, Layer 4 primitives, Layer 5 post-process. 4 broken effects ride along.

**Win**: fully canonical pipeline, all 195+ effects can be migrated to the new model with confidence.
**Loss**: scope creep, multi-week with no incremental ship, hardware testing gates everything, high risk of regressions in the existing 195+ effects during migration.

### Recommendation

**Option B.** It's the smallest delta that doesn't waste the research, doesn't lock us into the broken pattern for the rest of the tree, and ships in days-to-weeks, not weeks-to-months. Layer 1 audio reform (the `tempi[]` bank, MOOD-controlled spectrogram smoothing, beat oscillation scalar) becomes a separate milestone planned with its own roadmap — it's the bigger architectural prize but it doesn't need to gate the spazz fix.

Concretely Option B = `PORT_PLAN.md` Tier 1, but instead of bespoke per-effect code in §4 of that doc, the 4 effects each call into the new primitives library. Same fix, better foundation.

---

## §5 Testable invariants (post-reform lint rules)

After the reform, these become mechanically enforceable by `check_effect_contracts.py`:

1. **No `Spring` instance in effects**: smoothing happens upstream or by base class.
2. **No `fadeToBlackByDt` in effects**: persistence is Layer 5 post-process.
3. **No `is*Hit()` in `render()`**: binary triggers go through a `HoldDecayEnvelope` helper updated once per frame.
4. **Each effect declares ONE motion law** via base class inheritance (`AmplitudeEffect`, `ScrollEffect`, `RhythmicEffect`, `SpectralEffect`).
5. **No audio modulator on phase rate**: rate must be time-driven (`speedNorm × rawDt × BASE`) or `tempi[i].phase`-driven. Lint pattern: `*= dt` only allowed in base-class motion code.
6. **Effect render() body calls primitives, not bespoke per-pixel loops**: pattern detector for `for (i; i < ledCount; ...)` loops in non-base-class files.

These rules are currently violated by the broken 4 specifically — and by maybe 50+ other effects in the tree. The reform makes them enforceable; the violations become migration tasks, not bugs that ship.

---

## §6 Captain decision points

1. **Scope**: A, B, or C?  Recommendation: **B**.
2. **If B**: build primitives in `firmware-v3/src/effects/render/` as a new namespace? Or add to `CoreEffects.h`?
3. **If B**: Layer 1 audio reform as a separate planned milestone? When?
4. **If B**: do we also rip the false-canonical claims out of `EFFECT_FRAMEWORK_RATIFICATION_2026-04-29.md` and replace them with attribution to upstream SB/Emotiscope patterns? Or keep doctrine doc untouched until Layer 1 lands?
5. **Doctrine future**: should `firmware-v3/docs/EFFECT_FRAMEWORK_STANDARD.md` get a §"Canonical references" pointing at SB 4.1.0 + Emotiscope 1.2 + LightwaveOS internal Snapwave docs as the source-of-truth? (Yes from me — the current doc cites K1's own broken patterns.)

---

## §7 What the reform fixes that no per-effect bugfix can

- **Smoothing consistency**: today every effect picks a different tau. Tomorrow MOOD knob controls them globally and consistently.
- **Trail consistency**: today every effect picks a different `fadeToBlackBy` K. Tomorrow MOOD knob picks one. Effects never decide.
- **Beat coherence**: today every effect that wants beat-driven motion fights with the single locked phase. Tomorrow `tempi[]` gives 16 phases continuously.
- **Audio→visual confidence**: today nobody can predict how a new effect will respond to bass / chord / hihat without running it. Tomorrow the motion law is declared by the base class — predictable by construction.
- **Effect authoring time**: today a new audio-reactive effect is 200-400 lines of bespoke audio→DSP→position→pixel pipeline. Tomorrow it's a 30-line `drawContent()` override.
- **Lint coverage**: today the broken patterns are the de facto standard. Tomorrow they're mechanically rejected.

The 4 broken effects aren't 4 bugs. They're the visible tip of the architectural iceberg.

---

**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-04-30 | Claude (claude-sonnet-4-6) | Created — architecture reform proposal grounded in 18 SSA findings + Captain's directive to read upstream canonical sources. 5-layer model, 3 scope options, recommendation: Option B (Tier 1 fix + Layer 4 primitives kernel). |
