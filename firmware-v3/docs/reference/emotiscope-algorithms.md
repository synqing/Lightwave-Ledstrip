---
abstract: "Quick reference for Emotiscope algorithms: silence detection formula, novelty contrast computation, noise floor tracking constants, and LED fade parameters. For direct implementation reference."
---

# Emotiscope Algorithms — Quick Reference

## Silence Detection Formula

```c
// From tempo.h
float novelty_contrast = max_val - min_val;  // Peak-to-trough in last 128 samples
float silence_level_raw = 1.0f - novelty_contrast;
float silence_level = fmaxf(0.0f, (silence_level_raw - 0.5f) * 2.0f);
// silence_level: 0.0 (active) to 1.0 (silent)
```

**Activation threshold:** `silence_level_raw > 0.5`

---

## Noise Floor Tracking

```c
// From vu.h
// Constants
#define NUM_VU_LOG_SAMPLES 20
#define VU_LOG_INTERVAL_MS 250

// Circular buffer
float vu_log[NUM_VU_LOG_SAMPLES];
int vu_log_index = 0;
uint32_t last_vu_log = 0;

// Update every 250ms
if (t_now_ms - last_vu_log >= 250) {
    vu_log[vu_log_index] = max_amplitude_now;
    vu_log_index = (vu_log_index + 1) % NUM_VU_LOG_SAMPLES;
    last_vu_log = t_now_ms;
}

// Calculate floor (average - 10%)
float vu_sum = sum(vu_log);
float vu_floor = (vu_sum / NUM_VU_LOG_SAMPLES) * 0.90f;
```

**Time window:** 20 samples × 250ms = 5 seconds rolling average

---

## VU Level Processing

```c
// 1. Subtract noise floor (squelch)
float vu_raw = fmaxf(max_amplitude - vu_floor, 0.0f);

// 2. Auto-scaling (prevent clipping)
float auto_scale = 1.0f / fmaxf(max_amplitude_cap, 0.00001f);
float vu_level = vu_raw * auto_scale;

// 3. Smooth (12-sample moving average)
#define NUM_VU_SMOOTH_SAMPLES 12
vu_smooth[vu_smooth_index] = vu_level;
vu_smooth_index = (vu_smooth_index + 1) % NUM_VU_SMOOTH_SAMPLES;
float vu_out = sum(vu_smooth) / NUM_VU_SMOOTH_SAMPLES;
```

---

## LED Phosphor Decay

```c
// From leds.h
void apply_phosphor_decay(float strength) {
    strength = 1.0f - strength;
    strength *= 0.05f;  // 5% decay factor
    strength = fmaxf(strength, 0.001f);  // Floor at 0.1%

    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] *= (1.0f - strength);  // Retain 95% per frame
    }
}
```

**Result:** ~37% brightness after 400 frames (exponential decay)

---

## Silence-Driven Fade

```c
// From tempo.h
if (silence_level_raw > 0.5f) {
    // Apply tempo history reduction
    float reduction = silence_level * 0.10f;  // Max 10% per frame
    novelty_history *= (1.0f - reduction);
    vu_history *= (1.0f - reduction);
}

// Optional: In per-effect render loop
float fade_factor = 1.0f - (silence_level * 0.10f);
effect_brightness *= fade_factor;
```

---

## Novelty Normalization

```c
// From tempo.h
float novelty_normalized = novelty_raw * 0.05f +
                           novelty_normalized * 0.95f;

float vu_normalized = vu_raw * 0.01f +
                      vu_normalized * 0.99f;
```

**Blending ratios:**
- Novelty: 5% new + 95% history (fast responsiveness)
- VU: 1% new + 99% history (slow trending)

---

## Screensaver Activation

```c
// From screensaver.h
#define SCREENSAVER_THRESHOLD 1.0f
#define SCREENSAVER_WAIT_MS 5000

float mag_sum = 0;
for (int i = 0; i < NUM_MUSICAL_FREQS; i++) {
    mag_sum += frequencies[i].magnitude;
}

if (mag_sum < SCREENSAVER_THRESHOLD) {
    silence_timer += dt;
    if (silence_timer > SCREENSAVER_WAIT_MS) {
        screensaver_active = true;
    }
}

// Reset on any touch
if (touch_detected) {
    screensaver_active = false;
    silence_timer = 0;
}
```

---

## Configuration Parameters

```c
// From configuration.h
struct Config {
    float softness;        // 0.0–1.0 (default 0.25) — phosphor decay rate
    float speed;           // 0.0–1.0 (default 0.50) — smoothing responsiveness
    float brightness;      // 0.3–1.0 — master LED level
    float color_range;     // 0.0–1.0 — hue variation
    bool screensaver;      // true/false — enable screensaver
};
```

**NOT user-adjustable:**
- Silence threshold (hardcoded)
- Noise floor tracking window (5 seconds)
- Fade time constants (10% per frame)

---

## Silence Detection vs. Amplitude Thresholding

| Feature | Emotiscope | Simple Squelch |
|---------|-----------|----------------|
| Detects | Spectral change | Audio amplitude |
| Quiet passages | Responsive ✓ | Muted ✗ |
| Steady tones | Detects fade ✓ | Always "on" ✗ |
| Calibration | Automatic ✓ | Manual ✗ |
| False positives | Low (static noise ignored) | High (ambient noise triggers) |

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-25 | agent:research | Created quick-reference implementation guide with code snippets and constants for Emotiscope silence detection, noise floor tracking, and LED fade algorithms. |
