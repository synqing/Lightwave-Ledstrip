# Effect Development Standard — Canonical Reference

**Status:** MANDATORY — Every agent MUST read this before creating or modifying any effect.
**Authority:** Captain-approved. Violations produce effects that look like dog shit.
**Last Verified Against Codebase:** 2026-03-02

---

## Why This Document Exists

Agents have repeatedly made effects worse by tweaking parameters without understanding the rendering fundamentals. The codebase contains a complete, battle-tested infrastructure for producing professional-quality LED visuals. **Use it.** Do not reinvent smoothing, do not guess at constants, do not "fix" effects by adjusting a single number.

This document defines:
1. What makes an effect look good vs garbage
2. The mandatory baseline every effect must meet
3. The infrastructure that already exists (USE IT)
4. Anti-patterns that produce shit output
5. The audio integration pattern that actually works

---

## Part 1: The Rendering Pipeline (Know Where Your Code Sits)

Every frame follows this exact pipeline at 120 FPS (8.33ms budget):

```
┌─────────────────────────────────────────────────────────┐
│ RendererActor (Core 1, highest priority)                │
│                                                         │
│  1. Read ControlBusFrame from AudioActor (lock-free)    │
│  2. Advance beat phase (PLL at 120 FPS)                 │
│  3. ─── YOUR EFFECT render() RUNS HERE ─── (< 2ms!)    │
│  4. TAP_A: Pre-correction capture (debug)               │
│  5. ColorCorrectionEngine::processBuffer()              │
│     ├─ Auto-Exposure (BT.601 luminance)                 │
│     ├─ V-Clamping (prevent white saturation)            │
│     ├─ Saturation Boost                                 │
│     ├─ White Guardrail                                  │
│     ├─ Brown Guardrail                                  │
│     └─ Gamma Correction (LUT, default 2.2)              │
│  6. TAP_B: Post-correction capture (debug)              │
│  7. FastLED.show() (blocks ~6.3ms for wire transfer)    │
│  8. Frame-rate throttle to 120 FPS                      │
│  9. Watchdog reset (every 10 frames)                    │
└─────────────────────────────────────────────────────────┘
```

**Critical implications:**
- Your effect code runs at step 3. You have **< 2ms**.
- Color correction happens AFTER your render. Physics effects that compute exact RGB skip this (see Behavioral Gates in CONSTRAINTS.md).
- Gamma correction transforms everything. PWM 128 = 36% perceived brightness, not 50%. Your "subtle" effects may vanish after gamma.
- The pipeline already handles white saturation, brown mud, and exposure. You do NOT need to manually avoid these — unless you're on the skip list.

---

## Part 2: Mandatory Baseline — Every Effect Must Have These

An effect without these fundamentals will look cheap, flickery, or dead. No exceptions.

### 2.1 Frame-Rate Independent Timing

**Every time-varying value MUST use delta time.**

```cpp
// === CORRECT ===
void render(plugins::EffectContext& ctx) {
    float dt = ctx.getSafeDeltaSeconds();  // Clamped to [0.0001, 0.05]
    m_phase += speedNorm * someRate * dt;  // Frame-rate independent
}

// === WRONG — frame-rate dependent (runs 2x fast at 120 FPS vs 60 FPS) ===
void render(plugins::EffectContext& ctx) {
    m_phase += 0.01f;  // FORBIDDEN — tied to frame rate
}
```

`ctx.getSafeDeltaSeconds()` is your source of truth. It clamps to [0.0001s, 0.05s] to prevent physics explosion on frame drops.

### 2.2 Trail Persistence via `fadeToBlackBy()`

**Every animated effect MUST fade the previous frame before drawing.**

Without trails, effects appear as harsh binary dots jumping between positions. With trails, you get organic motion blur and glow.

```cpp
void render(plugins::EffectContext& ctx) {
    // FIRST: Fade previous frame (creates trails / motion blur)
    fadeToBlackBy(ctx.leds, ctx.ledCount, fadeAmount);

    // THEN: Draw new content on top of faded previous frame
    // ...
}
```

**Fade amount guide:**

| fadeAmount | Retention | Visual Result |
|-----------|-----------|---------------|
| 5-15 | 94-97% | Long luxurious trails (ambient, breathing) |
| 20-40 | 84-92% | Medium trails (standard effects) |
| 50-80 | 69-80% | Short trails (percussive, beat-reactive) |
| 100-150 | 41-61% | Very short (fast action, minimal blur) |
| 255 | 0% | Instant clear (special cases only) |

**For audio-reactive effects:** Make fadeAmount respond to audio energy. Loud = short punchy trails, quiet = long ambient trails.

```cpp
// Dynamic fade: loud music = fast trails, quiet = long trails
uint8_t fadeAmount = (uint8_t)(20 + 60 * (1.0f - audioEnergy));
fadeToBlackBy(ctx.leds, ctx.ledCount, fadeAmount);
```

### 2.3 Center-Origin Rendering

**ALL effects iterate from center outward. Linear left-to-right is FORBIDDEN.**

```cpp
// === CORRECT: Center-origin symmetric rendering ===
for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
    CRGB color = computeColor(dist);
    SET_CENTER_PAIR(ctx, dist, color);  // Sets 4 LEDs: L1, R1, L2, R2
}

// === ALSO CORRECT: Manual center-out with per-strip variation ===
for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
    uint16_t left1  = CENTER_LEFT  - dist;   // Strip 1 left
    uint16_t right1 = CENTER_RIGHT + dist;   // Strip 1 right
    uint16_t left2  = STRIP_LENGTH + CENTER_LEFT  - dist;  // Strip 2 left
    uint16_t right2 = STRIP_LENGTH + CENTER_RIGHT + dist;  // Strip 2 right

    if (left1  < STRIP_LENGTH)  ctx.leds[left1]  = strip1Color;
    if (right1 < STRIP_LENGTH)  ctx.leds[right1] = strip1Color;
    if (left2  < ctx.ledCount)  ctx.leds[left2]  = strip2Color;
    if (right2 < ctx.ledCount)  ctx.leds[right2] = strip2Color;
}

// === WRONG: Linear iteration ===
for (int i = 0; i < 160; i++) {
    ctx.leds[i] = color;  // FORBIDDEN — violates center-origin
}
```

**Constants** (from `CoreEffects.h`):

| Constant | Value | Meaning |
|----------|-------|---------|
| `CENTER_LEFT` | 79 | Last LED of left half |
| `CENTER_RIGHT` | 80 | First LED of right half |
| `HALF_LENGTH` | 80 | LEDs per half-strip |
| `STRIP_LENGTH` | 160 | LEDs per physical strip |

### 2.4 Palette-Driven Color (No Hardcoded RGB)

**Effects MUST use `ctx.palette.getColor()` for all color generation.** Hardcoded CRGB values make effects dead to palette changes.

```cpp
// === CORRECT: Palette-driven ===
CRGB color = ctx.palette.getColor(ctx.gHue + hueOffset, brightness);

// Also correct: ColorFromPalette with the current palette
CRGB color = ColorFromPalette(ctx.getCurrentPalette(), paletteIndex, brightness, LINEARBLEND);

// === WRONG: Hardcoded color ===
CRGB color = CRGB(255, 0, 0);  // DEAD — ignores palette entirely
CRGB color = CHSV(160, 255, 200);  // DEAD — ignores palette
```

**Exception:** Physics simulation effects (interference, wave optics) that compute wavelength-accurate RGB are exempt — they're on the color correction skip list for this reason. These are flagged `isLGPSensitive()`.

### 2.5 Brightness via `scale8()` (Not Float Multiplication)

```cpp
// === CORRECT: FastLED optimized integer math ===
uint8_t bright = scale8(rawBright, ctx.brightness);
CRGB dimmed = color;
dimmed.nscale8(brightness);

// === WRONG: Slow float multiplication ===
float bright = rawBright * (ctx.brightness / 255.0f);  // Wasteful
```

`scale8(a, b)` computes `(a * b) / 255` using fast integer math. Use it everywhere.

### 2.6 Additive Blending via `qadd8()` (Not Assignment)

When layering multiple light sources, use saturating addition to prevent overflow:

```cpp
// === CORRECT: Additive blending (light accumulates naturally) ===
ctx.leds[i].r = qadd8(ctx.leds[i].r, scale8(newColor.r, brightness));
ctx.leds[i].g = qadd8(ctx.leds[i].g, scale8(newColor.g, brightness));
ctx.leds[i].b = qadd8(ctx.leds[i].b, scale8(newColor.b, brightness));

// Or use the CRGB += operator (which uses qadd8 internally):
ctx.leds[i] += newColor;

// === WRONG: Overwrite (destroys previous layers) ===
ctx.leds[i] = newColor;  // Only correct if this is the ONLY layer
```

---

## Part 3: Smoothing Infrastructure (Already Built — USE IT)

**Location:** `src/effects/enhancement/SmoothingEngine.h`

### 3.1 ExpDecay — Basic Smoothing

For any value that should approach a target smoothly:

```cpp
// Header member:
enhancement::ExpDecay m_smoothBrightness{0.0f, 10.0f};  // lambda=10 (fast)

// In render():
float target = computeTargetBrightness();
float smooth = m_smoothBrightness.update(target, dt);
```

**Lambda guide:**

| Lambda | Time to 63% | Use Case |
|--------|-------------|----------|
| 2.0 | 500ms | Slow ambient breathing |
| 5.0 | 200ms | Standard visual smoothing |
| 10.0 | 100ms | Responsive beat tracking |
| 20.0 | 50ms | Fast attack for transients |
| 50.0 | 20ms | Near-instant snap |

**Factory from time constant:**
```cpp
auto smoother = enhancement::ExpDecay::withTimeConstant(0.1f);  // 100ms tau
```

### 3.2 AsymmetricFollower — Audio Envelope (CRITICAL)

**This is the single most important smoothing primitive for audio-reactive effects.** Fast attack (instant response to beats), slow release (smooth fade-out).

```cpp
// Header member:
enhancement::AsymmetricFollower m_envelope{0.0f, 0.05f, 0.30f};
//                                          init  rise   fall
//                                                50ms   300ms

// In render():
float audioLevel = getAudioEnergy();
float smoothed = m_envelope.update(audioLevel, dt);

// With MOOD knob support:
float smoothed = m_envelope.updateWithMood(audioLevel, dt, moodNorm);
// mood=0 (reactive): 50ms rise, 150ms fall
// mood=1 (smooth):   100ms rise, 300ms fall
```

**Typical time constants:**

| Use Case | Rise (attack) | Fall (release) |
|----------|--------------|----------------|
| Beat pulse / percussion | 0.02-0.05s | 0.15-0.30s |
| Bass breathing | 0.05-0.10s | 0.30-0.50s |
| Ambient modulation | 0.10-0.20s | 0.50-1.00s |
| Color shifting | 0.15-0.25s | 0.80-1.50s |

### 3.3 Spring — Physics-Based Motion

For values that should have momentum and natural motion feel:

```cpp
// Header member:
enhancement::Spring m_speedSpring;

// In init():
m_speedSpring.init(50.0f, 1.0f);  // stiffness=50, mass=1 (critically damped)
m_speedSpring.reset(1.0f);        // Start at base speed

// In render():
float targetSpeed = computeTargetSpeed();
float smoothSpeed = m_speedSpring.update(targetSpeed, dt);
```

**Stiffness guide:**

| Stiffness | Feel | Use Case |
|-----------|------|----------|
| 20-30 | Sluggish, heavy | Large mass effects, slow transitions |
| 50-80 | Natural, responsive | Speed modulation, position tracking |
| 100-200 | Snappy, immediate | UI feedback, beat response |
| 300+ | Almost instant | Near-zero overshoot |

### 3.4 SubpixelRenderer — Anti-Aliased Motion

**When anything moves along the strip, render at fractional positions:**

```cpp
// Render a point at LED position 45.7 (splits across LEDs 45 and 46)
enhancement::SubpixelRenderer::renderPoint(
    ctx.leds, STRIP_LENGTH,
    45.7f,           // Fractional position
    color,           // Color
    brightness       // Brightness (0-255)
);

// Render a line segment between fractional positions
enhancement::SubpixelRenderer::renderLine(
    ctx.leds, STRIP_LENGTH,
    30.2f, 50.8f,    // Start and end positions
    color, brightness
);
```

**SubpixelRenderer uses additive blending (`qadd8`), not assignment.** Multiple calls accumulate light naturally.

---

## Part 4: Audio Integration Pattern (The One That Works)

### Signals You Should Know

Quick reference for all audio signals available via `ctx.audio`. Use `ctx.audio.available` to guard access.

| Signal | Accessor | Range | What it measures | Use for |
|--------|----------|-------|-----------------|---------|
| RMS | `ctx.audio.rms()` | 0-1 | Audio volume level | Brightness, intensity |
| Beat Strength | `ctx.audio.beatStrength()` | 0-1 | Continuous beat envelope (peaks on beat, decays between) | Brightness pulsing, size pulsing |
| Beat Tick | `ctx.audio.isOnBeat()` | bool | Single-frame pulse on detected beat | Spawning particles, triggering one-shot events |
| Bass | `ctx.audio.bass()` | 0-1 | Bands 0-1 averaged | Breathing, pulsing |
| Treble | `ctx.audio.treble()` | 0-1 | Bands 5-7 averaged | Sparkle, shimmer |
| Band N | `ctx.audio.getBand(n)` | 0-1 | Octave band energy (0=sub-bass, 7=treble) | Per-frequency reactivity |
| Liveliness | `ctx.audio.liveliness()` | 0-1 | RMS + spectral flux composite | Animation speed scaling |
| Saliency | `ctx.audio.overallSaliency()` | 0-1 | Harmonic/rhythmic/timbral/dynamic novelty | Visual accents on musical surprises |
| Chroma | `ctx.audio.getChroma(0..11)` | 0-1 | Pitch class energy (C=0, C#=1, ..., B=11) | Hue mapping |
| BPM | `ctx.audio.bpm()` | float | Detected tempo (BPM) | Animation timing, phase sync |
| Spectral Flux | `ctx.audio.fastFlux()` | 0-1 | Rate of spectral change | Onset/transient detection |
| Onset Event | `ctx.audio.onsetEvent()` | 0-1 | Broadband onset pulse strength | Flash triggers |
| Kick Hit | `ctx.audio.isKickHit()` | bool | Semantic kick trigger | Centre resets, impact pulses |
| Snare Hit | `ctx.audio.isSnareHit()` | bool | Semantic snare trigger | Accent bursts |
| Hi-hat Hit | `ctx.audio.isHihatHit()` | bool | Semantic hi-hat trigger | Sparkle, shimmer |

**Backend-agnostic signals** (safe on both PipelineCore and ESV11): `rms`, `beatStrength`, `bass`/`mid`/`treble`, `getBand`, `getChroma`, `liveliness`, `overallSaliency`, `bpm`.

### 4.1 The Proven Audio Mapping

From `ChevronWavesEffect`, `LGPWaveCollisionEffect`, `LGPStarBurstEffect` — the three best audio-reactive effects in the system:

```
┌─────────────────────────────────────────────────┐
│ Audio Feature       → Visual Parameter          │
├─────────────────────────────────────────────────┤
│ heavy_bands[1..2]   → Speed / Size modulation   │
│ (300-1200 Hz bass)    via Spring (critically     │
│                       damped, stiffness=50)      │
│                                                  │
│ onset / percussion  → Flash / Burst trigger      │
│                       (exponential decay from     │
│                        center, exp(-dist*k))      │
│                                                  │
│ chromagram dominant  → Color / Hue offset        │
│ bin                   (smoothed with tau=250ms    │
│                        for stability)             │
│                                                  │
│ RMS energy          → Overall brightness /       │
│                       trail length               │
│                       (AsymmetricFollower)        │
└─────────────────────────────────────────────────┘
```

### 4.2 Critical Rule: heavy_bands Are PRE-SMOOTHED

```
WRONG (causes 630ms+ latency, feels disconnected):
  heavy_bands → rolling average → AsymmetricFollower → Spring
               160ms              150ms               200ms = 630ms!

CORRECT (200ms total, feels immediate):
  heavy_bands → Spring ONLY
                200ms natural momentum
```

**ControlBus already applies 80ms rise / 15ms fall smoothing** to heavy_bands. Adding more smoothing layers creates latency that makes the visual feel disconnected from the music.

### 4.3 Complete Audio Integration Example

```cpp
void MyEffect::render(plugins::EffectContext& ctx) {
    float dt = ctx.getSafeDeltaSeconds();

    // --- Audio feature extraction ---
    float heavyEnergy = 0.0f;
    float rmsEnergy = 0.0f;
    float beatMod = 1.0f;
    bool onsetDetected = false;

    #if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        // Bass energy for speed modulation
        heavyEnergy = (ctx.audio.getHeavyBand(1) +
                       ctx.audio.getHeavyBand(2)) / 2.0f;

        // RMS for overall brightness
        rmsEnergy = ctx.audio.rms();

        // Onset for flash triggers
        onsetDetected = ctx.audio.hasOnsetEvent();

        // Beat-strength brightness modulation (0.4 floor, +60% on beat)
        beatMod = 0.4f + 0.6f * ctx.audio.beatStrength();
    }
    #endif

    // --- Smooth audio values ---
    float smoothSpeed = m_speedSpring.update(0.6f + 1.2f * heavyEnergy, dt);
    if (smoothSpeed > 2.0f) smoothSpeed = 2.0f;
    if (smoothSpeed < 0.3f) smoothSpeed = 0.3f;

    float smoothBrightness = m_brightnessFollower.update(rmsEnergy, dt);

    // --- Handle onset flash ---
    if (onsetDetected) {
        m_flashIntensity = 1.0f;  // Instant attack
    }
    m_flashIntensity *= powf(0.05f, dt);  // Exponential decay (~60ms half-life)

    // --- Fade previous frame ---
    uint8_t fadeAmount = (uint8_t)(20 + 40 * (1.0f - smoothBrightness));
    fadeToBlackBy(ctx.leds, ctx.ledCount, fadeAmount);

    // --- Phase advancement (frame-rate independent) ---
    float speedNorm = ctx.speed / 50.0f;
    m_phase = enhancement::advancePhase(m_phase, speedNorm, smoothSpeed, dt);

    // --- Render from centre out ---
    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        float normalizedDist = (float)dist / (float)HALF_LENGTH;

        // Compute colour from palette (beatMod pulses brightness on beat)
        uint8_t hue = ctx.gHue + (uint8_t)(normalizedDist * 40.0f);
        uint8_t bright = scale8((uint8_t)(smoothBrightness * beatMod * 255.0f), ctx.brightness);
        CRGB color = ctx.palette.getColor(hue, bright);

        // Add onset flash at centre
        if (dist < 15 && m_flashIntensity > 0.01f) {
            float flashBright = m_flashIntensity * expf(-dist * 0.2f);
            CRGB flash = ctx.palette.getColor(ctx.gHue, (uint8_t)(flashBright * 200));
            color.r = qadd8(color.r, flash.r);
            color.g = qadd8(color.g, flash.g);
            color.b = qadd8(color.b, flash.b);
        }

        SET_CENTER_PAIR(ctx, dist, color);
    }
}
```

### 4.4 Audio-Visual Coupling Best Practices

An audit of 349 effects found that the most expressive audio signals are severely underutilised. Most effects use `isOnBeat()` (boolean, single-frame pulse) or manual RMS smoothing, resulting in poor audio-visual coupling. The following signals are pre-smoothed, backend-agnostic, and ready to use.

#### `beatStrength()` — Default Brightness Modulation

`ctx.audio.beatStrength()` returns a float 0.0-1.0. It peaks to 1.0 on detected beats and decays exponentially between beats, producing a continuous envelope that tracks the rhythmic contour of the music.

**Use as the default brightness modulation for any audio-reactive effect:**

```cpp
float beatMod = 0.4f + 0.6f * ctx.audio.beatStrength();
bright = (uint8_t)(bright * beatMod);
```

- The 0.4 floor prevents complete blackout between beats.
- The 0.6 range gives +60% brightness on beats — visible but not strobing.
- `beatStrength()` is already smoothed by EsBeatClock (exponential decay). Do NOT add another smoothing layer.

**Prefer `beatStrength()` over `isOnBeat()`.** `isOnBeat()` is a single-frame boolean pulse — at 120 FPS that is 8.33ms of visibility, impossible to perceive. `beatStrength()` gives the same timing information with natural visual persistence.

**When `isOnBeat()` IS appropriate:** spawning particles, triggering one-shot state changes, or counting beats for pattern sequencing. Not for modulating brightness or colour.

#### `liveliness()` — Animation Speed Scalar

`ctx.audio.liveliness()` returns a float 0.0-1.0. It is a pre-computed composite of RMS energy and spectral flux, reflecting how "active" the music is at any moment.

**Use to scale animation speed, particle spawn rate, or pattern evolution rate:**

```cpp
float speed = baseSpeed + ctx.audio.liveliness() * speedRange;
```

`liveliness()` is already used by RendererActor for global auto-speed trim. Using it in individual effects reinforces this mapping — quiet passages slow down, energetic passages speed up, without manual RMS smoothing or flux windowing.

#### `overallSaliency()` — Accent Detection

`ctx.audio.overallSaliency()` returns a float 0.0-1.0. It is a weighted composite of four novelty dimensions: harmonic (chord changes), rhythmic (beat pattern changes), timbral (spectral texture changes), and dynamic (loudness envelope changes).

**Use for visual accents on musical surprises — moments where the music itself changes character:**

```cpp
if (ctx.audio.overallSaliency() > 0.6f) {
    // Trigger visual accent: colour shift, flash, spatial expansion
    float accentStrength = (ctx.audio.overallSaliency() - 0.6f) * 2.5f;  // 0-1 above threshold
    // Apply to hue offset, brightness boost, or spatial scale
}
```

For finer control, query individual novelty dimensions:
- `ctx.audio.harmonicSaliency()` — chord/key changes
- `ctx.audio.rhythmicSaliency()` — beat pattern shifts
- `ctx.audio.timbralSaliency()` — spectral character changes
- `ctx.audio.dynamicSaliency()` — loudness envelope changes

**Saliency is NOT a beat signal.** It fires on musical structure changes (verse-to-chorus transitions, breakdowns, instrument entries). Do not use it as a substitute for `beatStrength()`.

---

## Part 5: Additional Infrastructure (Already Built)

### 5.1 ColorEngine — Cross-Palette Blending

**Location:** `src/effects/enhancement/ColorEngine.h`

Singleton providing palette blending, temporal rotation, and Gaussian color diffusion:

```cpp
auto& colorEngine = enhancement::ColorEngine::getInstance();
colorEngine.enableCrossBlend(true);
colorEngine.setBlendPalettes(palette1, palette2);
colorEngine.setBlendFactors(200, 55);  // 80% pal1, 20% pal2

// Apply spatial blur to smooth harsh color boundaries
colorEngine.applyDiffusion(ctx.leds, ctx.ledCount);
```

### 5.2 MotionEngine — Phase Control and Particle Physics

**Location:** `src/effects/enhancement/MotionEngine.h`

- **PhaseController:** Strip phase offset and auto-rotation for dual-strip sync
- **MomentumEngine:** 32-particle physics with position, velocity, acceleration, drag, boundary modes
- **SpeedModulator:** Sine wave and exponential decay speed modulation

```cpp
auto& motion = enhancement::MotionEngine::getInstance();
motion.enable();
motion.getPhaseController().setStripPhaseOffset(90.0f);  // 90° offset between strips
motion.getMomentumEngine().addParticle(0.5f, 1.0f, 1.0f, CRGB::Red, BOUNDARY_BOUNCE);
```

### 5.3 Easing Curves — Smooth Transitions

**Location:** `src/effects/transitions/Easing.h`

15 easing curves available: Linear, Quad, Cubic, Elastic, Bounce, Back (in/out/in-out variants).

```cpp
float progress = /* 0.0 to 1.0 */;
float eased = lightwaveos::transitions::ease(progress, EasingCurve::OUT_CUBIC);
```

### 5.4 FastLEDOptim Utilities

**Location:** `src/effects/utils/FastLEDOptim.h`

- `fastled_sin16_normalized(angle)` — Returns -1.0 to 1.0
- `fastled_scale8(value, scale)` — Efficient brightness scaling
- `fastled_qadd8(a, b)` — Saturating addition
- `fastled_beatsin16(bpm, min, max)` — Tempo-synced oscillation
- `fastled_wrap_hue_safe(hue, offset, maxRange)` — No-rainbows hue clamping
- `fastled_center_sin16(pos, center, halfLen, freq, phase)` — Center-origin wave

---

## Part 6: Visual Design Principles

These are not optional aesthetic preferences — they're perceptual science.

### 6.1 Contrast and Negative Space

**Darkness is as important as light.** Concert lighting professionals use blackout periods to make subsequent illumination more impactful.

- Use `fadeToBlackBy()` to create dark regions between active elements
- Don't fill the entire strip at constant brightness — leave shadow zones
- Beat-reactive effects should have true black between beats, not a constant base glow
- Weber's Law: after dark adaptation, smaller brightness changes create larger perceived differences

### 6.2 Flicker Fusion and Anti-Aliasing

Human flicker fusion threshold: 35-60 Hz for steady light, but **up to 500 Hz when high-frequency spatial edges are present** (which LED strips have).

- At 120 FPS, each frame is 8.33ms. Brightness changes > 6% per frame may be visible as stepping.
- Spatial frequency must respect Nyquist: movement should stay under half-wavelength per frame to prevent wagon-wheel aliasing.
- Use `SubpixelRenderer` for ALL moving elements to prevent integer-position jumping.

### 6.3 Gamma-Aware Design

The ColorCorrectionEngine applies gamma 2.2 after your render. This means:

| Your PWM Value | Perceived Brightness |
|----------------|---------------------|
| 255 | 100% |
| 200 | ~58% |
| 128 | ~22% |
| 64 | ~5% |
| 32 | ~1.2% |

**Implication:** Subtle low-brightness effects may vanish after gamma. If you need visible detail in dark regions, use higher minimum brightness values (32-50 minimum for visibility).

### 6.4 Color Science for WS2812

- Pure RGB (255,255,255) appears **bluish-cool** on WS2812. Avoid for "white."
- Warm white approximation: (255, 240, 220) for 2700K feel
- Green phosphor shifted toward blue spectrum — greens appear cyan-ish
- Adjacent LEDs blend optically at viewing distance (3-5 LED smoothing zone)
- **Use palette colors, not manual RGB.** The palette system handles these hardware quirks.

---

## Part 7: Anti-Patterns — What Destroys Effects

### 7.1 Raw Audio Driving Pixels Directly

```cpp
// === CATASTROPHIC: Raw audio → visual parameter ===
uint8_t brightness = (uint8_t)(ctx.audio.controlBus.rms * 255);
ctx.leds[i] = CRGB(brightness, brightness, brightness);
// Result: Seizure-inducing flicker. No temporal coherence.
```

**Every audio value must pass through smoothing** (AsymmetricFollower for envelopes, Spring for motion, ExpDecay for everything else).

### 7.2 Tweaking Constants Without Understanding

Changing a smoothing ratio from `5%/95%` to `20%/80%` does nothing when the effect is missing:
- Trail persistence (fadeToBlackBy)
- Subpixel rendering
- Asymmetric attack/release envelopes
- Proper audio integration

**Parameter tweaks are the LAST step, not the first.** Fix the architecture, then tune.

### 7.3 Frame-Count Based Timing

```cpp
// === WRONG: Frame-dependent ===
if (ctx.frameNumber % 60 == 0) spawnParticle();  // 0.5s at 120 FPS, 1s at 60 FPS
m_particlePos += 2;  // 2 pixels/frame = speed depends on FPS

// === CORRECT: Time-based ===
m_spawnTimer += dt;
if (m_spawnTimer > 0.5f) { spawnParticle(); m_spawnTimer = 0.0f; }
m_particlePos += velocity * dt;  // Consistent speed regardless of FPS
```

### 7.4 Dynamic Allocation in Render Loop

```cpp
// === FORBIDDEN (causes heap fragmentation on ESP32) ===
void render() {
    CRGB* buffer = new CRGB[160];
    // ...
    delete[] buffer;
}

// === CORRECT: Static allocation ===
void render() {
    static CRGB buffer[160];
    // ...
}
```

### 7.5 Overwriting Instead of Blending

```cpp
// === WRONG: Overwrites destroy layered effects ===
ctx.leds[i] = newColor;

// === RIGHT: Additive blending preserves layers ===
ctx.leds[i] += scaledColor;
// Or with explicit control:
ctx.leds[i].r = qadd8(ctx.leds[i].r, scale8(newColor.r, amount));
```

### 7.6 The `>> 8` Coordinate Collapse

```cpp
// === CATASTROPHIC: Destroys spatial resolution ===
uint8_t noise = inoise8(x >> 8, y >> 8);
// At 80 LEDs across, (0..79) >> 8 = 0 for ALL positions. Everything is identical.

// === CORRECT: Full resolution ===
uint8_t noise = inoise8(x * noiseScale, y * noiseScale);
```

### 7.7 Ignoring the Behavioral Gates

Three systems silently modify post-processing for specific effects (see CONSTRAINTS.md § Effect Behavioral Gates):

1. **Color Correction Skip** — Physics effects bypass the entire processBuffer() pipeline
2. **Tone Mapping** — Additive-blending effects need this to prevent white clipping
3. **Silence Gate** — Audio-reactive effects fade to black during silence

**Before "fixing" an unresponsive effect:** Check if it's intentionally on a skip list. Removing a physics effect from the skip list will produce garbled output.

---

## Part 8: Effect Header Template

Every new IEffect implementation should start from this structure:

```cpp
// === Header (.h) ===
#pragma once
#include "../plugins/api/IEffect.h"
#include "../effects/enhancement/SmoothingEngine.h"

namespace lightwaveos {
namespace effects {
namespace ieffect {

class MyNewEffect : public plugins::IEffect {
public:
    MyNewEffect();
    bool init(plugins::EffectContext& ctx) override;
    void render(plugins::EffectContext& ctx) override;
    void cleanup() override;
    const plugins::EffectMetadata& getMetadata() const override;

private:
    // Phase / time accumulators (frame-rate independent)
    float m_phase = 0.0f;

    // Smoothing primitives (USE THESE, don't roll your own)
    enhancement::AsymmetricFollower m_audioEnvelope{0.0f, 0.05f, 0.30f};
    enhancement::Spring m_speedSpring;
    enhancement::ExpDecay m_smoothBrightness{0.0f, 10.0f};

    // Flash / onset state
    float m_flashIntensity = 0.0f;
};

}}} // namespace
```

```cpp
// === Implementation (.cpp) ===
#include "MyNewEffect.h"
#include "../CoreEffects.h"
#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace ieffect {

MyNewEffect::MyNewEffect() : m_phase(0.0f) {}

bool MyNewEffect::init(plugins::EffectContext& ctx) {
    (void)ctx;
    m_phase = 0.0f;
    m_speedSpring.init(50.0f, 1.0f);
    m_speedSpring.reset(1.0f);
    m_audioEnvelope.reset(0.0f);
    m_smoothBrightness.reset(0.0f);
    m_flashIntensity = 0.0f;
    return true;
}

void MyNewEffect::render(plugins::EffectContext& ctx) {
    float dt = ctx.getSafeDeltaSeconds();

    // === 1. AUDIO FEATURE EXTRACTION ===
    float heavyEnergy = 0.0f;
    float rmsEnergy = 0.0f;
    float beatMod = 1.0f;
    bool onset = false;

    #if FEATURE_AUDIO_SYNC
    if (ctx.audio.available) {
        heavyEnergy = (ctx.audio.getHeavyBand(1) +
                       ctx.audio.getHeavyBand(2)) / 2.0f;
        rmsEnergy = ctx.audio.rms();
        onset = ctx.audio.hasOnsetEvent();

        // Beat-strength brightness modulation (0.4 floor, +60% on beat)
        beatMod = 0.4f + 0.6f * ctx.audio.beatStrength();
    }
    #endif

    // === 2. SMOOTH AUDIO VALUES ===
    float smoothSpeed = m_speedSpring.update(0.6f + 1.2f * heavyEnergy, dt);
    smoothSpeed = fminf(fmaxf(smoothSpeed, 0.3f), 2.0f);

    float brightness = m_audioEnvelope.update(rmsEnergy, dt);

    if (onset) m_flashIntensity = 1.0f;
    m_flashIntensity *= powf(0.05f, dt);

    // === 3. FADE PREVIOUS FRAME (TRAIL PERSISTENCE) ===
    uint8_t fadeAmount = (uint8_t)(20 + 40 * (1.0f - brightness));
    fadeToBlackBy(ctx.leds, ctx.ledCount, fadeAmount);

    // === 4. ADVANCE PHASE (FRAME-RATE INDEPENDENT) ===
    float speedNorm = ctx.speed / 50.0f;
    m_phase = enhancement::advancePhase(m_phase, speedNorm, smoothSpeed, dt);

    // === 5. RENDER FROM CENTER OUT ===
    for (uint16_t dist = 0; dist < HALF_LENGTH; dist++) {
        float normalizedDist = (float)dist / (float)HALF_LENGTH;

        // Your effect math here...
        uint8_t hue = ctx.gHue + (uint8_t)(normalizedDist * 30.0f);
        uint8_t bright = scale8((uint8_t)(brightness * beatMod * 255.0f), ctx.brightness);
        CRGB color = ctx.palette.getColor(hue, bright);

        // Centre flash on onset
        if (dist < 12 && m_flashIntensity > 0.01f) {
            float flash = m_flashIntensity * expf(-dist * 0.2f);
            CRGB flashColor = ctx.palette.getColor(ctx.gHue, (uint8_t)(flash * 200));
            color += flashColor;
        }

        SET_CENTER_PAIR(ctx, dist, color);
    }
}

void MyNewEffect::cleanup() {}

const plugins::EffectMetadata& MyNewEffect::getMetadata() const {
    static plugins::EffectMetadata meta{
        "My New Effect",
        "Description here",
        plugins::EffectCategory::UNCATEGORIZED,
        1  // complexity
    };
    return meta;
}

}}} // namespace
```

---

## Part 9: Checklist — Before Shipping Any Effect Change

Before any effect modification is considered complete:

- [ ] **Does it use `ctx.getSafeDeltaSeconds()` for ALL time-varying values?**
- [ ] **Does it call `fadeToBlackBy()` before rendering?** (unless intentionally clearing)
- [ ] **Does it render from center outward?** (not linear left-to-right)
- [ ] **Does it use `ctx.palette.getColor()` for color?** (not hardcoded RGB)
- [ ] **Does every audio value pass through smoothing?** (AsymmetricFollower, Spring, or ExpDecay)
- [ ] **Does it use `beatStrength()` for brightness modulation?** (not `isOnBeat()` — see 4.4)
- [ ] **Does it use `scale8()` for brightness?** (not float multiplication)
- [ ] **Does it use `qadd8()` for additive blending?** (not raw assignment for multi-layer)
- [ ] **Is motion anti-aliased via SubpixelRenderer?** (for moving elements)
- [ ] **Does it respect the 2ms render budget?** (no dynamic allocation, no heavy computation)
- [ ] **Does it build clean on both `esp32dev_audio_esv11` and `esp32dev_audio_esv11_k1v2`?**
- [ ] **Has it been visually verified on hardware?** (not just "it compiles")

---

## Part 10: AR Control-Liveness and Anti-Chaos Bounds (2026-03-05)

This section is mandatory for the 5-layer AR pack (`0x1Cxx`) and any future ambient-to-audio conversions.

### 10.1 Control-liveness is non-negotiable

If an effect exposes controls but does not materially react to them, it is broken even if it compiles.

Every AR effect render path must include all of the following:

- `lowrisk_ar::updateSignals(...)` — attack/release envelope + spectral/rhythmic conditioning.
- `lowrisk_ar::buildModulation(...)` — motion/colour/beat modulation profile.
- `lowrisk_ar::applyBedImpactMemoryMix(...)` — ambient/reactive blending (`audio_mix`, `beat_gain`, `memory_gain`, `motion_depth`).
- `m_ar.tonalHue = mod.baseHue` — ensures `colour_anchor_mix` actually drives hue anchoring.
- `mod.motionRate` in at least one phase/integration term — ensures motion control is live.
- At least one spectral term (`sig.bass/mid/treble/flux/harmonic/rhythmic`) in visible structure or colour.

### 10.2 Beat-forward retune defaults (without chaos)

Use these as safe defaults for expressive but controlled behaviour:

| Control | Recommended default | Practical safe range |
|---|---:|---:|
| `audio_mix` | `0.35` | `0.20-0.65` |
| `beat_gain` | `0.80` | `0.60-1.20` |
| `motion_depth` | `0.85` | `0.55-1.00` |
| `motion_rate` | `1.00` | `0.80-1.35` |
| `colour_anchor_mix` | `0.65` | `0.50-0.85` |

### 10.3 Anti-chaos bounds

- Cap additive impact terms to prevent strobe collapse (`impactAdd` typically `<= 0.40`).
- Keep memory tails bounded and slowly decaying (`0.70-0.95 s` decay windows).
- Avoid unbounded per-pixel exponentials/trig loops in heavy effects.
- For hotspot effects, reduce harmonic/sample budgets adaptively under high speed/load.
- Use `cinema::QualityPreset::LIGHTWEIGHT` for known heavy AR effects when frame budget is at risk.

### 10.4 Validation gate for AR changes

An AR change is not complete until all are true:

- Parameter changes (`audio_mix`, `beat_gain`, `motion_depth`, `colour_anchor_mix`) produce immediate visible differences.
- Render remains within budget on K1 target (`~2 ms` per effect code path).
- No new stutter/tearing under AP + active WS client load.

---

## Part 11: File Reference

| File | Purpose |
|------|---------|
| `src/effects/enhancement/SmoothingEngine.h` | ExpDecay, Spring, AsymmetricFollower, SubpixelRenderer |
| `src/effects/enhancement/ColorEngine.h` | Cross-palette blending, temporal rotation, diffusion |
| `src/effects/enhancement/MotionEngine.h` | Phase control, particle physics, speed modulation |
| `src/effects/enhancement/ColorCorrectionEngine.h` | Gamma, auto-exposure, V-clamp, guardrails |
| `src/effects/transitions/Easing.h` | 15 easing curves for smooth transitions |
| `src/effects/utils/FastLEDOptim.h` | sin16, scale8, qadd8, beatsin, hue wrapping |
| `src/effects/CoreEffects.h` | CENTER_LEFT/RIGHT, HALF_LENGTH, SET_CENTER_PAIR macro |
| `src/plugins/api/EffectContext.h` | ctx.palette, ctx.audio, ctx.getSafeDeltaSeconds() |
| `docs/CONSTRAINTS.md` | Hard limits, timing budgets, behavioral gates |
| `docs/EFFECTS_BEHAVIORAL_REFERENCE.md` | Per-effect tracking: palette, gates, flags |
| `docs/audio-visual/IMPLEMENTATION_PATTERNS.md` | Copy-paste ready proven patterns |

---

## Revision History

| Date | Change | Author |
|------|--------|--------|
| 2026-03-02 | Initial creation from episodic memory research (7200+ observations, 30+ source files) | Claude / Captain directive |
| 2026-03-05 | Added AR control-liveness contract, beat-forward anti-chaos bounds, and lightweight post-processing guidance | Codex |
| 2026-03-08 | Added audio-visual coupling best practices (beatStrength, liveliness, overallSaliency), signals quick-reference table, beatStrength in effect template | Claude |
