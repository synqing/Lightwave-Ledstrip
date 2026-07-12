---
abstract: "Effect Authoring Standard v2 for K1 LightwaveOS. Converts the older implementation checklist into a product-quality contract: intent first, centre-origin topology, audio-layer discipline, colour ownership, VP stack awareness, validation gates, and promotion rules."
---

# Effect Authoring Standard v2

**Status:** DRAFT - Captain-approved for documentation/tooling package on 2026-05-06. No firmware behaviour changes are made by this document.

**Authority anchors:**
- Project effect constraints define centre-origin, no rainbows, no heap in render paths, 120 FPS / 2.0 ms, dt-correct smoothing, sub-8 ms latency, and British English.
- `firmware-v3/docs/EFFECT_DEVELOPMENT_STANDARD.md:24-52` defines the current render pipeline and the 2 ms effect slot before colour correction, tone mapping, and LED output.
- `firmware-v3/docs/EFFECT_FRAMEWORK_STANDARD.md:32-114` records the existing MUST/SHOULD rules for thread separation, smoothing, centre-origin, chroma discipline, silence gating, and dt tau smoothing.
- `firmware-v3/docs/audio-visual/AUDIO_FEATURE_SURFACE_V2_CONTRACT.md:24-63` ranks raw substrates below effect-facing vectors, semantic scalars, events, envelopes, and validity metadata; new production effects must not directly scan raw `bins256`.
- `firmware-v3/docs/audio-visual/audio-visual-contract-surface.md:55-66` lists current audio accessors and states that raw FFT helpers are legacy/debug/research access.
- `firmware-v3/tools/check_effect_contracts.py:837-861` enforces the raw `bins256` containment gate for effects.
- `firmware-v3/docs/audit/VP_RENDER_PATH_LAYER_AUDIT_2026-05-05.md:36-55` defines the active visual-pipeline order.
- `firmware-v3/docs/research/PHASE5_EFFECTS_RESEARCH_SYNTHESIS_2026-04-27.md:15-90` records the failure mode behind the Phase 5 effect rebuilds: technically clean effects still failed visually because trails, palettes, smoothing, subpixel motion, and audio-field semantics were wrong.

## Purpose

V1 answered "does the effect compile and respect the local render contract?" V2 adds the product question: "does this produce a good K1 light show through the actual LGP, under the real VP stack, with audio signals interpreted correctly?"

An effect is not authoring-complete until it has:

1. a named visual intent;
2. an explicit authoring surface;
3. a mapped audio model;
4. a colour ownership decision;
5. a persistence model;
6. VP stack interaction notes;
7. trace or test evidence;
8. a promotion classification.

## Non-Goals

- This is not a rewrite of all existing effects.
- This is not permission to tune global colour correction, gamma, silence policy, EdgeMixer, or buffer ownership defaults.
- This is not a replacement for `EFFECT_DEVELOPMENT_STANDARD.md`; it sits above it and tightens the decision process.
- This is not a hardware sign-off. Hardware visual judgement still follows `firmware-v3/docs/audit/VP_VALIDATION_PROTOCOL_2026-05-06.md`.

## Required First Reads

Before creating, repairing, or promoting an effect, read:

1. Project hard constraints.
2. `firmware-v3/docs/EFFECT_DEVELOPMENT_STANDARD.md`.
3. `firmware-v3/docs/EFFECT_FRAMEWORK_STANDARD.md`.
4. `firmware-v3/docs/EFFECTS_BEHAVIORAL_REFERENCE.md` for colour-correction, tone-map, silence-gate, and palette caveats.
5. For audio-reactive effects, `firmware-v3/docs/audio-visual/AUDIO_FEATURE_SURFACE_V2_CONTRACT.md` and `firmware-v3/docs/audio-visual/audio-visual-contract-surface.md`.
6. `firmware-v3/docs/audit/VP_RENDER_PATH_LAYER_AUDIT_2026-05-05.md`.
7. For Phase 5 or complex audio-reactive effects, `firmware-v3/docs/research/PHASE5_EFFECTS_RESEARCH_SYNTHESIS_2026-04-27.md`.

## Authoring Intake Sheet

Every new or repaired effect starts with this sheet in the implementation note, PR body, or research doc:

| Field | Required Answer |
|---|---|
| Effect ID and name | Hex ID plus display name. |
| Intent sentence | One sentence in lay language: what should Captain see? |
| Product class | Hero / production / diagnostic / experimental / parked. |
| Spatial grammar | Field, radial wave, sprite, bloom, interference, scope, caustic, texture, or hybrid. |
| Authoring surface | `m_leds` unified, zone-composed unified, or direct physical strips. |
| Centre-origin proof | How writes originate from LED 79/80 outward. |
| Colour ownership | Palette-driven, chroma-anchored, optical exact RGB, or diagnostic fixed RGB. |
| Audio layers used | L0 bed, L1 structure, L2 impact, L3 tonal, L4 memory. |
| Persistence model | `fadeToBlackBy`, PSRAM trail, substrate ring, framebuffer blend, or none. |
| Silence policy | Ignore, soft-scale, hard-gate, or global inherited. |
| VP risk | Colour correction, tone map, buffer ownership, EdgeMixer, FastLED, or none. |
| Evidence plan | Native test, trace capture, serial status, Captain visual row, or all of these. |

If any row is unknown, the effect is not implementation-ready.

## Hard Authoring Rules

### 1. Topology

- Origin is LED 79/80 outward. No linear sweeps.
- Direct dual-strip effects must justify why they cannot be authored on the unified LGP surface.
- Direct-strip authorship is a product choice, not a shortcut around centre-origin.
- Any effect using top/bottom asymmetry must state what the viewer should perceive across the acrylic, not only what each strip does electrically.

### 2. Timing

- Use delta time from the context. Do not encode frame-count assumptions.
- Clamp dt through the existing safe context helpers.
- Motion frequency, decay, envelope release, and phase advance must remain stable if the renderer runs below or above nominal cadence.
- Moving points must use subpixel rendering when visible motion crosses LED boundaries.

### 3. Heap And Memory

- No `new`, `malloc`, `heap_caps_malloc`, `String`, `std::vector`, or allocation-owning containers in `render()` or any function called from `render()`.
- Buffers larger than 64 bytes belong in PSRAM unless they must be internal DRAM for measured hot-path reasons.
- PSRAM state allocates in `init()` and frees in `cleanup()`.
- Every render path must tolerate failed optional allocation by returning a bounded degraded visual, not by dereferencing null.

### 4. Audio Model

Use the five-layer model from the Phase 5 synthesis:

| Layer | Role | Preferred Sources | Typical Smoothing |
|---|---|---|---|
| L0 Bed | continuous visual floor | `rms`, `fast_rms`, heavy bands | 0.20-0.80 s |
| L1 Structure | texture and density | `flux`, `fast_flux`, saliency smooth fields | 0.08-0.30 s |
| L2 Impact | discrete events | kick/snare/hihat/onset fired flags | 0.12-0.30 s decay |
| L3 Tonal | colour anchor | chroma, root note, chord confidence | 0.30-0.80 s plus hysteresis |
| L4 Memory | story tail | integrated impact, novelty, phase memory | 0.40-1.60 s |

Rules:

- Do not drive pixels directly from raw audio fields.
- Do not use impulsive fields as continuous modulation.
- Do not require two events to coincide unless source timing proves they can.
- Prefer continuous level fields for continuous brightness and impulsive fields for spawn/accent decisions.

### 4.1 Audio Surface Hierarchy

Production effects should author from semantic surfaces before raw substrates:

| Rank | Surface | Use |
|---:|---|---|
| 1 | Named events/envelopes | Beat, downbeat, transient, kick, snare, hihat, held levels, age, confidence. |
| 2 | Named semantic scalars | RMS, flux, saliency, style, brightness, confidence, silentScale. |
| 3 | Musical vectors | `chroma[12]`, canonical 64-bin musical surface, named ranges. |
| 4 | Compatibility helpers | Legacy accessors retained for existing effects. |
| 5 | Raw substrates | `bins256`, FFT magnitudes, raw spectra: internal/debug/research/legacy only. |

Rules:

- New production effects must not directly scan `bins256`, `binHz`, or private linear FFT ranges.
- If a production effect needs a missing high-frequency or spectral semantic, add or request a named semantic field through the AFS v2 gate rather than re-creating raw extraction in render.
- Diagnostic visualisers may display raw substrate data, but they must be marked diagnostic/experimental and kept out of product promotion.
- Run `python3 firmware-v3/tools/check_effect_contracts.py` when changing effect audio access.

### 5. Colour Ownership

Pick exactly one primary colour model:

| Model | Use When | Required Discipline |
|---|---|---|
| Palette-driven | General production effects | Use `ctx.palette`; no free hue wheel. |
| Chroma-anchored | Musical effects | Use circular chroma smoothing, hysteresis, and bounded hue arcs. |
| Optical exact RGB | Interference/physics simulations | Declare colour-correction bypass need. |
| Diagnostic fixed RGB | Test patterns only | Mark diagnostic and keep out of production rotation. |

No effect may rely on a global correction layer to hide bad authored colour. The effect should author a believable colour field first; VP layers may protect or refine it, but must not be the reason the effect looks acceptable.

### 6. Persistence

Every animated effect must state its persistence model:

| Model | Fit | Failure If Misused |
|---|---|---|
| `fadeToBlackBy` plus additive draw | sprites, pulses, simple trails | hard overwrite kills trail depth. |
| PSRAM trail buffer | bloom, waveform, large state | DRAM pressure or stale buffer if lifecycle is wrong. |
| Substrate ring | time-axis radial scopes | graph/oscilloscope look if scalar is analytical. |
| Frame blend / LPF | global mood smoothing | accidental haze if applied in wrong order. |
| None | diagnostic static patterns | dead or flickery visuals in production. |

Persistence must be visually intentional: afterglow, memory, breath, wake, or decay. "It happens to smear" is not a design.

### 7. VP Stack Awareness

The effect author must answer:

- Does colour correction process the actual authored surface?
- Does the effect skip colour correction by metadata or hardcoded rule?
- Does it need tone mapping because of additive output?
- Does global silence scaling alter perceived behaviour?
- Does EdgeMixer mode change the diagnostic baseline?
- Does the RMT/FastLED layer have enough protected wire time?

If the answer is unknown, the effect is not ready for visual-default judgement.

### 8. Metadata

Every registered effect needs metadata that prevents downstream ambiguity:

| Field | Required Purpose |
|---|---|
| Family | Enables catalogue, gating, and UI grouping. |
| Audio reactive | Distinguishes authored behaviour from global silence scaling. |
| Experimental flag | Keeps research effects out of default production rotation. |
| Role flags | Declares direct-strip or dual-channel intent when applicable. |
| User-facing parameters | Names, bounds, types, and display names for UI surfaces. |
| Colour correction policy | Use global default, bypass, or explicit future policy. |

## Validation Ladder

| Gate | Required Evidence |
|---|---|
| Static contract | Centre-origin, no heap in render, no rainbow, dt-correct smoothing. |
| Effect contract checker | `python3 firmware-v3/tools/check_effect_contracts.py` for raw audio access, render allocation, centre-origin, and related guardrails. |
| Native harness | Unit or scoped harness for deterministic helpers. |
| Build | `esp32dev_audio_esv11_k1v2_32khz` unless task scope says otherwise. |
| Trace | Effect p99 under 2 ms and no show skips. |
| Serial status | Heap, stack, FPS, LED show time, `showSkips=0`. |
| VP baseline | EdgeMixer MIRROR and controlled correction state before A/B. |
| Captain visual | Timestamped judgement for production promotion. |

Docs-only standards work stops at static/source evidence. Firmware behaviour changes require hardware verification before commit.

## Promotion Classes

| Class | Meaning | Requirements |
|---|---|---|
| Hero | Product-defining light show | Full ladder plus Captain ship-gate sign-off. |
| Production | Default user-facing | Full ladder plus no unresolved visual-risk blocker. |
| Diagnostic | Useful for testing | May use fixed colours or artificial patterns; hidden from product rotation. |
| Experimental | Research or A/B | Tagged experimental and excluded from default production views. |
| Parked | Not worth current rescue | Keep build-safe, do not tune unless Captain reopens. |
| Retired | Removed or hidden permanently | Changelog and registry state explain why. |

## Rejection Patterns

An effect should be parked or redesigned if it:

- looks like a graph, oscilloscope, or test fixture;
- reads as several disconnected objects rather than one LGP field;
- produces white flashes, haze, or pastel wash not authored by intent;
- relies on full hue-wheel cycling;
- is a metronome blink rather than a musical response;
- needs constant manual explanation to perceive the intended phenomenon;
- only looks good after stacking broad global correction layers;
- cannot stay inside the 2 ms render budget.

## Definition Of Done

An effect authoring task is done only when the change includes:

1. implementation or documented non-implementation;
2. intake sheet;
3. source-backed constraint check;
4. VP stack interaction note;
5. validation evidence;
6. promotion class;
7. changelog fragment;
8. remaining debt or explicit "none".
