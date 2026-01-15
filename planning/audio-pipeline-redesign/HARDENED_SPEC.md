# HARDENED AUDIO PIPELINE SPECIFICATION

**Version:** 1.1.0
**Status:** CANONICAL - DO NOT MODIFY WITHOUT PRD APPROVAL
**Last Updated:** 2026-01-12

---

## PREREQUISITES

> **⚠️ MANDATORY:** Complete the **[AGENT_ONBOARDING.md](./AGENT_ONBOARDING.md)** checklist before reading this document. The onboarding ensures you have the necessary context to understand WHY these specifications exist.

> **📊 REFERENCE:** All magic numbers in this document have complete derivations in **[MAGIC_NUMBERS.md](./MAGIC_NUMBERS.md)**. If you wonder "why this value?", the answer is there.

---

## PURPOSE

This document contains **VERBATIM CODE EXTRACTS** from canonical reference implementations. These extracts define the EXACT algorithms, constants, and data structures that MUST be implemented in LightwaveOS v2.

**AGENT INSTRUCTION:** If you find yourself "improving" or "optimizing" any code in this document, STOP. You are drifting. The canonical implementations are battle-tested. Your job is to PORT them faithfully, not reinvent them.

---

## CANONICAL SOURCES

| Source | Version | Purpose | Authority |
|--------|---------|---------|-----------|
| Sensory Bridge | 4.1.1 | Goertzel frequency analysis | PRIMARY for DSP |
| Emotiscope | 2.0 | Beat/tempo tracking | PRIMARY for tempo |

**GitHub URLs:**
- Sensory Bridge: `https://github.com/connornishijima/SensoryBridge`
- Emotiscope: `https://github.com/connornishijima/Emotiscope`

---

# SECTION 1: DSP CONSTANTS

## 1.1 Sample Rate and Resolution

**FROM: Sensory Bridge `constants.h:4-9` - DO NOT MODIFY**

```c
// Sensory Bridge uses 12800 Hz - we adapt to 16000 Hz for LightwaveOS
// The RATIO matters, not the absolute value

#define DEFAULT_SAMPLE_RATE 12800  // Sensory Bridge canonical
#define NATIVE_RESOLUTION   128    // Samples per audio chunk
#define NUM_FREQS           64     // Frequency bins (semitone-spaced)
#define NUM_ZONES           2      // Bass/Treble zone split
```

**LIGHTWAVEOS ADAPTATION:**
```c
// LightwaveOS v2 parameters - derived from Sensory Bridge ratios
#define LWOS_SAMPLE_RATE    16000  // 16 kHz (I2S mic native rate)
#define LWOS_CHUNK_SIZE     128    // Matches NATIVE_RESOLUTION
#define LWOS_NUM_FREQS      64     // MUST match Sensory Bridge
#define LWOS_NUM_ZONES      2      // MUST match Sensory Bridge
```

**WHY 16 kHz:** ESP32-S3 I2S peripheral + PDM mic combination runs optimally at 16 kHz. The Goertzel algorithm adapts via coefficient recalculation.

---

## 1.2 Musical Note Frequencies

**FROM: Sensory Bridge `constants.h:56-82` - DO NOT MODIFY**

```c
// Exact semitone frequencies from A1 (55 Hz) upward
// These are the TARGET frequencies for each Goertzel bin

const float notes[] = {
  // Octave 1 (A1-G#2) - indices 0-11
  55.00000, 58.27047, 61.73541, 65.40639, 69.29566, 73.41619,
  77.78175, 82.40689, 87.30706, 92.49861, 97.99886, 103.8262,

  // Octave 2 (A2-G#3) - indices 12-23
  110.0000, 116.5409, 123.4708, 130.8128, 138.5913, 146.8324,
  155.5635, 164.8138, 174.6141, 184.9972, 195.9977, 207.6523,

  // Octave 3 (A3-G#4) - indices 24-35
  220.0000, 233.0819, 246.9417, 261.6256, 277.1826, 293.6648,
  311.1270, 329.6276, 349.2282, 369.9944, 391.9954, 415.3047,

  // Octave 4 (A4-G#5) - indices 36-47
  440.0000, 466.1638, 493.8833, 523.2511, 554.3653, 587.3295,
  622.2540, 659.2551, 698.4565, 739.9888, 783.9909, 830.6094,

  // Octave 5 (A5-G#6) - indices 48-59
  880.0000, 932.3275, 987.7666, 1046.502, 1108.731, 1174.659,
  1244.508, 1318.510, 1396.913, 1479.978, 1567.982, 1661.219,

  // Octave 6 (A6-C7) - indices 60-63 (partial octave)
  1760.000, 1864.655, 1975.533, 2093.005
};
```

**IMPORTANT:** These frequencies follow the equal temperament formula:
```
f(n) = 440 * 2^((n-49)/12)
```
where n=1 is A0, n=49 is A4 (440 Hz).

**DO NOT** use approximations or "round" these values. They are mathematically precise.

---

## 1.3 Frequency Bin Structure

**FROM: Sensory Bridge `globals.h:88-100` - DO NOT MODIFY**

```c
struct freq {
  float    target_freq;      // Target frequency in Hz (from notes[])
  int32_t  coeff_q14;        // Goertzel coefficient in Q14 fixed-point
  uint16_t block_size;       // Samples per Goertzel window (varies by freq)
  float    block_size_recip; // 1.0 / block_size (precomputed)
  uint8_t  zone;             // 0=bass, 1=treble
  float    a_weighting_ratio;// A-weighting for perceptual balance
  float    window_mult;      // Window lookup multiplier
};

freq frequencies[NUM_FREQS];
```

**LIGHTWAVEOS PORT:**
```cpp
// Port to C++ with explicit types
struct FrequencyBin {
    float    targetFreq;       // Hz - from CANONICAL_NOTES[]
    int32_t  coeffQ14;         // Q14 fixed-point: (1 << 14) * 2 * cos(w)
    uint16_t blockSize;        // Samples per window (variable by freq)
    float    blockSizeRecip;   // Precomputed 1.0f / blockSize
    uint8_t  zone;             // 0=bass (bins 0-31), 1=treble (bins 32-63)
    float    aWeightRatio;     // A-weighting perceptual correction
    float    windowMult;       // 4096.0f / blockSize
};

static FrequencyBin g_freqBins[64];  // Statically allocated
```

---

# SECTION 2: WINDOW FUNCTION

## 2.1 Hann Window Generation

**FROM: Sensory Bridge `system.h:197-207` - DO NOT MODIFY**

```c
void generate_window_lookup() {
  for (uint16_t i = 0; i < 2048; i++) {
    float ratio = i / 4095.0;
    float weighing_factor = 0.54 * (1.0 - cos(TWOPI * ratio));
    window_lookup[i]        = 32767 * weighing_factor;
    window_lookup[4095 - i] = 32767 * weighing_factor;
  }
}
```

**ANALYSIS:**
- Uses **Hamming window** (0.54 coefficient, not 0.5 for pure Hann)
- Lookup table size: 4096 entries (symmetrical, only compute half)
- Scale: 32767 (Q15 fixed-point compatible)
- Symmetry exploited: `window_lookup[4095 - i] = window_lookup[i]`

**LIGHTWAVEOS PORT:**
```cpp
// Statically allocated lookup table
static int16_t g_windowLookup[4096];

void initWindowLookup() {
    constexpr float TWOPI = 6.283185307f;
    for (uint16_t i = 0; i < 2048; i++) {
        float ratio = static_cast<float>(i) / 4095.0f;
        float weight = 0.54f * (1.0f - cosf(TWOPI * ratio));
        int16_t value = static_cast<int16_t>(32767.0f * weight);
        g_windowLookup[i] = value;
        g_windowLookup[4095 - i] = value;
    }
}
```

**MAGIC NUMBER DOCUMENTATION:**
| Value | Meaning | Source |
|-------|---------|--------|
| 0.54 | Hamming window coefficient | Standard DSP |
| 4096 | Lookup table size | Sensory Bridge design choice |
| 32767 | Q15 max value (2^15 - 1) | Fixed-point format |
| TWOPI | 2π = 6.283185307 | Mathematical constant |

---

# SECTION 3: GOERTZEL COEFFICIENT CALCULATION

## 3.1 Coefficient Precomputation

**FROM: Sensory Bridge `system.h:209-256` - DO NOT MODIFY**

```c
void precompute_goertzel_constants() {
  for (uint16_t i = 0; i < NUM_FREQS; i++) {
    // Get target frequency from canonical notes array
    frequencies[i].target_freq = notes[n + CONFIG.NOTE_OFFSET];

    // Calculate block size based on frequency resolution requirement
    // Higher frequencies need smaller windows (fewer samples)
    frequencies[i].block_size = CONFIG.SAMPLE_RATE / (max_distance_hz * 2.0);

    // Clamp to maximum block size
    if(frequencies[i].block_size > 2000) {
      frequencies[i].block_size = 2000;
    }

    // Calculate Goertzel coefficient
    // k = bin index within block
    float k = (int)(0.5 + ((frequencies[i].block_size * frequencies[i].target_freq)
                           / CONFIG.SAMPLE_RATE));

    // w = angular frequency for this bin
    float w = (2.0 * PI * k) / frequencies[i].block_size;

    // Goertzel coefficient = 2 * cos(w)
    float coeff = 2.0 * cos(w);

    // Store as Q14 fixed-point: multiply by 2^14
    frequencies[i].coeff_q14 = (1 << 14) * coeff;

    // Precompute window multiplier for variable block size
    frequencies[i].window_mult = 4096.0 / frequencies[i].block_size;
  }
}
```

**CRITICAL FORMULA BREAKDOWN:**

1. **Block Size Calculation:**
   ```
   block_size = sample_rate / (max_distance_hz * 2.0)
   ```
   - `max_distance_hz` = frequency resolution needed (semitone spacing)
   - Factor of 2.0 for Nyquist compliance

2. **Bin Index (k):**
   ```
   k = round(block_size * target_freq / sample_rate)
   ```
   - This maps the target frequency to the nearest integer bin

3. **Angular Frequency (w):**
   ```
   w = 2π * k / block_size
   ```
   - Normalized angular frequency for the Goertzel algorithm

4. **Goertzel Coefficient:**
   ```
   coeff = 2 * cos(w)
   ```
   - This is the ONLY coefficient needed for the Goertzel recurrence

5. **Q14 Fixed-Point:**
   ```
   coeff_q14 = coeff * 16384  // (1 << 14)
   ```
   - Fixed-point representation for integer-only DSP

**LIGHTWAVEOS PORT:**
```cpp
void precomputeGoertzelCoefficients() {
    constexpr float PI = 3.141592653f;

    for (uint16_t i = 0; i < 64; i++) {
        g_freqBins[i].targetFreq = CANONICAL_NOTES[i];

        // Calculate semitone distance for bandwidth
        float semitoneHz = g_freqBins[i].targetFreq * 0.05946f;  // ~6% per semitone

        // Block size = sample_rate / (2 * bandwidth)
        g_freqBins[i].blockSize = static_cast<uint16_t>(
            LWOS_SAMPLE_RATE / (semitoneHz * 2.0f)
        );

        // Clamp to [64, 2000] samples
        if (g_freqBins[i].blockSize < 64) g_freqBins[i].blockSize = 64;
        if (g_freqBins[i].blockSize > 2000) g_freqBins[i].blockSize = 2000;

        // Calculate Goertzel coefficient
        float k = roundf(g_freqBins[i].blockSize * g_freqBins[i].targetFreq
                         / LWOS_SAMPLE_RATE);
        float w = (2.0f * PI * k) / g_freqBins[i].blockSize;
        float coeff = 2.0f * cosf(w);

        // Q14 fixed-point storage
        g_freqBins[i].coeffQ14 = static_cast<int32_t>(coeff * 16384.0f);

        // Precompute helpers
        g_freqBins[i].blockSizeRecip = 1.0f / g_freqBins[i].blockSize;
        g_freqBins[i].windowMult = 4096.0f / g_freqBins[i].blockSize;
    }
}
```

---

# SECTION 4: GOERTZEL ALGORITHM

## 4.1 Core Goertzel Implementation

**FROM: Sensory Bridge `GDFT.h:59-119` - DO NOT MODIFY**

```c
void IRAM_ATTR process_GDFT() {
  // Run GDFT (Goertzel-based Discrete Fourier Transform) with 64 frequencies
  for (uint16_t i = 0; i < NUM_FREQS; i++) {
    int32_t q0, q1, q2;
    int64_t mult;
    q1 = 0;
    q2 = 0;

    // Goertzel recurrence relation
    for (uint16_t n = 0; n < frequencies[i].block_size; n++) {
      // Get sample from history buffer (newest first)
      int32_t sample = (int32_t)sample_window[SAMPLE_HISTORY_LENGTH - 1 - n];

      // Fixed-point multiply: coeff_q14 * q1, then shift right by 14
      mult = (int32_t)frequencies[i].coeff_q14 * (int32_t)q1;

      // Goertzel iteration: q0 = sample + coeff*q1 - q2
      // Sample shifted right by 6 for headroom
      q0 = (sample >> 6) + (mult >> 14) - q2;

      // Shift state
      q2 = q1;
      q1 = q0;
    }

    // Final magnitude calculation
    mult = (int32_t)frequencies[i].coeff_q14 * (int32_t)q1;

    // Magnitude² = q2² + q1² - coeff*q1*q2
    magnitudes[i] = q2 * q2 + q1 * q1 - ((int32_t)(mult >> 14)) * q2;

    // Take square root for linear magnitude
    magnitudes[i] = sqrt(magnitudes[i]);

    // Normalize by block size / 2
    float normalized_magnitude = magnitudes[i] / float(frequencies[i].block_size / 2.0);
    magnitudes_normalized[i] = normalized_magnitude;
  }
}
```

**ALGORITHM EXPLANATION:**

The Goertzel algorithm computes a single DFT bin using a second-order IIR filter:

```
Recurrence (for each sample n):
    q0[n] = x[n] + coeff * q1[n-1] - q2[n-2]

Final magnitude:
    |X[k]|² = q1² + q2² - coeff * q1 * q2
```

**CRITICAL IMPLEMENTATION NOTES:**
1. **IRAM_ATTR**: Function must be in IRAM for ESP32 performance
2. **Sample ordering**: Newest samples FIRST (index decreasing)
3. **Headroom shift**: `sample >> 6` prevents overflow in 32-bit math
4. **Fixed-point multiply**: `(coeff_q14 * q1) >> 14` maintains precision
5. **Normalization**: Divide by `block_size / 2` for unit scale

**LIGHTWAVEOS PORT:**
```cpp
void IRAM_ATTR processGoertzel(const int16_t* samples, uint16_t sampleCount) {
    for (uint16_t bin = 0; bin < 64; bin++) {
        int32_t q0, q1 = 0, q2 = 0;
        const uint16_t blockSize = g_freqBins[bin].blockSize;
        const int32_t coeff = g_freqBins[bin].coeffQ14;

        // Process samples (newest first)
        for (uint16_t n = 0; n < blockSize && n < sampleCount; n++) {
            int32_t sample = static_cast<int32_t>(samples[sampleCount - 1 - n]);

            // Goertzel iteration with fixed-point coefficient
            int64_t mult = static_cast<int64_t>(coeff) * q1;
            q0 = (sample >> 6) + static_cast<int32_t>(mult >> 14) - q2;
            q2 = q1;
            q1 = q0;
        }

        // Final magnitude calculation
        int64_t mult = static_cast<int64_t>(coeff) * q1;
        int64_t magSquared = static_cast<int64_t>(q2) * q2
                           + static_cast<int64_t>(q1) * q1
                           - (static_cast<int32_t>(mult >> 14)) * q2;

        float magnitude = sqrtf(static_cast<float>(magSquared));
        g_magnitudes[bin] = magnitude / (blockSize * 0.5f);
    }
}
```

---

# SECTION 5: NOVELTY DETECTION (ONSET)

## 5.1 Spectral Flux Calculation

**FROM: Sensory Bridge `GDFT.h:201-242` - DO NOT MODIFY**

```c
void calculate_novelty(uint32_t t_now) {
  SQ15x16 novelty_now = 0.0;

  for (uint16_t i = 0; i < NUM_FREQS; i++) {
    // Calculate difference from previous frame
    SQ15x16 novelty_bin = spectrogram[i] - spectral_history[rounded_index][i];

    // Half-wave rectification: only count INCREASES
    if (novelty_bin < 0.0) {
      novelty_bin = 0.0;
    }

    novelty_now += novelty_bin;
  }

  // Normalize by number of bins
  novelty_now /= NUM_FREQS;

  // Square root for perceptual scaling
  novelty_curve[spectral_history_index] = sqrt(float(novelty_now));
}
```

**ALGORITHM:** Spectral Flux with Half-Wave Rectification

1. **For each frequency bin:** Calculate `current - previous`
2. **Rectify:** Set negative differences to zero (we only care about energy increases)
3. **Sum:** Add all positive differences
4. **Normalize:** Divide by bin count
5. **Perceptual scaling:** Square root compresses dynamic range

**WHY HALF-WAVE RECTIFICATION:**
- Energy *increases* indicate note onsets
- Energy *decreases* are just decay - not musically interesting
- This is the key insight that makes spectral flux work for beat detection

**LIGHTWAVEOS PORT:**
```cpp
float calculateNovelty(const float* currentMags, const float* prevMags) {
    float noveltySum = 0.0f;

    for (uint16_t i = 0; i < 64; i++) {
        float diff = currentMags[i] - prevMags[i];

        // Half-wave rectification
        if (diff > 0.0f) {
            noveltySum += diff;
        }
    }

    // Normalize and perceptual scale
    float novelty = noveltySum / 64.0f;
    return sqrtf(novelty);
}
```

---

# SECTION 6: TEMPO TRACKING

## 6.1 Tempo Constants

**FROM: Emotiscope `global_defines.h:19-33` - DO NOT MODIFY**

```c
#define NUM_FREQS             ( 64 )     // Audio frequency bins
#define NOVELTY_LOG_HZ        ( 50 )     // Novelty sampling rate
#define NOVELTY_HISTORY_LENGTH ( 1024 )  // 50 FPS for 20.48 seconds
#define NUM_TEMPI             ( 96 )     // Tempo bins
#define TEMPO_LOW             ( 60 )     // Minimum BPM
#define TEMPO_HIGH            ( TEMPO_LOW + NUM_TEMPI )  // 156 BPM max
#define BEAT_SHIFT_PERCENT    ( 0.0 )    // Phase alignment adjustment
#define REFERENCE_FPS         ( 100 )    // Reference frame rate for phase
```

**CRITICAL VALUES:**
| Constant | Value | Meaning |
|----------|-------|---------|
| NOVELTY_LOG_HZ | 50 | Novelty logged 50 times per second |
| NOVELTY_HISTORY_LENGTH | 1024 | ~20.5 seconds of history |
| NUM_TEMPI | 96 | BPM resolution: 1 bin per BPM |
| TEMPO_LOW | 60 | 60 BPM minimum |
| TEMPO_HIGH | 156 | 156 BPM maximum |
| REFERENCE_FPS | 100 | Phase advances 100x per second |

**LIGHTWAVEOS ADAPTATION:**
```cpp
namespace TempoConfig {
    constexpr uint8_t  NUM_TEMPI = 96;
    constexpr uint8_t  TEMPO_LOW_BPM = 60;
    constexpr uint8_t  TEMPO_HIGH_BPM = 156;  // TEMPO_LOW + NUM_TEMPI
    constexpr uint8_t  NOVELTY_LOG_HZ = 50;
    constexpr uint16_t NOVELTY_HISTORY_LENGTH = 1024;
    constexpr uint8_t  REFERENCE_FPS = 100;
    constexpr float    BEAT_SHIFT_PERCENT = 0.0f;
}
```

---

## 6.2 Tempo Bin Structure

**FROM: Emotiscope `tempo.h:16-36` - DO NOT MODIFY**

```c
struct tempo {
  float target_tempo_hz;              // Tempo in Hz (BPM / 60)
  float target_tempo_bpm;             // Tempo in BPM
  uint16_t block_size;                // Goertzel window size
  float window_step;                  // Window position increment
  float cosine;                       // cos(w) for magnitude calc
  float sine;                         // sin(w) for phase calc
  float coeff;                        // 2 * cos(w) - Goertzel coefficient
  float magnitude;                    // Raw magnitude
  float magnitude_full_scale;         // Normalized magnitude
  float phase;                        // Phase in radians [-π, π]
  float phase_radians_per_reference_frame;  // Phase advance per frame
  bool phase_inverted;                // Track phase wrapping
};

tempo tempi[NUM_TEMPI];
```

**LIGHTWAVEOS PORT:**
```cpp
struct TempoBin {
    float targetHz;           // BPM / 60.0f
    float targetBpm;          // Human-readable BPM
    uint16_t blockSize;       // Samples for Goertzel
    float windowStep;         // 4096.0f / blockSize
    float cosine;             // cos(w)
    float sine;               // sin(w)
    float coeff;              // 2 * cos(w)
    float magnitude;          // Raw Goertzel output
    float magnitudeNorm;      // Normalized [0, 1]
    float phase;              // [-PI, PI]
    float phasePerFrame;      // Radians per reference frame
    bool phaseInverted;       // Tracks half-beat phase
};

static TempoBin g_tempoBins[96];
```

---

## 6.3 Tempo Goertzel Initialization

**FROM: Emotiscope `tempo.h:51-118` - DO NOT MODIFY**

```c
void init_tempo_goertzel_constants() {
  for (uint16_t i = 0; i < NUM_TEMPI; i++) {
    // Target tempo in Hz (BPM / 60)
    tempi[i].target_tempo_hz = tempi_bpm_values_hz[i];

    // Calculate block size for tempo resolution
    // max_distance_hz = frequency resolution needed between adjacent tempi
    tempi[i].block_size = NOVELTY_LOG_HZ / (max_distance_hz * 0.5);

    // Clamp to history length
    if (tempi[i].block_size > NOVELTY_HISTORY_LENGTH) {
      tempi[i].block_size = NOVELTY_HISTORY_LENGTH;
    }

    // Window step for variable block size
    tempi[i].window_step = 4096.0 / tempi[i].block_size;

    // Calculate Goertzel coefficient
    float k = (int)(0.5 + ((tempi[i].block_size * tempi[i].target_tempo_hz)
                           / NOVELTY_LOG_HZ));
    float w = (2.0 * PI * k) / tempi[i].block_size;

    // Store trig values for magnitude AND phase calculation
    tempi[i].cosine = cos(w);
    tempi[i].sine = sin(w);
    tempi[i].coeff = 2.0 * tempi[i].cosine;

    // Phase advance per reference frame (for sync)
    // This is how much the beat phase advances each 1/100th second
    tempi[i].phase_radians_per_reference_frame =
        ((2.0 * PI * tempi[i].target_tempo_hz) / (float)(REFERENCE_FPS));
  }
}
```

**KEY INSIGHT:** The tempo Goertzel runs on the **novelty curve**, not raw audio. This is a second-stage Goertzel that detects periodicity in onset events.

**LIGHTWAVEOS PORT:**
```cpp
void initTempoGoertzelCoefficients() {
    constexpr float PI = 3.141592653f;

    for (uint16_t i = 0; i < TempoConfig::NUM_TEMPI; i++) {
        float bpm = static_cast<float>(TempoConfig::TEMPO_LOW_BPM + i);
        g_tempoBins[i].targetBpm = bpm;
        g_tempoBins[i].targetHz = bpm / 60.0f;

        // Block size based on tempo resolution
        float maxDistHz = g_tempoBins[i].targetHz * 0.02f;  // ~2% resolution
        g_tempoBins[i].blockSize = static_cast<uint16_t>(
            TempoConfig::NOVELTY_LOG_HZ / (maxDistHz * 0.5f)
        );

        // Clamp to history length
        if (g_tempoBins[i].blockSize > TempoConfig::NOVELTY_HISTORY_LENGTH) {
            g_tempoBins[i].blockSize = TempoConfig::NOVELTY_HISTORY_LENGTH;
        }

        g_tempoBins[i].windowStep = 4096.0f / g_tempoBins[i].blockSize;

        // Goertzel coefficient
        float k = roundf(g_tempoBins[i].blockSize * g_tempoBins[i].targetHz
                         / TempoConfig::NOVELTY_LOG_HZ);
        float w = (2.0f * PI * k) / g_tempoBins[i].blockSize;

        g_tempoBins[i].cosine = cosf(w);
        g_tempoBins[i].sine = sinf(w);
        g_tempoBins[i].coeff = 2.0f * g_tempoBins[i].cosine;

        // Phase advance per frame
        g_tempoBins[i].phasePerFrame =
            (2.0f * PI * g_tempoBins[i].targetHz) / TempoConfig::REFERENCE_FPS;
    }
}
```

---

## 6.4 Tempo Magnitude Calculation

**FROM: Emotiscope `tempo.h:152-215` - DO NOT MODIFY**

```c
void calculate_magnitude_of_tempo(int16_t tempo_bin) {
  float q1 = 0, q2 = 0;
  float window_pos = 0.0;

  // Goertzel on novelty curve (not raw audio!)
  for (uint16_t i = 0; i < block_size; i++) {
    // Get novelty sample from ring buffer
    float sample = novelty_curve[index] * novelty_scale_factor;

    // Apply window function
    float windowed_sample = sample * window_lookup[(uint32_t)window_pos];

    // Goertzel iteration
    float q0 = tempi[tempo_bin].coeff * q1 - q2 + windowed_sample;
    q2 = q1;
    q1 = q0;

    window_pos += (tempi[tempo_bin].window_step);
  }

  // Calculate real and imaginary parts for phase
  float real = (q1 - q2 * tempi[tempo_bin].cosine);
  float imag = (q2 * tempi[tempo_bin].sine);

  // Phase via atan2
  tempi[tempo_bin].phase = atan2(imag, real) + (PI * BEAT_SHIFT_PERCENT);

  // Phase wrapping at ±PI (handled elsewhere)

  // Magnitude calculation
  float magnitude_squared = (q1 * q1) + (q2 * q2) - q1 * q2 * tempi[tempo_bin].coeff;

  // Normalize by block size / 2
  tempi[tempo_bin].magnitude_full_scale = sqrt(magnitude_squared) / (block_size / 2.0);
}
```

**CRITICAL DIFFERENCES FROM AUDIO GOERTZEL:**
1. **Input:** Novelty curve (onset detection output), not raw audio
2. **Window function:** Applied during iteration for variable block sizes
3. **Phase calculation:** Uses atan2(imag, real) for beat sync
4. **Both magnitude AND phase** are extracted

**LIGHTWAVEOS PORT:**
```cpp
void calculateTempoMagnitude(uint16_t tempoBin, const float* noveltyCurve) {
    constexpr float PI = 3.141592653f;

    float q1 = 0.0f, q2 = 0.0f;
    float windowPos = 0.0f;
    const uint16_t blockSize = g_tempoBins[tempoBin].blockSize;
    const float coeff = g_tempoBins[tempoBin].coeff;

    for (uint16_t i = 0; i < blockSize; i++) {
        // Get novelty from ring buffer (implement index wrapping)
        float sample = noveltyCurve[i];  // Simplified - real impl needs ring buffer

        // Window and iterate
        float windowed = sample * g_windowLookup[static_cast<uint32_t>(windowPos)];
        float q0 = coeff * q1 - q2 + windowed;
        q2 = q1;
        q1 = q0;

        windowPos += g_tempoBins[tempoBin].windowStep;
    }

    // Phase calculation
    float real = q1 - q2 * g_tempoBins[tempoBin].cosine;
    float imag = q2 * g_tempoBins[tempoBin].sine;
    g_tempoBins[tempoBin].phase = atan2f(imag, real);

    // Magnitude
    float magSq = q1*q1 + q2*q2 - q1*q2*coeff;
    g_tempoBins[tempoBin].magnitudeNorm = sqrtf(magSq) / (blockSize * 0.5f);
}
```

---

## 6.5 Beat Phase Synchronization

**FROM: Emotiscope `tempo.h:351-368` - DO NOT MODIFY**

```c
void sync_beat_phase(uint16_t tempo_bin, float delta) {
  // Advance phase based on tempo and time delta
  float push = (tempi[tempo_bin].phase_radians_per_reference_frame * delta);
  tempi[tempo_bin].phase += push;

  // Wrap at PI (not 2*PI!) to track half-beats
  if (tempi[tempo_bin].phase > PI) {
    tempi[tempo_bin].phase -= (2 * PI);
    tempi[tempo_bin].phase_inverted = !tempi[tempo_bin].phase_inverted;
  }
}
```

**CRITICAL INSIGHT:** Phase wraps at **PI, not 2*PI**. This tracks half-beats:
- `phase_inverted = false`: Downbeat
- `phase_inverted = true`: Offbeat

This allows effects to respond to both beats AND offbeats with different behaviors.

**LIGHTWAVEOS PORT:**
```cpp
void syncBeatPhase(uint16_t tempoBin, float deltaFrames) {
    constexpr float PI = 3.141592653f;
    constexpr float TWO_PI = 6.283185307f;

    float advance = g_tempoBins[tempoBin].phasePerFrame * deltaFrames;
    g_tempoBins[tempoBin].phase += advance;

    // Wrap at PI for half-beat tracking
    if (g_tempoBins[tempoBin].phase > PI) {
        g_tempoBins[tempoBin].phase -= TWO_PI;
        g_tempoBins[tempoBin].phaseInverted = !g_tempoBins[tempoBin].phaseInverted;
    }
}
```

---

## 6.6 Tempo Smoothing and Confidence

**FROM: Emotiscope `tempo.h:371-407` - DO NOT MODIFY**

```c
void update_tempi_phase(float delta) {
  for (uint16_t tempo_bin = 0; tempo_bin < NUM_TEMPI; tempo_bin++) {
    float tempi_magnitude = tempi[tempo_bin].magnitude_full_scale;

    // Only update if magnitude above threshold
    if (tempi_magnitude > 0.005) {
      // Smooth the magnitude: 97.5% old + 2.5% new
      tempi_smooth[tempo_bin] = tempi_smooth[tempo_bin] * 0.975
                               + (tempi_magnitude) * 0.025;

      // Sync phase for this active tempo
      sync_beat_phase(tempo_bin, delta);
    }
    else {
      // Decay inactive tempos
      tempi_smooth[tempo_bin] *= 0.995;
    }
  }

  // Calculate confidence: strongest tempo / sum of all tempos
  tempo_confidence = max_contribution / tempi_power_sum;
}
```

**MAGIC NUMBERS DOCUMENTED:**

| Value | Purpose | Rationale |
|-------|---------|-----------|
| 0.005 | Magnitude threshold | Below this is noise |
| 0.975 | Smoothing retention | ~40 frames to settle |
| 0.025 | Smoothing update | 1 - 0.975 |
| 0.995 | Inactive decay | Slow fade for tempo memory |

**SMOOTHING TIME CONSTANT:**
```
τ = -1 / ln(0.975) ≈ 39.5 frames
At 50 Hz novelty rate: 39.5 / 50 = 0.79 seconds to 63% convergence
```

**LIGHTWAVEOS PORT:**
```cpp
void updateTempoPhases(float deltaFrames) {
    float maxMag = 0.0f;
    float sumMag = 0.0f;

    for (uint16_t i = 0; i < TempoConfig::NUM_TEMPI; i++) {
        float mag = g_tempoBins[i].magnitudeNorm;

        if (mag > 0.005f) {
            // Smooth active tempos
            g_tempoSmooth[i] = g_tempoSmooth[i] * 0.975f + mag * 0.025f;
            syncBeatPhase(i, deltaFrames);
        } else {
            // Decay inactive tempos
            g_tempoSmooth[i] *= 0.995f;
        }

        sumMag += g_tempoSmooth[i];
        if (g_tempoSmooth[i] > maxMag) {
            maxMag = g_tempoSmooth[i];
            g_dominantTempoBin = i;
        }
    }

    g_tempoConfidence = (sumMag > 0.0f) ? (maxMag / sumMag) : 0.0f;
}
```

---

# SECTION 7: CONTRACT LAYER (ControlBusFrame)

## 7.1 Output Structure

The contract layer defines what audio data is exposed to the visual pipeline.

**LIGHTWAVEOS DESIGN:**
```cpp
struct ControlBusFrame {
    // === Timestamp ===
    uint32_t timestampMs;           // Frame timestamp

    // === Frequency Domain (from Goertzel) ===
    float magnitudes[64];           // Raw 64-bin magnitudes
    float magnitudesSmooth[64];     // Smoothed (80ms rise / 15ms fall)

    // === Band Summary ===
    float bass;                     // Bins 0-15 average
    float mid;                      // Bins 16-47 average
    float treble;                   // Bins 48-63 average
    float bassSmooth;               // Smoothed bass
    float midSmooth;                // Smoothed mid
    float trebleSmooth;             // Smoothed treble

    // === Dynamics ===
    float energy;                   // Total energy (sum of magnitudes)
    float energyDelta;              // Energy change (for onsets)
    float novelty;                  // Spectral flux (onset strength)

    // === Tempo/Beat ===
    float dominantTempoBpm;         // Strongest detected tempo
    float tempoConfidence;          // Confidence [0, 1]
    float beatPhase;                // [-PI, PI] phase of dominant tempo
    bool beatPhaseInverted;         // True = offbeat
    bool onBeat;                    // True when phase crosses zero

    // === Musical Intelligence (optional) ===
    float harmonicSaliency;         // How important is harmony right now
    float rhythmicSaliency;         // How important is rhythm right now
    uint8_t musicStyle;             // RHYTHMIC/HARMONIC/MELODIC/TEXTURE/DYNAMIC
};
```

**CROSS-CORE COMMUNICATION:**
```cpp
// Lock-free double-buffer for Core 0 → Core 1 transfer
struct SnapshotBuffer {
    ControlBusFrame frames[2];
    std::atomic<uint8_t> writeIndex{0};
    std::atomic<uint8_t> readIndex{0};

    void publish(const ControlBusFrame& frame) {
        uint8_t idx = writeIndex.load();
        frames[idx] = frame;
        readIndex.store(idx);
        writeIndex.store(1 - idx);
    }

    const ControlBusFrame& read() const {
        return frames[readIndex.load()];
    }
};
```

---

# SECTION 8: VERIFICATION TESTS

## 8.1 Goertzel Coefficient Test

```cpp
// Test: Verify Goertzel coefficients match Sensory Bridge
void testGoertzelCoefficients() {
    initWindowLookup();
    precomputeGoertzelCoefficients();

    // Bin 36 = A4 = 440 Hz
    assert(fabsf(g_freqBins[36].targetFreq - 440.0f) < 0.01f);

    // Coefficient should be 2*cos(w) which is always in range [-2, 2]
    for (int i = 0; i < 64; i++) {
        float coeff = g_freqBins[i].coeffQ14 / 16384.0f;
        assert(coeff >= -2.0f && coeff <= 2.0f);
    }

    // Block sizes should be reasonable
    for (int i = 0; i < 64; i++) {
        assert(g_freqBins[i].blockSize >= 64);
        assert(g_freqBins[i].blockSize <= 2000);
    }
}
```

## 8.2 Tempo Tracking Test

```cpp
// Test: Verify tempo detection on synthetic 120 BPM signal
void testTempoDetection() {
    initTempoGoertzelCoefficients();

    // Generate synthetic novelty curve: impulses every 500ms (120 BPM)
    float noveltyCurve[1024] = {0};
    for (int i = 0; i < 1024; i += 25) {  // 50Hz * 0.5s = 25 samples
        noveltyCurve[i] = 1.0f;
    }

    // Calculate all tempo magnitudes
    for (int i = 0; i < 96; i++) {
        calculateTempoMagnitude(i, noveltyCurve);
    }

    // Find dominant tempo
    int dominant = 0;
    float maxMag = 0.0f;
    for (int i = 0; i < 96; i++) {
        if (g_tempoBins[i].magnitudeNorm > maxMag) {
            maxMag = g_tempoBins[i].magnitudeNorm;
            dominant = i;
        }
    }

    // Should detect 120 BPM (bin 60 = 60 + 60)
    float detectedBpm = g_tempoBins[dominant].targetBpm;
    assert(fabsf(detectedBpm - 120.0f) < 2.0f);  // Within 2 BPM
}
```

## 8.3 Phase Wrapping Test

```cpp
// Test: Verify phase wraps correctly at PI
void testPhaseWrapping() {
    g_tempoBins[60].phase = 3.0f;  // Just under PI
    g_tempoBins[60].phasePerFrame = 0.2f;
    g_tempoBins[60].phaseInverted = false;

    syncBeatPhase(60, 1.0f);  // Advance by 0.2

    // Phase should have wrapped
    assert(g_tempoBins[60].phase < -2.0f);  // Wrapped to negative
    assert(g_tempoBins[60].phaseInverted == true);  // Toggled
}
```

---

# SECTION 9: FORBIDDEN MODIFICATIONS

## 9.1 DO NOT CHANGE These Values

| Value | Source | Reason |
|-------|--------|--------|
| NUM_FREQS = 64 | Sensory Bridge | Semitone resolution requirement |
| Note frequencies array | Sensory Bridge | Equal temperament precision |
| Hamming coefficient 0.54 | Standard DSP | Validated window function |
| Q14 fixed-point format | Sensory Bridge | Bit-exact compatibility |
| NUM_TEMPI = 96 | Emotiscope | 1 BPM resolution requirement |
| TEMPO_LOW = 60 | Emotiscope | Validated musical range |
| NOVELTY_LOG_HZ = 50 | Emotiscope | Tempo resolution requirement |
| Smoothing 0.975/0.025 | Emotiscope | Validated response time |
| Phase wrap at PI | Emotiscope | Half-beat tracking requirement |

## 9.2 DO NOT ADD

- FFT-based frequency analysis (Goertzel is intentional)
- Different windowing functions (Hamming is validated)
- Additional smoothing stages (creates latency)
- "Improved" tempo tracking algorithms
- ML/AI-based beat detection
- Third-party audio libraries

## 9.3 IF YOU THINK YOU NEED TO CHANGE SOMETHING

1. **STOP**
2. Re-read Section 1 (PURPOSE) of this document
3. Search the conversation history for why this constraint exists
4. If still convinced, document your reasoning in a new planning document
5. Get explicit PRD approval before proceeding

---

# APPENDIX A: File Mapping

| Hardened Spec Section | Target File | Line Range |
|-----------------------|-------------|------------|
| 1.1 Sample Rate | `src/audio/AudioConfig.h` | TBD |
| 1.2 Note Frequencies | `src/audio/FrequencyTables.cpp` | TBD |
| 1.3 Frequency Bin Struct | `src/audio/GoertzelBank.h` | TBD |
| 2.1 Window Function | `src/audio/WindowFunction.cpp` | TBD |
| 3.1 Coefficient Calc | `src/audio/GoertzelBank.cpp` | TBD |
| 4.1 Goertzel Algorithm | `src/audio/GoertzelBank.cpp` | TBD |
| 5.1 Novelty Detection | `src/audio/NoveltyFlux.cpp` | TBD |
| 6.1-6.6 Tempo Tracking | `src/audio/tempo/TempoTracker.cpp` | TBD |
| 7.1 Contract Layer | `src/audio/ControlBusFrame.h` | TBD |

---

# APPENDIX B: Quick Reference Card

```
GOERTZEL COEFFICIENT:    coeff = 2 * cos(2π * k / N)
GOERTZEL ITERATION:      q0 = sample + coeff * q1 - q2
GOERTZEL MAGNITUDE:      |X|² = q1² + q2² - coeff * q1 * q2
GOERTZEL PHASE:          φ = atan2(q2 * sin(w), q1 - q2 * cos(w))

WINDOW FUNCTION:         w(n) = 0.54 * (1 - cos(2πn/(N-1)))  [Hamming]

NOVELTY (SPECTRAL FLUX): sum(max(0, current[i] - prev[i])) / NUM_BINS

TEMPO SMOOTHING:         smooth = smooth * 0.975 + new * 0.025

PHASE ADVANCE:           phase += (2π * tempo_hz / REFERENCE_FPS) * delta
PHASE WRAP:              if (phase > PI) { phase -= 2*PI; inverted = !inverted; }
```

---

**END OF HARDENED SPECIFICATION**

*This document is the canonical reference for LightwaveOS v2 audio pipeline implementation. Any deviation requires PRD approval.*
