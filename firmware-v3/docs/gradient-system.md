---
abstract: "K1 gradient rendering system design — coordinate model, API reference, validation scenes, and forbidden patterns. Read before creating or modifying gradient-based effects."
---

# K1 Gradient Rendering System

Shared, ESP32-S3-safe, centre-origin, dual-edge gradient kernel for LightwaveOS effects.

## Architecture

The gradient kernel is an **effect-side utility** — a set of header-only modules that effects call from `render()`. It is NOT:
- A renderer primitive (gradients don't live inside RendererActor)
- An output-stage transform (not in EdgeMixer or ColorCorrectionEngine)
- A base class (effects don't inherit from it; they call it)

### Pipeline Position

```
effect.render(ctx)          ← gradient kernel called HERE
  → m_leds[320]
  → Tap A (raw effect output)
  → ColorCorrectionEngine   ← gradient effects SKIP this (distorts ramps)
  → Tap B
  → showLeds() → tone map → split → silence gate → EdgeMixer → Tap C → RMT
```

### File Layout

```
firmware-v3/src/effects/gradient/
├── GradientTypes.h     — GradientStop, RepeatMode, InterpolationMode, BlendMode
├── GradientRamp.h      — GradientRamp class: stops + sampling + blending
└── GradientCoord.h     — K1 coordinate helpers: uCenter, uSigned, uLocal, edgeId
```

## Coordinate Model

All coordinates are physically faithful to the K1 dual-edge Light Guide Plate.

| Coordinate | Range | Zero Point | Use Case |
|-----------|-------|------------|----------|
| `uCenter(i)` | 0.0-1.0 | Centre pair (79/80) | Radial gradients, concentric rings |
| `uSigned(i)` | -1.0 to +1.0 | Centre pair | Asymmetric shear, directional bias |
| `uLocal(i)` | 0.0-1.0 | LED 0 (per-strip) | Linear sweeps edge-to-edge |
| `edgeId(i)` | 0 or 1 | — | Dual-edge differentiation |
| `halfIndex(i)` | 0-79 | Centre pair | Discrete mirrored index |

### Physical Truth

```
Strip 1: LED 0 ←←←← LED 79 | LED 80 →→→→ LED 159
Strip 2: LED 160 ←← LED 239 | LED 240 →→ LED 319
                        ↑ CENTRE PAIR
```

The true geometric centre is at 79.5. All coordinate functions use this midpoint, producing symmetric results:
- `uCenter(79)` = `uCenter(80)` ≈ 0.00625
- `uCenter(0)` = `uCenter(159)` ≈ 0.99375

## GradientRamp API

### Configuration (init() or parameter change only)

```cpp
gradient::GradientRamp ramp;
ramp.clear();
ramp.addStop(0,   CRGB::Red);     // Centre
ramp.addStop(128, CRGB::Blue);    // Midpoint
ramp.addStop(255, CRGB::Green);   // Edge
ramp.setRepeatMode(gradient::RepeatMode::MIRROR);
ramp.setInterpolationMode(gradient::InterpolationMode::EASED);
```

### Sampling (render() safe)

```cpp
CRGB colour = ramp.samplef(gradient::uCenter(i));   // Float [0.0-1.0]
CRGB colour = ramp.sample(pos);                       // Fixed-point [0-255]
CRGB colour = ramp.sampleScaled(pos, ringCount, offset);  // Repeating
```

### Blending

```cpp
gradient::GradientRamp::blend(ctx.leds[i], gradColour,
                               gradient::BlendMode::SCREEN, 200);
```

### Memory

- GradientRamp: ~35 bytes on stack (8 stops × 4 bytes + 3 bytes overhead)
- Under the 64-byte PSRAM threshold — safe as a class member
- No heap allocation at any point

### Performance

- ~15 operations per sample (linear scan of ≤8 stops + lerp8)
- 160 LEDs × 15 ops ≈ 2,400 operations per frame — trivial (<0.1ms)
- No precomputed LUT needed in v1

## Repeat Modes

| Mode | Behaviour | Use Case |
|------|-----------|----------|
| CLAMP | Hold edge colours beyond range | Standard gradients |
| REPEAT | Tile (sawtooth wrap) | Concentric rings, bands |
| MIRROR | Ping-pong (triangle wrap) | Symmetric patterns |

## Interpolation Modes

| Mode | Behaviour | Use Case |
|------|-----------|----------|
| LINEAR | lerp8 between stops | Sharp, efficient |
| HARD_STOP | Step function at stop midpoint | Band boundaries, rings |
| EASED | Cubic hermite smooth step | Perceptual blending |

## Colour Correction

Gradient effects that construct precise colour ramps should skip the ColorCorrectionEngine, which applies 5+ non-linear transforms (saturation boost, white reduction, auto-exposure, gamma, V-clamping) that distort gradient fidelity.

To skip correction, add the effect ID to `shouldSkipColorCorrection()` in `PatternRegistry.cpp`.

Currently skipped: `EID_LGP_PERCEPTUAL_BLEND` (0x0709).

RadialRipple (0x0401) and ChromaticShear (0x0404) already skip via `isLGPSensitive()` (ADVANCED_OPTICAL family with CENTER_ORIGIN tag).

## Effects Using the Gradient Kernel

| Effect | ID | Kernel Usage | Status |
|--------|-----|-------------|--------|
| LGP Perceptual Blend | 0x0709 | 3-stop eased ramp, dual-edge complement | Retrofitted, correction skipped |
| LGP Gradient Field | 0x1F00 | Full kernel proof — 6 operator params | **NEW** — dedicated proof effect |

### Regression Decisions (2026-04-03)

**LGPRadialRippleEffect (0x0401):** REVERTED to original. Gradient kernel replaced sin16() brightness with a 5-stop linear ramp (triangular wave vs sinusoidal) and collapsed hue range from 0-63 to 0-3. Both are regressions. Effect restored to original implementation.

**LGPChromaticShearEffect (0x0404):** REVERTED to original. Gradient kernel replaced the hard centre-dimming threshold (50% at distance<20) with a smooth eased ramp (30% dimming). The interference zone was noticeably less pronounced. Effect restored to original implementation.

## LGP Gradient Field (0x1F00) — Operator Surface

The dedicated proof effect with 6 exposed parameters:

| Parameter | Type | Range | Default | Effect |
|-----------|------|-------|---------|--------|
| `basis` | ENUM | 0-2 (CENTER/LOCAL/SIGNED) | 0 (CENTER) | Coordinate basis for ramp sampling |
| `repeatMode` | ENUM | 0-2 (CLAMP/REPEAT/MIRROR) | 0 (CLAMP) | How ramp behaves beyond [0,1] |
| `interpolation` | ENUM | 0-2 (LINEAR/EASED/HARD_STOP) | 1 (EASED) | Interpolation between stops |
| `spread` | FLOAT | 0.1-2.0 | 1.0 | Ramp scale factor |
| `phaseOffset` | FLOAT | 0.0-1.0 | 0.0 | Ramp position offset |
| `edgeAsymmetry` | FLOAT | 0.0-1.0 | 0.0 | Edge A/B colour differentiation (0=symmetric) |

Parameters accessible via serial `p <name> <value>`, REST `/api/v1/effect/parameter`, and WebSocket `parameters.set`.

### Dirty-flag rebuild pattern (canonical)

Ramps are rebuilt ONLY when parameters change (dirty flag). `render()` is lookup-only.

```cpp
// In setParameter():
if (changed) m_dirty = true;

// In render():
if (m_dirty) { rebuildRamp(ctx); }
// sample ramp per LED — no allocation
```

## Validation Scenes

Flash the K1v2 firmware and select effect 0x1F00 (LGP Gradient Field).

### Scene 1: Centre-out two-colour ramp
- `basis=0` (CENTER), `repeatMode=0` (CLAMP), default stops
- **Expected**: Symmetric gradient radiating from centre pair outward on both strips
- **Watch for**: Banding, asymmetric centre pair
- **Capture**: Tap A = raw; Tap B should be identical (correction skipped)

### Scene 2: Repeating hard-stop rings
- `basis=0` (CENTER), `repeatMode=1` (REPEAT), `interpolation=2` (HARD_STOP), `spread=2.0`
- **Expected**: Discrete concentric colour bands, no bleeding between stops
- **Watch for**: Band edge sharpness, ring spacing uniformity

### Scene 3: Signed asymmetric field
- `basis=2` (SIGNED), `edgeAsymmetry=0.8`
- **Expected**: Left half of strip visually distinct from right half
- **Watch for**: Centre transition zone, colour shift direction

### Scene 4: Dual-edge complementary split
- `edgeAsymmetry=1.0`
- **Expected**: Edge A and Edge B show completely different colour ranges at same position
- **Watch for**: Both strips should have full colour range, not just brightness difference

### Scene 5: Low-brightness stability
- Set brightness to 10-20 via API, all default params
- **Expected**: No flickering, no banding from rounding, stable output
- **Watch for**: Quantisation artefacts at low brightness

## Forbidden Patterns

1. **No heap allocation in render()** — addStop/clear are OK in init() but wasteful in render(). Build ramps on parameter change, sample in render().
2. **No GradientRamp as base class** — it's a utility effects call, not an inheritance hierarchy.
3. **No fake 2D geometry** — K1 is 1D/dual-1D. Conic gradients are reinterpreted as cyclic ramps, not polar-angle sweeps.
4. **No arbitrary stop editor in v1** — max 8 stops, static allocation.
5. **Do NOT rebuild ramps per-LED** — build once per frame (or on parameter change), sample per LED.
6. **Dirty-flag ramp rebuild only** — never rebuild ramps unconditionally every frame. Set `m_dirty = true` in `setParameter()`, check and clear in `render()`.

## Known Limitations (v1)

- No general conic gradient semantics (cyclic stop logic only)
- No runtime arbitrary stop editor
- No perceptual (OKLab) interpolation — would need precomputed 160-entry ramp in PSRAM
- No composer/UI editing surface
- Maximum 8 stops per ramp
- No animated stop positions (stops are rebuilt per frame from palette, not interpolated)

## Extending the System

To create a new gradient-based effect:

1. Include `gradient/GradientRamp.h` and `gradient/GradientCoord.h`
2. Add a `GradientRamp` member to your effect class (~35 bytes)
3. Build stops in `init()` or at the start of `render()` when parameters change
4. Sample per LED using the appropriate coordinate: `uCenter()` for radial, `uLocal()` for linear, `uSigned()` for asymmetric
5. Use `writeCentrePairDual()` or `writeDualEdge()` for centre-origin dual-strip writes
6. If your gradient needs precise colours, add your effect ID to `shouldSkipColorCorrection()` in PatternRegistry.cpp

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-04-03 | agent:gradient-system | Created — v1 gradient kernel design and API reference |
| 2026-04-03 | agent:gradient-surfacing | Added LGPGradientField proof effect, regression decisions, dirty-flag pattern, updated validation scenes |
