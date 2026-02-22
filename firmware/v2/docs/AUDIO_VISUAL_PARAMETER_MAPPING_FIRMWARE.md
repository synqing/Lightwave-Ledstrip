# Audio-Visual Parameter Mapping: Firmware Implementation Guide

**Operational guide for mapping real-time audio features to LED choreography parameters**

---

## 1. Real-Time Audio Feature Extraction

### 1.1 Sample Configuration

**Audio Setup (ESP32-S3 with EsV11):**
- Sample rate: 48 kHz
- Frame size: 960 samples = 20ms window at 48 kHz
- Update frequency: 50 Hz (one update per 20ms frame)
- Buffer: Circular, overlapping (for FFT stability)

### 1.2 FFT and Spectral Analysis

**1024-point FFT on each 960-sample frame:**

```cpp
// Pseudo-code for FFT analysis
float audio_frame[960];  // captured from audio input
float fft_output[512];   // 1024-point FFT → 512 magnitude bins
float magnitude[512];    // magnitude spectrum

// 1. Window (Hann window reduces spectral leakage)
apply_hann_window(audio_frame, 960);

// 2. Compute FFT (use ESP32 DSP library or ARM CMSIS-DSP)
compute_fft_1024(audio_frame, fft_output);

// 3. Convert to magnitude spectrum
for (int i = 0; i < 512; i++) {
    magnitude[i] = sqrt(fft_output[2*i]^2 + fft_output[2*i+1]^2) / 512.0f;
}

// 4. Apply frequency weighting (optional A-weighting for perceived loudness)
// magnitude_weighted[i] = magnitude[i] * a_weight[i];
```

**Frequency Resolution:**
- Bin bandwidth = 48000 Hz / 1024 = 46.875 Hz/bin
- Bin 0 = 0 Hz, Bin 512 = 24 kHz
- Low freqs: bins 0-5 (0-234 Hz) = sub-bass region
- Mid freqs: bins 5-100 (234-4687 Hz) = presence
- High freqs: bins 100-256 (4687-12 kHz) = treble/brightness

### 1.3 Feature Extraction Algorithms

#### RMS Energy (Loudness)

```cpp
float compute_rms_energy(float* audio_frame, int frame_size) {
    float sum_sq = 0.0f;
    for (int i = 0; i < frame_size; i++) {
        sum_sq += audio_frame[i] * audio_frame[i];
    }
    float rms = sqrt(sum_sq / frame_size);
    return rms;
}

// Normalize to 0.0-1.0 range
float normalized_rms = rms / 1.0f;  // peak amplitude is typically ±0.9
```

**Output Range:** 0.0 - 1.0
**Typical Range:** 0.01 - 0.3 (for normal music)
**Update Rate:** Every frame (50 Hz)
**Smoothing:** Exponential moving average (EMA) with α = 0.2
```cpp
rms_smoothed = (1.0 - alpha) * rms_smoothed_prev + alpha * rms_current;
```

#### Spectral Centroid (Brightness)

```cpp
float compute_spectral_centroid(float* magnitude, int num_bins) {
    float numerator = 0.0f;
    float denominator = 0.0f;

    for (int i = 0; i < num_bins; i++) {
        float freq_hz = i * 46.875f;  // frequency of this bin
        numerator += freq_hz * magnitude[i];
        denominator += magnitude[i];
    }

    float centroid_hz = (denominator > 0) ? numerator / denominator : 0.0f;
    return centroid_hz;  // typically 100-8000 Hz
}

// Normalize to hue range (0-360 degrees)
float normalized_centroid = centroid_hz / 8000.0f;  // 0.0-1.0
float hue = normalized_centroid * 360.0f;  // 0-360° colour wheel
```

**Output Range:** 0 - 24000 Hz (Nyquist)
**Perceptually Useful Range:** 100 - 8000 Hz
**Colour Mapping:**
- 100 Hz (centroid) → 0° (RED)
- 500 Hz → 30° (ORANGE)
- 1500 Hz → 45° (YELLOW)
- 3000 Hz → 120° (GREEN)
- 5000 Hz → 210° (CYAN)
- 8000 Hz → 270° (BLUE)

**Smoothing:** Low-pass filter with τ = 1.0-2.0 seconds
```cpp
// First-order low-pass: H(s) = 1 / (τs + 1)
// Discrete: y[n] = (1 - α) * y[n-1] + α * x[n]
// where α = 1.0 / (τ * sample_rate + 1) = 1.0 / (τ * 50 + 1)
// For τ = 1.0s: α = 1/51 ≈ 0.0196
float alpha_centroid = 0.02f;  // ~1s smoothing at 50Hz update rate
```

#### Onset Strength (Rhythmic Density)

```cpp
// Spectral Flux: sum of positive magnitude changes
float compute_spectral_flux(float* magnitude_curr, float* magnitude_prev, int num_bins) {
    float flux = 0.0f;
    for (int i = 0; i < num_bins; i++) {
        float delta = magnitude_curr[i] - magnitude_prev[i];
        if (delta > 0) {
            flux += delta;  // only positive changes (new energy)
        }
    }
    return flux;
}

// Estimate onset/beat density from flux envelope
// Method: peak picking on smoothed flux
float onset_strength = clamp(flux / 100.0f, 0.0f, 1.0f);  // normalize to 0-1
```

**Output Range:** 0.0 - 1.0 (relative onset strength)
**Practical Meaning:**
- 0.0 - 0.2: Sparse, quiet, ambient music
- 0.2 - 0.5: Moderate density (verses)
- 0.5 - 0.8: High density (buildups, fast rhythms)
- 0.8 - 1.0: Very dense (drums breaks, peak moments)

**Motion Speed Mapping:**
```cpp
// BPM-normalized approach (preferred)
// Assume tempo detection has provided bpm
float motion_speed_factor = bpm / 120.0f;  // normalize to 120 BPM = 1.0x
// Clamp to reasonable range
motion_speed = clamp(motion_speed_factor, 0.2f, 3.0f);

// OR: Direct onset density mapping
motion_speed = 0.5f + (onset_strength * 2.5f);  // ranges 0.5-3.0x
```

**Smoothing:** Median filter (more robust to transient noise than EMA)
```cpp
// Median filter: keep circular buffer of last 10 onset values
// Output: median of buffer
// Advantage: rejects spurious peaks, maintains responsive edge detection
```

#### Sub-Bass Energy (Gravity/Weight)

```cpp
// Energy in 0-60 Hz band
float compute_sub_bass_energy(float* magnitude, int num_bins) {
    // Bins for 0-60 Hz: bin 0-2 (0, 47, 94 Hz)
    float sub_bass_power = 0.0f;
    for (int i = 0; i < 3; i++) {  // bins 0, 1, 2
        sub_bass_power += magnitude[i];
    }

    // Normalize by total power
    float total_power = 0.0f;
    for (int i = 0; i < num_bins; i++) {
        total_power += magnitude[i];
    }

    float sub_bass_ratio = (total_power > 0) ? sub_bass_power / total_power : 0.0f;
    return sub_bass_ratio;  // 0.0-1.0
}
```

**Output Range:** 0.0 - 1.0 (ratio of total power)
**Interpretation:**
- 0.0 - 0.1: Minimal sub-bass (acoustic, vocal-heavy)
- 0.1 - 0.2: Moderate bass (pop, rock)
- 0.2 - 0.4: Strong bass (EDM, hip-hop)
- 0.4+: Extreme bass (dubstep, sub-bass-focused)

**Visual Weight Mapping:**
```cpp
// Option 1: Darken entire strip
float bass_darkness_factor = sub_bass_energy;  // 0 = no darkening, 1 = maximum dark
float brightness_adjusted = brightness * (1.0f - bass_darkness_factor * 0.3f);

// Option 2: Emphasize lower LEDs (if physical layout permits)
// Lower 160 LEDs get extra power; upper 160 LEDs get reduced power
int lower_led_end = 160;
for (int i = 0; i < 160; i++) {
    led_brightness[i] = brightness * (1.0f + sub_bass_energy * 0.5f);
    led_brightness[160 + i] = brightness * (1.0f - sub_bass_energy * 0.3f);
}
```

**Smoothing:** EMA with α = 0.1 (slower, ~2-3s response)

#### Stereo Width (Spatial Spread)

*Note: Requires stereo audio input (L/R channels)*

```cpp
// Phase correlation between L and R channels
float compute_stereo_correlation(float* audio_L, float* audio_R, int frame_size) {
    float sum_lr = 0.0f;
    float sum_l = 0.0f;
    float sum_r = 0.0f;

    for (int i = 0; i < frame_size; i++) {
        sum_lr += audio_L[i] * audio_R[i];
        sum_l += audio_L[i] * audio_L[i];
        sum_r += audio_R[i] * audio_R[i];
    }

    float denominator = sqrt(sum_l * sum_r);
    float correlation = (denominator > 0) ? sum_lr / denominator : 1.0f;
    return correlation;  // -1.0 (opposite phase) to +1.0 (identical)
}

// Convert correlation to spatial spread
// -1.0 (out of phase) = maximum width
// +1.0 (in phase) = mono (minimum width)
float stereo_width = (1.0f - correlation) / 2.0f;  // normalize to 0.0-1.0
// 0.0 = mono (narrow), 1.0 = maximum stereo width
```

**Output Range:** 0.0 - 1.0
**LED Spread Mapping:**
```cpp
// Narrow (mono): concentrate LEDs in center zone
// Wide (stereo): spread LEDs across full strip

int center_zone = 80;  // middle of 320 LEDs
int spread_radius = (int)(stereo_width * 80.0f);  // 0-80 LEDs from center

// Activate LEDs within spread_radius of center
for (int i = 0; i < 320; i++) {
    int distance_from_center = abs(i - center_zone);
    if (distance_from_center <= spread_radius) {
        led_active[i] = true;
        led_brightness[i] = brightness;
    } else {
        led_active[i] = false;
    }
}
```

---

## 2. Feature Smoothing & Stability

### 2.1 Smoothing Strategies

**Exponential Moving Average (EMA):**
```cpp
float ema_update(float prev_smoothed, float new_sample, float alpha) {
    return (1.0f - alpha) * prev_smoothed + alpha * new_sample;
}

// Practical values (at 50 Hz update rate):
// α = 0.1   → ~10 frame lag = 200ms smoothing
// α = 0.05  → ~20 frame lag = 400ms smoothing
// α = 0.02  → ~50 frame lag = 1000ms smoothing
// α = 0.01  → ~100 frame lag = 2000ms smoothing
```

**Median Filtering (Robust to Outliers):**
```cpp
float median_filter(float* circular_buffer, int window_size) {
    // Sort buffer, return middle value
    float sorted[window_size];
    memcpy(sorted, circular_buffer, window_size * sizeof(float));
    qsort(sorted, window_size, sizeof(float), compare_floats);
    return sorted[window_size / 2];
}

// Practical: 10-frame median = 200ms window at 50 Hz
// Eliminates transient clicks while maintaining responsiveness
```

**Hybrid Approach (Recommended):**
```cpp
struct SmoothedFeature {
    float current_value;
    float smoothed_value;
    float median_buffer[10];
    int buffer_index;

    void update(float raw_value) {
        // Add to median buffer
        median_buffer[buffer_index] = raw_value;
        buffer_index = (buffer_index + 1) % 10;

        // Compute median
        float med = median_filter(median_buffer, 10);

        // Apply EMA to median
        smoothed_value = ema_update(smoothed_value, med, 0.02f);
        current_value = smoothed_value;
    }
};
```

### 2.2 Perceptual Thresholds for Smoothing

| Parameter | Perceptual Threshold | Recommended Smoothing | Rationale |
|-----------|---------------------|-----------------------|-----------|
| Brightness | 5-10% change | 0.5-1.0s EMA | Avoid flicker, maintain smoothness |
| Hue | 10-15° change | 1-2s EMA | Colour transitions should feel gradual |
| Saturation | 10-15% change | 1-2s EMA | Saturation shifts can be jarring if fast |
| Motion Speed | 0.2x change | 1-2s median | Acceleration/deceleration should be gradual |
| Sub-Bass | 0.1 ratio change | 2-3s EMA | Bass is low-frequency, changes are slow |

---

## 3. Parameter Mapping Implementation

### 3.1 Core Mapping Function

```cpp
struct VisualParameters {
    float brightness;      // 0.0-1.0
    float hue;             // 0-360° (HSV)
    float saturation;      // 0.0-1.0
    float motion_speed;    // 0.2x-3.0x
    float spatial_spread;  // 0.0-1.0
    float sub_bass_weight; // 0.0-1.0
};

struct AudioFeatures {
    float rms_energy;
    float spectral_centroid_hz;
    float onset_strength;
    float sub_bass_power;
    float stereo_width;
    float current_bpm;
};

VisualParameters map_audio_to_visual(const AudioFeatures& audio) {
    VisualParameters visual;

    // 1. Brightness ← RMS Energy
    float brightness_raw = audio.rms_energy / 0.3f;  // normalize peak
    visual.brightness = clamp(brightness_raw, 0.1f, 1.0f);  // min 10%

    // 2. Hue ← Spectral Centroid (100 Hz → 8000 Hz → 0-360°)
    float centroid_normalized = audio.spectral_centroid_hz / 8000.0f;
    visual.hue = centroid_normalized * 360.0f;
    visual.hue = fmod(visual.hue, 360.0f);  // keep in range

    // 3. Saturation ← Harmonic Complexity (simplified: onset + variance)
    // High complexity = high saturation; consonant = low saturation
    float complexity = audio.onset_strength;  // simplified: use onset as proxy
    visual.saturation = 0.2f + (complexity * 0.8f);  // range 20-100%

    // 4. Motion Speed ← Onset Density / BPM
    float speed_from_bpm = audio.current_bpm / 120.0f;
    visual.motion_speed = clamp(speed_from_bpm, 0.2f, 3.0f);

    // 5. Spatial Spread ← Stereo Width
    visual.spatial_spread = audio.stereo_width;  // 0.0-1.0

    // 6. Sub-Bass Weight ← Sub-Bass Energy
    visual.sub_bass_weight = audio.sub_bass_power;

    // 7. Coupling & Adjustments
    // Reduce saturation at very high brightness to avoid oversaturation
    if (visual.brightness > 0.8f) {
        visual.saturation *= (1.0f - (visual.brightness - 0.8f) * 0.5f);
    }

    // Darken strip proportional to bass energy
    visual.brightness *= (1.0f - visual.sub_bass_weight * 0.2f);

    return visual;
}
```

### 3.2 Movement Direction Mapping

```cpp
enum MovementDirection {
    INWARD,      // centre-outward suppressed, motion pulls to center
    OUTWARD,     // centre-outward emphasized, motion spreads from center
    ALTERNATING, // oscillates between inward/outward
    STATIC       // no directional motion
};

struct SongPhase {
    bool is_intro;
    bool is_verse;
    bool is_pre_chorus;
    bool is_chorus;
    bool is_bridge;
    bool is_outro;
    float confidence;  // 0.0-1.0
};

MovementDirection get_movement_direction(const SongPhase& phase) {
    if (phase.is_chorus || phase.confidence > 0.7f) {
        return OUTWARD;  // Release, expansion
    } else if (phase.is_verse || phase.is_intro) {
        return INWARD;   // Tension, gathering
    } else if (phase.is_bridge) {
        return ALTERNATING;  // Contrast, ambiguity
    }
    return STATIC;  // Default
}

// LED pattern generation based on direction
void generate_led_pattern(
    uint32_t* led_colors,
    int num_leds,
    float phase,  // 0.0-1.0 animation phase
    MovementDirection direction
) {
    int center = num_leds / 2;

    for (int i = 0; i < num_leds; i++) {
        float distance_from_center = abs(i - center);

        switch (direction) {
            case INWARD:
                // Suppress outer zones, emphasize center
                led_colors[i] *= (1.0f - (distance_from_center / center) * 0.5f);
                break;

            case OUTWARD:
                // Amplify outer zones
                led_colors[i] *= (1.0f + (distance_from_center / center) * 0.3f);
                break;

            case ALTERNATING:
                // Oscillate with phase
                float oscillation = sin(phase * 2 * M_PI);
                if (oscillation > 0) {
                    led_colors[i] *= (1.0f + (distance_from_center / center) * 0.3f);
                } else {
                    led_colors[i] *= (1.0f - (distance_from_center / center) * 0.3f);
                }
                break;

            case STATIC:
                // No modulation
                break;
        }
    }
}
```

### 3.3 Brightness to RGB Conversion

```cpp
// Convert HSV to RGB (standard algorithm)
void hsv_to_rgb(
    float h,  // 0-360
    float s,  // 0-1
    float v,  // 0-1 (brightness)
    uint8_t& r,
    uint8_t& g,
    uint8_t& b
) {
    h = fmod(h, 360.0f);
    float c = v * s;
    float x = c * (1.0f - fabs(fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    float r_f, g_f, b_f;
    if (h < 60) {
        r_f = c; g_f = x; b_f = 0;
    } else if (h < 120) {
        r_f = x; g_f = c; b_f = 0;
    } else if (h < 180) {
        r_f = 0; g_f = c; b_f = x;
    } else if (h < 240) {
        r_f = 0; g_f = x; b_f = c;
    } else if (h < 300) {
        r_f = x; g_f = 0; b_f = c;
    } else {
        r_f = c; g_f = 0; b_f = x;
    }

    r = (uint8_t)((r_f + m) * 255);
    g = (uint8_t)((g_f + m) * 255);
    b = (uint8_t)((b_f + m) * 255);
}
```

---

## 4. Real-Time Processing Pipeline

### 4.1 Audio Actor Integration

```cpp
class AudioVisualMapper {
private:
    // Feature buffers (circular, overlapping)
    AudioFeatures current_features;
    VisualParameters current_visual;

    // Smoothing buffers
    SmoothedFeature brightness_smooth;
    SmoothedFeature centroid_smooth;
    SmoothedFeature onset_smooth;

    // Song structure (from pre-analysis or real-time detection)
    SongPhase current_phase;

public:
    void process_audio_frame(
        const float* audio_frame_mono,
        int frame_size,
        int sample_rate
    ) {
        // 1. Extract features
        AudioFeatures raw_features;
        raw_features.rms_energy = compute_rms_energy(audio_frame_mono, frame_size);
        raw_features.spectral_centroid_hz = compute_spectral_centroid(
            frequency_domain_magnitude, 512
        );
        raw_features.onset_strength = compute_spectral_flux(
            current_magnitude, prev_magnitude, 512
        );
        raw_features.sub_bass_power = compute_sub_bass_energy(
            frequency_domain_magnitude, 512
        );
        raw_features.stereo_width = compute_stereo_correlation(
            audio_L, audio_R, frame_size
        );

        // 2. Smooth features
        brightness_smooth.update(raw_features.rms_energy);
        centroid_smooth.update(raw_features.spectral_centroid_hz);
        onset_smooth.update(raw_features.onset_strength);

        current_features.rms_energy = brightness_smooth.current_value;
        current_features.spectral_centroid_hz = centroid_smooth.current_value;
        current_features.onset_strength = onset_smooth.current_value;
        current_features.sub_bass_power = raw_features.sub_bass_power;
        current_features.stereo_width = raw_features.stereo_width;

        // 3. Map to visual parameters
        current_visual = map_audio_to_visual(current_features);

        // 4. Update movement direction based on phase
        current_visual.movement_direction = get_movement_direction(current_phase);

        // 5. Send to renderer
        update_renderer(current_visual);
    }

    const VisualParameters& get_current_visual() const {
        return current_visual;
    }

    void set_song_phase(const SongPhase& phase) {
        current_phase = phase;
    }
};
```

### 4.2 Performance Budget

**ESP32-S3 Task Breakdown (per 20ms frame):**

| Task | Time Budget | Typical | Headroom |
|------|-----------|---------|----------|
| FFT (1024-point) | 2ms | 1.2ms | 0.8ms |
| Feature Extraction | 1ms | 0.4ms | 0.6ms |
| Smoothing/EMA | 0.5ms | 0.2ms | 0.3ms |
| Mapping | 0.5ms | 0.15ms | 0.35ms |
| LED driver output | 2ms | 1.8ms | 0.2ms |
| **TOTAL** | **6ms** | **3.75ms** | **2.25ms (30% budget)** |

**Available CPU:** 20ms frame time
**Actual task time:** ~3.75ms
**Remaining for other tasks:** ~16.25ms (music playback, networking, etc.)

---

## 5. Colour Palette Presets

### 5.1 Pre-Defined Mappings

**Preset 1: "Energy Mirror" (Default)**
- Direct spectral centroid → hue mapping
- High saturation during buildups
- Brightness directly mirrors loudness
- Use case: DJ sets, live concerts

**Preset 2: "Emotional Journey" (Narrative-Aware)**
- Requires song phase detection
- Verse: cool, desaturated colours; inward motion
- Chorus: warm, saturated colours; outward motion
- Bridge: mixed or inverted palettes
- Use case: Curated playlists, storytelling

**Preset 3: "Thermal" (Temperature-Based)**
- Red/warm (0-3000 Hz) = sub-bass and mids
- Yellow/neutral (3000-5000 Hz) = midrange
- Cyan/cool (5000+ Hz) = treble/air
- Use case: Production monitoring, analysis

**Preset 4: "Synesthetic" (Creative Remapping)**
- User-configurable
- Can invert/reverse mappings
- Custom colour palettes
- Use case: Artists, performers, installations

### 5.2 Implementation

```cpp
enum ColourPreset {
    ENERGY_MIRROR,
    EMOTIONAL_JOURNEY,
    THERMAL,
    SYNESTHETIC,
    CUSTOM
};

struct ColourPaletteConfig {
    // Hue mapping: frequency → hue offset
    float hue_min_hz;     // frequency for hue=0°
    float hue_max_hz;     // frequency for hue=360°
    bool hue_reversed;    // flip the mapping

    // Saturation control
    float sat_min;        // minimum saturation at low complexity
    float sat_max;        // maximum saturation at high complexity
    float sat_brightness_coupling;  // reduce sat at high brightness (0-1)

    // Brightness control
    float bright_min;     // minimum brightness floor
    float bright_max;     // maximum brightness ceiling
    float bright_gamma;   // gamma correction (2.2 typical)

    // Sub-bass darkening
    float bass_darken_factor;  // 0-1: how much bass dims overall brightness
};

ColourPaletteConfig get_preset_config(ColourPreset preset) {
    ColourPaletteConfig cfg = {0};

    switch (preset) {
        case ENERGY_MIRROR:
            cfg.hue_min_hz = 100.0f;
            cfg.hue_max_hz = 8000.0f;
            cfg.hue_reversed = false;
            cfg.sat_min = 0.3f;
            cfg.sat_max = 1.0f;
            cfg.sat_brightness_coupling = 0.5f;
            cfg.bright_min = 0.1f;
            cfg.bright_max = 1.0f;
            cfg.bright_gamma = 2.2f;
            cfg.bass_darken_factor = 0.2f;
            break;

        case THERMAL:
            cfg.hue_min_hz = 0.0f;
            cfg.hue_max_hz = 8000.0f;
            cfg.hue_reversed = false;
            cfg.sat_min = 0.5f;
            cfg.sat_max = 1.0f;
            cfg.sat_brightness_coupling = 0.3f;
            cfg.bright_min = 0.15f;
            cfg.bright_max = 1.0f;
            cfg.bright_gamma = 2.0f;
            cfg.bass_darken_factor = 0.1f;
            break;

        // ... other presets
    }
    return cfg;
}
```

---

## 6. Testing & Calibration

### 6.1 Feature Validation

**Checklist for each audio feature:**

- [ ] RMS Energy: Peaks during loud sections? (Use pink noise @ various levels)
- [ ] Spectral Centroid: Shifts to higher hues during bright/treble sections? (Sweep sine wave)
- [ ] Onset Strength: Peaks during drum hits? (Isolate drum tracks)
- [ ] Sub-Bass: Activated by 20-60 Hz content? (Sine sweep in bass region)
- [ ] Stereo Width: Narrows for mono content? (Compare mono vs. stereo tracks)

**Test Signals:**
```cpp
// Pink noise (broadband, spectrally flat)
// Expected: moderate centroid, no extreme brightness

// Sine sweep (20-8000 Hz, 10 seconds)
// Expected: smooth hue transition from red → blue

// Drum loop (isolated kick + snare)
// Expected: sharp onset peaks, stable centroid around 150-300 Hz

// Orchestral (multiple instruments)
// Expected: varying centroid, increasing onset density during crescendos

// Vocals (speech/singing)
// Expected: bright spectrum (formants), variable onset (consonants), narrow stereo
```

### 6.2 Perceptual Calibration

**User Feedback Loop:**

1. Play test track, observe LED output
2. Adjust smoothing constants if response is too jittery or sluggish
3. Adjust colour mapping if hues don't match musical intuition
4. Adjust brightness scaling if LEDs seem over/under-driven
5. Repeat until response feels "natural" and "in sync" with music

**Calibration Parameters:**
```cpp
struct CalibrationConfig {
    float brightness_smoothing_alpha;  // EMA for brightness (default 0.02)
    float centroid_smoothing_alpha;    // EMA for hue (default 0.02)
    float onset_smoothing_window;      // median filter window (default 10)

    float brightness_scale;             // multiplier on RMS (default 1.0)
    float centroid_scale;               // multiplier on spectral centroid (default 1.0)
    float onset_scale;                  // multiplier on onset strength (default 1.0)

    float bass_darken_strength;         // 0-1 intensity of bass darkening (default 0.2)
    float saturation_boost;             // global saturation multiplier (default 1.0)
};
```

---

## 7. Integration Checklist

- [ ] FFT implementation verified (use ARM CMSIS-DSP or ESP-DSP)
- [ ] Feature extraction algorithms tested on known audio
- [ ] Smoothing parameters tuned to user preference
- [ ] Colour mapping visually verified against music
- [ ] Brightness range (10-100%) set appropriately
- [ ] Movement patterns responding to motion_speed
- [ ] Stereo width detection working (if L/R input available)
- [ ] Sub-bass darkening active and calibrated
- [ ] Real-time performance: <30% CPU budget
- [ ] Latency measurement: <50ms audio → LED update
- [ ] Test with diverse music genres (as per Part 9.2 of research doc)

---

## References

- ARM CMSIS-DSP FFT: https://arm-software.github.io/CMSIS_5/DSP/html/index.html
- ESP32-S3 I2S Audio: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/i2s.html
- Librosa (Python audio analysis): https://librosa.org/
- HSV to RGB conversion: https://en.wikipedia.org/wiki/HSL_and_HSV

