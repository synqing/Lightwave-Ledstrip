# Professional LED Visual Persistence Patterns

**Research Date:** March 17, 2026
**Focus:** Implementation patterns for visual persistence, temporal blending, and audio-reactive animation in professional LED systems.

---

## 1. Core Persistence Mechanisms

### 1.1 Exponential Decay / Temporal Smoothing (FOUNDATIONAL)

**Formula:**
```
output_value = current_input * α + previous_output * (1 - α)
```

Where:
- `α` (alpha) = smoothing factor, range [0, 1]
- Small α (e.g., 0.1) = slow decay, long memory, ~10-frame half-life
- Large α (e.g., 0.9) = fast response, short memory, ~1-frame half-life

**Time-Constant Conversion:**
For a desired decay half-life of `t_half` frames at FPS `f`:
```
α = 1 - exp(-0.693 / (t_half))  // 0.693 = ln(2)
```

**Audio Reactive Context:**
The Lightwave project pattern `dt-decay` implements frame-rate-compensated decay:
```cpp
var = effects::chroma::dtDecay(var, rate, rawDt)
// Not: var *= rate  (frame-rate dependent)
// Instead: multiply by rate^(dt/16.667ms) for 60fps invariance
```

**Why This Matters for K1:**
- Decouples visual persistence from frame rate (critical when frame budget varies 60→120fps)
- Ensures audio-reactive elements decay at consistent speed regardless of load
- More stable than simple linear fade (`fadeToBlackBy`)

---

## 2. System-Specific Implementation Patterns

### 2.1 TouchDesigner — Feedback TOP (Professional VJ Standard)

**Architecture:**
```
Input → Composite/Blend → Feedback TOP → Level TOP (opacity control) ↻
                                      ↓
                                    Output
```

**Key Parameters:**
- **Level TOP Opacity:** Primary decay control
  - `opacity = 0.99` → very slow fade (~1% loss per frame)
  - `opacity = 0.90` → moderate trail (~10% loss per frame)
  - `opacity = 0.80` → fast decay (~20% loss per frame)

**Temporal Accumulation:**
Frame N receives opacity contribution:
```
frame_opacity = 0.9 * 0.9 * 0.9... (N multiplications)
              = 0.9^N
```

This is **geometric decay**, not exponential but practically equivalent for N < 30 frames.

**Composite TOP Blend Mode:** Defines how each layer stacks—Alpha blend (standard), Multiply, Add, etc.

**Actionable Pattern for K1:**
Replace simple `fadeToBlackBy()` with opacity-controlled blend:
```cpp
// Instead of:
fadeToBlackBy(leds, fade_amount);

// Use:
blend(current_frame, previous_frame, blend_factor);
// where blend_factor = 0.9 for 10% loss per frame
```

**Source:** [Level TOP documentation](https://docs.derivative.ca/Level_TOP), [Understanding Feedback Loops in TouchDesigner](https://interactiveimmersive.io/blog/touchdesigner-lessons/understanding-feedback-loops-in-touchdesigner/)

---

### 2.2 WLED — 2D Trail Implementation

**Pattern in Matrix/Trail Effects:**
```cpp
// Fade control via custom slider mapping
uint8_t fade = map(SEGMENT.custom1, 0, 255, 50, 250);  // 50-250 units/frame
fadeToBlackBy(leds, SEGLEN, fade);
```

**Key Insight:**
- `fadeToBlackBy(leds, count, amount)` — dims ALL pixels in array by `amount` (0-255)
- Parameter is *not* opacity (0-1) but raw fade units (0-255)
- Larger values = faster fade-to-black
- **Applied every frame** to the entire frame buffer

**Multi-Layer Trail Approach:**
1. Render new content (e.g., pixel movement down a line)
2. `fadeToBlackBy()` all previous content by fade amount
3. Composite new frame on top
4. Output to LEDs

**Why Simpler Than Feedback Loop:**
- No separate history buffer needed
- Fade integrated into single-pass rendering
- Works well for linear trails and "falling" patterns

**Limitation:**
- `fadeToBlackBy` is *multiplicative* on RGB: `new_rgb = old_rgb * (1 - fade/255)`
- For `fade=128`: multiplies by ~0.5 per frame (fast)
- Cannot distinguish "colored decay" (trail fades to specific hue) — always fades to black

**WLED Source:** [FX.cpp FX_fcn.cpp](https://github.com/Aircoookie/WLED/blob/main/wled00/FX.cpp)

---

### 2.3 FastLED — Community Patterns

**Core Primitives:**
```cpp
// Multiplicative fade (frame-dependent)
fadeToBlackBy(leds, NUM_LEDS, fade_amount);  // fade_amount: 0-255

// Color blending (per-pixel)
leds[i] = blend(color_a, color_b, blend_factor);  // blend_factor: 0-255 (0=A, 255=B)

// Logarithmic scale inversion
uint8_t brightness_scaled = nscale8(leds[i].r, scale_factor);  // inverse of fadeToBlackBy
```

**Fire2012 Case Study — Spatial + Temporal Persistence:**
- Maintains heat array across frames
- Each frame: cool cells → diffuse heat sideways → add sparks → render
- Heat naturally decays via `heat[i] -= random(0, COOLING)` per frame
- Diffusion: `heat[i] = (heat[i-1] + heat[i] + heat[i+1]) / 3`
- **Result:** Organic persistence without explicit feedback loop

**Why Fire2012 Works So Well:**
1. **Physics-based:** Heat dissipates naturally
2. **Frame-independent:** Cooling rate is per-update, not per-frame
3. **Spatial coherence:** Diffusion prevents single-pixel noise from exploding
4. **Easy to tune:** Two parameters (COOLING, SPARKING) control feel

**FastLED Sources:** [Fire2012 blog post](https://blog.kriegsman.org/2014/04/04/fire2012-an-open-source-fire-simulation-for-arduino-and-leds/), [FastLED Examples](https://github.com/FastLED/FastLED/tree/master/examples/Fire2012)

---

### 2.4 Resolume Arena — Professional VJ Feedback

**Architecture:**
```
Feedback Source (composition buffer) → Layer with opacity → Composite
↑ (fed back into next frame)
```

**Key Parameters:**
- **Feedback Opacity:** Controls retention in next frame
  - 50-90% opacity typical for trails
  - Lower = faster decay, higher = longer memory

**Blend Modes Interaction:**
- Alpha blend: `output = clip * opacity + background * (1 - opacity)`
- Multiply, Add, Screen blend modes *after* opacity shift the decay characteristic
- Example: Multiply mode + 70% opacity = darker trails that fade faster perceptually

**Advanced: Wire Patches for Feedback Manipulation**
- Can apply Transform (scale, rotate) to feedback before re-compositing
- Creates "spiraling" or "shrinking" trails (spatial + temporal)
- Hue shift on feedback creates color shifts with age

**Practical Pattern:**
```
For "fading echo" effect:
1. Place Feedback source at 70% opacity as bottom layer
2. Place current content at 100% opacity as top layer
3. Each previous frame shows at reduced opacity (0.7^n)
```

**Resolume Sources:** [Blend Modes](https://resolume.com/support/en/blend-modes), [Feedback Source discussions](https://resolume.com/forum/viewtopic.php?t=22481)

---

## 3. Audio-Reactive Integration

### 3.1 Beat-Reset Persistence Pattern

**Concept:** Persistence resets or "flashes" on beat/onset, creates visual impact.

**Implementation:**
```cpp
// Pseudocode
if (controlBus.beat || controlBus.onset) {
    persistence_factor = 0.95;  // Start fresh with slow decay
    reset_decay_timer();
} else {
    // Normal temporal decay
    persistence_factor = lerp(0.95, 0.50, time_since_beat / sustain_time);
}

blend(current, previous, persistence_factor);
```

**Why This Works:**
- Beat detection = energy spike in audio
- Visual reset makes rhythm visually obvious
- Decay afterward creates "settling" effect that mirrors audio envelope

### 3.2 Energy-Coupled Decay Rate

**Pattern:** Faster decay when audio is quiet, slower when loud.

```cpp
// Map RMS (energy) to decay rate
float rms = controlBus.rms;  // 0-1
float decay_rate = 0.50 + 0.40 * (1.0 - rms);  // 0.5-0.9 range
// High RMS (loud) → decay_rate = 0.5 (slow, long trails)
// Low RMS (quiet) → decay_rate = 0.9 (fast, short trails)

blend(current, previous, decay_rate);
```

**Perceptual Result:**
- Loud sections have "heavy" visual momentum
- Quiet sections are "responsive" and clean
- Audio energy visually persists proportionally

### 3.3 Frequency-Specific Trails

**Pattern:** Different frequency bands have different persistence.

```cpp
// Bass has longest trails (heavy, slow)
float bass_decay = 0.85 + 0.10 * (1.0 - controlBus.bands[0]);

// Treble has short trails (crisp, responsive)
float treble_decay = 0.60 + 0.30 * (1.0 - (controlBus.bands[6] + controlBus.bands[7]) / 2.0);

// Render each band with its own persistence
render_bass_visuals(bass_decay);
render_treble_visuals(treble_decay);
```

**Result:** Multi-layered visual response that mirrors audio complexity.

---

## 4. Preventing Feedback Divergence

### 4.1 The Divergence Problem

**What Happens:**
```
Frame 0: rgb(100, 100, 100)
Frame 1: old + add_energy → rgb(150, 150, 150)
Frame 2: old + add_energy → rgb(200, 200, 200)
... → rgb(255, 255, 255) white-out or clipping
```

**Root Cause:** Feedback gain ≥ 1.0. Each iteration amplifies instead of decaying.

### 4.2 Clamping / Saturation Control

**Solution 1: Explicit Clamping**
```cpp
float new_value = blend(current, previous, 0.8);
new_value = min(new_value, 1.0);  // Never exceed max
```

**Solution 2: Attenuate Before Adding**
```cpp
// Only add if there's "room"
if (feedback_level < max_brightness * 0.9) {
    output += energy_input * energy_scale;
}
```

**Solution 3: Normalize by Layer Count**
```cpp
// If stacking N layers, divide contribution by N
output = (new_content + N-1 * old_content) / N;
```

**TouchDesigner Approach:** Composite TOP's opacity inherently prevents divergence—opacity < 1.0 ensures decay.

**Resolume Approach:** Feedback source is explicitly attenuated by layer opacity before re-use.

---

## 5. Frame-Rate Invariance

### 5.1 The Problem

**Simple Decay:**
```cpp
leds[i] *= 0.9;  // 10% loss per frame
// At 60fps: 10% loss per 16.67ms ✓
// At 120fps: 10% loss per 8.33ms ✗ (too much decay!)
```

**Solution: Delta-Time Compensated Decay**

```cpp
// Compute decay per-millisecond
float decay_per_ms = 0.06;  // 6% per millisecond
float decay_this_frame = pow(1.0 - decay_per_ms, dt_ms);
// At 60fps (16.67ms): decay = 0.9 ✓
// At 120fps (8.33ms): decay = 0.95 ✓

new_value = current * decay_this_frame + previous * (1.0 - decay_this_frame);
```

**General Formula:**
```cpp
// Half-life of N frames at 60fps
float alpha_60fps = 0.5;  // decays to 50% in N frames
float dt_ms = (dt / 16.667f);  // normalize by 60fps frame time
float alpha_actual = pow(alpha_60fps, dt_ms);
```

**LightwaveOS Implementation:** The `dtDecay()` helper function applies this pattern.

---

## 6. Implementation Checklist for K1 LightwaveOS

### 6.1 Minimal Persistence Effect (Baseline)

```cpp
class TrailEffect : public EffectBase {
    std::array<CRGB, 320> history{};

    void render(RenderContext& ctx) override {
        // Fade history
        const float decay = 0.85f;  // 15% loss per frame
        for (int i = 0; i < NLEDS; i++) {
            history[i] = blend(history[i], CRGB::Black, uint8_t(255 * (1.0f - decay)));
        }

        // Render new content
        // (e.g., moving dot, audio-reactive bars, etc.)

        // Composite onto output
        for (int i = 0; i < NLEDS; i++) {
            ctx.leds[i] = history[i];  // Show accumulated history
        }

        // Update history for next frame
        memcpy(history.data(), ctx.leds, sizeof(ctx.leds));
    }
};
```

### 6.2 Audio-Coupled Trail

```cpp
void render(RenderContext& ctx) override {
    // Energy-scaled decay
    float rms = ctx.controlBus.rms;
    float decay = 0.50f + 0.40f * (1.0f - rms);  // 0.5-0.9

    // Apply decay to history
    for (int i = 0; i < NLEDS; i++) {
        history[i].nscale8(uint8_t(255 * decay));
    }

    // Render content (e.g., beat-triggered flash)
    if (ctx.controlBus.beat) {
        // Bright pulse
        ctx.leds[79] = CRGB::White;
        ctx.leds[80] = CRGB::White;
    }

    // Accumulate into history
    for (int i = 0; i < NLEDS; i++) {
        history[i] = blend(history[i], ctx.leds[i], 128);
        ctx.leds[i] = history[i];
    }
}
```

### 6.3 Beat-Reset Pattern

```cpp
void render(RenderContext& ctx) override {
    static float decay_rate = 0.85f;

    // Reset on beat
    if (ctx.controlBus.beat) {
        decay_rate = 0.95f;  // Start with slow decay
    } else {
        // Gradually increase decay (fade out)
        decay_rate = lerp(0.95f, 0.50f, beat_age / 500.0f);
    }

    // Apply decay and render
    for (int i = 0; i < NLEDS; i++) {
        history[i].nscale8(uint8_t(255 * decay_rate));
    }

    // ... render content, accumulate, output
}
```

### 6.4 Delta-Time Compensated Decay

```cpp
void render(RenderContext& ctx) override {
    // Half-life = 10 frames at 60fps (0.167 seconds)
    float target_decay_60fps = 0.933f;  // 2^(-dt/10frames)

    // Compensate for actual frame time
    float dt_normalized = ctx.dt / 16.667f;  // normalize by 60fps frame
    float decay = pow(target_decay_60fps, dt_normalized);

    // Apply
    for (int i = 0; i < NLEDS; i++) {
        history[i].nscale8(uint8_t(255 * decay));
    }
}
```

### 6.5 Multi-Zone Persistence (with Bounds Checking)

```cpp
void render(RenderContext& ctx) override {
    const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;  // CRITICAL

    // Each zone has its own history buffer
    float decay = 0.85f;
    for (int i = 0; i < zone_led_counts[z]; i++) {
        zone_history[z][i].nscale8(uint8_t(255 * decay));
    }

    // Render and accumulate
    // ...
}
```

---

## 7. Performance Considerations for 2.0ms Budget

### 7.1 Static Buffers (REQUIRED)

**Do NOT allocate in render():**
```cpp
// ❌ WRONG (heap alloc in render = crash risk + jitter)
void render(RenderContext& ctx) {
    std::vector<CRGB> history(NLEDS);  // NO!
}

// ✓ RIGHT (static member)
class TrailEffect : public EffectBase {
    std::array<CRGB, 320> history{};  // Pre-allocated

    void render(RenderContext& ctx) override {
        // Use history directly
    }
};
```

### 7.2 Blend Operations are Fast

FastLED `blend()` is ~0.5µs per pixel on ESP32 (SSE optimized).
320 pixels × 0.5µs = 160µs = well under 2ms budget.

### 7.3 Avoid in-loop Multiplication

**Slow:**
```cpp
for (int i = 0; i < NLEDS; i++) {
    history[i].r = history[i].r * decay;  // float mult per pixel
}
```

**Fast:**
```cpp
uint8_t decay_u8 = uint8_t(255 * decay);
for (int i = 0; i < NLEDS; i++) {
    history[i].nscale8(decay_u8);  // Single lookup + shift
}
```

---

## 8. Comparison Table: When to Use Which Pattern

| Pattern | Best For | Complexity | Performance | Audio-Coupled |
|---------|----------|-----------|-------------|---------------|
| **fadeToBlackBy** | Simple trails, 1D effects | ⭐ | ⭐ Fast | Moderate |
| **Exponential decay** | Smooth motion blur, general persistence | ⭐⭐ | ⭐⭐ Good | Excellent |
| **Feedback loop** | Complex spatial transforms, spirals | ⭐⭐⭐ | ⭐⭐ Good | Excellent |
| **Physics-based** (Fire2012 style) | Organic motion, diffusion | ⭐⭐⭐ | ⭐ Fair | Good |
| **Beat-reset** | Rhythmic visual impact | ⭐⭐ | ⭐⭐ Good | Critical |

---

## 9. Open Questions for K1 Implementation

1. **Zone-Specific Persistence:** Should global render and zone renders share history buffers or have separate decay rates?
2. **Composite Rendering:** Does K1 render per-zone first, then composite? Or render zones into a shared buffer?
3. **Audio Update Frequency:** Does ControlBus update every LED frame or at a slower audio rate? (Critical for beat-reset timing)
4. **Memory Constraints:** Is 320 pixels × 3 bytes (RGB) × N history frames within budget?
5. **Cross-Effect Persistence:** Should persistence carry across effect changes, or reset on transition?

---

## 10. Resources

### TouchDesigner
- [Feedback TOP Documentation](https://docs.derivative.ca/Feedback_TOP)
- [Level TOP Documentation](https://docs.derivative.ca/Level_TOP)
- [The Feedback TOP Tutorial](https://matthewragan.com/2013/06/16/the-feedback-top-touchdesigner/)
- [Understanding Feedback Loops](https://interactiveimmersive.io/blog/touchdesigner-lessons/understanding-feedback-loops-in-touchdesigner/)

### WLED
- [WLED 2D Effects Documentation](https://mm.kno.wled.ge/WLEDSR/2D-Support/)
- [FX.cpp Source Code](https://github.com/Aircoookie/WLED/blob/main/wled00/FX.cpp)

### FastLED
- [fadeToBlackBy Documentation](https://fastled.io/docs/d1/dfb/colorutils_8h_a399e4e094995b8e97420b89a2dd6548b.html)
- [Fire2012 Blog Post](https://blog.kriegsman.org/2014/04/04/fire2012-an-open-source-fire-simulation-for-arduino-and-leds/)

### Resolume Arena
- [Blend Modes Guide](https://resolume.com/support/en/blend-modes)
- [Feedback Source Discussion](https://resolume.com/forum/viewtopic.php?t=22481)

### Academic/General
- [Exponential Smoothing — NIST Handbook](https://www.itl.nist.gov/div898/handbook/pmc/section4/pmc431.htm)
- [Motion Blur and Frame Blending](https://thegreatswamp.xyz/posts/056_motionblur/)
- [Exponential Smoothing Blog](https://lisyarus.github.io/blog/posts/exponential-smoothing.html)

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-17 | claude:research | Created comprehensive LED persistence patterns research covering TouchDesigner, WLED, FastLED, Resolume, and audio-reactive integration. |
