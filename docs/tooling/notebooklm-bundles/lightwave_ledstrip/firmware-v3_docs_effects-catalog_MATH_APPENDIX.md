# LightwaveOS Effects -- Mathematical Functions Reference

**Version:** 1.0.0
**Generated:** 2026-02-21 08:54:38

---

## Overview

This document catalogues all mathematical functions, constants, and lookup tables used by the
LightwaveOS effects engine. Functions are organised by domain and cross-referenced to the
[Effects Inventory](EFFECTS_INVENTORY.md) and [Pattern Taxonomy](PATTERN_TAXONOMY.md).

| Category | Functions | Description |
|----------|-----------|-------------|
| Chroma Processing | 4 | Circular weighted mean chroma-to-hue conversion and smoothing. Replaces broken argmax pattern. |
| Smoothing Primitives | 10 | Frame-rate-independent smoothing primitives: exponential decay, springs, asymmetric followers, EMA alpha computation. |
| Rendering Helpers | 12 | Colour manipulation, tone mapping, blend modes, brightness scaling, subpixel rendering, film post-processing. |
| Audio Feature Access | 2 | Delta time policies separating audio-coupled (raw) from visual-only (SPEED-scaled) timing. |
| Noise | 5 | Deterministic RNG, approximate Gaussian, Kuramoto feature extraction, FastLED sin16/cos16 wrappers. |
| Interpolation | 1 | Linear interpolation utilities. |
| Geometry | 11 | Centre-origin geometry, ring profiles, distance calculations, phase/sine wave generation. |
| Beat Processing | 8 | Beat envelope, ring position, intensity computation, beat tick detection with confidence gating and fallback metronome. |

**Total functions documented:** 58

**Total constants documented:** 18

## Chroma Processing Functions

Circular weighted mean chroma-to-hue conversion and smoothing. Replaces broken argmax pattern.

### circularChromaHue

**File:** `effects/ieffect/ChromaUtils.h` (line 47)

**Namespace:** `lightwaveos::effects::chroma`

**Signature:**

```cpp
static inline uint8_t circularChromaHue(const float chroma[12])
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `chroma` | `const float[12]` | Array of 12 chroma magnitudes (one per semitone, C through B) |

**Returns:** `uint8_t`

**Formula:**

```
hue = atan2(sum(chroma[i]*kSin[i]), sum(chroma[i]*kCos[i])) * (255 / 2pi). Circular weighted mean of 12 chroma
bins mapped to 0-255 palette hue.
```

**Domain:** Chroma-to-hue conversion (instantaneous, no smoothing)

**Computational Cost:**

- 1 trig call(s); 24 multiply-accumulate(s); ~120 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Instantaneous variant. Prefer circularChromaHueSmoothed() for frame-to-frame stability. Replaces broken argmax pattern that caused jarring hue jumps.

---

### circularChromaHueSmoothed

**File:** `effects/ieffect/ChromaUtils.h` (line 94)

**Namespace:** `lightwaveos::effects::chroma`

**Signature:**

```cpp
static inline uint8_t circularChromaHueSmoothed(const float chroma[12], float& prevAngle, float dt, float tau = 0.20f)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `chroma` | `const float[12]` | Array of 12 chroma magnitudes |
| `prevAngle` | `float&` | Previous smoothed angle in radians (caller must persist, initialise to 0.0f) |
| `dt` | `float` | Delta time in seconds (use rawDt for frame-rate independence) |
| `tau` | `float` | Time constant in seconds. 0.12=responsive, 0.25=smooth, 0.40=very stable. Default 0.20 |

**Returns:** `uint8_t`

**Formula:**

```
angle = atan2(sum(chroma[i]*kSin[i]), sum(chroma[i]*kCos[i])); alpha = 1 - exp(-dt/time constant (tau));
prevAngle = circularEma(angle, prevAngle, alpha); return prevAngle * (255/2pi)
```

**Domain:** Chroma-to-hue conversion with temporal smoothing

**Computational Cost:**

- 1 trig call(s); 1 exp call(s); 26 multiply-accumulate(s); ~200 estimated cycles

**Frame-rate independent:** Yes

**Used by:**

- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)

**Notes:** PRIMARY chroma function for effects. 22 effects use this. Replaces broken dominantChromaBin12() argmax. Frame-rate independent via dt-corrected EMA alpha.

---

## Smoothing Primitives

Frame-rate-independent smoothing primitives: exponential decay, springs, asymmetric followers, EMA alpha computation.

### circularEma

**File:** `effects/ieffect/ChromaUtils.h` (line 68)

**Namespace:** `lightwaveos::effects::chroma`

**Signature:**

```cpp
static inline float circularEma(float newAngle, float prevAngle, float alpha)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `newAngle` | `float` | New angle in radians [0, 2pi) |
| `prevAngle` | `float` | Previous smoothed angle (state, modified in place) |
| `alpha` | `float` | EMA alpha (0 = no change, 1 = instant snap) |

**Returns:** `float`

**Formula:**

```
diff = newAngle - prevAngle; wrap diff to [-pi, pi]; result = prevAngle + diff * alpha; wrap result to [0,
2pi)
```

**Domain:** Circular angle smoothing in radians

**Computational Cost:**

- 2 multiply-accumulate(s); ~15 estimated cycles

**Frame-rate independent:** No

**Used by:**

- [?](EFFECTS_INVENTORY.md) (ID ?)

**Notes:** Shortest-arc interpolation. Always takes the minimal path around the circle. Used internally by circularChromaHueSmoothed. Alpha must be pre-computed with dt-correction for frame-rate independence.

---

### dtDecay

**File:** `effects/ieffect/ChromaUtils.h` (line 124)

**Namespace:** `lightwaveos::effects::chroma`

**Signature:**

```cpp
static inline float dtDecay(float value, float rate60fps, float dt)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `value` | `float` | Current value |
| `rate60fps` | `float` | Decay rate per frame at 60fps (e.g. 0.88) |
| `dt` | `float` | Delta time in seconds |

**Returns:** `float`

**Formula:**

```
value * pow(rate60fps, dt * 60.0)
```

**Domain:** Frame-rate-independent exponential decay

**Computational Cost:**

- 2 multiply-accumulate(s); ~80 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Replaces bare 'value *= rate' pattern. The rate parameter is specified at 60fps reference and auto-scales to actual dt via powf.

---

### computeEmaAlpha

**File:** `audio/AudioMath.h` (line 29)

**Namespace:** `lightwaveos::audio`

**Signature:**

```cpp
inline float computeEmaAlpha(float tau_seconds, float hop_rate_hz)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `tau_seconds` | `float` | Time constant in seconds (63% settling time) |
| `hop_rate_hz` | `float` | Frame/hop rate in Hz |

**Returns:** `float`

**Formula:**

```
1 - exp(-1 / (tau_seconds * hop_rate_hz))
```

**Domain:** EMA alpha from time constant and frame rate

**Computational Cost:**

- 1 exp call(s); 2 multiply-accumulate(s); ~80 estimated cycles

**Frame-rate independent:** Yes

**Used by:**

- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)

**Notes:** Converts between time constants (seconds) and per-frame EMA alphas. Enables single set of perceptual tuning values to work across any hop rate.

---

### tauFromAlpha

**File:** `audio/AudioMath.h` (line 45)

**Namespace:** `lightwaveos::audio`

**Signature:**

```cpp
inline float tauFromAlpha(float alpha, float hop_rate_hz)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `alpha` | `float` | Per-frame EMA alpha in (0, 1) |
| `hop_rate_hz` | `float` | Frame/hop rate in Hz |

**Returns:** `float`

**Formula:**

```
-1 / (hop_rate_hz * ln(1 - alpha))
```

**Domain:** Reverse-engineer time constant from alpha and frame rate

**Computational Cost:**

- 2 multiply-accumulate(s); ~60 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Diagnostic utility. Uses logf internally. Useful for documenting what time constant a hardcoded alpha corresponds to.

---

### alphaFromHalfLife

**File:** `audio/AudioMath.h` (line 58)

**Namespace:** `lightwaveos::audio`

**Signature:**

```cpp
inline float alphaFromHalfLife(float half_life_frames)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `half_life_frames` | `float` | Number of frames for signal to decay to 50% |

**Returns:** `float`

**Formula:**

```
1 - exp(-ln(2) / half_life_frames) = 1 - exp(-0.693147 / half_life_frames)
```

**Domain:** EMA alpha from half-life in frames

**Computational Cost:**

- 1 exp call(s); 1 multiply-accumulate(s); ~80 estimated cycles

**Frame-rate independent:** No

**Notes:** Frame-count-based variant. Not frame-rate independent by itself; use computeEmaAlpha for frame-rate independence.

---

### retunedAlpha

**File:** `audio/AudioMath.h` (line 73)

**Namespace:** `lightwaveos::audio`

**Signature:**

```cpp
inline float retunedAlpha(float alpha_at_ref, float ref_hz, float target_hz)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `alpha_at_ref` | `float` | Original alpha value |
| `ref_hz` | `float` | Frame rate the original alpha was tuned for |
| `target_hz` | `float` | Frame rate to retune to |

**Returns:** `float`

**Formula:**

```
time constant (tau) = tauFromAlpha(alpha_at_ref, ref_hz); return computeEmaAlpha(time constant (tau),
target_hz)
```

**Domain:** Re-scale alpha across frame rates preserving perceptual time constant

**Computational Cost:**

- 1 exp call(s); 4 multiply-accumulate(s); ~140 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Preserves perceptual time constant when porting alphas between different hop rates.

---

### ExpDecay::update

**File:** `effects/enhancement/SmoothingEngine.h` (line 55)

**Namespace:** `lightwaveos::effects::enhancement`

**Signature:**

```cpp
float update(float target, float dt)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `target` | `float` | Target value to approach |
| `dt` | `float` | Delta time in seconds |

**Returns:** `float`

**Formula:**

```
alpha = 1 - exp(-lambda * dt); value += (target - value) * alpha
```

**Domain:** True frame-rate-independent exponential smoothing

**Computational Cost:**

- 1 exp call(s); 3 multiply-accumulate(s); ~90 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Foundation smoothing primitive. lambda = 1/time constant (tau). Higher lambda = faster response. Produces identical results at any frame rate.

---

### Spring::update

**File:** `effects/enhancement/SmoothingEngine.h` (line 115)

**Namespace:** `lightwaveos::effects::enhancement`

**Signature:**

```cpp
float update(float target, float dt)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `target` | `float` | Target position |
| `dt` | `float` | Delta time in seconds |

**Returns:** `float`

**Formula:**

```
acceleration = (-stiffness * (position - target) - damping * velocity) / mass; velocity += acceleration * dt;
position += velocity * dt
```

**Domain:** Critically damped spring physics

**Computational Cost:**

- 6 multiply-accumulate(s); ~20 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Critical damping: damping = 2*sqrt(stiffness*mass). Fastest approach without overshoot. Semi-implicit Euler integration.

---

### AsymmetricFollower::update

**File:** `effects/enhancement/SmoothingEngine.h` (line 162)

**Namespace:** `lightwaveos::effects::enhancement`

**Signature:**

```cpp
float update(float target, float dt)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `target` | `float` | Target value |
| `dt` | `float` | Delta time in seconds |

**Returns:** `float`

**Formula:**

```
time constant (tau) = (target > value) ? riseTau : fallTau; alpha = 1 - exp(-dt/time constant (tau)); value +=
(target - value) * alpha
```

**Domain:** Asymmetric attack/release smoothing (Sensory Bridge pattern)

**Computational Cost:**

- 1 exp call(s); 3 multiply-accumulate(s); ~90 estimated cycles

**Frame-rate independent:** Yes

**Used by:**

- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)

**Notes:** Sensory Bridge MOOD knob pattern. Different time constants for rising vs falling. Essential for audio viz where attacks should be fast but decays smooth. 12 effects use this.

---

### AsymmetricFollower::updateWithMood

**File:** `effects/enhancement/SmoothingEngine.h` (line 180)

**Namespace:** `lightwaveos::effects::enhancement`

**Signature:**

```cpp
float updateWithMood(float target, float dt, float moodNorm)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `target` | `float` | Target value |
| `dt` | `float` | Delta time in seconds |
| `moodNorm` | `float` | Normalised mood (0.0=reactive, 1.0=smooth) |

**Returns:** `float`

**Formula:**

```
adjRiseTau = riseTau * (1+moodNorm); adjFallTau = fallTau * (0.5 + 0.5*moodNorm); time constant (tau) =
(target>value) ? adjRiseTau : adjFallTau; alpha = 1-exp(-dt/time constant (tau))
```

**Domain:** MOOD-adjusted asymmetric smoothing

**Computational Cost:**

- 1 exp call(s); 6 multiply-accumulate(s); ~100 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Mood=0 (reactive): riseTau*1.0, fallTau*0.5. Mood=1 (smooth): riseTau*2.0, fallTau*1.0.

---

### getSafeDeltaSeconds

**File:** `effects/enhancement/SmoothingEngine.h` (line 304)

**Namespace:** `lightwaveos::effects::enhancement`

**Signature:**

```cpp
inline float getSafeDeltaSeconds(float deltaSeconds)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `deltaSeconds` | `float` | Delta time in seconds |

**Returns:** `float`

**Formula:**

```
clamp(deltaSeconds, 0.0001, 0.05)
```

**Domain:** Safe delta time clamping for physics stability

**Computational Cost:**

- ~5 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Prevents physics explosion on frame drops (>50ms) and ensures minimum timestep (0.1ms).

---

### tauToLambda

**File:** `effects/enhancement/SmoothingEngine.h` (line 316)

**Namespace:** `lightwaveos::effects::enhancement`

**Signature:**

```cpp
inline float tauToLambda(float tauSeconds)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `tauSeconds` | `float` | Time constant in seconds |

**Returns:** `float`

**Formula:**

```
1 / tauSeconds
```

**Domain:** Convert time constant to convergence rate

**Computational Cost:**

- ~5 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Simple reciprocal conversion for ExpDecay lambda parameter.

---

## Rendering Helpers

Colour manipulation, tone mapping, blend modes, brightness scaling, subpixel rendering, film post-processing.

### clamp01

**File:** `effects/ieffect/BeatPulseRenderUtils.h` (line 31)

**Namespace:** `lightwaveos::effects::ieffect`

**Signature:**

```cpp
static inline float clamp01(float v)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `v` | `float` | Input value |

**Returns:** `float`

**Formula:**

```
max(0, min(1, v))
```

**Domain:** Clamp to unit interval

**Computational Cost:**

- ~5 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Used pervasively across BeatPulse family, transport cores, and film post. Multiple local redefinitions exist in KuramotoTransportBuffer and LGPFilmPost.

---

### floatToByte

**File:** `effects/ieffect/BeatPulseRenderUtils.h` (line 51)

**Namespace:** `lightwaveos::effects::ieffect`

**Signature:**

```cpp
static inline uint8_t floatToByte(float v)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `v` | `float` | Input value [0, 1] |

**Returns:** `uint8_t`

**Formula:**

```
clamp(v * 255 + 0.5, 0, 255)
```

**Domain:** Unit float to byte conversion with rounding

**Computational Cost:**

- 1 multiply-accumulate(s); ~5 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Used in all BeatPulse family effects for palette indexing and brightness conversion.

---

### scaleBrightness

**File:** `effects/ieffect/BeatPulseRenderUtils.h` (line 63)

**Namespace:** `lightwaveos::effects::ieffect`

**Signature:**

```cpp
static inline uint8_t scaleBrightness(uint8_t baseBrightness, float factor)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `baseBrightness` | `uint8_t` | Original brightness [0, 255] |
| `factor` | `float` | Scale factor [0, 1] |

**Returns:** `uint8_t`

**Formula:**

```
clamp(baseBrightness * clamp01(factor) + 0.5, 0, 255)
```

**Domain:** Brightness scaling

**Computational Cost:**

- 1 multiply-accumulate(s); ~8 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Used in BeatPulse family for final brightness output.

---

### ColourUtil::addWhiteSaturating

**File:** `effects/ieffect/BeatPulseRenderUtils.h` (line 174)

**Namespace:** `lightwaveos::effects::ieffect::ColourUtil`

**Signature:**

```cpp
static inline void addWhiteSaturating(CRGB& c, uint8_t w)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `c` | `CRGB&` | Colour to modify (in-place) |
| `w` | `uint8_t` | Brightness boost [0, 255] |

**Returns:** `void`

**Formula:**

```
lum = (r+g+b)/3; lumNew = min(255, lum+w); scale each channel by lumNew/lum. Hue-preserving luminance boost.
```

**Domain:** Hue-preserving brightness boost

**Computational Cost:**

- 6 multiply-accumulate(s); ~25 estimated cycles

**Frame-rate independent:** Yes

**Used by:**

- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)

**Notes:** Hue-preserving. Scales luminance so r:g:b ratio is maintained. Replaces original addWhiteSaturating which drove toward (255,255,255).

---

### ColourUtil::additive

**File:** `effects/ieffect/BeatPulseRenderUtils.h` (line 199)

**Namespace:** `lightwaveos::effects::ieffect::ColourUtil`

**Signature:**

```cpp
static inline CRGB additive(const CRGB& base, const CRGB& overlay)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `base` | `const CRGB&` | Base colour |
| `overlay` | `const CRGB&` | Overlay colour |

**Returns:** `CRGB`

**Formula:**

```
((base.ch + overlay.ch + 1) >> 1) per channel -- average blend
```

**Domain:** Colour averaging (hue-preserving blend)

**Computational Cost:**

- 3 multiply-accumulate(s); ~12 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Averages R, G, B so both layers are visible without washing out palette. Replaces former additive which summed channels driving output to white.

---

### BlendMode::softAccumulate

**File:** `effects/ieffect/BeatPulseRenderUtils.h` (line 226)

**Namespace:** `lightwaveos::effects::ieffect::BlendMode`

**Signature:**

```cpp
static inline float softAccumulate(float accumulated, float knee = 1.5f)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `accumulated` | `float` | Sum of all layer contributions |
| `knee` | `float` | Softness of curve (higher = softer saturation). Default 1.5 |

**Returns:** `float`

**Formula:**

```
x / (x + knee)
```

**Domain:** Soft accumulation for multi-layer blend

**Computational Cost:**

- 1 multiply-accumulate(s); ~5 estimated cycles

**Frame-rate independent:** Yes

**Used by:**

- [?](EFFECTS_INVENTORY.md) (ID ?)

**Notes:** Maps [0, inf) to [0, 1) with configurable knee. Multiple overlapping rings accumulate without harsh clipping.

---

### BlendMode::screen

**File:** `effects/ieffect/BeatPulseRenderUtils.h` (line 240)

**Namespace:** `lightwaveos::effects::ieffect::BlendMode`

**Signature:**

```cpp
static inline float screen(float a, float b)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `a` | `float` | First value [0, 1] |
| `b` | `float` | Second value [0, 1] |

**Returns:** `float`

**Formula:**

```
1 - (1 - a) * (1 - b)
```

**Domain:** Screen blend mode

**Computational Cost:**

- 3 multiply-accumulate(s); ~8 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Graceful additive that avoids clipping. Multiple layers blend without harsh saturation.

---

### acesFilm

**File:** `effects/ieffect/LGPFilmPost.cpp` (line 40)

**Namespace:** `lightwaveos::effects::cinema`

**Signature:**

```cpp
static inline float acesFilm(float x)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `x` | `float` | Linear input value |

**Returns:** `float`

**Formula:**

```
x *= 0.85; y = (x*(2.51x+0.03)) / (x*(2.43x+0.59)+0.14); clamp01(y). Narkowicz ACES fitted curve.
```

**Domain:** Filmic tone mapping (ACES fitted curve)

**Computational Cost:**

- 8 multiply-accumulate(s); ~20 estimated cycles

**Frame-rate independent:** Yes

**Used by:**

- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)

**Notes:** Narkowicz ACES fitted curve for highlight rolloff. Applied per-channel after spatial softening in film post chain.

---

### cinema::apply

**File:** `effects/ieffect/LGPFilmPost.cpp` (line 57)

**Namespace:** `lightwaveos::effects::cinema`

**Signature:**

```cpp
void apply(plugins::EffectContext& ctx, float speedNorm)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `ctx` | `plugins::EffectContext&` | Effect context with LED buffer |
| `speedNorm` | `float` | Normalised speed (ctx.speed / 50.0f) |

**Returns:** `void`

**Formula:**

```
Pipeline: 3-tap spatial soften -> ACES tone map -> gamma LUT -> +/-1 temporal dither -> temporal EMA. Alpha8 =
40 + 120*speedNorm.
```

**Domain:** Cinema-grade post-processing chain

**Computational Cost:**

- 40 multiply-accumulate(s); ~600 estimated cycles

**Frame-rate independent:** No

**Used by:**

- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)

**Notes:** Full post chain. Operates on strip A only, copies to strip B. Uses static buffers (no heap alloc). Per-LED cost: ~4 MACs (soften) + 3 ACES + 3 gamma LUT + 3 EMA = ~13 ops/LED.

---

### SubpixelRenderer::renderPoint

**File:** `effects/enhancement/SmoothingEngine.h` (line 220)

**Namespace:** `lightwaveos::effects::enhancement`

**Signature:**

```cpp
static void renderPoint(CRGB* buffer, uint16_t bufferSize, float position, CRGB color, uint8_t brightness)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `buffer` | `CRGB*` | LED buffer |
| `bufferSize` | `uint16_t` | Size of LED buffer |
| `position` | `float` | Fractional position (e.g., 45.7) |
| `color` | `CRGB` | Colour to render |
| `brightness` | `uint8_t` | Overall brightness (0-255) |

**Returns:** `void`

**Formula:**

```
frac = position - floor(position); left LED gets (1-frac)*brightness, right LED gets frac*brightness. Uses
qadd8 for saturating addition.
```

**Domain:** Anti-aliased fractional LED positioning

**Computational Cost:**

- 8 multiply-accumulate(s); ~30 estimated cycles

**Frame-rate independent:** Yes

**Used by:**

- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)

**Notes:** Essential for smooth motion at low speeds. Distributes brightness between adjacent LEDs based on fractional position.

---

### SubpixelRenderer::renderLine

**File:** `effects/enhancement/SmoothingEngine.h` (line 252)

**Namespace:** `lightwaveos::effects::enhancement`

**Signature:**

```cpp
static void renderLine(CRGB* buffer, uint16_t bufferSize, float start, float end, CRGB color, uint8_t brightness)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `buffer` | `CRGB*` | LED buffer |
| `bufferSize` | `uint16_t` | Size of LED buffer |
| `start` | `float` | Start position (fractional) |
| `end` | `float` | End position (fractional) |
| `color` | `CRGB` | Colour to render |
| `brightness` | `uint8_t` | Overall brightness (0-255) |

**Returns:** `void`

**Formula:**

```
Partial first LED + full LEDs in between + partial last LED. Anti-aliased endpoints. Uses qadd8 per channel.
```

**Domain:** Anti-aliased line rendering between fractional positions

**Computational Cost:**

- 12 multiply-accumulate(s); ~50 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Renders a filled line segment with anti-aliased endpoints.

---

### KuramotoTransportBuffer::toneMapToCRGB8

**File:** `effects/ieffect/KuramotoTransportBuffer.h` (line 328)

**Namespace:** `lightwaveos::effects::ieffect`

**Signature:**

```cpp
static CRGB toneMapToCRGB8(const RGB16& in, float exposure, float saturationBoost)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `in` | `const RGB16&` | 16-bit HDR input colour |
| `exposure` | `float` | Exposure multiplier |
| `saturationBoost` | `float` | Saturation boost [0..1] |

**Returns:** `CRGB`

**Formula:**

```
r = in.r/65535 * exposure; lum = (r+g+b)/3; lumT = lum/(1+lum); scale = lumT/lum. Then Rec.709 luma-based
saturation boost: luma = 0.2126r + 0.7152g + 0.0722b; ch = luma + (ch - luma)*(1+sat).
```

**Domain:** HDR-to-LDR luminance-preserving tone map with saturation boost

**Computational Cost:**

- 15 multiply-accumulate(s); ~50 estimated cycles

**Frame-rate independent:** Yes

**Used by:**

- [?](EFFECTS_INVENTORY.md) (ID ?)

**Notes:** Luminance-based tone map preserves hue (fixes colour corruption from per-channel tone mapping). Soft-knee: lumT = lum/(1+lum). Rec.709 luma weights for saturation boost.

---

### BeatPulseTransportCore::kneeToneMap

**File:** `effects/ieffect/BeatPulseTransportCore.h` (line 378)

**Namespace:** `lightwaveos::effects::ieffect`

**Signature:**

```cpp
static inline float kneeToneMap(float in01, float knee = 0.5f)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `in01` | `float` | Input value [0..1] |
| `knee` | `float` | Knee softness. Default 0.5 |

**Returns:** `float`

**Formula:**

```
mapped = in01 / (in01 + knee); return mapped * (1 + knee). At knee=0.5, 1.0 maps to ~1.0.
```

**Domain:** Knee tone map for HDR readout (boosts lows, compresses highs)

**Computational Cost:**

- 3 multiply-accumulate(s); ~10 estimated cycles

**Frame-rate independent:** Yes

**Used by:**

- [?](EFFECTS_INVENTORY.md) (ID ?)

**Notes:** Keeps low-level energy visible and prevents harsh clipping. Rescaled so peak brightness stays punchy.

---

### fastled_wrap_hue_safe

**File:** `effects/utils/FastLEDOptim.h` (line 182)

**Namespace:** `lightwaveos::effects::utils`

**Signature:**

```cpp
inline uint8_t fastled_wrap_hue_safe(uint8_t hue, int16_t offset, uint8_t maxRange = 60)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `hue` | `uint8_t` | Base hue (0-255) |
| `offset` | `int16_t` | Hue offset to add |
| `maxRange` | `uint8_t` | Maximum hue range from base. Default 60 (no-rainbows rule) |

**Returns:** `uint8_t`

**Formula:**

```
result = (hue + offset) mod 256; clamp to maxRange from base hue if needed
```

**Domain:** No-rainbows-rule compliant hue wrapping

**Computational Cost:**

- 2 multiply-accumulate(s); ~10 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Enforces the no-rainbows rule (<60 degree hue range) by clamping hue offset within maxRange.

---

## Audio Feature Access

Delta time policies separating audio-coupled (raw) from visual-only (SPEED-scaled) timing.

### AudioReactivePolicy::signalDt

**File:** `effects/ieffect/AudioReactivePolicy.h` (line 19)

**Namespace:** `lightwaveos::effects::ieffect::AudioReactivePolicy`

**Signature:**

```cpp
static inline float signalDt(const plugins::EffectContext& ctx)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `ctx` | `const plugins::EffectContext&` | Effect context |

**Returns:** `float`

**Formula:**

```
ctx.getSafeRawDeltaSeconds() -- raw (unscaled by SPEED) delta time clamped to [0.0001, 0.05]
```

**Domain:** Audio-coupled delta time (unscaled by SPEED)

**Computational Cost:**

- ~5 estimated cycles

**Frame-rate independent:** Yes

**Notes:** SPEED slider must NOT distort DSP-coupled maths. All audio-reactive effects must use this for signal processing dt.

---

### AudioReactivePolicy::visualDt

**File:** `effects/ieffect/AudioReactivePolicy.h` (line 26)

**Namespace:** `lightwaveos::effects::ieffect::AudioReactivePolicy`

**Signature:**

```cpp
static inline float visualDt(const plugins::EffectContext& ctx)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `ctx` | `const plugins::EffectContext&` | Effect context |

**Returns:** `float`

**Formula:**

```
ctx.getSafeDeltaSeconds() -- SPEED-scaled delta time clamped to [0.0001, 0.05]
```

**Domain:** Visual-only motion delta time (SPEED-scaled)

**Computational Cost:**

- ~5 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Use for visual-only motion (pattern speed, palette cycling). NOT for audio-coupled maths.

---

## Beat Processing

Beat envelope, ring position, intensity computation, beat tick detection with confidence gating and fallback metronome.

### BeatPulseHTML::decayMul

**File:** `effects/ieffect/BeatPulseRenderUtils.h` (line 272)

**Namespace:** `lightwaveos::effects::ieffect::BeatPulseHTML`

**Signature:**

```cpp
static inline float decayMul(float dtSeconds)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `dtSeconds` | `float` | Delta time in seconds |

**Returns:** `float`

**Formula:**

```
pow(0.94, dtSeconds * 60)
```

**Domain:** dt-correct beat decay multiplier matching HTML demo at 60fps

**Computational Cost:**

- 1 multiply-accumulate(s); ~80 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Equivalent to repeatedly multiplying by 0.94 once per 1/60s frame. Uses powf internally.

---

### BeatPulseHTML::updateBeatIntensity

**File:** `effects/ieffect/BeatPulseRenderUtils.h` (line 283)

**Namespace:** `lightwaveos::effects::ieffect::BeatPulseHTML`

**Signature:**

```cpp
static inline void updateBeatIntensity(float& beatIntensity, bool beatTick, float dtSeconds)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `beatIntensity` | `float&` | Current intensity state [0..1], modified in place |
| `beatTick` | `bool` | True on beat (slams to 1.0) |
| `dtSeconds` | `float` | Delta time in seconds |

**Returns:** `void`

**Formula:**

```
if(beatTick) beatIntensity=1; beatIntensity *= pow(0.94, dt*60); if < 0.0005 then 0
```

**Domain:** Beat envelope update with HTML-parity behaviour

**Computational Cost:**

- 2 multiply-accumulate(s); ~90 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Used by BeatPulseCore::stepEnvelope. Locks exact maths from HTML Beat Pulse demo.

---

### BeatPulseHTML::ringCentre01

**File:** `effects/ieffect/BeatPulseRenderUtils.h` (line 297)

**Namespace:** `lightwaveos::effects::ieffect::BeatPulseHTML`

**Signature:**

```cpp
static inline float ringCentre01(float beatIntensity)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `beatIntensity` | `float` | Global beat intensity state [0..1] |

**Returns:** `float`

**Formula:**

```
beatIntensity * 0.6 (from HTML: wavePos=beatIntensity*1.2, centre=wavePos*0.5)
```

**Domain:** Ring centre position in normalised distance units

**Computational Cost:**

- 1 multiply-accumulate(s); ~3 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Maps beatIntensity to radial ring position. At 1.0 (beat hit), ring is at 0.6 (60% from centre).

---

### BeatPulseHTML::intensityAtDist

**File:** `effects/ieffect/BeatPulseRenderUtils.h` (line 310)

**Namespace:** `lightwaveos::effects::ieffect::BeatPulseHTML`

**Signature:**

```cpp
static inline float intensityAtDist(float dist01, float beatIntensity)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `dist01` | `float` | Centre-origin distance [0..1] |
| `beatIntensity` | `float` | Global beat intensity state [0..1] |

**Returns:** `float`

**Formula:**

```
waveHit = 1 - min(1, |dist01 - ringCentre01(beatIntensity)| * 3); intensity = max(0, waveHit) * beatIntensity
```

**Domain:** Per-LED pulse intensity from HTML formula

**Computational Cost:**

- 4 multiply-accumulate(s); ~10 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Exact port of HTML Beat Pulse demo maths.

---

### BeatPulseHTML::brightnessFactor

**File:** `effects/ieffect/BeatPulseRenderUtils.h` (line 324)

**Namespace:** `lightwaveos::effects::ieffect::BeatPulseHTML`

**Signature:**

```cpp
static inline float brightnessFactor(float intensity)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `intensity` | `float` | Intensity [0..1] |

**Returns:** `float`

**Formula:**

```
0.5 + clamp01(intensity) * 0.5
```

**Domain:** Brightness factor from intensity (HTML parity)

**Computational Cost:**

- 1 multiply-accumulate(s); ~5 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Returns [0.5..1.0]. Ensures minimum 50% brightness even at zero intensity.

---

### BeatPulseHTML::whiteMix

**File:** `effects/ieffect/BeatPulseRenderUtils.h` (line 335)

**Namespace:** `lightwaveos::effects::ieffect::BeatPulseHTML`

**Signature:**

```cpp
static inline float whiteMix(float intensity)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `intensity` | `float` | Intensity [0..1] |

**Returns:** `float`

**Formula:**

```
clamp01(intensity) * 0.3
```

**Domain:** White mix fraction for beat pulse (HTML parity)

**Computational Cost:**

- 1 multiply-accumulate(s); ~5 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Returns [0..0.3]. Controls how much white is mixed into beat pulse centre.

---

### BeatPulseTiming::computeBeatTick

**File:** `effects/ieffect/BeatPulseRenderUtils.h` (line 355)

**Namespace:** `lightwaveos::effects::ieffect::BeatPulseTiming`

**Signature:**

```cpp
static inline bool computeBeatTick(const plugins::EffectContext& ctx, float fallbackBpm, uint32_t& lastBeatMs)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `ctx` | `const plugins::EffectContext&` | Effect context with audio data |
| `fallbackBpm` | `float` | Fallback BPM when no audio lock |
| `lastBeatMs` | `uint32_t&` | Persisted last beat timestamp (modified in place) |

**Returns:** `bool`

**Formula:**

```
If tempo confidence >= 0.25 and audio available, use audio beat tick. Otherwise, use raw-time fallback:
trigger when elapsed >= 60000/fallbackBpm.
```

**Domain:** Beat tick with confidence gate and fallback metronome

**Computational Cost:**

- 2 multiply-accumulate(s); ~15 estimated cycles

**Frame-rate independent:** Yes

**Used by:**

- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)

**Notes:** Via AudioReactivePolicy::audioBeatTick -> BeatPulseCore::stepEnvelope. Confidence threshold kTempoConfMin = 0.25.

---

### BeatPulseCore::stepEnvelope

**File:** `effects/ieffect/BeatPulseCore.h` (line 43)

**Namespace:** `lightwaveos::effects::ieffect::BeatPulseCore`

**Signature:**

```cpp
static inline bool stepEnvelope(const plugins::EffectContext& ctx, State& s)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `ctx` | `const plugins::EffectContext&` | Effect context |
| `s` | `State&` | Beat pulse state (beatIntensity, lastBeatMs, fallbackBpm) |

**Returns:** `bool`

**Formula:**

```
beatTick = AudioReactivePolicy::audioBeatTick(ctx, ...); dt = signalDt(ctx);
BeatPulseHTML::updateBeatIntensity(s.beatIntensity, beatTick, dt);
```

**Domain:** Canonical beat envelope step for BeatPulse family

**Computational Cost:**

- 4 multiply-accumulate(s); ~100 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Combines beat tick detection with HTML-parity envelope decay. Used by BeatPulseCore::renderSingleRing.

---

### BeatPulseCore::intensityAt

**File:** `effects/ieffect/BeatPulseCore.h` (line 55)

**Namespace:** `lightwaveos::effects::ieffect::BeatPulseCore`

**Signature:**

```cpp
static inline float intensityAt(float dist01, float ringPos01, const State& s, float slope)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `dist01` | `float` | Normalised distance from centre [0..1] |
| `ringPos01` | `float` | Ring position [0..1] |
| `s` | `const State&` | Beat state with beatIntensity |
| `slope` | `float` | Profile slope (higher = narrower ring) |

**Returns:** `float`

**Formula:**

```
waveHit = 1 - min(1, |dist01 - ringPos01| * slope); return max(0, waveHit) * s.beatIntensity
```

**Domain:** Per-LED intensity with configurable slope

**Computational Cost:**

- 4 multiply-accumulate(s); ~10 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Generalised version of BeatPulseHTML::intensityAtDist with configurable slope parameter.

---

## Noise and Pseudo-Random

Deterministic RNG, approximate Gaussian, Kuramoto feature extraction, FastLED sin16/cos16 wrappers.

### XorShift32::next01

**File:** `effects/ieffect/KuramotoOscillatorField.h` (line 41)

**Namespace:** `lightwaveos::effects::ieffect`

**Signature:**

```cpp
float next01()
```

**Returns:** `float`

**Formula:**

```
(nextU32() >> 8) * (1.0 / 16777216.0)
```

**Domain:** Uniform random float in [0, 1) from XorShift32 PRNG

**Computational Cost:**

- 1 multiply-accumulate(s); ~8 estimated cycles

**Frame-rate independent:** Yes

**Used by:**

- [?](EFFECTS_INVENTORY.md) (ID ?)

**Notes:** Lightweight deterministic RNG for embedded use. Takes top 24 bits of XorShift32 state for uniform mantissa.

---

### XorShift32::approxNormal

**File:** `effects/ieffect/KuramotoOscillatorField.h` (line 46)

**Namespace:** `lightwaveos::effects::ieffect`

**Signature:**

```cpp
float approxNormal()
```

**Returns:** `float`

**Formula:**

```
(sum_of_6_uniform_samples - 3.0) * sqrt(2) -- CLT-ish approximation of N(0,1)
```

**Domain:** Approximate Gaussian random (N(0,1)) from sum of uniforms

**Computational Cost:**

- 8 multiply-accumulate(s); ~50 estimated cycles

**Frame-rate independent:** Yes

**Used by:**

- [?](EFFECTS_INVENTORY.md) (ID ?)

**Notes:** Central Limit Theorem approximation using 6 uniform samples. Mean 0, variance ~1. Cheap alternative to Box-Muller.

---

### KuramotoFeatureExtractor::extract

**File:** `effects/ieffect/KuramotoFeatureExtractor.h` (line 45)

**Namespace:** `lightwaveos::effects::ieffect`

**Signature:**

```cpp
static void extract(const float* theta, const float* prevTheta, uint16_t radius, const float* kernel, float* outVelocity, float* outCoherence, float* outEvent)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `theta` | `const float*` | Current phases (length N=80) |
| `prevTheta` | `const float*` | Previous phases (length N=80) |
| `radius` | `uint16_t` | Nonlocal radius used by oscillator field |
| `kernel` | `const float*` | Kernel weights array length (2*radius+1) |
| `outVelocity` | `float*` | Output: signed velocity [-1,1] |
| `outCoherence` | `float*` | Output: local order parameter [0,1] |
| `outEvent` | `float*` | Output: injection strength [0,1] |

**Returns:** `void`

**Formula:**

```
Coherence: r = |sum(w*exp(j*theta))| / sum_w. Velocity: wrapPi(theta[i+1]-theta[i-1]) / pi. Event: 0.8*slip +
0.6*edge + 0.5*curvature (soft threshold at 0.12).
```

**Domain:** Extract renderable features from invisible Kuramoto phase field

**Computational Cost:**

- 320 trig call(s); 800 multiply-accumulate(s); ~15000 estimated cycles

**Frame-rate independent:** Yes

**Used by:**

- [?](EFFECTS_INVENTORY.md) (ID ?)

**Notes:** Expensive: 80 cells x (2*radius+1) sinf+cosf calls for coherence computation. Dominant cost in Kuramoto pipeline. Phase slips, coherence edges, curvature combined for event detection.

---

### fastled_sin16_normalized

**File:** `effects/utils/FastLEDOptim.h` (line 52)

**Namespace:** `lightwaveos::effects::utils`

**Signature:**

```cpp
inline float fastled_sin16_normalized(uint16_t angle)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `angle` | `uint16_t` | Angle in FastLED units (0-65535 maps to 0-2pi) |

**Returns:** `float`

**Formula:**

```
sin16(angle) / 32768.0f
```

**Domain:** FastLED sin16 normalised to [-1, 1]

**Computational Cost:**

- 1 trig call(s); 1 multiply-accumulate(s); ~15 estimated cycles

**Frame-rate independent:** Yes

**Notes:** FastLED sin16 is a lookup-table-based approximation -- much cheaper than sinf. Returns float for easy math.

---

### fastled_cos16_normalized

**File:** `effects/utils/FastLEDOptim.h` (line 65)

**Namespace:** `lightwaveos::effects::utils`

**Signature:**

```cpp
inline float fastled_cos16_normalized(uint16_t angle)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `angle` | `uint16_t` | Angle in FastLED units (0-65535 maps to 0-2pi) |

**Returns:** `float`

**Formula:**

```
cos16(angle) / 32768.0f
```

**Domain:** FastLED cos16 normalised to [-1, 1]

**Computational Cost:**

- 1 trig call(s); 1 multiply-accumulate(s); ~15 estimated cycles

**Frame-rate independent:** Yes

**Notes:** FastLED cos16 is a lookup-table-based approximation.

---

## Interpolation

Linear interpolation utilities.

### lerp

**File:** `effects/ieffect/BeatPulseRenderUtils.h` (line 44)

**Namespace:** `lightwaveos::effects::ieffect`

**Signature:**

```cpp
static inline float lerp(float a, float b, float t)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `a` | `float` | Start value |
| `b` | `float` | End value |
| `t` | `float` | Interpolation factor [0, 1] |

**Returns:** `float`

**Formula:**

```
a + (b - a) * t
```

**Domain:** Linear interpolation

**Computational Cost:**

- 2 multiply-accumulate(s); ~5 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Standard lerp. Not clamped -- caller must ensure t is in [0,1].

---

## Geometry and Spatial

Centre-origin geometry, ring profiles, distance calculations, phase/sine wave generation.

### RingProfile::gaussian

**File:** `effects/ieffect/BeatPulseRenderUtils.h` (line 87)

**Namespace:** `lightwaveos::effects::ieffect::RingProfile`

**Signature:**

```cpp
static inline float gaussian(float distance, float sigma)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `distance` | `float` | Distance from ring centre |
| `sigma` | `float` | Gaussian sigma (controls ring spread) |

**Returns:** `float`

**Formula:**

```
exp(-0.5 * (distance / sigma)^2)
```

**Domain:** Gaussian ring profile for soft natural falloff

**Computational Cost:**

- 1 exp call(s); 3 multiply-accumulate(s); ~80 estimated cycles

**Frame-rate independent:** Yes

**Used by:**

- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)
- [?](EFFECTS_INVENTORY.md) (ID ?)

**Notes:** At distance=sigma intensity is ~0.6, at 2*sigma ~0.13, at 3*sigma ~0.01.

---

### RingProfile::tent

**File:** `effects/ieffect/BeatPulseRenderUtils.h` (line 98)

**Namespace:** `lightwaveos::effects::ieffect::RingProfile`

**Signature:**

```cpp
static inline float tent(float distance, float width)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `distance` | `float` | Distance from ring centre |
| `width` | `float` | Half-width of the tent |

**Returns:** `float`

**Formula:**

```
max(0, 1 - distance/width)
```

**Domain:** Linear tent ring profile

**Computational Cost:**

- 1 multiply-accumulate(s); ~5 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Cheapest ring profile. Linear falloff.

---

### RingProfile::cosine

**File:** `effects/ieffect/BeatPulseRenderUtils.h` (line 109)

**Namespace:** `lightwaveos::effects::ieffect::RingProfile`

**Signature:**

```cpp
static inline float cosine(float distance, float width)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `distance` | `float` | Distance from ring centre |
| `width` | `float` | Half-width of the ring |

**Returns:** `float`

**Formula:**

```
0.5 * (1 + cos(pi * distance / width))
```

**Domain:** Cosine ring profile (zero derivative at edges)

**Computational Cost:**

- 1 trig call(s); 2 multiply-accumulate(s); ~40 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Smooth falloff with zero derivative at edges. Good for seamless layering.

---

### RingProfile::glow

**File:** `effects/ieffect/BeatPulseRenderUtils.h` (line 124)

**Namespace:** `lightwaveos::effects::ieffect::RingProfile`

**Signature:**

```cpp
static inline float glow(float distance, float coreWidth, float haloWidth)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `distance` | `float` | Distance from ring centre |
| `coreWidth` | `float` | Width of bright core |
| `haloWidth` | `float` | Width of soft halo beyond core |

**Returns:** `float`

**Formula:**

```
Core: 1 - (d/coreWidth)^2 * 0.2. Halo: 0.8 * (1 - (d-coreWidth)/haloWidth)^2
```

**Domain:** Bright core + soft halo for water-like spread

**Computational Cost:**

- 4 multiply-accumulate(s); ~15 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Two-zone profile: 1.0 at centre, 0.8 at core edge, quadratic decay in halo to 0.

---

### RingProfile::hardEdge

**File:** `effects/ieffect/BeatPulseRenderUtils.h` (line 149)

**Namespace:** `lightwaveos::effects::ieffect::RingProfile`

**Signature:**

```cpp
static inline float hardEdge(float diff, float width, float softness)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `diff` | `float` | Absolute distance from ring centre |
| `width` | `float` | Ring half-width |
| `softness` | `float` | Anti-aliasing transition width |

**Returns:** `float`

**Formula:**

```
1 if diff <= width-softness; 0 if diff >= width+softness; linear in transition zone
```

**Domain:** Hard-edged ring for shockwave/detonation effects

**Computational Cost:**

- 2 multiply-accumulate(s); ~8 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Sharp pressure wave front with subtle anti-aliasing at edges.

---

### centerPairDistance

**File:** `effects/CoreEffects.h` (line 27)

**Namespace:** `lightwaveos::effects`

**Signature:**

```cpp
constexpr uint16_t centerPairDistance(uint16_t index)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `index` | `uint16_t` | LED index |

**Returns:** `uint16_t`

**Formula:**

```
if index <= 79: (79 - index), else: (index - 80)
```

**Domain:** Distance from centre pair (0 at 79/80, increases outward)

**Computational Cost:**

- ~3 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Core geometry function for centre-origin pattern. 105+ effect files use SET_CENTER_PAIR macro which relies on same geometry.

---

### centerPairSignedPosition

**File:** `effects/CoreEffects.h` (line 31)

**Namespace:** `lightwaveos::effects`

**Signature:**

```cpp
constexpr float centerPairSignedPosition(uint16_t index)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `index` | `uint16_t` | LED index |

**Returns:** `float`

**Formula:**

```
if index <= 79: -((79-index)+0.5), else: ((index-80)+0.5)
```

**Domain:** Signed position from centre pair (negative=left, positive=right)

**Computational Cost:**

- ~5 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Returns half-integer positions: -79.5 at LED 0, -0.5 at LED 79, +0.5 at LED 80, +79.5 at LED 159.

---

### KuramotoOscillatorField::wrapPi

**File:** `effects/ieffect/KuramotoOscillatorField.h` (line 211)

**Namespace:** `lightwaveos::effects::ieffect`

**Signature:**

```cpp
static float wrapPi(float x)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `x` | `float` | Angle in radians |

**Returns:** `float`

**Formula:**

```
x = fmod(x + pi, 2*pi); if x<0: x += 2*pi; return x - pi
```

**Domain:** Wrap angle to [-pi, pi]

**Computational Cost:**

- 2 multiply-accumulate(s); ~20 estimated cycles

**Frame-rate independent:** Yes

**Used by:**

- [?](EFFECTS_INVENTORY.md) (ID ?)

**Notes:** Uses fmodf. Bounded -- prevents infinite loop on NaN/large values. Used extensively in Kuramoto engine.

---

### fastled_center_sin16

**File:** `effects/utils/FastLEDOptim.h` (line 217)

**Namespace:** `lightwaveos::effects::utils`

**Signature:**

```cpp
inline float fastled_center_sin16(int position, int center, float halfLength, float frequency, uint16_t phase)
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `position` | `int` | LED position (0 to STRIP_LENGTH-1) |
| `center` | `int` | Centre position (typically CENTER_LEFT) |
| `halfLength` | `float` | Half the strip length for normalisation |
| `frequency` | `float` | Wave frequency multiplier |
| `phase` | `uint16_t` | Phase offset |

**Returns:** `float`

**Formula:**

```
dist = min(|pos - centre|, |pos - (centre+1)|) / halfLength; return sin16(dist * freq * 256 + phase) / 32768
```

**Domain:** Centre-origin distance-based sine wave

**Computational Cost:**

- 1 trig call(s); 5 multiply-accumulate(s); ~25 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Combined helper: calculates normalised centre distance then applies FastLED sin16 wave.

---

### EffectContext::getDistanceFromCenter

**File:** `plugins/api/EffectContext.h` (line 643)

**Namespace:** `lightwaveos::plugins`

**Signature:**

```cpp
float getDistanceFromCenter(uint16_t index) const
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `index` | `uint16_t` | LED index (0 to ledCount-1) |

**Returns:** `float`

**Formula:**

```
|index - centerPoint| / centerPoint
```

**Domain:** Normalised distance from centre (0.0 at centre, 1.0 at edges)

**Computational Cost:**

- 1 multiply-accumulate(s); ~5 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Core method for centre-origin compliance. Effects should use this instead of raw index for position-based calculations.

---

### EffectContext::getSignedPosition

**File:** `plugins/api/EffectContext.h` (line 659)

**Namespace:** `lightwaveos::plugins`

**Signature:**

```cpp
float getSignedPosition(uint16_t index) const
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `index` | `uint16_t` | LED index |

**Returns:** `float`

**Formula:**

```
(index - centerPoint) / centerPoint
```

**Domain:** Signed position from centre (-1.0 to +1.0)

**Computational Cost:**

- 1 multiply-accumulate(s); ~5 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Useful for effects that need to know which side of centre an LED is on.

---

### EffectContext::getPhase

**File:** `plugins/api/EffectContext.h` (line 695)

**Namespace:** `lightwaveos::plugins`

**Signature:**

```cpp
float getPhase(float frequencyHz) const
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `frequencyHz` | `float` | Oscillation frequency in Hz |

**Returns:** `float`

**Formula:**

```
fmod(totalTimeMs, 1000/frequencyHz) / (1000/frequencyHz)
```

**Domain:** Time-based phase for smooth animations [0.0 to 1.0]

**Computational Cost:**

- 2 multiply-accumulate(s); ~10 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Returns sawtooth phase value. Uses totalTimeMs (SPEED-scaled).

---

### EffectContext::getSineWave

**File:** `plugins/api/EffectContext.h` (line 705)

**Namespace:** `lightwaveos::plugins`

**Signature:**

```cpp
float getSineWave(float frequencyHz) const
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `frequencyHz` | `float` | Oscillation frequency in Hz |

**Returns:** `float`

**Formula:**

```
sin(getPhase(frequencyHz) * 2 * pi)
```

**Domain:** Time-based sine wave [-1.0 to +1.0]

**Computational Cost:**

- 1 trig call(s); 3 multiply-accumulate(s); ~50 estimated cycles

**Frame-rate independent:** Yes

**Notes:** Uses sinf (software trig). For cheaper alternative see fastled_sin16_normalized.

---

## Constants and Lookup Tables

| Name | Type | File | Domain |
|------|------|------|--------|
| `kCos` | `static constexpr float[12]` | `effects/ieffect/ChromaUtils.h` | Precomputed cosines for 12 chroma bin positions (30-degree steps) |
| `kSin` | `static constexpr float[12]` | `effects/ieffect/ChromaUtils.h` | Precomputed sines for 12 chroma bin positions (30-degree steps) |
| `TWO_PI_F` | `static constexpr float` | `effects/ieffect/ChromaUtils.h` | Two pi constant |
| `PI_F` | `static constexpr float` | `effects/ieffect/ChromaUtils.h` | Pi constant |
| `CENTER_LEFT` | `constexpr uint16_t` | `effects/CoreEffects.h` | Last LED of left half (centre pair left) |
| `CENTER_RIGHT` | `constexpr uint16_t` | `effects/CoreEffects.h` | First LED of right half (centre pair right) |
| `HALF_LENGTH` | `constexpr uint16_t` | `effects/CoreEffects.h` | LEDs per half strip |
| `STRIP_LENGTH` | `constexpr uint16_t` | `effects/CoreEffects.h` | LEDs per strip |
| `kTempoConfMin` | `static constexpr float` | `effects/ieffect/BeatPulseRenderUtils.h` | Minimum tempo confidence for audio beat gate |
| `CONTROLBUS_NUM_BANDS` | `static constexpr uint8_t` | `audio/contracts/ControlBus.h` | Number of frequency bands in ControlBusFrame |
| `CONTROLBUS_NUM_CHROMA` | `static constexpr uint8_t` | `audio/contracts/ControlBus.h` | Number of chroma bins (one per semitone) |
| `CONTROLBUS_WAVEFORM_N` | `static constexpr uint8_t` | `audio/contracts/ControlBus.h` | Waveform sample count (Sensory Bridge NATIVE_RESOLUTION) |
| `CONTROLBUS_NUM_ZONES` | `static constexpr uint8_t` | `audio/contracts/ControlBus.h` | Number of AGC zones (Sensory Bridge pattern) |
| `BINS_64_COUNT` | `static constexpr uint8_t` | `audio/contracts/ControlBus.h` | Number of 64-bin Goertzel spectrum bins (110-4186 Hz) |
| `BINS_256_COUNT` | `static constexpr uint16_t` | `audio/contracts/ControlBus.h` | Number of 256-bin FFT magnitude bins (62.5 Hz spacing @ 32kHz/512-pt) |
| `KuramotoOscillatorField::N` | `static constexpr uint16_t` | `effects/ieffect/KuramotoOscillatorField.h` | Oscillator count (one per radial bin, centre-to-edge) |
| `KuramotoOscillatorField::MAX_R` | `static constexpr uint16_t` | `effects/ieffect/KuramotoOscillatorField.h` | Maximum nonlocal coupling radius for kernel |
| `KuramotoOscillatorField::PI_F` | `static constexpr float` | `effects/ieffect/KuramotoOscillatorField.h` | Pi constant for Kuramoto engine |

### kCos

**File:** `effects/ieffect/ChromaUtils.h` (line 26)

**Type:** `static constexpr float[12]`

**Formula:** `cos(i * 2*pi/12) for i = 0..11`

**Values:** `[1.0, 0.866025, 0.5, 0.0, -0.5, -0.866025, -1.0, -0.866025, -0.5, 0.0, 0.5, 0.866025]`

**Domain:** Precomputed cosines for 12 chroma bin positions (30-degree steps)


### kSin

**File:** `effects/ieffect/ChromaUtils.h` (line 30)

**Type:** `static constexpr float[12]`

**Formula:** `sin(i * 2*pi/12) for i = 0..11`

**Values:** `[0.0, 0.5, 0.866025, 1.0, 0.866025, 0.5, 0.0, -0.5, -0.866025, -1.0, -0.866025, -0.5]`

**Domain:** Precomputed sines for 12 chroma bin positions (30-degree steps)


### TWO_PI_F

**File:** `effects/ieffect/ChromaUtils.h` (line 35)

**Type:** `static constexpr float`

**Formula:** `2 * pi`

**Value:** `6.2831853`

**Domain:** Two pi constant


### PI_F

**File:** `effects/ieffect/ChromaUtils.h` (line 36)

**Type:** `static constexpr float`

**Formula:** `pi`

**Value:** `3.1415927`

**Domain:** Pi constant


### CENTER_LEFT

**File:** `effects/CoreEffects.h` (line 22)

**Type:** `constexpr uint16_t`

**Value:** `79`

**Domain:** Last LED of left half (centre pair left)


### CENTER_RIGHT

**File:** `effects/CoreEffects.h` (line 23)

**Type:** `constexpr uint16_t`

**Value:** `80`

**Domain:** First LED of right half (centre pair right)


### HALF_LENGTH

**File:** `effects/CoreEffects.h` (line 24)

**Type:** `constexpr uint16_t`

**Value:** `80`

**Domain:** LEDs per half strip


### STRIP_LENGTH

**File:** `effects/CoreEffects.h` (line 25)

**Type:** `constexpr uint16_t`

**Value:** `160`

**Domain:** LEDs per strip


### kTempoConfMin

**File:** `effects/ieffect/BeatPulseRenderUtils.h` (line 347)

**Type:** `static constexpr float`

**Value:** `0.25`

**Domain:** Minimum tempo confidence for audio beat gate


### CONTROLBUS_NUM_BANDS

**File:** `audio/contracts/ControlBus.h` (line 10)

**Type:** `static constexpr uint8_t`

**Value:** `8`

**Domain:** Number of frequency bands in ControlBusFrame


### CONTROLBUS_NUM_CHROMA

**File:** `audio/contracts/ControlBus.h` (line 11)

**Type:** `static constexpr uint8_t`

**Value:** `12`

**Domain:** Number of chroma bins (one per semitone)


### CONTROLBUS_WAVEFORM_N

**File:** `audio/contracts/ControlBus.h` (line 12)

**Type:** `static constexpr uint8_t`

**Value:** `128`

**Domain:** Waveform sample count (Sensory Bridge NATIVE_RESOLUTION)


### CONTROLBUS_NUM_ZONES

**File:** `audio/contracts/ControlBus.h` (line 20)

**Type:** `static constexpr uint8_t`

**Value:** `4`

**Domain:** Number of AGC zones (Sensory Bridge pattern)


### BINS_64_COUNT

**File:** `audio/contracts/ControlBus.h` (line 67)

**Type:** `static constexpr uint8_t`

**Value:** `64`

**Domain:** Number of 64-bin Goertzel spectrum bins (110-4186 Hz)


### BINS_256_COUNT

**File:** `audio/contracts/ControlBus.h` (line 72)

**Type:** `static constexpr uint16_t`

**Value:** `256`

**Domain:** Number of 256-bin FFT magnitude bins (62.5 Hz spacing @ 32kHz/512-pt)


### KuramotoOscillatorField::N

**File:** `effects/ieffect/KuramotoOscillatorField.h` (line 71)

**Type:** `static constexpr uint16_t`

**Value:** `80`

**Domain:** Oscillator count (one per radial bin, centre-to-edge)


### KuramotoOscillatorField::MAX_R

**File:** `effects/ieffect/KuramotoOscillatorField.h` (line 72)

**Type:** `static constexpr uint16_t`

**Value:** `24`

**Domain:** Maximum nonlocal coupling radius for kernel


### KuramotoOscillatorField::PI_F

**File:** `effects/ieffect/KuramotoOscillatorField.h` (line 73)

**Type:** `static constexpr float`

**Value:** `3.14159265`

**Domain:** Pi constant for Kuramoto engine


## Domain Cross-Reference

Functions grouped by domain, for quick lookup when implementing or debugging effects.

### Chroma Processing

- [`circularChromaHue`](#circularchromahue)
- [`circularEma`](#circularema)
- [`circularChromaHueSmoothed`](#circularchromahuesmoothed)
- [`dtDecay`](#dtdecay)

### Smoothing Primitives

- [`ExpDecay::update`](#expdecayupdate)
- [`Spring::update`](#springupdate)
- [`AsymmetricFollower::update`](#asymmetricfollowerupdate)
- [`AsymmetricFollower::updateWithMood`](#asymmetricfollowerupdatewithmood)
- [`getSafeDeltaSeconds`](#getsafedeltaseconds)
- [`tauToLambda`](#tautolambda)
- [`computeEmaAlpha`](#computeemaalpha)
- [`tauFromAlpha`](#taufromalpha)
- [`alphaFromHalfLife`](#alphafromhalflife)
- [`retunedAlpha`](#retunedalpha)

### Rendering Helpers

- [`clamp01`](#clamp01)
- [`floatToByte`](#floattobyte)
- [`scaleBrightness`](#scalebrightness)
- [`ColourUtil::addWhiteSaturating`](#colourutiladdwhitesaturating)
- [`ColourUtil::additive`](#colourutiladditive)
- [`BlendMode::softAccumulate`](#blendmodesoftaccumulate)
- [`BlendMode::screen`](#blendmodescreen)
- [`SubpixelRenderer::renderPoint`](#subpixelrendererrenderpoint)
- [`SubpixelRenderer::renderLine`](#subpixelrendererrenderline)
- [`acesFilm`](#acesfilm)
- [`cinema::apply`](#cinemaapply)
- [`fastled_wrap_hue_safe`](#fastledwraphuesafe)
- [`KuramotoTransportBuffer::toneMapToCRGB8`](#kuramototransportbuffertonemaptocrgb8)
- [`BeatPulseTransportCore::kneeToneMap`](#beatpulsetransportcorekneetonemap)

### Audio Feature Access

- [`AudioReactivePolicy::signalDt`](#audioreactivepolicysignaldt)
- [`AudioReactivePolicy::visualDt`](#audioreactivepolicyvisualdt)

### Noise

- [`XorShift32::next01`](#xorshift32next01)
- [`XorShift32::approxNormal`](#xorshift32approxnormal)
- [`KuramotoFeatureExtractor::extract`](#kuramotofeatureextractorextract)
- [`fastled_sin16_normalized`](#fastledsin16normalized)
- [`fastled_cos16_normalized`](#fastledcos16normalized)

### Interpolation

- [`lerp`](#lerp)

### Geometry

- [`RingProfile::gaussian`](#ringprofilegaussian)
- [`RingProfile::tent`](#ringprofiletent)
- [`RingProfile::cosine`](#ringprofilecosine)
- [`RingProfile::glow`](#ringprofileglow)
- [`RingProfile::hardEdge`](#ringprofilehardedge)
- [`centerPairDistance`](#centerpairdistance)
- [`centerPairSignedPosition`](#centerpairsignedposition)
- [`KuramotoOscillatorField::wrapPi`](#kuramotooscillatorfieldwrappi)
- [`fastled_center_sin16`](#fastledcentersin16)
- [`EffectContext::getDistanceFromCenter`](#effectcontextgetdistancefromcenter)
- [`EffectContext::getSignedPosition`](#effectcontextgetsignedposition)
- [`EffectContext::getPhase`](#effectcontextgetphase)
- [`EffectContext::getSineWave`](#effectcontextgetsinewave)

### Beat Processing

- [`BeatPulseHTML::decayMul`](#beatpulsehtmldecaymul)
- [`BeatPulseHTML::updateBeatIntensity`](#beatpulsehtmlupdatebeatintensity)
- [`BeatPulseHTML::ringCentre01`](#beatpulsehtmlringcentre01)
- [`BeatPulseHTML::intensityAtDist`](#beatpulsehtmlintensityatdist)
- [`BeatPulseHTML::brightnessFactor`](#beatpulsehtmlbrightnessfactor)
- [`BeatPulseHTML::whiteMix`](#beatpulsehtmlwhitemix)
- [`BeatPulseTiming::computeBeatTick`](#beatpulsetimingcomputebeattick)
- [`BeatPulseCore::stepEnvelope`](#beatpulsecorestepenvelope)
- [`BeatPulseCore::intensityAt`](#beatpulsecoreintensityat)

## Computational Cost Summary

Estimated per-call cost for each function, to aid in per-frame budget planning.
Target: keep total per-frame effect code under ~2 ms at 120 FPS.

| Function | Trig | Exp | MACs | Est. Cycles |
|----------|------|-----|------|-------------|
| `circularChromaHue` | 1 | 0 | 24 | 120 |
| `circularEma` | 0 | 0 | 2 | 15 |
| `circularChromaHueSmoothed` | 1 | 1 | 26 | 200 |
| `dtDecay` | 0 | 0 | 2 | 80 |
| `clamp01` | 0 | 0 | 0 | 5 |
| `lerp` | 0 | 0 | 2 | 5 |
| `floatToByte` | 0 | 0 | 1 | 5 |
| `scaleBrightness` | 0 | 0 | 1 | 8 |
| `RingProfile::gaussian` | 0 | 1 | 3 | 80 |
| `RingProfile::tent` | 0 | 0 | 1 | 5 |
| `RingProfile::cosine` | 1 | 0 | 2 | 40 |
| `RingProfile::glow` | 0 | 0 | 4 | 15 |
| `RingProfile::hardEdge` | 0 | 0 | 2 | 8 |
| `ColourUtil::addWhiteSaturating` | 0 | 0 | 6 | 25 |
| `ColourUtil::additive` | 0 | 0 | 3 | 12 |
| `BlendMode::softAccumulate` | 0 | 0 | 1 | 5 |
| `BlendMode::screen` | 0 | 0 | 3 | 8 |
| `BeatPulseHTML::decayMul` | 0 | 0 | 1 | 80 |
| `BeatPulseHTML::updateBeatIntensity` | 0 | 0 | 2 | 90 |
| `BeatPulseHTML::ringCentre01` | 0 | 0 | 1 | 3 |
| `BeatPulseHTML::intensityAtDist` | 0 | 0 | 4 | 10 |
| `BeatPulseHTML::brightnessFactor` | 0 | 0 | 1 | 5 |
| `BeatPulseHTML::whiteMix` | 0 | 0 | 1 | 5 |
| `BeatPulseTiming::computeBeatTick` | 0 | 0 | 2 | 15 |
| `AudioReactivePolicy::signalDt` | 0 | 0 | 0 | 5 |
| `AudioReactivePolicy::visualDt` | 0 | 0 | 0 | 5 |
| `BeatPulseCore::stepEnvelope` | 0 | 0 | 4 | 100 |
| `BeatPulseCore::intensityAt` | 0 | 0 | 4 | 10 |
| `acesFilm` | 0 | 0 | 8 | 20 |
| `cinema::apply` | 0 | 0 | 40 | 600 |
| `computeEmaAlpha` | 0 | 1 | 2 | 80 |
| `tauFromAlpha` | 0 | 0 | 2 | 60 |
| `alphaFromHalfLife` | 0 | 1 | 1 | 80 |
| `retunedAlpha` | 0 | 1 | 4 | 140 |
| `ExpDecay::update` | 0 | 1 | 3 | 90 |
| `Spring::update` | 0 | 0 | 6 | 20 |
| `AsymmetricFollower::update` | 0 | 1 | 3 | 90 |
| `AsymmetricFollower::updateWithMood` | 0 | 1 | 6 | 100 |
| `SubpixelRenderer::renderPoint` | 0 | 0 | 8 | 30 |
| `SubpixelRenderer::renderLine` | 0 | 0 | 12 | 50 |
| `getSafeDeltaSeconds` | 0 | 0 | 0 | 5 |
| `tauToLambda` | 0 | 0 | 0 | 5 |
| `centerPairDistance` | 0 | 0 | 0 | 3 |
| `centerPairSignedPosition` | 0 | 0 | 0 | 5 |
| `KuramotoOscillatorField::wrapPi` | 0 | 0 | 2 | 20 |
| `XorShift32::next01` | 0 | 0 | 1 | 8 |
| `XorShift32::approxNormal` | 0 | 0 | 8 | 50 |
| `KuramotoTransportBuffer::toneMapToCRGB8` | 0 | 0 | 15 | 50 |
| `BeatPulseTransportCore::kneeToneMap` | 0 | 0 | 3 | 10 |
| `KuramotoFeatureExtractor::extract` | 320 | 0 | 800 | 15000 |
| `fastled_sin16_normalized` | 1 | 0 | 1 | 15 |
| `fastled_cos16_normalized` | 1 | 0 | 1 | 15 |
| `fastled_wrap_hue_safe` | 0 | 0 | 2 | 10 |
| `fastled_center_sin16` | 1 | 0 | 5 | 25 |
| `EffectContext::getDistanceFromCenter` | 0 | 0 | 1 | 5 |
| `EffectContext::getSignedPosition` | 0 | 0 | 1 | 5 |
| `EffectContext::getPhase` | 0 | 0 | 2 | 10 |
| `EffectContext::getSineWave` | 1 | 0 | 3 | 50 |
