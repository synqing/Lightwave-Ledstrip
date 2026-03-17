# Visual Persistence Architecture — Research Findings

## Executive Summary

Visual persistence is the single most critical factor separating professional-quality LED effects from mediocre ones. Every Tier 1 effect in the LightwaveOS codebase implements persistence; none of the 5 new oscilloscope exploration effects do. This document captures the research findings, concrete numbers, and architectural patterns needed to fix this.

## Vector A: SNAPWAVE Analysis

**Finding:** SNAPWAVE already exists as `SnapwaveLinearEffect` (0x0E0A), registered and in the display order. It is NOT an oscilloscope — it's a chromagram-driven oscillating dot (musical pendulum) that uses 12 sine waves weighted by chromagram bins, compressed through `tanh()` for a "snapping" characteristic.

**SNAPWAVE's persistence architecture (all three layers):**
1. `fadeToBlackBy(ctx.leds, ctx.ledCount, fadeAmount)` — audio-coupled decay (loud=more fade)
2. History buffer — PSRAM ring buffer of dot positions + colours
3. Additive blending — `ctx.leds[pos] += fadedColor` from history entries

**Verdict:** SNAPWAVE doesn't solve the oscilloscope problem, but it IS a working reference for the persistence pattern. The persistence layers are exactly what the new effects need.

## Vector F: Baseline Captures

**Captured via led_capture.py, 10 seconds each, with music playing:**

### K1 Waveform (0x1302) — Waterfall
- Warm pink fill across entire strip (high frame-to-frame retention)
- Thin bright accent line where dot traverses
- Trail buffer persistence creates continuous colour coverage
- No black gaps between frames

### K1 Bloom (0x1301) — Waterfall
- Dark background with structured vertical colour columns
- Scroll persistence pushes colour outward from centre
- Temporal structure visible as vertical streaks over time
- Sparse but intentional — energy radiates then fades

### New Effects (0x130E-0x1312) — Waterfall
- Mostly black with scattered bright spots
- No fill, no retention, no temporal continuity
- Each frame computed independently — visible as horizontal discontinuities
- Dim overall (30-65 RGB where baselines show 80-200+)

**The persistence gap is stark and quantifiable.** Tier 1 effects maintain continuous colour coverage; new effects have frame-to-frame discontinuity.

## The Universal Persistence Pattern

Every professional system (TouchDesigner, WLED, Resolume, FastLED Fire2012, K1 Lightwave original) converges on the same core loop:

```
History Buffer -> Decay -> Blend New -> Output
```

### Concrete Parameters (from perceptual science research)

| Parameter | Value | Source |
|-----------|-------|--------|
| Retinal integration window | ~100ms (~12 frames at 120 FPS) | Peer-reviewed psychophysics |
| Audio-visual sync sweet spot | 80-120ms visual lag | Peer-reviewed |
| Beat transient decay tau | 150-250ms | Industry + perceptual |
| Bass persistence tau | 300-600ms | Industry practice |
| Ambient colour transition tau | 1000ms+ | Philips Hue practice |
| Spring damping for beat impacts | zeta = 0.65 | iOS/Android animation |
| Feedback blend ratio | 0.85-0.95 per frame | TouchDesigner/Resolume |
| Performance overhead | ~200us (10% of 2ms budget) | K1 implementation estimate |

### Three Layers of Persistence

**Layer 1: Temporal Pixel Memory** (frame-to-frame)
- Persistent PSRAM buffer survives across frames
- New values blend IN, old values decay OUT
- Implementation: CRGB_F persistBuffer[160] in PSRAM

**Layer 2: Audio-Coupled Dynamics** (reactive decay)
- Decay rate tracks audio energy
- K1 Waveform: `decayRate = 0.8 + 3.5 * amplitude`
- Bass should persist 300-600ms, treble 100-200ms

**Layer 3: Spatial Motion** (scroll/advect/diffuse)
- Pixel content moves through space over time
- Creates visual "follow-through" (animation principle)
- Types: scroll (K1 Bloom), advection (BeatPulse), diffusion (RD)

### Anti-Patterns (from codebase audit)

1. No persistence at all (compute from scratch each frame) — **this is what our 5 new effects do**
2. Fixed decay rate (no audio responsiveness)
3. Frame-rate-dependent decay (`*= 0.95` instead of `expf(-rate * dt)`)
4. No silence gating (frozen trails)
5. fadeToBlackBy + scroll simultaneously (dims trails during motion)

## Key Discovery: SB 4.x Sprite Persistence

SB 4.1.1 Bloom uses **sprite rendering with 0.99 alpha frame persistence** — keep 99% of previous frame, blend new content on top. Simpler than K1's scroll approach but equally effective. Implementation: `draw_sprite(dest, sprite, w, h, position, alpha=0.99)`.

## Key Discovery: K1 Lightwave Origins

- `processColorShift()` originates from K1 Lightwave (`led_utilities.h:1408-1483`), NOT from SB
- K1 Lightwave had 8 modes, all with persistence
- Waveform mode was a bouncing dot (confirming K1 Waveform IS faithful)
- SNAPWAVE was a unique K1 Lightwave creation

## Next Steps

1. Add Layer 1 persistence (PSRAM trail buffer + decay) to Effect B (Full Spectrum) as proof of concept
2. Validate with waterfall capture against K1 Waveform baseline
3. If successful, apply to remaining 4 exploration effects
4. Consider framework-level persistence in RendererActor (Vector C — depends on proof of concept)

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-18 | agent:synthesis | Created from 5 parallel research agent findings + 2 baseline captures |
