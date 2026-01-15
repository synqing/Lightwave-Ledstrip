# Magic Numbers Reference: Audio Pipeline

**Version:** 1.0.0
**Status:** CANONICAL REFERENCE
**Last Updated:** 2026-01-12

---

## Purpose

This document provides **complete derivations** for every magic number in the audio pipeline. If you encounter a number in the codebase and wonder "why this value?", the answer is here.

**AGENT INSTRUCTION:** Before changing ANY numeric constant, find it in this document. If it's listed here, DO NOT CHANGE IT without PRD approval.

---

# CATEGORY 1: Sample Rates & Timing

## 1.1 Sample Rate: 16000 Hz

| Property | Value |
|----------|-------|
| **Constant** | `LWOS_SAMPLE_RATE` |
| **Value** | 16000 |
| **Unit** | Hz (samples per second) |
| **Source** | LightwaveOS hardware design |

**Derivation:**
```
ESP32-S3 I2S + SPH0645 I2S MEMS microphone
- SPH0645 supports 32-64 kHz sample rates
- 16 kHz chosen for optimal CPU/quality balance
- Direct I2S digital output (no analog conversion needed)
- Standard audio sample rate, widely supported
```

**Why not 12800 Hz (Sensory Bridge)?**
Sensory Bridge uses 12800 Hz with different hardware. LightwaveOS uses the SPH0645 I2S MEMS microphone at 16 kHz for cleaner division and broader compatibility. The Goertzel coefficients are recalculated for our sample rate.

**Constraints:**
- Nyquist limit: 8000 Hz (highest detectable frequency)
- Our highest bin (C7): 2093 Hz - well within Nyquist

---

## 1.2 Chunk Size: 128 Samples

| Property | Value |
|----------|-------|
| **Constant** | `LWOS_CHUNK_SIZE` |
| **Value** | 128 |
| **Unit** | samples |
| **Source** | Sensory Bridge `NATIVE_RESOLUTION` |

**Derivation:**
```
Chunk duration = 128 / 16000 = 0.008 seconds = 8 ms
Hop rate = 16000 / 128 = 125 Hz (hops per second)
```

**Why 128?**
- Power of 2 for efficient DMA transfers
- 8ms window provides good time resolution for beat detection
- Matches Sensory Bridge for proven performance
- Small enough to fit in ESP32-S3 DMA buffers

**Constraints:**
- Minimum for bass detection: ~46 samples (for 55 Hz with 3 cycles)
- Maximum for latency: ~256 samples (16ms feels sluggish)

---

## 1.3 Hop Period: 8 ms

| Property | Value |
|----------|-------|
| **Derived From** | `CHUNK_SIZE / SAMPLE_RATE` |
| **Value** | 0.008 |
| **Unit** | seconds |
| **Frequency** | 125 Hz |

**Derivation:**
```
hop_period = 128 samples / 16000 samples/sec = 0.008 sec = 8 ms
hop_rate = 1 / 0.008 = 125 Hz
```

**Why this matters:**
- All audio processing must complete within 8ms
- Budget: ~6ms for DSP, ~2ms margin for OS jitter
- At 240 MHz: 8ms = 1,920,000 CPU cycles available

---

# CATEGORY 2: Frequency Analysis

## 2.1 Number of Frequency Bins: 64

| Property | Value |
|----------|-------|
| **Constant** | `NUM_FREQS` |
| **Value** | 64 |
| **Unit** | bins |
| **Source** | Sensory Bridge `constants.h:8` |

**Derivation:**
```
64 bins = 5 octaves + 4 semitones
Starting note: A1 (55 Hz)
Ending note: C7 (2093 Hz)

Octave coverage:
- Octave 1: A1-G#2 (bins 0-11) = 12 semitones
- Octave 2: A2-G#3 (bins 12-23) = 12 semitones
- Octave 3: A3-G#4 (bins 24-35) = 12 semitones
- Octave 4: A4-G#5 (bins 36-47) = 12 semitones
- Octave 5: A5-G#6 (bins 48-59) = 12 semitones
- Octave 6: A6-C7 (bins 60-63) = 4 semitones (partial)

Total: 60 + 4 = 64 bins
```

**Why 64?**
- Covers musically relevant range (bass through brilliance)
- Power of 2 for efficient array indexing
- Not too many (CPU cost) or too few (resolution)

---

## 2.2 Note Frequency Formula

| Property | Value |
|----------|-------|
| **Formula** | `f(n) = 440 × 2^((n-49)/12)` |
| **Reference** | A4 = 440 Hz at n=49 |
| **Source** | Equal temperament standard (ISO 16) |

**Derivation for key frequencies:**
```
A1 (n=13): 440 × 2^((13-49)/12) = 440 × 2^(-3) = 55.000 Hz
A4 (n=49): 440 × 2^((49-49)/12) = 440 × 2^0 = 440.000 Hz
C7 (n=76): 440 × 2^((76-49)/12) = 440 × 2^(27/12) = 2093.005 Hz
```

**Why equal temperament?**
- Standard tuning for Western music
- Mathematically precise (no approximations needed)
- Every semitone is exactly 2^(1/12) = 1.05946... ratio

**Semitone ratio:**
```
2^(1/12) = 1.0594630943592953...
Each semitone is ~5.946% higher than the previous
```

---

## 2.3 Goertzel Coefficient: 2×cos(w)

| Property | Value |
|----------|-------|
| **Formula** | `coeff = 2 × cos(2π × k / N)` |
| **Range** | [-2.0, +2.0] |
| **Source** | Goertzel algorithm mathematics |

**Derivation:**
```
For target frequency f at sample rate fs with block size N:

1. Calculate bin index k:
   k = round(N × f / fs)

2. Calculate angular frequency w:
   w = 2π × k / N

3. Calculate coefficient:
   coeff = 2 × cos(w)
```

**Example for A4 (440 Hz) at 16 kHz, block size 200:**
```
k = round(200 × 440 / 16000) = round(5.5) = 6
w = 2π × 6 / 200 = 0.1885 radians
coeff = 2 × cos(0.1885) = 2 × 0.9823 = 1.9646
```

**Why this formula?**
The Goertzel algorithm is a specialized DFT that computes a single frequency bin using a second-order IIR filter. The recurrence relation `q0 = x + coeff×q1 - q2` requires only this one coefficient.

---

## 2.4 Q14 Fixed-Point Format

| Property | Value |
|----------|-------|
| **Shift** | 14 bits |
| **Multiplier** | 16384 (2^14) |
| **Source** | Sensory Bridge optimization |

**Derivation:**
```
Q14 format: value_fixed = value_float × 2^14

For coeff = 1.9646:
coeff_q14 = 1.9646 × 16384 = 32,183

To convert back:
coeff_float = 32183 / 16384 = 1.9646
```

**Why Q14?**
- Coefficients range [-2, +2], so Q14 fits in int16_t with headroom
- 14 bits of fractional precision = 0.00006 resolution
- Integer multiply is faster than float on ESP32 without FPU
- Matches Sensory Bridge for bit-exact compatibility

---

## 2.5 Block Size: 64-2000 Samples (Variable)

| Property | Value |
|----------|-------|
| **Minimum** | 64 samples |
| **Maximum** | 2000 samples |
| **Source** | Sensory Bridge `system.h:219` |

**Derivation:**
```
Block size depends on frequency resolution needed:

block_size = sample_rate / (bandwidth × 2)

For semitone spacing, bandwidth ≈ 6% of center frequency:
- Low frequencies (55 Hz): need large windows for resolution
- High frequencies (2093 Hz): can use smaller windows

Example for A1 (55 Hz):
bandwidth = 55 × 0.0595 = 3.27 Hz
block_size = 16000 / (3.27 × 2) = 2446 → clamped to 2000

Example for C7 (2093 Hz):
bandwidth = 2093 × 0.0595 = 124.5 Hz
block_size = 16000 / (124.5 × 2) = 64
```

**Why variable block sizes?**
- Frequency resolution is proportional to block size
- Bass frequencies need more samples to resolve
- Treble frequencies can use fewer samples
- This is the key insight that makes Goertzel superior to FFT for music

---

# CATEGORY 3: Window Function

## 3.1 Hamming Coefficient: 0.54

| Property | Value |
|----------|-------|
| **Constant** | 0.54 |
| **Window Type** | Hamming |
| **Source** | Standard DSP / Sensory Bridge |

**Derivation:**
```
Hamming window formula:
w(n) = α - (1-α) × cos(2πn / (N-1))

where α = 0.54 (Hamming) or α = 0.5 (Hann)

Hamming: w(n) = 0.54 - 0.46 × cos(2πn / (N-1))
Hann:    w(n) = 0.50 - 0.50 × cos(2πn / (N-1))
```

**Why Hamming (0.54) instead of Hann (0.5)?**
- Better sidelobe suppression (-43 dB vs -32 dB)
- Slightly wider main lobe (acceptable tradeoff)
- Sensory Bridge uses 0.54, so we match it

**Window lookup table:**
```
4096 entries, symmetrical
Scale factor: 32767 (Q15 max)
window_lookup[i] = 32767 × 0.54 × (1 - cos(2π × i / 4095))
```

---

## 3.2 Window Table Size: 4096

| Property | Value |
|----------|-------|
| **Constant** | 4096 |
| **Unit** | entries |
| **Source** | Sensory Bridge design |

**Derivation:**
```
4096 = 2^12 (power of 2 for efficient indexing)

Window step calculation:
window_step = 4096 / block_size

For block_size = 200:
window_step = 4096 / 200 = 20.48
Each sample advances window position by 20.48
```

**Why 4096?**
- Large enough for smooth interpolation across all block sizes
- Power of 2 for fast modulo operations
- Symmetrical: only compute half (2048 entries), mirror the rest
- Memory: 4096 × 2 bytes = 8 KB (acceptable for ESP32-S3)

---

## 3.3 Q15 Scale Factor: 32767

| Property | Value |
|----------|-------|
| **Constant** | 32767 |
| **Formula** | 2^15 - 1 |
| **Source** | 16-bit signed integer max |

**Derivation:**
```
int16_t range: -32768 to +32767
Window values are always positive: 0 to 32767
Using 32767 as max preserves full resolution
```

**Why Q15?**
- Window × sample stays within int32_t range
- `32767 × 32767 = 1,073,676,289` (fits in int32_t)
- Efficient on ARM Cortex-M and RISC-V cores

---

# CATEGORY 4: Tempo Tracking

## 4.1 Number of Tempo Bins: 96

| Property | Value |
|----------|-------|
| **Constant** | `NUM_TEMPI` |
| **Value** | 96 |
| **Unit** | bins |
| **Source** | Emotiscope `global_defines.h:22` |

**Derivation:**
```
Tempo range: 60 BPM to 156 BPM
Resolution: 1 BPM per bin
NUM_TEMPI = 156 - 60 = 96 bins
```

**Why 60-156 BPM?**
- 60 BPM: Slowest common musical tempo (ballads, ambient)
- 156 BPM: Fast EDM, drum & bass fundamentals
- Covers 99% of popular music
- Higher tempos (>156) are often perceived as half-time

---

## 4.2 Tempo Range: 60-156 BPM

| Property | Value |
|----------|-------|
| **TEMPO_LOW** | 60 BPM |
| **TEMPO_HIGH** | 156 BPM |
| **Source** | Emotiscope design |

**Derivation:**
```
BPM to Hz conversion:
tempo_hz = BPM / 60

60 BPM = 1.0 Hz (one beat per second)
120 BPM = 2.0 Hz (two beats per second)
156 BPM = 2.6 Hz

These are the frequencies the tempo Goertzel detects
in the novelty curve.
```

**Why not higher?**
- Above 156 BPM, perception shifts to half-time
- 180 BPM drum & bass is felt as 90 BPM pulse
- Tracking higher wastes CPU without benefit

---

## 4.3 Novelty Log Rate: 50 Hz

| Property | Value |
|----------|-------|
| **Constant** | `NOVELTY_LOG_HZ` |
| **Value** | 50 |
| **Unit** | Hz |
| **Source** | Emotiscope `global_defines.h:20` |

**Derivation:**
```
Novelty is logged every 20ms (1/50 second)
This determines tempo Goertzel resolution.

Nyquist limit for tempo: 50/2 = 25 Hz = 1500 BPM
Our max (156 BPM = 2.6 Hz) is well below this.

Why 50 Hz?
- Fast enough to capture beat transients
- Slow enough to average out noise
- Power of 10 for easy timing calculation
```

---

## 4.4 Novelty History Length: 1024

| Property | Value |
|----------|-------|
| **Constant** | `NOVELTY_HISTORY_LENGTH` |
| **Value** | 1024 |
| **Unit** | samples |
| **Source** | Emotiscope `global_defines.h:21` |

**Derivation:**
```
Duration = 1024 / 50 Hz = 20.48 seconds of history

Why 20+ seconds?
- Tempo tracking needs multiple beat cycles
- At 60 BPM: 20 seconds = 20 beats
- At 156 BPM: 20 seconds = 52 beats
- More history = more stable tempo detection

Power of 2 for efficient ring buffer indexing.
```

---

## 4.5 Reference FPS: 100

| Property | Value |
|----------|-------|
| **Constant** | `REFERENCE_FPS` |
| **Value** | 100 |
| **Unit** | frames per second |
| **Source** | Emotiscope `global_defines.h:27` |

**Derivation:**
```
Phase advances at 100 Hz reference rate.

phase_advance = (2π × tempo_hz) / REFERENCE_FPS

For 120 BPM (2.0 Hz):
phase_advance = (2π × 2.0) / 100 = 0.1257 radians/frame

At actual frame rate, scale by delta:
actual_advance = phase_advance × (actual_fps / 100)
```

**Why 100?**
- Nice round number for calculations
- Higher than typical display rates (60-120 Hz)
- Ensures smooth phase interpolation

---

## 4.6 Smoothing Coefficients: 0.975 / 0.025

| Property | Value |
|----------|-------|
| **Retention** | 0.975 |
| **Update** | 0.025 |
| **Sum** | 1.0 |
| **Source** | Emotiscope `tempo.h:375` |

**Derivation:**
```
Exponential moving average:
smooth = smooth × 0.975 + new × 0.025

Time constant τ:
τ = -1 / ln(0.975) = 39.5 frames

At 50 Hz novelty rate:
settling_time = 39.5 / 50 = 0.79 seconds to 63%
full_settle = 0.79 × 5 = ~4 seconds to 99%
```

**Why these values?**
- Fast enough to track tempo changes within a song
- Slow enough to reject noise and false beats
- Proven in Emotiscope across thousands of songs

---

## 4.7 Inactive Decay: 0.995

| Property | Value |
|----------|-------|
| **Constant** | 0.995 |
| **Source** | Emotiscope `tempo.h:383` |

**Derivation:**
```
When a tempo bin is inactive (magnitude < 0.005):
smooth *= 0.995

Time constant τ:
τ = -1 / ln(0.995) = 199.5 frames

At 50 Hz:
settling_time = 199.5 / 50 = 4.0 seconds to 63%

This slow decay provides "tempo memory" - if the beat
stops briefly, the system remembers the last tempo.
```

---

## 4.8 Magnitude Threshold: 0.005

| Property | Value |
|----------|-------|
| **Constant** | 0.005 |
| **Source** | Emotiscope `tempo.h:373` |

**Derivation:**
```
Threshold for considering a tempo bin "active"

Below 0.005: noise floor, no meaningful periodicity
Above 0.005: potential tempo detected

This is ~0.5% of full scale, empirically tuned to
reject noise while detecting weak but real beats.
```

---

# CATEGORY 5: Mathematical Constants

## 5.1 PI: 3.141592653

| Property | Value |
|----------|-------|
| **Constant** | `PI` |
| **Value** | 3.141592653 |
| **Precision** | 9 decimal places |

**Why this precision?**
- float32 has ~7 significant digits
- 9 digits ensures no loss in float conversion
- Standard mathematical constant

---

## 5.2 TWO_PI: 6.283185307

| Property | Value |
|----------|-------|
| **Constant** | `TWO_PI` or `TWOPI` |
| **Value** | 6.283185307 |
| **Formula** | 2 × π |

**Usage:**
- Angular frequency calculations: `w = TWO_PI × f / fs`
- Phase wrapping: `phase -= TWO_PI`
- Window function: `cos(TWO_PI × ratio)`

---

## 5.3 Semitone Ratio: 1.0594630943592953

| Property | Value |
|----------|-------|
| **Formula** | `2^(1/12)` |
| **Value** | 1.0594630943592953 |
| **Inverse** | 0.9438743126816935 |

**Usage:**
- Each semitone is this ratio higher than the previous
- Frequency bandwidth = center_freq × 0.05946 (approximately)
- Used to calculate block sizes for frequency resolution

---

# CATEGORY 6: Hardware Constraints

## 6.1 LED Count: 320

| Property | Value |
|----------|-------|
| **Constant** | `NUM_LEDS` |
| **Value** | 320 |
| **Configuration** | 2 strips × 160 LEDs |

**Derivation:**
- Strip 1: 160 LEDs (GPIO 4)
- Strip 2: 160 LEDs (GPIO 5)
- Center point: LED 79/80

---

## 6.2 Center Point: LED 79/80

| Property | Value |
|----------|-------|
| **Center LEDs** | 79, 80 |
| **Distance from edge** | 79 LEDs each direction |

**Derivation:**
```
160 LEDs per strip, 0-indexed
Middle = 160 / 2 = 80
Center LEDs = 79 and 80 (both sides of center)
Edge LEDs = 0 and 159
```

**Why center origin?**
Light Guide Plate (LGP) creates interference patterns. Effects originating from center create visually pleasing symmetric interference.

---

# Quick Reference Card

```
═══════════════════════════════════════════════════════════════
                    AUDIO PIPELINE MAGIC NUMBERS
═══════════════════════════════════════════════════════════════

SAMPLE RATE & TIMING
  SAMPLE_RATE      = 16000 Hz     (I2S mic native)
  CHUNK_SIZE       = 128 samples  (8 ms window)
  HOP_RATE         = 125 Hz       (125 hops/second)

FREQUENCY ANALYSIS
  NUM_FREQS        = 64           (semitone bins, A1-C7)
  BLOCK_SIZE_MIN   = 64           (treble, fast response)
  BLOCK_SIZE_MAX   = 2000         (bass, high resolution)
  Q14_SCALE        = 16384        (fixed-point multiplier)

WINDOW FUNCTION
  HAMMING_COEFF    = 0.54         (window shape)
  TABLE_SIZE       = 4096         (lookup entries)
  Q15_MAX          = 32767        (window scale)

TEMPO TRACKING
  NUM_TEMPI        = 96           (60-156 BPM)
  TEMPO_LOW        = 60 BPM       (slowest)
  TEMPO_HIGH       = 156 BPM      (fastest)
  NOVELTY_LOG_HZ   = 50 Hz        (onset rate)
  NOVELTY_HISTORY  = 1024         (20.48 seconds)
  REFERENCE_FPS    = 100          (phase reference)

SMOOTHING
  ACTIVE_RETAIN    = 0.975        (EMA coefficient)
  ACTIVE_UPDATE    = 0.025        (1 - RETAIN)
  INACTIVE_DECAY   = 0.995        (slow fade)
  MAG_THRESHOLD    = 0.005        (noise floor)

MATH CONSTANTS
  PI               = 3.141592653
  TWO_PI           = 6.283185307
  SEMITONE_RATIO   = 1.0594630943592953

═══════════════════════════════════════════════════════════════
```

---

**END OF MAGIC NUMBERS REFERENCE**

*If a number isn't in this document, it's either derived from these values or needs to be added here before use.*
