# Sensory Bridge Audio-to-Visual Pipeline Analysis

## Executive Summary

This document provides a comprehensive analysis of how Sensory Bridge (v4.0.0, 4.1.0, 4.1.1) translates audio data into visual motion, with specific focus on **Bloom mode** and **Waveform mode**. The analysis reveals critical design patterns that fundamentally differ from our current implementation approach.

## Key Findings

### 1. **NO Fixed Time Constants for Motion**
Sensory Bridge does **NOT** use fixed time constants (like 120ms/400ms) for audio-reactive motion. Instead:
- **Bloom mode**: Uses `draw_sprite()` with **MOOD-controlled position** (`0.250 + 1.750 * CONFIG.MOOD`)
- **Waveform mode**: Uses **mood-dependent smoothing** (`(0.1 + CONFIG.MOOD * 0.9) * 0.05`)
- Motion speed is **user-controllable via MOOD knob**, not tempo-adaptive

### 2. **Multi-Stage Smoothing Pipeline**
Audio data goes through **multiple smoothing stages** before reaching visual rendering:
1. Low-pass filtering (mood-dependent cutoff)
2. Asymmetric peak tracking (adaptive normalization)
3. Spectrogram smoothing (0.75 coefficient, **symmetric**)
4. Chromagram aggregation
5. Final smoothing in visual mode (mood-dependent)

### 3. **History Buffers for Spike Filtering**
Waveform mode uses **4-frame rolling average** before smoothing to filter single-frame audio spikes.

### 4. **Frame Persistence, Not Time Constants**
Bloom mode uses **alpha blending** (0.99 alpha) with previous frame, creating persistence through **frame accumulation**, not exponential decay time constants.

---

## Complete Audio-to-Visual Data Chain

### Stage 1: Audio Capture → Raw Magnitudes

**File**: `GDFT.h` - `process_GDFT()`

```cpp
// Goertzel algorithm runs 64 parallel instances
for (uint16_t i = 0; i < NUM_FREQS; i++) {
    // ... Goertzel calculation ...
    magnitudes[i] = q2 * q2 + q1 * q1 - ((int32_t)(mult >> 14)) * q2;
    magnitudes[i] = sqrt(magnitudes[i]);
    magnitudes_normalized[i] = magnitudes[i] / float(frequencies[i].block_size / 2.0);
}
```

**Output**: `magnitudes_normalized[64]` - Raw frequency magnitudes

### Stage 2: Noise Reduction

```cpp
if (noise_complete == true) {
    magnitudes_normalized_avg[i] -= float(noise_samples[i] * SQ15x16(1.5));
    if (magnitudes_normalized_avg[i] < 0.0) {
        magnitudes_normalized_avg[i] = 0.0;
    }
}
```

**Output**: Noise-subtracted magnitudes

### Stage 3: Low-Pass Filtering (Mood-Dependent)

```cpp
// MOOD_VAL = 1.0 for Bloom mode (forces maximum smoothing)
float MOOD_VAL = CONFIG.MOOD;
if (CONFIG.LIGHTSHOW_MODE == LIGHT_MODE_BLOOM) {
    MOOD_VAL = 1.0;  // Bloom always uses maximum smoothing
}

low_pass_array(magnitudes_final, magnitudes_last, NUM_FREQS, SYSTEM_FPS, 1.0 + (10.0 * MOOD_VAL));
```

**Formula**: `alpha = 1.0 - expf(-2.0 * PI * cutoff_freq / sample_rate)`

**Cutoff frequency**: `1.0 + (10.0 * MOOD_VAL)` Hz
- Low mood (0.0): 1.0 Hz cutoff (very smooth)
- High mood (1.0): 11.0 Hz cutoff (more reactive)
- **Bloom mode**: Always 11.0 Hz (maximum smoothing)

**Output**: `magnitudes_final[64]` - Low-pass filtered magnitudes

### Stage 4: Adaptive Peak Tracking (Asymmetric)

```cpp
static SQ15x16 goertzel_max_value = 0.0001;
SQ15x16 max_value = 0.00001;

// Find peak
for (uint8_t i = 0; i < NUM_FREQS; i++) {
    if (magnitudes_final[i] > max_value) {
        max_value = magnitudes_final[i];
    }
}

max_value *= SQ15x16(0.995);  // Decay peak

// Asymmetric attack/release
if (max_value > goertzel_max_value) {
    SQ15x16 delta = max_value - goertzel_max_value;
    goertzel_max_value += delta * SQ15x16(0.0050);  // Fast attack (0.5%)
} else if (goertzel_max_value > max_value) {
    SQ15x16 delta = goertzel_max_value - max_value;
    goertzel_max_value -= delta * SQ15x16(0.0025);  // Slow release (0.25%)
}
```

**Key Insight**: Peak tracking uses **asymmetric smoothing** (fast attack 0.5%, slow release 0.25%) for adaptive normalization.

**Output**: `goertzel_max_value` - Adaptive peak for normalization

### Stage 5: Normalization

```cpp
SQ15x16 multiplier = SQ15x16(1.0) / goertzel_max_value;
for (uint16_t i = 0; i < NUM_FREQS; i++) {
    spectrogram[i] = magnitudes_final[i] * multiplier;
}
```

**Output**: `spectrogram[64]` - Normalized spectrogram (0.0-1.0 range)

### Stage 6: Spectrogram Smoothing (Symmetric!)

**File**: `lightshow_modes.h` - `get_smooth_spectrogram()`

```cpp
void get_smooth_spectrogram() {
    static SQ15x16 spectrogram_smooth_last[64];

    for (uint8_t bin = 0; bin < 64; bin++) {
        SQ15x16 note_brightness = spectrogram[bin];

        if (spectrogram_smooth[bin] < note_brightness) {
            SQ15x16 distance = note_brightness - spectrogram_smooth[bin];
            spectrogram_smooth[bin] += distance * SQ15x16(0.75);  // 75% of distance
        } else if (spectrogram_smooth[bin] > note_brightness) {
            SQ15x16 distance = spectrogram_smooth[bin] - note_brightness;
            spectrogram_smooth[bin] -= distance * SQ15x16(0.75);  // 75% of distance (SAME!)
        }
    }
}
```

**CRITICAL FINDING**: The smoothing is **SYMMETRIC** (0.75 for both rise and fall), NOT asymmetric! This is a simple linear interpolation, not exponential decay.

**Output**: `spectrogram_smooth[64]` - Smoothed spectrogram

### Stage 7: Chromagram Aggregation

**File**: `led_utilities.h` - `make_smooth_chromagram()`

```cpp
void make_smooth_chromagram() {
    memset(chromagram_smooth, 0, sizeof(SQ15x16) * 12);

    for (uint8_t i = 0; i < CONFIG.CHROMAGRAM_RANGE; i++) {
        SQ15x16 note_magnitude = spectrogram_smooth[i];  // Uses SMOOTHED spectrogram
        uint8_t chroma_bin = i % 12;
        chromagram_smooth[chroma_bin] += note_magnitude / SQ15x16(CONFIG.CHROMAGRAM_RANGE / 12.0);
    }

    // Adaptive peak tracking for chromagram normalization
    static SQ15x16 max_peak = 0.001;
    max_peak *= 0.999;  // Decay
    if (chromagram_smooth[i] > max_peak) {
        SQ15x16 distance = chromagram_smooth[i] - max_peak;
        max_peak += distance *= SQ15x16(0.05);  // 5% attack
    }
    SQ15x16 multiplier = 1.0 / max_peak;
    chromagram_smooth[i] *= multiplier;
}
```

**Output**: `chromagram_smooth[12]` - Normalized chromagram (0.0-1.0 range)

---

## Bloom Mode Visual Rendering

### Step 1: Calculate Color from Chromagram

**File**: `lightshow_modes.h` - `light_mode_bloom()`

```cpp
void light_mode_bloom() {
    // Clear output
    memset(leds_16, 0, sizeof(CRGB16) * NATIVE_RESOLUTION);

    // Draw previous frame with alpha blending (PERSISTENCE)
    draw_sprite(leds_16, leds_16_prev, 128, 128, 0.250 + 1.750 * CONFIG.MOOD, 0.99);
    //                                                              ^^^^^^^^^^^^^^^^^^^^
    //                                                              Position controlled by MOOD
    //                                                                      ^^^
    //                                                                      Alpha (99% persistence)

    // Calculate color from chromagram
    CRGB16 sum_color;
    SQ15x16 share = 1 / 6.0;
    for (uint8_t i = 0; i < 12; i++) {
        float prog = i / 12.0;
        SQ15x16 bin = chromagram_smooth[i];  // Uses SMOOTHED chromagram
        CRGB16 add_color = hsv(prog, CONFIG.SATURATION, bin*bin * share);
        sum_color.r += add_color.r;
        sum_color.g += add_color.g;
        sum_color.b += add_color.b;
    }

    // Square iterations for contrast
    for (uint8_t i = 0; i < CONFIG.SQUARE_ITER; i++) {
        sum_color.r *= sum_color.r;
        sum_color.g *= sum_color.g;
        sum_color.b *= sum_color.b;
    }

    // Set center LEDs
    leds_16[63] = sum_color;
    leds_16[64] = leds_16[63];

    // Copy to previous frame buffer
    memcpy(leds_16_prev, leds_16, sizeof(CRGB16) * NATIVE_RESOLUTION);

    // Exponential fade at edges
    for(uint8_t i = 0; i < 32; i++){
        float prog = i / 31.0;
        leds_16[128-1-i].r *= (prog*prog);  // Quadratic fade
        leds_16[128-1-i].g *= (prog*prog);
        leds_16[128-1-i].b *= (prog*prog);
    }

    // Mirror
    for (uint8_t i = 0; i < 64; i++) {
        leds_16[i] = leds_16[128 - 1 - i];
    }
}
```

### Step 2: `draw_sprite()` - The Expansion Mechanism

**File**: `led_utilities.h` - `draw_sprite()`

```cpp
void draw_sprite(CRGB16 dest[], CRGB16 sprite[], uint32_t dest_length, uint32_t sprite_length, float position, SQ15x16 alpha) {
    int32_t position_whole = position;
    float position_fract = position - position_whole;
    SQ15x16 mix_right = position_fract;
    SQ15x16 mix_left = 1.0 - mix_right;

    for (uint16_t i = 0; i < sprite_length; i++) {
        int32_t pos_left = i + position_whole;
        int32_t pos_right = i + position_whole + 1;

        // Blend sprite pixel into destination at fractional position
        dest[pos_left].r += sprite[i].r * mix_left * alpha;
        dest[pos_left].g += sprite[i].g * mix_left * alpha;
        dest[pos_left].b += sprite[i].b * mix_left * alpha;

        dest[pos_right].r += sprite[i].r * mix_right * alpha;
        dest[pos_right].g += sprite[i].g * mix_right * alpha;
        dest[pos_right].b += sprite[i].b * mix_right * alpha;
    }
}
```

**Key Insights**:
1. **Position parameter**: `0.250 + 1.750 * CONFIG.MOOD`
   - Low mood (0.0): position = 0.25 (slow expansion)
   - High mood (1.0): position = 2.0 (fast expansion)
   - **NOT tempo-adaptive!** User-controlled via MOOD knob

2. **Alpha = 0.99**: 99% persistence means previous frame is almost fully preserved
   - Creates **frame accumulation** effect
   - Motion is smooth because each frame adds a small amount
   - **NOT exponential decay with time constants!**

3. **Expansion mechanism**: Previous frame is drawn at a shifted position, creating radial expansion from center

---

## Waveform Mode Visual Rendering

### History Buffer for Spike Filtering

**File**: `lightshow_modes.h` (v3.1.0) - `light_mode_waveform()`

```cpp
void light_mode_waveform() {
    static float waveform_last[1024] = { 0 };
    static float sum_color_last[3] = {0, 0, 0};

    // Smooth peak level
    waveform_peak_scaled_last = (waveform_peak_scaled * 0.05 + waveform_peak_scaled_last * 0.95);

    // ... calculate sum_color from chromagram ...

    // Smooth color separately (very slow)
    sum_color_float[0] = sum_color_float[0] * 0.05 + sum_color_last[0] * 0.95;
    sum_color_float[1] = sum_color_float[1] * 0.05 + sum_color_last[1] * 0.95;
    sum_color_float[2] = sum_color_float[2] * 0.05 + sum_color_last[2] * 0.95;

    for (uint8_t i = 0; i < NATIVE_RESOLUTION; i++) {
        // 4-FRAME ROLLING AVERAGE (spike filtering)
        float waveform_sample = 0.0;
        for (uint8_t s = 0; s < 4; s++) {
            waveform_sample += waveform_history[s][i];
        }
        waveform_sample /= 4.0;
        float input_wave_sample = (waveform_sample / 128.0);

        // Mood-dependent smoothing
        float smoothing = (0.1 + CONFIG.MOOD * 0.9) * 0.05;
        // Low mood: 0.1 * 0.05 = 0.005 (very smooth)
        // High mood: 1.0 * 0.05 = 0.05 (more reactive)

        waveform_last[i] = input_wave_sample * (smoothing) + waveform_last[i] * (1.0 - smoothing);

        // Apply peak scaling
        float peak = waveform_peak_scaled_last * 4.0;
        float output_brightness = waveform_last[i];
        output_brightness = 0.5 + output_brightness * 0.5;  // Bias to 0.5
        output_brightness *= peak;

        leds[i] = CRGB(sum_color_float[0] * output_brightness, ...);
    }
}
```

**Key Insights**:
1. **4-frame history buffer**: Averages 4 frames before smoothing to filter single-frame spikes
2. **Mood-dependent smoothing**: `(0.1 + CONFIG.MOOD * 0.9) * 0.05` - user-controllable, NOT tempo-adaptive
3. **Separate color smoothing**: Color is smoothed independently with 0.05/0.95 mix (very slow)

---

## Critical Design Patterns

### 1. **User-Controlled Motion, Not Tempo-Adaptive**

Sensory Bridge does **NOT** adapt motion speed to tempo. Instead:
- Motion speed is controlled by **MOOD knob** (user preference)
- Bloom mode: `position = 0.250 + 1.750 * CONFIG.MOOD`
- Waveform mode: `smoothing = (0.1 + CONFIG.MOOD * 0.9) * 0.05`

**Implication**: Our approach of tempo-adaptive time constants is **fundamentally different** from Sensory Bridge's design philosophy.

### 2. **Frame Persistence, Not Exponential Decay**

Bloom mode uses **alpha blending** (0.99 alpha) to create persistence:
- Previous frame is drawn at shifted position with 99% opacity
- Creates smooth expansion through **frame accumulation**
- **NOT** exponential decay with fixed time constants

**Implication**: Our `AsymmetricFollower` with time constants is a different approach. Sensory Bridge uses frame-based persistence.

### 3. **Multi-Stage Smoothing Pipeline**

Audio data is smoothed **multiple times** before visual rendering:
1. Low-pass filter (mood-dependent cutoff)
2. Peak tracking (asymmetric attack/release)
3. Spectrogram smoothing (symmetric 0.75)
4. Chromagram aggregation
5. Final smoothing in visual mode (mood-dependent)

**Implication**: We may need **more smoothing stages** in our pipeline, not just one AsymmetricFollower.

### 4. **History Buffers for Spike Filtering**

Waveform mode uses **4-frame rolling average** before smoothing:
- Filters single-frame audio spikes
- Prevents jittery motion from transient noise

**Implication**: Our history buffer approach is correct, but we may need to apply it **before** the AsymmetricFollower, not after.

### 5. **Symmetric Smoothing in Spectrogram**

`get_smooth_spectrogram()` uses **symmetric** 0.75 coefficient for both rise and fall:
- NOT asymmetric like we assumed
- Simple linear interpolation, not exponential decay

**Implication**: The smoothing is simpler than we thought - just linear interpolation with 75% coefficient.

---

## Comparison with Our Current Implementation

### What We're Doing Wrong

1. **Fixed time constants**: We use `riseTau = 0.12s`, `fallTau = 0.40s` - Sensory Bridge doesn't use fixed time constants for motion
2. **Tempo-adaptive smoothing**: We try to adapt to BPM - Sensory Bridge uses user-controlled MOOD
3. **Single smoothing stage**: We use one AsymmetricFollower - Sensory Bridge uses multiple stages
4. **Exponential decay**: We use true exponential decay - Sensory Bridge uses frame persistence (alpha blending)

### What We're Doing Right

1. **History buffers**: We use 4-frame rolling average - matches Waveform mode
2. **Asymmetric peak tracking**: We could use this for adaptive normalization
3. **Multi-stage pipeline**: We have audio → history → follower, which is similar

---

## Recommendations

### 1. **Remove Tempo-Adaptive Time Constants**

Sensory Bridge doesn't adapt motion to tempo. Instead:
- Use **user-controllable smoothing** (via mood/intensity parameter)
- Or use **fixed time constants** that feel good, not tempo-adaptive

### 2. **Consider Frame Persistence Instead of Exponential Decay**

For Bloom-like effects:
- Use **alpha blending** with previous frame (like `draw_sprite()`)
- Position shift controlled by audio level, not time constants
- Creates smoother, more organic motion

### 3. **Add More Smoothing Stages**

Consider:
1. Low-pass filter on raw audio (mood-dependent)
2. Peak tracking for adaptive normalization
3. History buffer (4-frame average)
4. Final smoothing (mood-dependent)

### 4. **Use Symmetric Smoothing for Spectrogram**

The spectrogram smoothing is symmetric (0.75), not asymmetric. Consider using symmetric smoothing for audio data, and asymmetric only for visual motion.

---

## Conclusion

Sensory Bridge's audio-to-visual pipeline is **fundamentally different** from our current approach:

- **Motion**: User-controlled (MOOD), not tempo-adaptive
- **Persistence**: Frame-based (alpha blending), not exponential decay
- **Smoothing**: Multi-stage pipeline, not single AsymmetricFollower
- **Spike filtering**: History buffers before smoothing, not after

Our current implementation may be **over-engineered** for the problem. Sensory Bridge achieves smooth, fluid motion through simpler mechanisms: frame persistence, multi-stage smoothing, and user-controlled parameters.

