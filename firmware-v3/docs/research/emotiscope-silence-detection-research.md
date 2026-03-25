---
abstract: "Emotiscope silence detection and LED fade algorithms: novelty_curve contrast-based silence detection, adaptive noise floor tracking, exponential LED decay, and confidence-based darkness transitions. For implementing advanced silence handling in firmware audio-visual systems."
---

# Emotiscope Silence Detection & LED Fade Research

**Source:** Emotiscope by Lixie Labs (connornishijima)
**Repository:** https://github.com/connornishijima/Emotiscope
**Research Date:** 2026-03-25

## 1. Silence Detection Algorithm

### 1.1 Core Mechanism: Novelty Contrast

Emotiscope detects silence using **spectral novelty contrast**, not direct amplitude thresholding. This allows it to distinguish between:
- Actual silence (no audio)
- Quiet passages (soft music)
- Dynamic variations in tempo

**Algorithm (from `tempo.h`):**

```c
// Examine last 128 samples of novelty curve
float novelty_contrast = max_val - min_val;  // Peak-to-trough in recent history

// Convert contrast to silence metric
float silence_level_raw = 1.0 - novelty_contrast;

// Apply threshold + scaling
float silence_level = max(0.0, (silence_level_raw - 0.5) * 2.0);
// Output range: 0.0 (no silence) to 1.0 (complete silence)
```

**Interpretation:**
- When novelty varies widely (busy music) → low contrast → `silence_level ≈ 0.0`
- When novelty is flat (silence/ambient) → high contrast → `silence_level → 1.0`
- Threshold: activates when `silence_level_raw > 0.5`

### 1.2 Novelty Curve

The **novelty curve** is a circular buffer storing spectral change data over time:

```c
// Defined as circular history buffer
float novelty_curve[NOVELTY_HISTORY_LENGTH];  // ~128 samples typical

// Updated via
novelty_curve[NOVELTY_HISTORY_LENGTH - 1] = input;  // Append new spectral change
// ... shift buffer left each update
```

**How novelty is computed:**
- Spectral flux: frame-to-frame magnitude changes across Goertzel frequency bins
- Windowed with Gaussian (σ=0.8) to reduce artifacts
- Per-frequency noise subtraction removes baseline environmental noise

### 1.3 Handling Quiet Passages vs. True Silence

**The novelty contrast approach naturally handles both:**

1. **Quiet musical passage** (e.g., soft piano solo):
   - Amplitude is low, but notes still change
   - Novelty curve shows variation despite low values
   - Contrast > 0.5 → `silence_level` stays low → LEDs remain responsive

2. **True silence** (no audio):
   - Novelty curve is flat/near-zero
   - Contrast ≈ 0 → `silence_level` → 1.0
   - LEDs fade to darkness

**Advantage:** Cannot be fooled by sustained low-amplitude tones (which would fool amplitude-based squelch).

---

## 2. Noise Floor & VU Level Implementation

### 2.1 Automatic Noise Floor Tracking

**From `vu.h`:**

```c
// Circular buffer of 20 samples, logged every 250ms
float vu_log[NUM_VU_LOG_SAMPLES];  // NUM_VU_LOG_SAMPLES = 20
int vu_log_index = 0;
uint32_t last_vu_log = 0;

// In processing loop:
if (t_now_ms - last_vu_log >= 250) {
    vu_log[vu_log_index] = max_amplitude_now;
    vu_log_index = (vu_log_index + 1) % NUM_VU_LOG_SAMPLES;
    last_vu_log = t_now_ms;
}

// Calculate noise floor (average of recent amplitudes)
float vu_floor = vu_sum / NUM_VU_LOG_SAMPLES;
vu_floor *= 0.90;  // Reduce by 10% to get floor threshold
```

**Behavior:**
- Tracks 20 amplitude samples over ~5 seconds (250ms × 20)
- Averages them and applies 90% multiplier
- Creates adaptive squelch that tracks the noise floor
- Auto-calibrates to microphone preamp level and room ambient noise

### 2.2 Signal Processing Pipeline

```c
// 1. Raw amplitude detection
float max_amplitude_now = sqrt(max_squared_value);
max_amplitude_now = max(max_amplitude_now, 0.000001f);

// 2. Noise floor subtraction (squelch)
max_amplitude_now = fmaxf(max_amplitude_now - vu_floor, 0.0f);

// 3. Auto-scaling (prevent clipping, normalize)
float auto_scale = 1.0f / fmaxf(max_amplitude_cap, 0.00001f);
float vu_level_raw = max_amplitude_now * auto_scale;

// 4. Smoothing (12-sample moving average)
vu_smooth[vu_smooth_index] = vu_level_raw;
vu_smooth_index = (vu_smooth_index + 1) % NUM_VU_SMOOTH_SAMPLES;

// Output is average of circular buffer
float vu_level = sum(vu_smooth) / NUM_VU_SMOOTH_SAMPLES;
```

**Key feature:** Auto-scaling creeps toward current peaks at 10% rate per update, preventing saturation from DC bias or offset.

---

## 3. LED Fade & Darkness Transition Logic

### 3.1 Phosphor Decay (Gradual Dimming)

**From `leds.h`:**

```c
void apply_phosphor_decay() {
    float strength = 1.0 - strength;  // Invert
    strength *= 0.05;                  // Scale decay rate
    strength = max(strength, 0.001f);  // Floor at 0.1% to prevent instant blackout

    // Apply to all LEDs
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] *= (1.0 - strength);   // Multiplicative decay
    }
}
```

**Effect:**
- Each frame, LEDs retain 95% of previous brightness (0.05 decay factor)
- Minimum floor of 0.1% prevents instant cut-off
- Creates phosphor-like glow fade rather than hard blackout

### 3.2 Silence-Responsive Darkening

**From `leds.h` - `update_auto_color()`:**

```c
float novelty = novelty_curve_normalized[NOVELTY_HISTORY_LENGTH - 1] * 0.75;
// Apply aggressive dampening during silence
novelty = pow(novelty, 6.0f);  // Sixth-power compression

// When silence_level is high, novelty → 0 very quickly
// This suppresses color momentum and causes visual darkening
```

**Mechanism:**
1. During silence, `novelty_curve` flattens
2. Sixth-power function amplifies this reduction
3. Color momentum gets heavily suppressed
4. Result: LEDs darken smoothly rather than cutting

### 3.3 Multi-Layer Brightness Control

**Three complementary systems manage overall brightness:**

1. **Phosphor decay** (`apply_phosphor_decay()`)
   - Per-LED exponential fade
   - Smooth dimming of existing colors

2. **Fade display** (`fade_display()`)
   - Scales all LEDs by a softness parameter
   - User-configurable intensity reduction

3. **Master brightness** (`apply_master_brightness()`)
   - Ramps brightness from 0 to 1.0 over initial seconds
   - Prevents jarring power-on

**Combined effect:** Layered fade behavior creates natural darkness transitions responsive to both silence and user configuration.

### 3.4 Tempo History Reduction on Silence

**From `tempo.h`:**

```c
// When silence is detected
if (silence_level_raw > 0.5) {
    reduce_tempo_history(silence_level * 0.10);
}

// reduce_tempo_history() multiplies history buffers by (1.0 - reduction_amount)
// This progressively darkens accumulated tempo data
```

**Time constant:**
- Silence level smoothly ramps from 0.0 to 1.0
- Reduction amount = `silence_level * 0.10` → max 10% per frame
- Exponential decay of tempo history creates natural fade

---

## 4. Novelty Curve Details

### 4.1 Novelty History Management

```c
// Circular buffer for novelty samples
float novelty_curve[NOVELTY_HISTORY_LENGTH];
float novelty_curve_normalized[NOVELTY_HISTORY_LENGTH];

// Updated per audio frame
novelty_curve[NOVELTY_HISTORY_LENGTH - 1] = compute_spectral_flux();
// Shift buffer left, discard oldest sample
```

**Spectral flux computation:**
- Compares magnitude of consecutive Goertzel frequency bins
- Gaussian-windowed to reduce artifacts
- Per-frequency noise subtraction removes baseline

### 4.2 Normalization for Activity Detection

```c
// Two separate smoothing paths for analysis:
float novelty_normalized = novelty_curve * 0.05 +
                           novelty_normalized * (1.0 - 0.05);
// Blending: 0.05 new + 0.95 history

// Similar for VU curve
float vu_curve_normalized = vu_values * 0.01 +
                            vu_curve_normalized * (1.0 - 0.01);
// Slower blending: 0.01 new + 0.99 history
```

**Purpose:**
- Novelty responds quickly to changes (5% blend)
- VU responds slowly to amplitude trends (1% blend)
- Together they provide multi-timescale activity metrics

---

## 5. Screensaver Activation

### 5.1 Trigger Conditions

**From `screensaver.h`:**

```c
// Silence detection
float mag_sum = 0;
for (int i = 0; i < NUM_MUSICAL_FREQS; i++) {
    mag_sum += frequencies_musical[i].magnitude;
}

// Activation logic
if (mag_sum < screensaver_threshold) {  // Default: 1.0
    silence_timer += dt;

    if (silence_timer > SCREENSAVER_WAIT_MS) {  // 5000ms = 5 seconds
        screensaver_active = true;
    }
}

// Touch resets it
if (device_touch_active || app_touch_active || slider_touch_active) {
    screensaver_active = false;
    silence_timer = 0;
}
```

**Conditions:**
1. Audio magnitude sums below threshold (1.0 by default)
2. Must remain silent for 5 seconds
3. Any touch input immediately exits screensaver

### 5.2 Screensaver Visual Effect

```c
// Fade LEDs + animated dots
screensaver_mix = min(1.0, screensaver_mix + fade_speed);  // Ramp 0→1

// Display four sine-wave dots with intensity = screensaver_mix
for (int i = 0; i < 4; i++) {
    pos[i] = sin(time + i * PI/2) * LED_COUNT/2 + LED_COUNT/2;
    brightness[i] = 0.3 * screensaver_mix;
}
```

---

## 6. User-Adjustable Parameters

### 6.1 Configuration Options

From `configuration.h`, user-controllable audio-visual parameters:

| Parameter | Type | Default | Range | Effect |
|-----------|------|---------|-------|--------|
| `softness` | float | 0.25 | 0.0–1.0 | Phosphor decay / fade filter drag |
| `speed` | float | 0.50 | 0.0–1.0 | Smoothing responsiveness, color change rate |
| `screensaver` | bool | true | — | Enable/disable screensaver |
| `color_range` | float | — | 0.0–1.0 | Hue variation in light modes |
| `brightness` | float | 0.3–1.0 | — | Master LED output level |

### 6.2 NOT User-Adjustable

**Notably absent:**
- Silence detection threshold (hardcoded in `check_silence()`)
- Noise floor tracking parameters (250ms log interval, 20-sample history)
- Fade time constants (10% reduction per frame)
- Screensaver timeout (5 seconds)

**Implication:** Silence detection is designed to be automatic and always-on. Users adjust perceived brightness/speed but not the underlying silence algorithm.

---

## 7. Key Differences from Amplitude-Based Squelch

| Aspect | Emotiscope (Novelty) | Traditional Squelch (Amplitude) |
|--------|----------------------|--------------------------------|
| **Silence indicator** | Spectral contrast flatness | Audio level below threshold |
| **Quiet passages** | Responsive (note changes detected) | Muted (below squelch level) |
| **Sustained tones** | Gradual fade (falling novelty) | Immediate response (if above threshold) |
| **Calibration** | Automatic (12.5 second history window) | Manual threshold adjustment |
| **False positives** | Low (ambient noise ignored if static) | High (ambient noise triggers without silence) |

---

## 8. Implementation Summary for LightwaveOS

### Recommended Porting Strategy

1. **Implement novelty curve** as spectral flux from audio bins/bands
2. **Track contrast** over rolling history window (~128 samples)
3. **Compute silence_level** from contrast formula (step 1.1)
4. **Integrate with render fade:**
   ```c
   // In RendererActor or per-effect render
   float fade_factor = 1.0f - (silence_level * 0.10f);  // 0% to 10% fade per frame
   ctx.controlBus.brightness *= fade_factor;
   ```
5. **Auto-fade effect state** via history reduction similar to tempo.h
6. **Avoid adding squelch** to ControlBus amplitude — let effects use `bands[]` and `chroma[]` directly

### Parameters to Consider for K1

- **Novelty history window:** 128 samples @ 48kHz = ~2.67ms frame rate
- **Silence threshold:** `silence_level_raw > 0.5`
- **Fade rate:** 5–10% per frame (adjust for desired darkness transition speed)
- **Screensaver timeout:** 5 seconds default (user configurable via REST API)

---

## References

- **Repository:** https://github.com/connornishijima/Emotiscope
- **Project site:** https://emotiscope.rocks/
- **Key files examined:**
  - `src/tempo.h` — novelty/silence calculation
  - `src/vu.h` — noise floor and smoothing
  - `src/leds.h` — phosphor decay and color darkening
  - `src/screensaver.h` — silence-triggered screensaver
  - `src/goertzel.h` — spectral analysis
  - `src/configuration.h` — user parameters

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-25 | agent:research | Created comprehensive Emotiscope silence detection algorithm analysis with novelty contrast, noise floor tracking, LED fade mechanisms, and implementation guidance for LightwaveOS K1. |
