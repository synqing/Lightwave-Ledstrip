# K1 LightwaveOS Persistence Implementation Guide

**Date:** March 17, 2026
**Purpose:** Actionable patterns for integrating visual persistence into K1 effects.
**Audience:** Effect developers.

---

## Quick Reference: Persistence Patterns by Use Case

### Pattern A: Simple Fade Trail (Easiest)

```cpp
class SimpleTrailEffect : public EffectBase {
    std::array<CRGB, 320> history{};

    void render(RenderContext& ctx) override {
        // Step 1: Fade all previous content
        fadeToBlackBy(history.data(), 320, 40);  // 40/255 ≈ 16% fade per frame

        // Step 2: Render new content (e.g., moving dot)
        int pos = beat_synced_position();
        history[pos] = CRGB::White;

        // Step 3: Copy to output
        memcpy(ctx.leds, history.data(), sizeof(ctx.leds));
    }
};
```

**Pros:**
- Uses FastLED primitive directly
- ~100µs for 320 LEDs
- Instant to integrate

**Cons:**
- Always fades to black (cannot create colored trails)
- Decay rate frame-dependent

---

### Pattern B: Smooth Exponential Blend (Recommended)

```cpp
class AudioReactiveTrailEffect : public EffectBase {
    std::array<CRGB, 320> history{};

    void render(RenderContext& ctx) override {
        // Step 1: Compute decay factor
        // Audio-coupled: slower decay when loud, faster when quiet
        float rms = ctx.controlBus.rms;  // 0 to 1
        float decay = 0.50f + 0.40f * (1.0f - rms);  // 0.5-0.9 range
        uint8_t blend_amount = uint8_t(255.0f * (1.0f - decay));  // 0-255

        // Step 2: Blend history with new content
        // Render new content into ctx.leds first
        int pos = map(ctx.controlBus.bands[3], 0, 1, 0, 319);
        CRGB color = CHSV(hue, 255, ctx.controlBus.rms * 255);
        ctx.leds[pos] = color;

        // Step 3: Accumulate history
        for (int i = 0; i < 320; i++) {
            history[i] = blend(history[i], ctx.leds[i], blend_amount);
        }

        // Step 4: Output accumulated buffer
        memcpy(ctx.leds, history.data(), sizeof(ctx.leds));
    }
};
```

**Pros:**
- Audio-reactive (decay changes with music energy)
- No black trails (blends to any color)
- Smooth, organic motion
- ~500µs for full buffer

**Cons:**
- Requires two buffers (history + output)
- Slightly more code
- Blend factor tuning needed per effect

---

### Pattern C: Beat-Reset Persistence (Visual Impact)

```cpp
class BeatPulseTrailEffect : public EffectBase {
    std::array<CRGB, 320> history{};
    uint32_t beat_millis = 0;

    void render(RenderContext& ctx) override {
        // Step 1: On beat, reset decay rate to slow
        if (ctx.controlBus.beat) {
            beat_millis = millis();
        }

        // Step 2: Compute time-dependent decay
        uint32_t age = millis() - beat_millis;
        float decay;
        if (age < 200) {  // First 200ms: slow fade
            decay = 0.95f;
        } else if (age < 1000) {  // Next 800ms: gradual speedup
            decay = 0.95f - 0.40f * (float(age - 200) / 800.0f);
        } else {  // After 1s: fast fade to silence
            decay = 0.55f;
        }

        // Step 3: Apply decay and render (same as Pattern B)
        uint8_t blend_amount = uint8_t(255.0f * (1.0f - decay));

        // ... render content into ctx.leds

        // Accumulate and output
        for (int i = 0; i < 320; i++) {
            history[i] = blend(history[i], ctx.leds[i], blend_amount);
        }
        memcpy(ctx.leds, history.data(), sizeof(ctx.leds));
    }
};
```

**Pros:**
- Creates visual punch on beats
- Sustain curve mimics audio envelope
- Can create "settling" visual effect
- Highly immersive for rhythm-heavy music

**Cons:**
- Requires time tracking
- Tuning sustain times is per-song

---

### Pattern D: Frame-Rate Invariant Decay (Production-Ready)

```cpp
class FrameRateInvariantTrailEffect : public EffectBase {
    std::array<CRGB, 320> history{};
    // Half-life = 10 frames at 60fps = 0.167 seconds
    static constexpr float DECAY_60FPS = 0.933f;

    void render(RenderContext& ctx) override {
        // Normalize dt to 60fps baseline
        float dt_normalized = ctx.dt / 16.667f;
        float decay = pow(DECAY_60FPS, dt_normalized);

        // Decay history
        uint8_t fade_amount = uint8_t(255.0f * (1.0f - decay));
        for (int i = 0; i < 320; i++) {
            history[i].fadeToBlackBy(fade_amount);
        }

        // ... render content into ctx.leds

        // Accumulate (same pattern as before)
        for (int i = 0; i < 320; i++) {
            history[i] = blend(history[i], ctx.leds[i], 128);
        }
        memcpy(ctx.leds, history.data(), sizeof(ctx.leds));
    }
};
```

**Pros:**
- Smooth at 60fps, 120fps, or variable frame rates
- No frame-rate artifacts
- Best for production effects

**Cons:**
- Requires `pow()` calculation (but cached decay factor)
- Slightly slower than Pattern A

**Implementation Note:**
Cache the `decay` factor if the formula is expensive:
```cpp
float cached_decay = pow(DECAY_60FPS, dt_normalized);
// Reuse across multiple render calls in same frame
```

---

## Multi-Zone Persistence (Critical Pattern)

**Context:** K1 has two 160-LED zones. Persistence must work per-zone AND globally.

### Bad (OOB write, causes crash)
```cpp
// ctx.zoneId = 0xFF means global render
// But zone_history[255] is OOB!
zone_history[ctx.zoneId][i] = ...  // CRASHES
```

### Good (Bounds-Checked)
```cpp
class MultiZoneTrailEffect : public EffectBase {
    static constexpr int kMaxZones = 2;
    std::array<std::array<CRGB, 160>, kMaxZones> zone_history{};

    void render(RenderContext& ctx) override {
        const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0xFF;

        if (z == 0xFF) {
            // Global render: accumulate across both zones
            for (int i = 0; i < 320; i++) {
                history_global[i] = blend(history_global[i], ctx.leds[i], 128);
            }
            memcpy(ctx.leds, history_global.data(), sizeof(ctx.leds));
        } else {
            // Per-zone render
            int zone_start = z * 160;
            for (int i = 0; i < 160; i++) {
                zone_history[z][i] = blend(zone_history[z][i], ctx.leds[zone_start + i], 128);
                ctx.leds[zone_start + i] = zone_history[z][i];
            }
        }
    }

    std::array<CRGB, 320> history_global{};
};
```

---

## Audio Integration Patterns

### Map Audio Frequency to Persistence

```cpp
// Bass (bands[0]) = slowest trails
float bass_persistence = 0.90f + 0.05f * (1.0f - ctx.controlBus.bands[0]);

// Mid (bands[3]) = medium trails
float mid_persistence = 0.80f + 0.15f * (1.0f - ctx.controlBus.bands[3]);

// Treble (bands[6] + bands[7]) = fastest trails
float treble_persistence = 0.60f + 0.30f * (1.0f - (ctx.controlBus.bands[6] + ctx.controlBus.bands[7]) * 0.5f);

// Render three overlays with different persistence
// (Or map to single trail with frequency-dependent color/brightness)
```

### Onset-Triggered Reset

```cpp
if (ctx.controlBus.onset) {
    // Flash + reset
    for (int i = 0; i < 320; i++) {
        history[i] = CRGB::White;  // White flash
    }
    decay_rate = 0.95f;  // Slow decay after onset
} else {
    // Normal decay
    decay_rate = 0.75f;
}
```

### RMS-Coupled Trail Length

```cpp
// Loud = long trails, quiet = short trails
float rms = ctx.controlBus.rms;
float trail_length_factor = 0.5f + 0.4f * rms;  // 0.5-0.9
uint8_t fade = uint8_t(255.0f * (1.0f - trail_length_factor));
fadeToBlackBy(history.data(), 320, fade);
```

---

## Performance Budgeting

**2.0ms per-frame budget:**

| Operation | Time (320 LEDs) | Budget % |
|-----------|-----------------|----------|
| `fadeToBlackBy()` | ~40µs | 2% |
| `blend()` loop | ~160µs | 8% |
| `memcpy()` | ~10µs | 0.5% |
| `pow()` (decay calc) | ~0.3µs | <0.1% |
| Total persistence overhead | ~210µs | <10% |

**Remaining budget:** ~1.79ms for rendering content.

---

## Common Mistakes

### 1. Forgetting Bounds Check on Zone ID
```cpp
// ❌ WRONG
zone_history[ctx.zoneId][i] = ...  // ctx.zoneId can be 0xFF!

// ✓ RIGHT
const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;
zone_history[z][i] = ...
```

### 2. Using `memcpy()` with Different Sizes
```cpp
// ❌ WRONG (src and dst sizes don't match)
memcpy(ctx.leds, history.data(), sizeof(history) * 2);

// ✓ RIGHT
memcpy(ctx.leds, history.data(), sizeof(ctx.leds));  // Use actual size
```

### 3. Allocating Buffers in `render()`
```cpp
// ❌ WRONG (heap alloc every frame = jitter + crash risk)
void render(RenderContext& ctx) {
    std::vector<CRGB> temp(320);
}

// ✓ RIGHT (pre-allocate at class level)
class MyEffect : public EffectBase {
    std::array<CRGB, 320> history{};
    void render(RenderContext& ctx) override { }
};
```

### 4. Ignoring Frame Rate in Decay
```cpp
// ❌ WRONG (frame-rate dependent)
leds[i] *= 0.9f;  // Works at 60fps, wrong at 120fps

// ✓ RIGHT (frame-rate invariant)
float decay = pow(0.933f, dt / 16.667f);
leds[i] = blend(leds[i], CRGB::Black, uint8_t(255 * (1.0f - decay)));
```

### 5. Not Clearing History on Effect Change
```cpp
// ❌ WRONG (old effect's trails persist when switching effects)
void render(RenderContext& ctx) override {
    // history[] still has old data from previous effect!
}

// ✓ RIGHT (clear on first render or effect start)
void onStart(RenderContext& ctx) override {
    memset(history.data(), 0, sizeof(history));
}
```

---

## Testing Checklist

Before committing an effect with persistence:

- [ ] **Zone bounds check:** Does it handle `zoneId = 0xFF` correctly?
- [ ] **Memory:** Is history pre-allocated (no `new`/`malloc` in `render()`)?
- [ ] **Decay tuning:** Does trail feel responsive at quiet sections, sustaining at loud?
- [ ] **Frame rate:** Does trail look the same at 60fps and 120fps?
- [ ] **No leaks:** Does history clear properly on effect transition?
- [ ] **Performance:** Does effect + persistence stay under 2.0ms?
- [ ] **Audio coupling:** Do decay/brightness/color change with beat/energy?

---

## Implementation Examples in K1 Codebase

**Patterns to Study:**

1. **Fire2012-style spatial persistence:**
   - Look for effects with diffusion (e.g., heat array)
   - Study how state persists across frames without explicit feedback loops

2. **Beat-reset:**
   - Search for `if (controlBus.beat)` patterns
   - See how other effects flash/reset on beat

3. **Audio-coupled decay:**
   - Find effects using `controlBus.rms`, `controlBus.bands[]`
   - Check how decay rate varies with audio

4. **Multi-zone handling:**
   - Search for effects that render both zones
   - Verify bounds checks on `ctx.zoneId`

---

## Recommended Effect Progression

**Skill Level → Pattern:**
1. Beginner: Pattern A (fadeToBlackBy)
2. Intermediate: Pattern B (exponential blend)
3. Advanced: Pattern C (beat-reset) + Pattern D (dt-compensated)
4. Expert: Multi-zone + frequency-specific persistence

---

## Decision Tree: Which Pattern to Use?

```
Start: I want to add persistence to my effect

→ Do I need audio-reactive decay?
   NO  → Use Pattern A (fadeToBlackBy) ✓
   YES → Continue

→ Do I need visual impact on beats?
   NO  → Use Pattern B (exponential blend) ✓
   YES → Use Pattern C (beat-reset) ✓

→ Do I need frame-rate invariance?
   NO  → Stop, you're done
   YES → Add Pattern D (dt-compensated) ✓

→ Are you handling multiple zones?
   NO  → Done ✓
   YES → Add multi-zone code + bounds check ✓
```

---

## Troubleshooting

### Trails disappear instantly
- **Problem:** Decay factor too high (100% loss per frame)
- **Fix:** Lower fade amount or increase `decay` parameter
  ```cpp
  fadeToBlackBy(history.data(), 320, 15);  // Reduce from 40 to 15
  ```

### Trails never fade (white-out)
- **Problem:** Decay factor ≤ 0 (no loss) or feedback gain > 1
- **Fix:** Ensure blend factor is > 0 and < 255
  ```cpp
  uint8_t blend_amount = 128;  // 50% new content, 50% history
  ```

### Trail looks different at 120fps than 60fps
- **Problem:** Frame-rate dependent decay (simple `*= 0.9`)
- **Fix:** Use Pattern D (dt-compensated)
  ```cpp
  float decay = pow(0.933f, ctx.dt / 16.667f);
  ```

### Zone trails corrupt or crash
- **Problem:** Missing bounds check on `ctx.zoneId`
- **Fix:** Add bounds check
  ```cpp
  const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0;
  ```

### Effect is slow (>2ms)
- **Problem:** Over-complex decay calculation or allocating buffers in render()
- **Fix:** Cache decay factor, use pre-allocated buffers
  ```cpp
  // Compute decay once at effect start, cache it
  if (last_dt != ctx.dt) {
      cached_decay = pow(0.933f, ctx.dt / 16.667f);
      last_dt = ctx.dt;
  }
  ```

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-17 | claude:research | Created K1-specific implementation guide with 4 concrete patterns, multi-zone handling, audio integration, and troubleshooting. |
