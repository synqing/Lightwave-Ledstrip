# LED Persistence — Copy-Paste Code Snippets

**Quick Reference:** Working code patterns ready to integrate into K1 effects.

---

## Template 1: Basic Trail Effect

```cpp
#include "EffectBase.h"
#include <array>

class BasicTrailEffect : public EffectBase {
private:
    std::array<CRGB, 320> history{};  // Pre-allocated history buffer

public:
    void render(RenderContext& ctx) override {
        // Fade history by 15%
        fadeToBlackBy(history.data(), 320, 40);

        // Example: Render moving dot
        float pos = fmod(beat_position + ctx.dt * 50.0f, 320.0f);
        int led_index = int(pos);
        ctx.leds[led_index] = CRGB::White;

        // Accumulate into history
        for (int i = 0; i < 320; i++) {
            history[i] = blend(history[i], ctx.leds[i], 200);  // 200/255 ≈ 78% new
        }

        // Output
        memcpy(ctx.leds, history.data(), sizeof(ctx.leds));
    }
};
```

**Integration:** Copy to `firmware-v3/src/effects/ieffect/your_folder/BasicTrailEffect.cpp`

---

## Template 2: Audio-Reactive Trail

```cpp
class AudioReactiveTrailEffect : public EffectBase {
private:
    std::array<CRGB, 320> history{};
    uint32_t hue_offset = 0;

public:
    void render(RenderContext& ctx) override {
        // Audio-coupled decay: louder = longer trails
        float rms = ctx.controlBus.rms;  // 0 to 1
        float decay = 0.50f + 0.40f * (1.0f - rms);  // 0.5-0.9
        uint8_t blend_amount = uint8_t(255.0f * (1.0f - decay));

        // Decay history
        for (int i = 0; i < 320; i++) {
            history[i].fadeToBlackBy(uint8_t(255.0f * (1.0f - decay)));
        }

        // Render frequency-based bars
        for (int band = 0; band < 8; band++) {
            float energy = ctx.controlBus.bands[band];
            int led_start = band * 40;
            int led_count = int(energy * 40.0f);

            for (int i = 0; i < led_count; i++) {
                int idx = led_start + i;
                if (idx < 320) {
                    uint8_t hue = hue_offset + (band * 32);
                    ctx.leds[idx] = CHSV(hue, 255, uint8_t(energy * 255.0f));
                }
            }
        }

        // Accumulate
        for (int i = 0; i < 320; i++) {
            history[i] = blend(history[i], ctx.leds[i], blend_amount);
        }
        memcpy(ctx.leds, history.data(), sizeof(ctx.leds));

        hue_offset += 2;
    }
};
```

**Integration:** Copy to effects folder, wire `controlBus` references.

---

## Template 3: Beat-Reset Sustain Effect

```cpp
class BeatResetTrailEffect : public EffectBase {
private:
    std::array<CRGB, 320> history{};
    uint32_t beat_time_ms = 0;
    uint32_t last_beat_ms = 0;

public:
    void onStart(RenderContext& ctx) override {
        memset(history.data(), 0, sizeof(history));
        beat_time_ms = millis();
        last_beat_ms = millis();
    }

    void render(RenderContext& ctx) override {
        uint32_t now = millis();

        // On beat: reset sustain curve
        if (ctx.controlBus.beat && (now - last_beat_ms) > 100) {  // Debounce 100ms
            beat_time_ms = now;
            last_beat_ms = now;
        }

        // Time-dependent decay envelope
        uint32_t age = now - beat_time_ms;
        float decay;

        if (age < 200) {
            // Fast 200ms: slow decay (sustain)
            decay = 0.95f;
        } else if (age < 1000) {
            // Next 800ms: gradual speedup
            float ratio = float(age - 200) / 800.0f;
            decay = 0.95f - 0.40f * ratio;  // 0.95 → 0.55
        } else {
            // After 1s: fast fade
            decay = 0.50f;
        }

        uint8_t blend_amount = uint8_t(255.0f * (1.0f - decay));

        // Render beat-triggered content
        if (age < 200) {
            // Bright flash
            for (int i = 100; i < 220; i++) {
                ctx.leds[i] = CRGB::White;
            }
        } else if (age < 500) {
            // Fading bars
            int brightness = map(age - 200, 0, 300, 255, 100);
            for (int i = 79; i < 241; i++) {
                ctx.leds[i] = CHSV(ctx.controlBus.chroma[0] * 255, 255, brightness);
            }
        }

        // Accumulate
        for (int i = 0; i < 320; i++) {
            history[i] = blend(history[i], ctx.leds[i], blend_amount);
        }
        memcpy(ctx.leds, history.data(), sizeof(ctx.leds));
    }
};
```

---

## Template 4: Frame-Rate Invariant Decay

```cpp
class FrameRateInvariantTrailEffect : public EffectBase {
private:
    std::array<CRGB, 320> history{};
    // 10-frame half-life at 60fps = decay factor 0.933
    static constexpr float DECAY_60FPS = 0.933f;
    float last_dt = 0.0f;
    float cached_decay = 0.933f;

public:
    void render(RenderContext& ctx) override {
        // Cache decay factor if dt changes
        if (fabsf(ctx.dt - last_dt) > 0.001f) {
            float dt_normalized = ctx.dt / 16.667f;
            cached_decay = pow(DECAY_60FPS, dt_normalized);
            last_dt = ctx.dt;
        }

        // Decay history using cached factor
        uint8_t fade_amount = uint8_t(255.0f * (1.0f - cached_decay));
        for (int i = 0; i < 320; i++) {
            history[i].fadeToBlackBy(fade_amount);
        }

        // Render content (example: reactive bars)
        for (int i = 0; i < 320; i++) {
            float energy = (i % 40) < int(ctx.controlBus.rms * 40.0f) ? 1.0f : 0.0f;
            if (energy > 0.0f) {
                ctx.leds[i] = CHSV(120, 255, 200);  // Green
            }
        }

        // Accumulate
        for (int i = 0; i < 320; i++) {
            history[i] = blend(history[i], ctx.leds[i], 128);  // 50/50
        }
        memcpy(ctx.leds, history.data(), sizeof(ctx.leds));
    }
};
```

---

## Template 5: Multi-Zone Trail (Critical for K1)

```cpp
class MultiZoneTrailEffect : public EffectBase {
private:
    static constexpr int kMaxZones = 2;
    std::array<CRGB, 320> history_global{};
    std::array<std::array<CRGB, 160>, kMaxZones> history_zone{};

public:
    void onStart(RenderContext& ctx) override {
        memset(history_global.data(), 0, sizeof(history_global));
        for (int z = 0; z < kMaxZones; z++) {
            memset(history_zone[z].data(), 0, sizeof(history_zone[z]));
        }
    }

    void render(RenderContext& ctx) override {
        float decay = 0.85f;  // 15% loss per frame
        uint8_t blend_amount = uint8_t(255.0f * (1.0f - decay));

        // Bounds-checked zone ID
        const int z = (ctx.zoneId < kMaxZones) ? ctx.zoneId : 0xFF;

        if (z == 0xFF) {
            // Global render: use shared buffer
            for (int i = 0; i < 320; i++) {
                history_global[i].fadeToBlackBy(uint8_t(255 * (1 - decay)));
            }

            // Render content
            int pos = map(ctx.controlBus.rms, 0, 1, 0, 319);
            ctx.leds[pos] = CRGB::White;

            // Accumulate global
            for (int i = 0; i < 320; i++) {
                history_global[i] = blend(history_global[i], ctx.leds[i], blend_amount);
            }
            memcpy(ctx.leds, history_global.data(), sizeof(ctx.leds));
        } else {
            // Per-zone render
            int zone_start = z * 160;

            // Fade zone history
            for (int i = 0; i < 160; i++) {
                history_zone[z][i].fadeToBlackBy(uint8_t(255 * (1 - decay)));
            }

            // Render zone content
            int pos = map(ctx.controlBus.bands[z * 4], 0, 1, 0, 159);
            ctx.leds[zone_start + pos] = CHSV(z * 128, 255, 200);

            // Accumulate zone
            for (int i = 0; i < 160; i++) {
                history_zone[z][i] = blend(history_zone[z][i], ctx.leds[zone_start + i], blend_amount);
                ctx.leds[zone_start + i] = history_zone[z][i];
            }
        }
    }
};
```

---

## Template 6: Fire2012-Style Physics (Spatial + Temporal)

```cpp
class Organic2DFireEffect : public EffectBase {
private:
    static constexpr int COOLING = 55;
    static constexpr int SPARKING = 120;
    std::array<uint8_t, 320> heat{};

public:
    void render(RenderContext& ctx) override {
        // Step 1: Cool down
        for (int i = 0; i < 320; i++) {
            heat[i] = qsub8(heat[i], random8(0, ((COOLING * 10) / 320) + 2));
        }

        // Step 2: Diffuse (spatial persistence)
        for (int i = 319; i >= 2; i--) {
            heat[i] = (heat[i - 1] + heat[i - 2] + heat[i - 2]) / 3;
        }

        // Step 3: Sparks (energy input)
        if (random8() < SPARKING) {
            int y = random8(320 / 4);  // Lower half
            heat[y] = qadd8(heat[y], random8(160, 255));
        }

        // Step 4: Render heat as color
        for (int i = 0; i < 320; i++) {
            uint8_t colorindex = scale8(heat[i], 240);  // 0-240 range for palette
            ctx.leds[i] = ColorFromPalette(HeatColorPalette, colorindex);
        }
    }
};
```

**Integration Note:** Requires `ColorFromPalette` and FastLED heat palette.

---

## Utility: Generic Decay Helper

Add to a shared utility header:

```cpp
namespace effects {
namespace temporal {

/// Exponential smoothing (exponential moving average)
/// output = current * alpha + previous * (1 - alpha)
inline CRGB exponential_decay(CRGB current, CRGB previous, float alpha) {
    return blend(previous, current, uint8_t(255.0f * alpha));
}

/// Frame-rate invariant decay for time-constant decay
/// target_decay: decay factor at 60fps (e.g., 0.933f for 10-frame half-life)
/// dt_ms: frame time in milliseconds
inline float dt_compensated_decay(float target_decay_60fps, float dt_ms) {
    float dt_normalized = dt_ms / 16.667f;  // Normalize by 60fps frame
    return pow(target_decay_60fps, dt_normalized);
}

/// Convert half-life (frames at 60fps) to decay factor
/// Example: half_life_frames=10 → decay_factor=0.933
inline float half_life_to_decay(float half_life_frames) {
    return pow(0.5f, 1.0f / half_life_frames);
}

/// Fade with clamping (prevents white-out in feedback)
inline CRGB fade_clamped(CRGB color, uint8_t fade_amount, uint8_t max_brightness = 255) {
    color.fadeToBlackBy(fade_amount);
    if (color.r > max_brightness) color.r = max_brightness;
    if (color.g > max_brightness) color.g = max_brightness;
    if (color.b > max_brightness) color.b = max_brightness;
    return color;
}

} // namespace temporal
} // namespace effects
```

**Usage:**
```cpp
decay_factor = effects::temporal::dt_compensated_decay(0.933f, ctx.dt);
clamped = effects::temporal::fade_clamped(color, 40, 200);
```

---

## Comparison: Which Template to Use?

| Template | Best For | Complexity | Audio-Reactive |
|----------|----------|-----------|-----------------|
| 1: BasicTrail | Learning, 1D effects | ⭐ | ❌ |
| 2: AudioReactive | General use, responsive | ⭐⭐ | ✓ |
| 3: BeatReset | Rhythm effects, punch | ⭐⭐ | ✓ |
| 4: FrameRateInvariant | Production, variable FPS | ⭐⭐⭐ | ✓ |
| 5: MultiZone | K1 dual-strip | ⭐⭐⭐ | ✓ |
| 6: Fire2012Physics | Organic, diffusion | ⭐⭐⭐ | Moderate |

---

## Quick Integration Checklist

- [ ] Copy template to new file in `src/effects/ieffect/your_folder/`
- [ ] Update `#include` paths for `EffectBase.h`, `RenderContext.h`
- [ ] Add bounds check for `ctx.zoneId` if multi-zone
- [ ] Verify history buffer is pre-allocated (not in `render()`)
- [ ] Test at 60fps and 120fps (for frame-rate invariance)
- [ ] Verify under 2.0ms budget (profile with `esp_timer_get_time()`)
- [ ] Clear history in `onStart()` to avoid stale trails
- [ ] Remove debug `Serial.println()` before commit

---

## Common Modifications

### Change decay rate
```cpp
// In Template 1-5, replace:
uint8_t blend_amount = uint8_t(255.0f * (1.0f - decay));
// With:
uint8_t blend_amount = 100;  // Faster decay, more responsive
```

### Add multiple colors
```cpp
// In Template 2, replace CHSV(hue, 255, ...) with:
uint8_t hue = uint8_t(ctx.controlBus.chroma[band % 12] * 255.0f);
ctx.leds[idx] = CHSV(hue, 255, uint8_t(energy * 255.0f));
```

### Reverse direction
```cpp
// In Template 1, replace:
int led_index = int(pos);
// With:
int led_index = 319 - int(pos);  // Reverse
```

### Zone-aware colors
```cpp
// In Template 5, replace:
ctx.leds[zone_start + pos] = CHSV(z * 128, 255, 200);
// With:
uint8_t hue = z == 0 ? 0 : 170;  // Red for zone 0, cyan for zone 1
ctx.leds[zone_start + pos] = CHSV(hue, 255, 200);
```

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-17 | claude:research | Created copy-paste code templates for 6 persistence patterns + utility helpers. |
