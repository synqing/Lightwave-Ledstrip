# Audio-to-Visual Pipeline Research: Sensory Bridge Analysis & LightwaveOS Comparison

## Document Status: ⚠️ CORRECTED - SECOND PASS INVESTIGATION COMPLETE

**Research Date:** December 31, 2025
**Purpose:** Deep technical analysis of professional audio-reactive LED visualization systems to inform LightwaveOS v2 effect design architecture.
**Scope:** Sensory Bridge firmware v3.1.0, v4.0.0, v4.1.0, v4.1.1 + LightwaveOS v2 ControlBus audit

---

# PART 1: SENSORY BRIDGE ARCHITECTURE ANALYSIS

## 1.1 Executive Summary (CORRECTED)

Sensory Bridge is an ESP32-based music visualization system created by Lixie Labs. After comprehensive source code analysis across 4 firmware versions, **including a critical second-pass investigation**, the following insights were discovered:

### ⚠️ CRITICAL CORRECTION: Audio-Motion Coupling EXISTS - But Not Where Expected

**User Observation:** "Without audio input, bloom sprites do NOT expand/propagate."
**Finding:** This is CORRECT. But the mechanism is multi-layered:

## THE COMPLETE AUDIO-MOTION COUPLING ARCHITECTURE

### Layer 1: Global Brightness Gate (`silent_scale`)
**Location:** `i2s_audio.h:168-186` and `led_utilities.h:211`

```cpp
// i2s_audio.h:180 - SILENCE DETECTION
float silent_scale_raw = 1.0 - silence;  // silence=true → 0.0
silent_scale = silent_scale_raw * 0.1 + silent_scale_last * 0.9;

// led_utilities.h:211 - BRIGHTNESS APPLICATION
brightness = MASTER_BRIGHTNESS * (CONFIG.PHOTONS * CONFIG.PHOTONS) * silent_scale;
//                                                                   ^^^^^^^^^^^^
//                                               ALL LEDs multiplied by this!
```

**Effect:** After 10 seconds of silence, `silent_scale → 0.0`, making ALL LED output BLACK.
Even if motion calculations continue, the OUTPUT is invisible.

### Layer 2: Chromagram-Dependent Color Injection
**Location:** `lightshow_modes.h` Bloom implementation

```cpp
for (uint8_t i = 0; i < 12; i++) {
    SQ15x16 bin = chromagram_smooth[i];  // From audio analysis
    CRGB16 add_color = hsv(prog, CONFIG.SATURATION, bin*bin * share);
    sum_color += add_color;  // Color is AUDIO-DEPENDENT
}
leds_16[63] = sum_color;  // CENTER INJECTION
leds_16[64] = sum_color;
```

**Effect:** No audio → chromagram values = 0 → sum_color = BLACK → nothing propagates outward because there's nothing to propagate.

### Layer 3: Mode-Specific Motion (Varies by Mode)

| Mode | Motion Speed Source | Motion Position Source | Audio Coupling |
|------|---------------------|------------------------|----------------|
| **Bloom** | `CONFIG.MOOD` (user) | Time accumulation | NO direct coupling |
| **Kaleidoscope** | `CONFIG.MOOD` | `pos += shift_speed * spectral_sum` | **YES - spectral energy** |
| **VU Dot** | N/A | `audio_vu_level_smooth` | **YES - direct VU** |
| **Waveform** | N/A | `waveform[]` samples | **YES - time domain** |

### THE COMBINED EFFECT (Why "No Audio = No Motion")

```
┌────────────────────────────────────────────────────────────────┐
│                     WITHOUT AUDIO INPUT                         │
├────────────────────────────────────────────────────────────────┤
│ 1. chromagram_smooth[12] = {0,0,0,0,0,0,0,0,0,0,0,0}           │
│    → Color injection at center = BLACK                          │
│                                                                 │
│ 2. After 10 seconds: silence = true                             │
│    → silent_scale ramps down from 1.0 → 0.0                     │
│    → Global brightness multiplier kills ALL output              │
│                                                                 │
│ 3. For Kaleidoscope: spectral_sum = 0                           │
│    → Position accumulation stops (pos += 0)                     │
│    → Motion LITERALLY FREEZES (not just invisible)              │
├────────────────────────────────────────────────────────────────┤
│ RESULT: No visible motion - sprites don't propagate             │
│         Motion math may run, but nothing visible to see         │
└────────────────────────────────────────────────────────────────┘
```

### REVISED Summary Table

| Visual Aspect | Audio-Derived? | Actual Source | Notes |
|--------------|----------------|---------------|-------|
| **Color/Hue** | YES | Chromagram (12 pitch classes) | No audio = black |
| **Brightness** | YES | Per-note magnitude² | No audio = zero |
| **Global Output** | YES | `silent_scale` gate | 10s timeout → fade to black |
| **Motion Speed (Bloom)** | NO* | `CONFIG.MOOD` (user) | *But invisible without audio |
| **Motion Speed (Kaleidoscope)** | YES | `spectral_sum * shift_speed` | Direct coupling |
| **Motion Direction** | NO | Always center→outward | Hardcoded |

**Critical Implication:** Sensory Bridge has MULTIPLE INTERACTING SYSTEMS:
1. Mode-specific logic (some audio-coupled, some not)
2. Global brightness gating (`silent_scale`)
3. Color intensity dependent on spectral content

The user's observation was CORRECT. The system requires audio for visible output, even when motion speed itself isn't audio-reactive.

---

## 1.2 Audio Pipeline Architecture

### 1.2.1 Hardware Configuration

```
MICROPHONE:      SPH0645 MEMS I2S microphone
SAMPLE RATE:     12,800 Hz (non-standard, optimized for musical range)
BIT DEPTH:       32-bit (I2S native)
DMA BUFFERS:     2 × 96 samples = 192 samples total
CHUNK SIZE:      96 samples per audio frame
AUDIO FRAME RATE: 12,800 ÷ 96 = 133.33 Hz (~7.5ms per frame)
```

### 1.2.2 Complete Sample Flow Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                    I2S MICROPHONE INPUT                         │
│                    SPH0645 @ 12,800 Hz                          │
└───────────────────────────┬─────────────────────────────────────┘
                            │ 96 samples per DMA transfer
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│              SAMPLE ACQUISITION (i2s_audio.h)                   │
│                                                                 │
│  Raw I2S Processing:                                            │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ sample = (i2s_samples_raw[i] * 0.000512) + 56000 - 5120  │  │
│  │ sample = sample >> 2  // Fixed-point prep                │  │
│  │ sample *= CONFIG.SENSITIVITY  // Gain adjustment         │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  DC Offset Removal:                                             │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ waveform[i] = sample - CONFIG.DC_OFFSET                  │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  Clipping Protection:                                           │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ if (sample > 32767)  sample = 32767;                     │  │
│  │ if (sample < -32767) sample = -32767;                    │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  Waveform History (4-frame circular buffer):                    │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ waveform_history[waveform_history_index][i] = waveform[i]│  │
│  │ waveform_history_index = (index + 1) % 4                 │  │
│  └──────────────────────────────────────────────────────────┘  │
└───────────────────────────┬─────────────────────────────────────┘
                            │
              ┌─────────────┼─────────────┐
              ▼             ▼             ▼
┌─────────────────┐ ┌─────────────┐ ┌─────────────────────────────┐
│ VU CALCULATION  │ │ PEAK TRACK  │ │ SLIDING WINDOW (4096 samp.) │
│ (RMS-based)     │ │ (envelope)  │ │ For Goertzel processing     │
└────────┬────────┘ └──────┬──────┘ └──────────────┬──────────────┘
         │                 │                        │
         ▼                 ▼                        ▼
┌─────────────────────────────────────────────────────────────────┐
│           GOERTZEL-BASED DISCRETE FOURIER TRANSFORM             │
│                        (GDFT.h)                                 │
│                                                                 │
│  NOT FFT! 64 PARALLEL GOERTZEL FILTERS                          │
│                                                                 │
│  Frequency Targets: Piano keys from A1 (55 Hz) to C7 (2093 Hz)  │
│  Spacing: Equal temperament (semitone-spaced)                   │
│  Per-bin window: 100-2000 samples (adaptive to frequency)       │
│  A-weighting: Psychoacoustic equal-loudness curve applied       │
│                                                                 │
│  Goertzel Algorithm (per frequency bin):                        │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ for (n = 0; n < block_size; n++) {                       │  │
│  │   sample = sample_window[HISTORY_LENGTH - 1 - n]         │  │
│  │   mult = coeff_q14 * q1                                  │  │
│  │   q0 = (sample >> 6) + (mult >> 14) - q2                 │  │
│  │   q2 = q1; q1 = q0                                       │  │
│  │ }                                                        │  │
│  │ magnitude = sqrt(q2² + q1² - (coeff*q2))                 │  │
│  │ magnitude_normalized = magnitude / (block_size / 2.0)    │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  Coefficient Precomputation:                                    │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ w = 2π * k / block_size                                  │  │
│  │ coeff = 2 * cos(w)                                       │  │
│  │ coeff_q14 = (1 << 14) * coeff  // Q14 fixed-point        │  │
│  └──────────────────────────────────────────────────────────┘  │
└───────────────────────────┬─────────────────────────────────────┘
                            │ 64 frequency bin magnitudes
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│              EXPONENTIAL MOVING AVERAGE (α=0.30)                │
│  magnitudes_avg[i] = current * 0.3 + previous * 0.7             │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                    NOISE FLOOR SUBTRACTION                      │
│                                                                 │
│  Calibration (256 frames at startup):                           │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ noise_samples[i] = max(magnitudes_avg[i]) during silence │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  Runtime Application:                                           │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ magnitudes_avg[i] -= noise_samples[i] * 1.5              │  │
│  │ if (result < 0.0) result = 0.0                           │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  Safety Margin: 1.5x amplification prevents low-level artifacts │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│              DYNAMIC RANGE NORMALIZATION                        │
│                                                                 │
│  Asymmetric Envelope Follower:                                  │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ max_value = max(magnitudes_final[]) * 0.995  // Hysteresis│  │
│  │                                                          │  │
│  │ if (max_value > goertzel_max_value)                      │  │
│  │   goertzel_max_value += delta * 0.0050  // FAST attack   │  │
│  │ else                                                     │  │
│  │   goertzel_max_value -= delta * 0.0025  // SLOW release  │  │
│  │                                                          │  │
│  │ multiplier = 1.0 / goertzel_max_value                    │  │
│  │ spectrogram[i] = magnitudes_final[i] * multiplier        │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  Output: spectrogram[64] normalized to [0.0, 1.0] range         │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
╔═════════════════════════════════════════════════════════════════╗
║            LED RENDERING THREAD (Core 1, ~120 FPS)              ║
║                  INDEPENDENT OF AUDIO THREAD                    ║
╚═════════════════════════════════════════════════════════════════╝
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│            PER-BIN SMOOTHING (get_smooth_spectrogram)           │
│                                                                 │
│  Fast Attack/Release (α=0.75):                                  │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ if (spectrogram_smooth[i] < spectrogram[i])              │  │
│  │   spectrogram_smooth[i] += distance * 0.75  // Attack    │  │
│  │ else                                                     │  │
│  │   spectrogram_smooth[i] -= distance * 0.75  // Release   │  │
│  └──────────────────────────────────────────────────────────┘  │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│          CHROMAGRAM AGGREGATION (make_smooth_chromagram)        │
│                                                                 │
│  64 frequency bins → 12 pitch classes (octave folding):         │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ memset(chromagram_smooth, 0, sizeof(SQ15x16) * 12)       │  │
│  │                                                          │  │
│  │ for (i = 0; i < CONFIG.CHROMAGRAM_RANGE; i++) {  // 60   │  │
│  │   note_magnitude = spectrogram_smooth[i]                 │  │
│  │   chroma_bin = i % 12  // Fold octaves                   │  │
│  │   chromagram_smooth[chroma_bin] += note_magnitude /      │  │
│  │                                    (CHROMAGRAM_RANGE/12) │  │
│  │ }                                                        │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  Peak Normalization (adaptive, asymmetric):                     │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ max_peak *= 0.999  // Slow decay                         │  │
│  │ if (chromagram_smooth[i] > max_peak)                     │  │
│  │   max_peak += (value - max_peak) * 0.05  // Slow attack  │  │
│  │                                                          │  │
│  │ multiplier = 1.0 / max_peak                              │  │
│  │ chromagram_smooth[i] *= multiplier  // Normalize [0,1]   │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  Pitch Classes (Western 12-TET):                                │
│  [0]=C, [1]=C#, [2]=D, [3]=D#, [4]=E, [5]=F                    │
│  [6]=F#, [7]=G, [8]=G#, [9]=A, [10]=A#, [11]=B                 │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                VISUALIZATION MODE DISPATCH                      │
│                                                                 │
│  ├─ LIGHT_MODE_GDFT:       Uses spectrogram_smooth[64]          │
│  ├─ LIGHT_MODE_CHROMAGRAM: Uses chromagram_smooth[12]           │
│  ├─ LIGHT_MODE_BLOOM:      Uses chromagram → color, time motion │
│  ├─ LIGHT_MODE_VU:         Uses audio_vu_level                  │
│  ├─ LIGHT_MODE_VU_DOT:     Uses audio_vu_level + physics        │
│  ├─ LIGHT_MODE_WAVEFORM:   Uses waveform_history[4][1024]       │
│  └─ LIGHT_MODE_KALEIDOSCOPE: Uses novelty + Perlin noise        │
└─────────────────────────────────────────────────────────────────┘
```

---

## 1.3 Bloom Mode: Complete Algorithm Deep Dive

### 1.3.1 Overview

Bloom mode is Sensory Bridge's signature effect - an expanding color burst that creates a "Stargate Sequence" aesthetic. It is the MOST IMPORTANT mode to understand because it demonstrates the separation between audio-reactive color and time-based motion.

**File Location:** `lightshow_modes.h` lines 398-499 (identical across v4.0.0, v4.1.0, v4.1.1)

### 1.3.2 Complete Annotated Implementation

```cpp
void light_mode_bloom() {
    // ════════════════════════════════════════════════════════════
    // STEP 1: SPRITE MOTION (AUDIO-INDEPENDENT!)
    // ════════════════════════════════════════════════════════════
    //
    // Motion speed is controlled ONLY by CONFIG.MOOD (user parameter)
    // Audio has ZERO influence on motion speed or direction
    //
    // position range: 0.250 (slow) to 2.0 (fast) LEDs per frame
    // At 120 FPS: 30-240 LEDs/second expansion rate

    memset(leds_16, 0, sizeof(CRGB16) * NATIVE_RESOLUTION);

    float position = 0.250 + 1.750 * CONFIG.MOOD;  // USER-CONTROLLED SPEED

    // draw_sprite() shifts previous frame outward by 'position' LEDs
    // alpha=0.99 creates exponential decay trail (1% loss per frame)
    draw_sprite(leds_16, leds_16_prev, 128, 128, position, 0.99);

    // ════════════════════════════════════════════════════════════
    // STEP 2: COLOR CALCULATION FROM CHROMAGRAM (AUDIO-REACTIVE)
    // ════════════════════════════════════════════════════════════
    //
    // Color IS derived from audio: each of 12 pitch classes
    // contributes its energy to a summed RGB color

    CRGB16 sum_color = {0, 0, 0};
    SQ15x16 share = 1.0 / 6.0;  // Divide total brightness among notes

    for (uint8_t i = 0; i < 12; i++) {
        float prog = i / 12.0;  // 0.0 to 0.9167 (hue position)

        // Read chromagram energy for this pitch class
        SQ15x16 bin = chromagram_smooth[i];

        // Convert pitch class to hue: C=0°, C#=30°, D=60°, ... B=330°
        // Saturation from CONFIG, brightness = energy² (non-linear)
        CRGB16 add_color = hsv(prog, CONFIG.SATURATION, bin * bin * share);

        // Accumulate color contributions
        sum_color.r += add_color.r;
        sum_color.g += add_color.g;
        sum_color.b += add_color.b;
    }

    // Clip to valid range [0, 1]
    if (sum_color.r > 1.0) sum_color.r = 1.0;
    if (sum_color.g > 1.0) sum_color.g = 1.0;
    if (sum_color.b > 1.0) sum_color.b = 1.0;

    // ════════════════════════════════════════════════════════════
    // STEP 3: CONTRAST BOOST (Repeated Squaring)
    // ════════════════════════════════════════════════════════════
    //
    // Squaring the RGB values multiple times increases contrast
    // CONFIG.SQUARE_ITER = 1: color² (moderate contrast)
    // CONFIG.SQUARE_ITER = 2: color⁴ (high contrast)
    // CONFIG.SQUARE_ITER = 3: color⁸ (extreme contrast)

    for (uint8_t i = 0; i < CONFIG.SQUARE_ITER; i++) {
        sum_color.r *= sum_color.r;
        sum_color.g *= sum_color.g;
        sum_color.b *= sum_color.b;
    }

    // Optional: Force saturation on final color
    CRGB temp_col = {uint8_t(sum_color.r*255), uint8_t(sum_color.g*255),
                     uint8_t(sum_color.b*255)};
    temp_col = force_saturation(temp_col, 255 * CONFIG.SATURATION);

    // Chromatic vs Monochromatic mode handling
    if (chromatic_mode == false) {
        SQ15x16 led_hue = chroma_val + hue_position + (sqrt(1.0) * 0.05);
        temp_col = force_hue(temp_col, 255 * float(led_hue));
    }

    // ════════════════════════════════════════════════════════════
    // STEP 4: WRITE NEW COLOR TO CENTER (Bloom Origin)
    // ════════════════════════════════════════════════════════════
    //
    // New audio-reactive color injected at LEDs 63-64 (center)
    // Previous colors have already been shifted outward by draw_sprite()

    leds_16[63] = {temp_col.r/255.0, temp_col.g/255.0, temp_col.b/255.0};
    leds_16[64] = leds_16[63];  // Symmetric center pair

    // ════════════════════════════════════════════════════════════
    // STEP 5: SAVE CURRENT FRAME FOR NEXT ITERATION
    // ════════════════════════════════════════════════════════════

    memcpy(leds_16_prev, leds_16, sizeof(CRGB16) * NATIVE_RESOLUTION);

    // ════════════════════════════════════════════════════════════
    // STEP 6: SPATIAL FALLOFF (Quadratic Edge Fade)
    // ════════════════════════════════════════════════════════════
    //
    // Fade outer 32 LEDs using quadratic curve for smooth edge
    // This creates the "bloom" shape with soft boundaries

    for (uint8_t i = 0; i < 32; i++) {
        float prog = i / 31.0;          // 0.0 to 1.0
        float falloff = prog * prog;     // Quadratic: 0, 0.001, 0.004, ..., 1.0

        leds_16[128 - 1 - i].r *= falloff;
        leds_16[128 - 1 - i].g *= falloff;
        leds_16[128 - 1 - i].b *= falloff;
    }

    // ════════════════════════════════════════════════════════════
    // STEP 7: MIRROR (Create Symmetric Display)
    // ════════════════════════════════════════════════════════════
    //
    // Copy right half to left half (LEDs 0-63 mirror LEDs 64-127)

    for (uint8_t i = 0; i < 64; i++) {
        leds_16[i] = leds_16[128 - 1 - i];
    }
}
```

### 1.3.3 Sprite Motion Function (Sub-Pixel Expansion)

**File Location:** `led_utilities.h` lines 1247-1290

```cpp
void draw_sprite(CRGB16* dest, CRGB16* sprite,
                 uint16_t dest_length, uint16_t sprite_length,
                 float position, float alpha) {

    // ════════════════════════════════════════════════════════════
    // SUB-PIXEL INTERPOLATION
    // ════════════════════════════════════════════════════════════
    //
    // Fractional LED positioning enables smooth motion at any speed
    // E.g., position=0.7 means 70% of pixel goes to integer position,
    //       30% bleeds into next position

    int32_t position_whole = (int32_t)position;  // Integer part
    float position_fract = position - position_whole;  // Fractional part

    SQ15x16 mix_right = position_fract;
    SQ15x16 mix_left = 1.0 - position_fract;

    // ════════════════════════════════════════════════════════════
    // SHIFT PREVIOUS FRAME OUTWARD
    // ════════════════════════════════════════════════════════════
    //
    // Each LED value moves outward by 'position' LEDs
    // Values are blended between integer positions for smoothness

    for (uint16_t i = 0; i < sprite_length; i++) {
        // Calculate destination positions
        int16_t pos_left = i + position_whole;
        int16_t pos_right = i + position_whole + 1;

        // Bounds checking
        if (pos_left < 0 || pos_left >= dest_length) continue;
        if (pos_right >= dest_length) pos_right = dest_length - 1;

        // Blend sprite pixel into destination with sub-pixel interpolation
        dest[pos_left].r += sprite[i].r * mix_left * alpha;
        dest[pos_left].g += sprite[i].g * mix_left * alpha;
        dest[pos_left].b += sprite[i].b * mix_left * alpha;

        dest[pos_right].r += sprite[i].r * mix_right * alpha;
        dest[pos_right].g += sprite[i].g * mix_right * alpha;
        dest[pos_right].b += sprite[i].b * mix_right * alpha;
    }
}
```

---

## 1.4 Smoothing Architecture (Critical for Responsiveness)

### 1.4.1 The Universal Pattern

Sensory Bridge uses exponential moving average (EMA) everywhere:

```cpp
// The pattern that appears dozens of times in the codebase
value_smooth = value_new * alpha + value_smooth * (1.0 - alpha);

// Equivalent form:
value_smooth += (value_new - value_smooth) * alpha;
```

### 1.4.2 Smoothing Constants Reference Table

| Processing Stage | Alpha (α) | Time Constant* | Frames to 90% | Purpose |
|-----------------|-----------|----------------|---------------|---------|
| **Goertzel Output** | 0.30 | ~3 frames | ~7 | Initial spectral noise reduction |
| **Spectrogram Smooth** | 0.75 | ~1.3 frames | ~3 | Fast attack for transients |
| **Chromagram Peak (attack)** | 0.05 | ~20 frames | ~45 | Slow color drift |
| **Chromagram Peak (release)** | 0.001 | ~1000 frames | ~2300 | Very slow fade |
| **Goertzel Max (attack)** | 0.0050 | ~200 frames | ~460 | Slow AGC adaptation |
| **Goertzel Max (release)** | 0.0025 | ~400 frames | ~920 | Very slow AGC decay |
| **Waveform Display** | 0.005-0.05 | MOOD-dependent | Variable | User control |
| **Sprite Decay** | 0.99 | ~100 frames | ~230 | Motion blur trail |

*Time constant = 1/α at this frame rate

### 1.4.3 Asymmetric Envelope Pattern

```cpp
// This pattern appears in AGC, peak tracking, and normalization
if (new_value > tracked_value) {
    // FAST ATTACK: Respond quickly to increases
    tracked_value += (new_value - tracked_value) * ATTACK_RATE;
} else {
    // SLOW RELEASE: Decay slowly on decreases
    tracked_value -= (tracked_value - new_value) * RELEASE_RATE;
}
```

**Typical ratios:**
- **Attack** : **Release** = 2:1 to 10:1 (attack is 2-10x faster)
- **Example:** Attack=0.05, Release=0.0025 → 20:1 ratio

**Why asymmetric?**
- Fast attack catches musical transients (drum hits, note onsets)
- Slow release prevents flicker during sustained notes or silence
- Creates visually pleasing "punch" followed by smooth fade

---

## 1.5 Waveform Mode: Time-Domain Display

### 1.5.1 What Makes It Unique

Waveform is the **only mode that displays time-domain audio directly**. Instead of frequency analysis, it renders the raw audio waveform's amplitude as LED brightness across the strip.

**File Location:** `lightshow_modes.h` lines 160-260

### 1.5.2 Complete Implementation

```cpp
void light_mode_waveform() {
    const float led_share = 255 / float(12);

    // ════════════════════════════════════════════════════════════
    // STEP 1: PEAK TRACKING (Heavy Smoothing)
    // ════════════════════════════════════════════════════════════
    //
    // 95:5 smoothing ratio creates very slow response
    // This prevents the waveform from "jumping" with volume changes

    waveform_peak_scaled_last = (waveform_peak_scaled * 0.05 +
                                  waveform_peak_scaled_last * 0.95);

    // ════════════════════════════════════════════════════════════
    // STEP 2: COLOR FROM CHROMAGRAM (Same as Bloom)
    // ════════════════════════════════════════════════════════════

    CRGB sum_color = CRGB(0, 0, 0);
    float brightness_sum = 0.0;

    for (uint8_t c = 0; c < 12; c++) {
        float prog = c / float(12);
        float bin = note_chromagram[c] * (1.0 / chromagram_max_val);

        // Non-linear brightness: bin²
        float bright = bin;
        for (uint8_t s = 0; s < CONFIG.SQUARE_ITER; s++) {
            bright *= bright;
        }
        bright *= 1.5;
        if (bright > 1.0) bright = 1.0;
        bright *= led_share;

        if (chromatic_mode == true) {
            hsv2rgb_spectrum(CHSV(255 * prog, 255, bright), out_col);
            sum_color += out_col;
        } else {
            brightness_sum += bright;
        }
    }

    // ════════════════════════════════════════════════════════════
    // STEP 3: COLOR SMOOTHING (Very Slow: 95:5 ratio)
    // ════════════════════════════════════════════════════════════
    //
    // Only 5% new color per frame = ~20 frame half-life
    // This creates gentle color drift without sudden changes

    sum_color_float[0] = sum_color_float[0] * 0.05 + sum_color_last[0] * 0.95;
    sum_color_float[1] = sum_color_float[1] * 0.05 + sum_color_last[1] * 0.95;
    sum_color_float[2] = sum_color_float[2] * 0.05 + sum_color_last[2] * 0.95;

    // ════════════════════════════════════════════════════════════
    // STEP 4: WAVEFORM RENDERING WITH 4-TAP FILTER
    // ════════════════════════════════════════════════════════════
    //
    // Multi-frame averaging creates ultra-smooth trace
    // 4 historical frames are averaged before display

    for (uint8_t i = 0; i < NATIVE_RESOLUTION; i++) {
        // 4-tap temporal filter
        float waveform_sample = 0.0;
        for (uint8_t s = 0; s < 4; s++) {
            waveform_sample += waveform_history[s][i];
        }
        waveform_sample /= 4.0;

        // Normalize to [-1, 1] range
        float input_wave_sample = (waveform_sample / 128.0);

        // ════════════════════════════════════════════════════════
        // STEP 5: MOOD-DEPENDENT SMOOTHING
        // ════════════════════════════════════════════════════════
        //
        // CONFIG.MOOD = 0.0 → smoothing = 0.1 * 0.05 = 0.005 (frozen)
        // CONFIG.MOOD = 1.0 → smoothing = 1.0 * 0.05 = 0.05 (reactive)

        float smoothing = (0.1 + CONFIG.MOOD * 0.9) * 0.05;
        waveform_last[i] = input_wave_sample * smoothing +
                           waveform_last[i] * (1.0 - smoothing);

        // ════════════════════════════════════════════════════════
        // STEP 6: PEAK ENVELOPE MULTIPLICATION
        // ════════════════════════════════════════════════════════
        //
        // Overall brightness tracks audio level
        // Quiet passages → dim display
        // Loud passages → bright display

        float peak = waveform_peak_scaled_last * 4.0;
        if (peak > 1.0) peak = 1.0;

        // Stretch waveform to fill [0.5, 1.0] for visibility
        float output_brightness = waveform_last[i];
        output_brightness = 0.5 + output_brightness * 0.5;
        if (output_brightness > 1.0) output_brightness = 1.0;
        else if (output_brightness < 0.0) output_brightness = 0.0;

        output_brightness *= peak;  // Apply overall envelope

        // ════════════════════════════════════════════════════════
        // STEP 7: FINAL RENDERING
        // ════════════════════════════════════════════════════════

        leds[i] = CRGB(
            sum_color_float[0] * output_brightness,
            sum_color_float[1] * output_brightness,
            sum_color_float[2] * output_brightness
        );
    }
}
```

---

## 1.6 Mode Inventory Summary (CORRECTED)

### Audio-Motion Coupling by Mode

| Mode | Audio Input | Color Driven By | Motion Driven By | Audio→Motion? |
|------|-------------|-----------------|------------------|---------------|
| **GDFT** | spectrogram[64] | Spectral bins | Static positions | NO |
| **GDFT Chromagram** | chromagram[12] | 12 pitch classes | Static positions | NO |
| **Bloom** | chromagram[12] | Pitch classes | `CONFIG.MOOD` (user) | NO* |
| **Bloom Fast** | chromagram[12] | Pitch classes | `CONFIG.MOOD` (user) | NO* |
| **VU** | audio_vu_level | VU level | Static anchor | NO |
| **VU Dot** | audio_vu_level | VU level | `audio_vu_level_smooth` | **YES** |
| **Waveform** | waveform[1024] | Chromagram | Waveform samples | **YES** |
| **Kaleidoscope** | spectral bands | Low/mid/high | `pos += spectral_sum` | **YES** |

*Bloom: Motion speed is user-controlled, but OUTPUT is gated by `silent_scale`

### Kaleidoscope: The Audio→Motion Mode

**File Location:** `lightshow_modes.h` lines 224-341

Kaleidoscope is the **only mode with direct spectral→position coupling**:

```cpp
// Sum spectral bands into low/mid/high energy
SQ15x16 sum_low = 0, sum_mid = 0, sum_high = 0;
for (uint8_t i = 0; i < 20; i++) sum_low += spectrogram_smooth[i];      // 0-19
for (uint8_t i = 20; i < 40; i++) sum_mid += spectrogram_smooth[i];     // 20-39
for (uint8_t i = 40; i < 60; i++) sum_high += spectrogram_smooth[i];    // 40-59

// Calculate position shift based on spectral energy
SQ15x16 shift_speed = 100 + (500 * CONFIG.MOOD);  // User speed + base
SQ15x16 shift_r = (shift_speed * sum_low);
SQ15x16 shift_g = (shift_speed * sum_mid);
SQ15x16 shift_b = (shift_speed * sum_high);

// ACCUMULATE POSITION (this is the audio→motion coupling!)
pos_r += (float)shift_r;  // Position changes based on audio energy
pos_g += (float)shift_g;
pos_b += (float)shift_b;

// Use positions as Perlin noise offsets
SQ15x16 r_val = inoise16(i_scaled * 0.5 + y_pos_r) / 65536.0;
```

**Key Insight:** In silence:
- `sum_low = sum_mid = sum_high = 0`
- `shift_r = shift_g = shift_b = 0`
- `pos_r += 0` → Position FROZEN
- Perlin noise pattern becomes STATIC

This is fundamentally different from Bloom, where motion continues (but is invisible due to `silent_scale`).

### VU Dot: Direct Position Mapping

```cpp
// Position IS the VU level (normalized)
SQ15x16 dot_pos = (audio_vu_level_smooth * multiplier);
set_dot_position(RESERVED_DOTS + 0, dot_pos_smooth * 0.5 + 0.5);
```

No audio → `audio_vu_level_smooth = 0` → Dot at center (no movement).

---

## 1.7 Novelty/Onset Detection

**File Location:** `GDFT.h` lines 201-242

```cpp
void calculate_novelty() {
    novelty_now = 0.0;

    // Compare current frame to previous frame
    for (i = 0; i < NUM_FREQS; i++) {
        // Only positive changes count (onset, not sustain/release)
        novelty_bin = spectrogram[i] - spectral_history[previous_index][i];
        if (novelty_bin < 0.0) novelty_bin = 0.0;

        novelty_now += novelty_bin;
    }

    novelty_now /= NUM_FREQS;  // Normalize

    // Store in circular buffer
    spectral_history[spectral_history_index][] = current spectrogram;
    novelty_curve[spectral_history_index] = sqrt(novelty_now);
}
```

**Note:** This is NOT beat detection - just spectral change detection. Used as secondary input for modes like Kaleidoscope.

---

## 1.8 Three-Level Automatic Gain Control (AGC)

Sensory Bridge uses three cascaded AGC systems:

### Level 1: Noise Floor Calibration (One-Time)

```
Duration: 256 frames at startup (~2 seconds)
Method: Record maximum magnitude per frequency bin during silence
Application: Subtract noise_samples[i] * 1.5 from all future magnitudes
Purpose: Eliminates low-level background noise from triggering visuals
```

### Level 2: Per-Frame Envelope Follower

```
Attack rate: 0.25 (fast - responds in ~4 frames)
Release rate: 0.005 (slow - decays over ~200 frames)
Purpose: Tracks overall audio level for VU-style effects
Output: waveform_peak_scaled (0.0-1.0)
```

### Level 3: Spectral Normalization

```
Attack rate: 0.0050 (very slow - ~200 frame constant)
Release rate: 0.0025 (extremely slow - ~400 frame constant)
Purpose: Maintains spectrogram in [0, 1] range regardless of input level
Output: Normalized spectrogram[64]
```

---

## 1.9 ⚠️ CRITICAL: `silent_scale` Global Brightness Gate (Second-Pass Finding)

**THIS IS THE PRIMARY MECHANISM THAT PREVENTS VISIBLE MOTION WITHOUT AUDIO**

### Overview

The `silent_scale` system is a **global brightness multiplier** applied to ALL LED output. When audio is absent for 10+ seconds, this value ramps down to zero, making ALL visual output BLACK—regardless of what's happening in the rendering pipeline.

**File Locations:**
- Silence detection: `i2s_audio.h:135-186`
- Brightness application: `led_utilities.h:211-217`
- Global variable: `globals.h`

### Complete Implementation

#### Step 1: Silence Detection (`i2s_audio.h:135-175`)

```cpp
// Sweet spot state tracking
if (max_waveform_val_raw <= CONFIG.SWEET_SPOT_MIN_LEVEL * 0.95) {
    sweet_spot_state = -1;  // SILENT
    if (sweet_spot_state_last != -1) {  // Just became silent
        silence_temp = true;
        silence_switched = t_now;  // Start 10-second timer
    }
}
else if (max_waveform_val_raw >= CONFIG.SWEET_SPOT_MAX_LEVEL) {
    sweet_spot_state = 1;   // LOUD
}
else {
    sweet_spot_state = 0;   // MID (acceptable)
}

// 10-SECOND DEBOUNCE before declaring silence
if (silence_temp == true) {
    if (t_now - silence_switched >= 10000) {  // 10,000 ms = 10 seconds
        silence = true;  // Now officially SILENT
    }
}
```

**Key Points:**
- `CONFIG.SWEET_SPOT_MIN_LEVEL = 750` (default threshold)
- Must be silent for **10 continuous seconds** before `silence = true`
- Any audio above threshold resets the timer

#### Step 2: `silent_scale` Calculation (`i2s_audio.h:168-186`)

```cpp
if (CONFIG.STANDBY_DIMMING == true) {
    // Convert boolean silence to 0.0-1.0 scale
    float silent_scale_raw = 1.0 - silence;  // silence=true → 0.0, silence=false → 1.0

    // Exponential smoothing with 90% hysteresis (SLOW fade)
    silent_scale = silent_scale_raw * 0.1 + silent_scale_last * 0.9;
    silent_scale_last = silent_scale;
}
else {
    silent_scale = 1.0;  // Standby dimming disabled → always full brightness
}
```

**Smoothing Behavior:**
| Frame | `silent_scale_raw` | `silent_scale` (smoothed) |
|-------|-------------------|---------------------------|
| 0 | 1.0 → 0.0 | 1.0 * 0.9 + 0.0 * 0.1 = 0.90 |
| 1 | 0.0 | 0.90 * 0.9 + 0.0 * 0.1 = 0.81 |
| 2 | 0.0 | 0.81 * 0.9 = 0.729 |
| 10 | 0.0 | ~0.35 |
| 20 | 0.0 | ~0.12 |
| 30 | 0.0 | ~0.04 |
| 50 | 0.0 | ~0.005 (nearly black) |

At ~120 FPS, the fade takes **~400-500ms** after the 10-second silence threshold.

#### Step 3: Brightness Application (`led_utilities.h:211-217`)

```cpp
void apply_brightness() {
    // THE GLOBAL BRIGHTNESS GATE
    SQ15x16 brightness = MASTER_BRIGHTNESS * (CONFIG.PHOTONS * CONFIG.PHOTONS) * silent_scale;
    //                                                                          ^^^^^^^^^^^^
    //                                               ALL LED OUTPUT MULTIPLIED BY THIS!

    for (uint16_t i = 0; i < 128; i++) {
        leds_16[i].r *= brightness;
        leds_16[i].g *= brightness;
        leds_16[i].b *= brightness;
    }
}
```

**This is called in the render loop AFTER effects render**, so even if:
- Bloom is shifting sprites
- Kaleidoscope is updating positions
- VU Dot is calculating levels

...if `silent_scale = 0`, ALL output is BLACK.

### Why This Matters for LightwaveOS

**Without this mechanism:**
- Effects would continue running with stale/zero audio data
- Bloom would show black propagating outward (pointless)
- VU meters would sit at zero but still consume power/attention

**With this mechanism:**
- Clean fade-to-black after silence
- Power saving (LEDs at zero)
- Visual indication that "no music = no show"

### Configuration Variables

| Variable | Default | Location | Effect |
|----------|---------|----------|--------|
| `CONFIG.STANDBY_DIMMING` | `true` | globals.h | Enable/disable silence detection |
| `CONFIG.SWEET_SPOT_MIN_LEVEL` | `750` | globals.h | Silence threshold (lower = more sensitive) |
| `MASTER_BRIGHTNESS` | `1.0` | globals.h | Base brightness multiplier |
| `CONFIG.PHOTONS` | `1.0` | globals.h | User brightness setting |

### Complete Signal Flow

```
┌──────────────────────────────────────────────────────────────────┐
│                    SILENCE DETECTION PIPELINE                     │
├──────────────────────────────────────────────────────────────────┤
│                                                                   │
│  max_waveform_val_raw                                             │
│         │                                                         │
│         ▼                                                         │
│  ┌──────────────────────┐                                         │
│  │ < SWEET_SPOT_MIN_LEVEL? ─── NO ──→ silence_temp = false        │
│  └──────────────────────┘             silence = false             │
│         │ YES                         silent_scale → 1.0          │
│         ▼                                                         │
│  ┌──────────────────────┐                                         │
│  │ Start 10s timer      │                                         │
│  └──────────────────────┘                                         │
│         │                                                         │
│         ▼                                                         │
│  ┌──────────────────────┐                                         │
│  │ t_now - silence_switched >= 10000? ─── NO ──→ Keep waiting    │
│  └──────────────────────┘                                         │
│         │ YES                                                     │
│         ▼                                                         │
│  ┌──────────────────────┐                                         │
│  │ silence = true       │                                         │
│  │ silent_scale_raw=0.0 │                                         │
│  └──────────────────────┘                                         │
│         │                                                         │
│         ▼                                                         │
│  ┌──────────────────────────────────────────────┐                 │
│  │ silent_scale = silent_scale_raw * 0.1        │                 │
│  │             + silent_scale_last * 0.9        │  90% smoothing  │
│  └──────────────────────────────────────────────┘                 │
│         │                                                         │
│         ▼                                                         │
│  ┌──────────────────────────────────────────────┐                 │
│  │ brightness = MASTER * PHOTONS² * silent_scale│                 │
│  └──────────────────────────────────────────────┘                 │
│         │                                                         │
│         ▼                                                         │
│  ┌──────────────────────────────────────────────┐                 │
│  │ ALL LED OUTPUT *= brightness                 │ → BLACK         │
│  └──────────────────────────────────────────────┘                 │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘
```

---

# PART 2: LIGHTWAVEOS V2 CONTROLBUS AUDIT

## 2.1 Executive Summary

**Critical Finding:** LightwaveOS v2 already has a COMPLETE 12-bin chromagram implementation that is production-ready and currently deployed.

The ControlBus provides:
- 12-bin chromagram (octave-folded pitch classes)
- 64-bin full Goertzel spectrum
- 8-band frequency analysis
- Beat detection (K1 integration)
- Musical saliency metrics
- Chord detection (MAJOR/MINOR/DIMINISHED/AUGMENTED)
- Onset detection (snare/hi-hat triggers)
- Style classification

---

## 2.2 ControlBusFrame Complete Field Reference

### 2.2.1 Energy Metrics

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| `rms` | float | [0,1] | Smoothed RMS energy (slow attack/release) |
| `fast_rms` | float | [0,1] | Raw RMS (unsmoothed) |
| `flux` | float | [0,1] | Spectral flux (novelty/change detector) |
| `fast_flux` | float | [0,1] | Raw spectral flux |

### 2.2.2 Frequency Bands (8-Band Legacy)

| Field | Type | Frequencies |
|-------|------|-------------|
| `bands[0]` | float | 60 Hz (sub-bass) |
| `bands[1]` | float | 120 Hz (bass) |
| `bands[2]` | float | 250 Hz (low-mid) |
| `bands[3]` | float | 500 Hz (mid) |
| `bands[4]` | float | 1000 Hz (high-mid) |
| `bands[5]` | float | 2000 Hz (presence) |
| `bands[6]` | float | 4000 Hz (brilliance) |
| `bands[7]` | float | 7800 Hz (air) |
| `heavy_bands[8]` | float | Extra-smoothed versions |

### 2.2.3 Chromagram (12-Bin Pitch Classes)

| Field | Type | Description |
|-------|------|-------------|
| `chroma[12]` | float[12] | 12-bin chromagram (C, C#, D, ..., B) |
| `heavy_chroma[12]` | float[12] | Extra-smoothed chromagram |

**Pitch Class Mapping:**
```
[0] = C    [1] = C#   [2] = D    [3] = D#
[4] = E    [5] = F    [6] = F#   [7] = G
[8] = G#   [9] = A    [10] = A#  [11] = B
```

### 2.2.4 Chord Detection

| Field | Type | Description |
|-------|------|-------------|
| `chordState.rootNote` | uint8_t | Root note (0-11) |
| `chordState.type` | ChordType | MAJOR, MINOR, DIMINISHED, AUGMENTED, NONE |
| `chordState.confidence` | float | Chord detection confidence (0-1) |
| `chordState.rootStrength` | float | Energy at root |
| `chordState.thirdStrength` | float | Energy at third interval |
| `chordState.fifthStrength` | float | Energy at fifth interval |

### 2.2.5 Beat Detection (K1 Integration)

| Field | Type | Description |
|-------|------|-------------|
| `k1Locked` | bool | K1 beat tracker locked state |
| `k1Confidence` | float | Beat detection confidence (0-1) |
| `k1BeatTick` | bool | True on beat frame |

### 2.2.6 Onset Detection

| Field | Type | Description |
|-------|------|-------------|
| `snareEnergy` | float | Snare band (150-300 Hz) energy |
| `hihatEnergy` | float | Hi-hat band (6-12 kHz) energy |
| `snareTrigger` | bool | True on snare onset |
| `hihatTrigger` | bool | True on hi-hat onset |

### 2.2.7 Musical Saliency

| Field | Type | Description |
|-------|------|-------------|
| `saliency.harmonicNovelty` | float | Chord/key changes |
| `saliency.rhythmicNovelty` | float | Beat pattern changes |
| `saliency.timbralNovelty` | float | Spectral character changes |
| `saliency.dynamicNovelty` | float | Loudness changes |
| `saliency.overallSaliency` | float | Weighted combination |

### 2.2.8 Full Spectrum

| Field | Type | Description |
|-------|------|-------------|
| `bins64[64]` | float[64] | Full 64-bin Goertzel spectrum |
| `waveform[128]` | int16_t[128] | Time-domain waveform snapshot |

---

## 2.3 ControlBus Post-Processing Pipeline

### Stage 1: Clamping

```cpp
// All values clamped to [0, 1] range
for (int i = 0; i < 8; i++) {
    bands[i] = constrain(bands[i], 0.0f, 1.0f);
}
for (int i = 0; i < 12; i++) {
    chroma[i] = constrain(chroma[i], 0.0f, 1.0f);
}
```

### Stage 2: Lookahead Spike Detection

```cpp
// 3-frame ring buffer analysis
// Detects single-frame spikes and replaces with neighbor average
// Threshold: 15% deviation from expected value (min 0.02)
// Output delay: 2 frames (~32ms at 60 FPS)

Spike detection criteria:
  Peak:   history[0] < history[1] > history[2]  // Rising→Falling
  Trough: history[0] > history[1] < history[2]  // Falling→Rising

Correction: history[1] = (history[0] + history[2]) / 2
```

### Stage 3: Zone-Based Automatic Gain Control

**Band Zones (4 zones × 2 bands each):**
```
Zone 0: Bands 0-1 (60-120 Hz)   → Sub-bass/Bass
Zone 1: Bands 2-3 (250-500 Hz)  → Low-mid
Zone 2: Bands 4-5 (1-2 kHz)     → Mid/High-mid
Zone 3: Bands 6-7 (4-7.8 kHz)   → Presence/Brilliance
```

**Chroma Zones (4 zones × 3 bins each):**
```
Zone 0: C, C#, D    (0-2)  → Low notes
Zone 1: D#, E, F    (3-5)  → Mid-low
Zone 2: F#, G, G#   (6-8)  → Mid-high
Zone 3: A, A#, B    (9-11) → High notes
```

**Algorithm:**
```cpp
// Per-zone maximum tracking
zone_max *= 0.999;  // Slow decay
if (zone_value > zone_max) {
    zone_max += (zone_value - zone_max) * 0.05;  // Fast attack
}

// Normalization
if (zone_max > 0.01) {
    normalized_value = value / zone_max;
}
```

### Stage 4: Asymmetric Attack/Release Smoothing

| Parameter | Normal Bands | Heavy Bands |
|-----------|-------------|-------------|
| Attack α | 0.15 | 0.08 |
| Release α | 0.03 | 0.015 |

**Mood Scaling (0-255):**
```cpp
// Mood 0 (Reactive): Fast attack (0.25), slow release (0.02)
// Mood 127 (Balanced): Mid attack (0.165), mid release (0.05)
// Mood 255 (Smooth): Slow attack (0.08), faster release (0.06)
```

### Stage 4b: Chord Detection

```cpp
// Music theory-based chord classification
// 1. Find dominant pitch class (root)
// 2. Check interval patterns:
//    - MAJOR: Root + Major 3rd (+4) + Perfect 5th (+7)
//    - MINOR: Root + Minor 3rd (+3) + Perfect 5th (+7)
//    - DIMINISHED: Root + Minor 3rd (+3) + Diminished 5th (+6)
//    - AUGMENTED: Root + Major 3rd (+4) + Augmented 5th (+8)
// 3. Confidence = (triad energy / total energy) / 0.4
```

---

## 2.4 ChromaAnalyzer Implementation Details

**File:** `src/audio/ChromaAnalyzer.h` and `.cpp`

```cpp
class ChromaAnalyzer {
    static constexpr size_t WINDOW_SIZE = 512;     // Samples per analysis
    static constexpr uint8_t NUM_CHROMA = 12;      // Pitch classes
    static constexpr uint8_t NUM_OCTAVES = 4;      // 48 notes total
    static constexpr uint32_t SAMPLE_RATE_HZ = 16000;
};
```

**Octave Coverage:**
```
Octave 2: C2 (65.4 Hz)  to B2 (123.5 Hz)
Octave 3: C3 (130.8 Hz) to B3 (246.9 Hz)
Octave 4: C4 (261.6 Hz) to B4 (493.9 Hz)
Octave 5: C5 (523.3 Hz) to B5 (987.8 Hz)
```

**Algorithm:**
1. Accumulate 512 samples (2 hops × 256 samples)
2. Compute Goertzel magnitude for each of 48 note frequencies
3. Fold into 12 pitch classes by summing across octaves (0.5 weight per octave)
4. Normalize output to [0, 1] range

---

## 2.5 Comparison: Sensory Bridge vs LightwaveOS

| Feature | Sensory Bridge | LightwaveOS v2 | Notes |
|---------|---------------|----------------|-------|
| **Chromagram bins** | 12 | 12 | FULL PARITY |
| **Octave range** | 4+ octaves | 4 octaves (48 notes) | Equivalent |
| **Goertzel approach** | 64 bins (piano keys) | 64 bins (semitone-spaced) | Equivalent |
| **Spike detection** | 3-frame lookahead | 3-frame lookahead | IDENTICAL |
| **Zone AGC** | 4 zones | 4 zones | IDENTICAL |
| **Asymmetric smoothing** | Yes | Yes | IDENTICAL |
| **Mood control** | MOOD knob (0-1) | setMoodSmoothing(0-255) | Equivalent |
| **Chord detection** | Equivalent | MAJOR/MINOR/DIM/AUG | LW more detailed |
| **Beat detection** | Basic novelty | K1 full tracker | LW MORE ADVANCED |
| **Saliency metrics** | None | 4-dimensional | LW UNIQUE |
| **Style detection** | None | Yes | LW UNIQUE |

**Conclusion:** LightwaveOS v2 has FEATURE PARITY or EXCEEDS Sensory Bridge in all audio analysis capabilities.

---

# PART 3: PROBLEM ANALYSIS & RECOMMENDATIONS

## 3.1 The Core Problem

### What Sensory Bridge Does

```
Audio → [Color/Brightness]     (AUDIO-REACTIVE)
Time  → [Motion Speed/Direction] (TIME-BASED, USER-CONTROLLED)
```

### What LightwaveOS Effects Often Do

```
Audio → [Color/Brightness]     (AUDIO-REACTIVE)
Audio → [Motion Speed/Direction] (AUDIO-REACTIVE - PROBLEMATIC)
```

### Why Audio→Motion Is Problematic

1. **Jitter:** High-frequency audio content causes rapid motion changes
2. **Unpredictability:** Motion doesn't follow expected patterns
3. **Visual Chaos:** Competing motion sources create confusing visuals
4. **Silent Passages:** No audio = no motion = dead display

---

## 3.2 Recommended Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    AUDIO LAYER (Existing)                       │
│  ControlBus provides:                                           │
│  - chroma[12]: 12-bin chromagram                               │
│  - rms: Energy envelope                                         │
│  - k1BeatTick: Beat detection                                  │
│  - saliency: Musical importance metrics                         │
│  - chordState: Chord analysis                                  │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│              NEW: EFFECT PARAMETER MAPPING                      │
│                                                                 │
│  struct EffectAudioParams {                                     │
│      CRGB chromatic_color;    // From chroma[12] → HSV         │
│      float energy_envelope;    // From rms (smoothed)           │
│      float beat_phase;         // From K1 (0-1, wraps at beat) │
│      bool onset_trigger;       // From saliency.rhythmicNovelty │
│  };                                                             │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                    VISUAL LAYER (Effects)                       │
│                                                                 │
│  Motion: TIME-BASED only                                        │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ position += base_speed * dt;  // User-configurable speed │  │
│  │ if (onset_trigger) position = 0;  // Beat reset option   │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  Color: CHROMAGRAM-DERIVED                                      │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ color = chromatic_color;  // From pitch analysis         │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  Brightness: ENERGY-MODULATED                                   │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ brightness = base_brightness * energy_envelope;          │  │
│  └──────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3.3 Specific Code Changes

### 3.3.1 Add Chromatic Color Helper

```cpp
// In ControlBus.cpp or new helper file
CRGB computeChromaticColor(const float chroma[12]) {
    CRGB sum = CRGB::Black;

    for (int i = 0; i < 12; i++) {
        float hue = i / 12.0f;  // 0.0 to 0.917
        float brightness = chroma[i] * chroma[i];  // Quadratic contrast

        CRGB noteColor;
        hsv2rgb_spectrum(CHSV(hue * 255, 255, brightness * 255), noteColor);

        sum.r = qadd8(sum.r, noteColor.r / 6);
        sum.g = qadd8(sum.g, noteColor.g / 6);
        sum.b = qadd8(sum.b, noteColor.b / 6);
    }

    return sum;
}
```

### 3.3.2 Refactor AudioBloomEffect

**Current (Problematic):**
```cpp
void AudioBloomEffect::render(...) {
    float speed = bass_magnitude * 10.0f;  // Audio-driven motion
    phase_ += speed * dt;
    // ...
}
```

**Recommended (Sensory Bridge Pattern):**
```cpp
void AudioBloomEffect::render(...) {
    // Motion: TIME-BASED ONLY
    float speed = base_speed_;  // User-configurable, NOT audio
    phase_ += speed * dt;

    // Color: CHROMAGRAM-DERIVED
    CRGB color = computeChromaticColor(frame.chroma);

    // Brightness: ENERGY-MODULATED
    float brightness = frame.rms * frame.rms;  // Quadratic

    // Beat sync: OPTIONAL phase reset
    if (frame.k1BeatTick && sync_to_beat_) {
        phase_ = 0.0f;
    }

    // Render with separated concerns...
}
```

---

## 3.4 Key Techniques to Implement

### 3.4.1 Sub-Pixel Motion (From Sensory Bridge)

```cpp
// Enable fractional LED positioning for smooth motion
void renderWithSubpixel(CRGB* leds, int numLeds, float position, CRGB color) {
    int whole = (int)position;
    float fract = position - whole;

    if (whole >= 0 && whole < numLeds) {
        leds[whole] += color * (1.0f - fract);
    }
    if (whole + 1 >= 0 && whole + 1 < numLeds) {
        leds[whole + 1] += color * fract;
    }
}
```

### 3.4.2 Multi-Frame Averaging (From Waveform Mode)

```cpp
// 4-tap temporal filter for ultra-smooth waveform display
template<typename T, int TAPS>
class MultiTapFilter {
    T history_[TAPS];
    int index_ = 0;

public:
    T process(T input) {
        history_[index_] = input;
        index_ = (index_ + 1) % TAPS;

        T sum = T{};
        for (int i = 0; i < TAPS; i++) {
            sum += history_[i];
        }
        return sum / TAPS;
    }
};
```

### 3.4.3 Asymmetric Envelope Follower

```cpp
class AsymmetricEnvelope {
    float value_ = 0.0f;
    float attack_;
    float release_;

public:
    AsymmetricEnvelope(float attack = 0.05f, float release = 0.0025f)
        : attack_(attack), release_(release) {}

    float process(float input) {
        if (input > value_) {
            value_ += (input - value_) * attack_;
        } else {
            value_ -= (value_ - input) * release_;
        }
        return value_;
    }
};
```

---

# PART 4: CONCLUSIONS (CORRECTED)

## 4.1 What Sensory Bridge ACTUALLY Taught Us (Revised)

### The Multi-Layer Audio-Visual Coupling

1. **`silent_scale` Global Brightness Gate** - THE CRITICAL MECHANISM MISSED IN FIRST PASS
   - After 10 seconds of silence, `silent_scale → 0.0`
   - ALL LED output is multiplied by this value
   - Even if motion calculations continue, output is BLACK
   - **Location:** `i2s_audio.h:180`, `led_utilities.h:211`

2. **Color Injection is Audio-Dependent**
   - Bloom injects `sum_color` at center, derived from `chromagram_smooth[12]`
   - No audio → chromagram values = 0 → nothing to propagate
   - Motion math runs but propagates BLACKNESS

3. **Mode-Specific Motion Coupling**
   - **Bloom:** Motion speed is `CONFIG.MOOD` (user) - NOT audio-reactive
   - **Kaleidoscope:** `pos += shift_speed * spectral_sum` - IS audio-reactive
   - **VU Dot:** Position = `audio_vu_level_smooth` - IS audio-reactive
   - Different modes have DIFFERENT coupling strategies!

4. **Heavy smoothing everywhere** - 95:5 ratios, asymmetric attack/release

5. **Chromatic aggregation** - 64→12 bin folding preserves musical meaning

6. **Asynchronous threads** - Audio and visual decoupled (~133 Hz vs ~120 FPS)

7. **Three-level AGC** - Noise floor + envelope + normalization

8. **Sub-pixel interpolation** - Fractional LED positioning for smooth motion

### Why "No Audio = No Visible Motion" (The Complete Picture)

```
WITHOUT AUDIO:
├─ chromagram[12] = all zeros → color = BLACK
├─ After 10s: silent_scale → 0.0 → brightness = 0
├─ Kaleidoscope: spectral_sum = 0 → position frozen
└─ RESULT: Nothing visible, even if Bloom motion math runs
```

## 4.2 What LightwaveOS Already Has

1. **Complete 12-bin chromagram** - Production-ready, identical algorithm
2. **Zone-based AGC** - Per-zone normalization prevents bass dominance
3. **Spike detection** - 3-frame lookahead removes transient artifacts
4. **Asymmetric smoothing** - Fast attack, slow release
5. **Mood control** - 0-255 parameter scales smoothing
6. **K1 beat tracker** - More advanced than Sensory Bridge
7. **Musical saliency** - 4-dimensional importance metrics (UNIQUE)
8. **Chord detection** - MAJOR/MINOR/DIMINISHED/AUGMENTED

## 4.3 What LightwaveOS Should Consider (Revised)

### The Real Question: What Should Drive Motion?

Based on Sensory Bridge's multi-mode approach:

| Approach | Sensory Bridge Mode | LightwaveOS Equivalent |
|----------|---------------------|------------------------|
| **User-controlled speed** | Bloom (`CONFIG.MOOD`) | Add per-effect speed parameter |
| **Spectral-driven position** | Kaleidoscope (`pos += spectral_sum`) | Some effects already do this |
| **VU-driven position** | VU Dot | Use `ControlBus.rms` |
| **Beat-triggered resets** | (Not in SB) | Use `k1BeatTick` for phase sync |

### Recommended Changes:

1. **Add `silent_scale` equivalent** - Global brightness gate during silence
   - Prevents "dead" display from running motion with no audio
   - Graceful fade-out after configurable timeout

2. **Chromatic color helper** (`chroma[12]` → CRGB) - For Bloom-style effects

3. **Sub-pixel motion interpolation** - Already in FX system, ensure consistent use

4. **Differentiate effect types:**
   - "Ambient" effects: Time-based motion, audio→color only (like Bloom)
   - "Reactive" effects: Audio→position coupling (like Kaleidoscope)
   - "Beat-sync" effects: K1 phase-locked motion

5. **Multi-frame averaging for waveform-style effects**

## 4.4 Files Reference

### Sensory Bridge (Reference) - CRITICAL FILES IDENTIFIED
```
/SensoryBridge-X.X.X/SENSORY_BRIDGE_FIRMWARE/
├── lightshow_modes.h       # All visualization modes
├── GDFT.h                  # Goertzel frequency analysis
├── i2s_audio.h            # Audio capture, SILENCE DETECTION, silent_scale
├── led_utilities.h        # Chromagram, sprite motion, BRIGHTNESS GATE
├── globals.h              # Shared state including silent_scale
└── constants.h            # Frequency tables
```

### LightwaveOS v2 (Implementation)
```
/v2/src/audio/
├── contracts/ControlBus.h  # Main audio feature bus
├── contracts/ControlBus.cpp # Post-processing pipeline
├── ChromaAnalyzer.h       # 12-bin chromagram
├── ChromaAnalyzer.cpp     # Chromagram implementation
├── GoertzelAnalyzer.h     # 64-bin spectrum
├── AudioActor.cpp         # Audio processing loop
└── contracts/MusicalSaliency.h # Saliency metrics
```

---

## Document Complete

**Research Status:** CORRECTED AND COMPREHENSIVE
**Key Finding:** `silent_scale` global brightness gate is the primary mechanism preventing motion without audio
**Lesson Learned:** Sensory Bridge uses MULTIPLE interacting systems, not just "audio→color, time→motion"
**Implementation Required:** Consider adding silence detection and global brightness gating to LightwaveOS
