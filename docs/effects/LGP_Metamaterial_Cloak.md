# LGP Metamaterial Cloak – Technical Walkthrough

This document fully deconstructs the **LGP Metamaterial Cloak** visual pattern that is compiled into the current Lightwave firmware. All references point to concrete code locations so the behaviour can be audited or modified with confidence.

---

## Source of Truth

| Component | Path | Notes |
|-----------|------|-------|
| Effect entry | `src/main.cpp:211-236` | Registers `lgpMetamaterialCloaking` in the global `effects[]` table. |
| Implementation | `src/effects/strip/LGPQuantumEffects.cpp:440-478` | Contains the frame generator for the cloak. |
| Shared inputs | `src/core/EffectTypes.h:6-17` | Defines the `VisualParams` structure used throughout the pipeline. |

The effect executes inside the standard render loop (`effectUpdateCallback()` in `src/main.cpp:713-728`) and therefore participates in the same transition, palette and FastLED pipelines as the rest of the catalogue.

---

## Runtime Dependencies

The function `lgpMetamaterialCloaking()` pulls in the following globals:

- `strip1[]`, `strip2[]` – dual strip working buffers (`src/effects/strip/LGPQuantumEffects.cpp:446-447`).
- `leds[]` – the unified 320‑pixel buffer used by transition/output stages (`src/effects/strip/LGPQuantumEffects.cpp:495-496`).
- `currentPalette`, `gHue`, `paletteSpeed` – palette and animation drivers shared by every pattern.
- `visualParams` – encoder‑driven parameters: Intensity, Saturation, Complexity, Variation (`src/core/EffectTypes.h:6-17`).

Because the effect updates `leds[]` after rendering, it is transition-friendly and can be blended by `TransitionEngine` without extra work.

---

## Static State & Animation Clock

```cpp
static uint16_t time = 0;
static float    cloakPos = HardwareConfig::STRIP_CENTER_POINT;
static float    cloakVel = 0.5f;
time += paletteSpeed >> 2;
```

- `time` advances at one quarter of `paletteSpeed` so higher palette rates accelerate the animation (`src/effects/strip/LGPQuantumEffects.cpp:443-450`).
- `cloakPos` and `cloakVel` maintain a gliding position for the cloak bubble across the 160‑pixel strip.
- The cloak centre is initialised to the enforced project centre (LEDs 79/80), ensuring the first frame respects the CENTER ORIGIN rule.

---

## Parameter Mapping (Encoders → Behaviour)

| Encoder | `VisualParams` field | Usage inside effect |
|---------|---------------------|----------------------|
| 4 – Intensity | `visualParams.intensity` | Controls magnitude of the **negative refractive index** – higher values deepen the phase inversion (`src/effects/strip/LGPQuantumEffects.cpp:452-454`). |
| 5 – Saturation | `visualParams.saturation` | Drives the HSV saturation when writing pixels (line 491). |
| 6 – Complexity | `visualParams.complexity` | Sets cloak radius (10–25 pixels) (`src/effects/strip/LGPQuantumEffects.cpp:452-454`). |
| 7 – Variation | `visualParams.variation` | Not referenced directly; defaults to previous value, leaving future headroom. |
| Palette Speed | `paletteSpeed` | Governs the global wave clock (`time += paletteSpeed >> 2`). |

The cloak therefore scales physically with Complexity, while Intensity changes how aggressively it bends light backwards.

---

## Frame Generation Pipeline

Each invocation of `lgpMetamaterialCloaking()` performs the following sequence:

1. **Animate position** – `cloakPos += cloakVel` and reflect the velocity if the cloak would exceed the strip bounds (`src/effects/strip/LGPQuantumEffects.cpp:456-459`). The radius computed from Complexity is used to keep the bubble inside the renderable area.

2. **Baseline plane wave** – For every index `i` in `[0, STRIP_LENGTH)` produce a forward-moving sine wave and a hue ramp:
   ```cpp
   uint8_t wave = sin8(i * 4 + (time >> 2));
   uint8_t hue  = gHue + (i >> 2);
   ```
   (`src/effects/strip/LGPQuantumEffects.cpp:462-466`)

3. **Distance from cloak centre** – `distFromCloak = abs(i - cloakPos)` prepares a mask used for the cloaking maths (`src/effects/strip/LGPQuantumEffects.cpp:470-471`).

4. **Negative-index transform** – If the pixel lies inside the cloak radius, we simulate a metamaterial shell:
   - `bendAngle = (distFromCloak / cloakRadius) * PI` gradually increases as we move toward the perimeter (`src/effects/strip/LGPQuantumEffects.cpp:473-475`).
   - The wave’s spatial frequency is multiplied by the **negative** refractive index: `refractiveIndex = -1.0f - intensity_norm` (`src/effects/strip/LGPQuantumEffects.cpp:452-454`). Substituting this into `sin8()` flips the propagation direction (light “bends backwards”).
   - `bendAngle * 128` injects an additional phase twist near the cloak edge to give the appearance of light following the shell.

5. **Destructive interference core** – The inner half of the cloak gradually nulls the wave amplitude:
   ```cpp
   if (distFromCloak < cloakRadius * 0.5f) {
       wave = scale8(wave, 255 * (distFromCloak / (cloakRadius * 0.5f)));
   }
   ```
   (`src/effects/strip/LGPQuantumEffects.cpp:478-482`). This linear scaling collapses brightness to zero at the centre, creating the invisibility region.

6. **Edge highlight** – Pixels whose distance is within ~2 units of the radius are forced to maximum brightness and recoloured cyan, mimicking trapped surface waves (`src/effects/strip/LGPQuantumEffects.cpp:484-487`).

7. **Emit colour to both strips** – The effect uses complementary hues to maintain the Lightwave dual-strip aesthetic:
   ```cpp
   strip1[i] = CHSV(hue,      200, wave);
   strip2[i] = CHSV(hue + 128, 200, wave);
   ```
   (`src/effects/strip/LGPQuantumEffects.cpp:489-492`)

8. **Synchronise unified buffer** – The 160‑pixel strip arrays are copied into the 320‑pixel `leds[]` buffer (`src/effects/strip/LGPQuantumEffects.cpp:495-496`). This keeps the effect transitionable and ready for `FastLED.show()`.

The entire render path is **O(n)** with `n = 160` per strip, using lookup-table trigonometry (`sin8`, `scale8`) for speed.

---

## Visual Behaviour Summary

- The cloak drifts gently across the light guide plate, maintaining a dark centre caused by destructive interference.
- Background plane waves continue to propagate, but the negative index remaps their phase so they appear to wrap around the cloaked region.
- The cyan rim highlights the shell, conveying the “metamaterial” illusion as the wavefront reconnects behind the cloak.
- `Complexity` widens the cloak; `Intensity` strengthens the back-bending and central nulling; higher `paletteSpeed` increases motion.

---

## Performance & Safety Notes

- **Timing**: One full traversal of the strips requires ~160 iterations with a handful of fixed-point operations, keeping render time well under a millisecond on the ESP32-S3.
- **Memory**: No dynamic allocation. Only the existing strip buffers and a few static scalars are touched.
- **CENTER ORIGIN compliance**: Because both strips share the same brightness envelope and hue ramp, and the cloak centre is initialised at LED 79/80, the effect respects the project’s centre-origin rule.
- **Transitions**: Safe for all transition types because the unified buffer is updated every frame.

---

## Extension Ideas

- Drive `cloakPos` from `VisualParams.variation` for manual placement or oscillation depth.
- Layer a secondary cloak with a slightly different refractive index to create nested shells.
- Couple the cloak radius to audio envelopes (once the audio pipeline returns) for dynamic “breathing” cloaks.

This explainer should give you every moving part necessary to refactor, debug, or extend **LGP Metamaterial Cloak** without needing to reverse-engineer the source again.

