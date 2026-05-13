---
abstract: "Landing-page Liquid Light direction note pivoting first-contact LGP hero motion from Waveform Hybrid to controlled Holographic-family capture: 0x0201 first, constrained 0x1000 only after palette narrowing."
---

# LP Hero Liquid Light — Holographic Direction

**Date:** 2026-05-08
**RBDO label:** GROUNDED for source facts; DEGRADED-MODE for visual-quality claims until K1 hardware capture is reviewed.
**Status:** execution direction, not final approval.

## Decision

Captain rejected the handover's suggested `0x1313 K1 Waveform Hybrid` starting point for the landing-page first-contact hero. The active Liquid Light direction is now:

```text
Primary:   0x0201 LGP Holographic
Alternate: 0x1000 LGP Holo Auto-Cycle, only as a constrained palette variant
```

Waveform-family work remains parked at the committed baseline. Do not resume `0x1313` for this hero-motion task unless Captain explicitly reopens it.

## Source Findings

- `0x0201` is `EID_LGP_HOLOGRAPHIC`; it renders centre-origin, dual-strip, moire/depth interference and consumes the externally selected palette through `ctx.palette.getColor(...)`.
- `0x1000` is `EID_LGP_HOLOGRAPHIC_AUTO_CYCLE`; its render maths are intentionally identical to `0x0201`, but it owns an internal 20-palette playlist and shuffles it on `init()`.
- `0x1000` is therefore less suitable as the first capture source unless its playlist is narrowed or made deterministic; otherwise two takes can start on different colour stories.
- `0x0201` is the better immediate capture source because selected colours can be controlled from the normal palette path.
- Both Holographic implementations currently advance phase by frame increments (`speedNorm * 0.02/0.03/0.05`) rather than `ctx.getSafeDeltaSeconds()`. Do not extend them without making any new variant dt-correct.

Source anchors:

- `firmware-v3/src/config/effect_ids.h`: `EID_LGP_HOLOGRAPHIC = 0x0201`, `EID_LGP_HOLOGRAPHIC_AUTO_CYCLE = 0x1000`.
- `firmware-v3/src/effects/ieffect/LGPHolographicEffect.cpp`: four radial interference layers from `centerPairDistance()`, external palette use.
- `firmware-v3/src/effects/ieffect/LGPHolographicAutoCycleEffect.cpp`: same four-layer field plus `PALETTE_IDS[]`, `shufflePlaylist()`, and palette crossfade.
- `firmware-v3/docs/research/synergy-topology/MOVE_4_3_LIQUID_STILLNESS_CURATION_2026-05-05.md`: Liquid Stillness already treats curation as the gate and keeps final visual arbitration with Captain.

## Recommended Creative Solution

### 1. Capture `0x0201` As The Controlled Proof

Use `0x0201` for the first hardware audition because it gives deterministic colour control.

Candidate looks:

| Candidate | Effect | Palette direction | Motion intent | Initial settings |
|---|---|---|---|---|
| `LL-HOLO-01` | `0x0201` | Ocean Breeze / cool teal-cyan | premium liquid depth, no neon | brightness `200-220`, speed `12-18`, saturation high but not clipped |
| `LL-HOLO-02` | `0x0201` | Rivendell / soft blue-green | calm optical depth | brightness `180-210`, speed `10-15` |
| `LL-HOLO-03` | `0x0201` | restrained warm-to-cool pair | living colour without rainbow | brightness `200`, speed `14-18` |

Capture each as an 8-12 second clip with the same readiness packet used by the LP handover: `vp stack`, `s`, `dbg memory`, `show_skips=0`, no RMT/output faults, and effect timing.

### 2. Treat `0x1000` As A Variant Only After Palette Narrowing

Raw `0x1000` is useful for auditioning motion, but not for final capture until it stops randomising through the broad 20-palette playlist. The hero clip needs a deliberately selected colour story, not a shuffled one.

Minimal firmware variant if needed:

- Clone or parameterise `LGPHolographicAutoCycleEffect` into a `Liquid Light Holographic` capture variant.
- Replace the 20-palette shuffled playlist with a deterministic 3-5 palette sequence selected for cool optical depth and premium restraint.
- Make phase advancement dt-correct.
- Use the corrected `0x1000` brightness formula style rather than the base `0x0201` fixed midpoint floor.
- Slow crossfades so an 8-12 second hero clip feels like liquid colour changing state, not a palette demo.

### 3. Acceptance Gate

For the landing-page hero, only promote a clip that passes:

- Reads as liquid light, not waveform, graph, screen, or RGB strip.
- Centre-origin behaviour remains visible enough to be product-truthful.
- Colour is selected and restrained; no full hue-wheel/rainbow read.
- Gold caps and product identity remain visible in downstream video staging.
- Hardware capture has timing/readiness evidence, but visual judgement still decides `PROMOTE`, `REVISE`, or `KILL`.

## Next Action

Run a focused K1v2 audition of `0x0201` with 2-3 selected palettes and capture the best 8-12 second candidate for LandingPage pipeline processing. Use `0x1000` only if a constrained playlist variant is created or Captain wants broad auto-cycle as an exploratory visual reference.
