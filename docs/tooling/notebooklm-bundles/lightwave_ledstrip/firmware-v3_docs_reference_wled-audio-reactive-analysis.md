---
abstract: "Comprehensive analysis of WLED Sound Reactive audio processing implementation on ESP32. Covers FFT processing, beat detection, AGC, noise gating, frequency band mapping, and pink noise correction. Direct reference for K1 audio architecture decisions."
---

# WLED Audio Reactive Processing — K1 Reference Analysis

## Overview

WLED Sound Reactive (WLED-SR) is a mature, battle-tested audio processing system for ESP32-based LED controllers. Deployed in thousands of user installations worldwide. This document extracts the core algorithms and parameters that inform K1's audio pipeline design.

**Key distinction:** WLED-SR provides **frequency bin extraction + beat/peak detection** but does NOT implement kick/snare/hihat separation. It offers beat presence as a boolean flag derived from spectral peaks in user-selected frequency bins.

---

## FFT Processing Pipeline

### Sample Acquisition & Preprocessing

**Digitisation:**
- Base sample rate: **10.24 kHz** (50ms physical window per 512-sample batch)
- Alternative rates: 20.48 kHz, 22.05 kHz (experimental)
- FFT block size: **512 samples** (power-of-2 requirement)
- Minimum cycle time: 45ms before repeat (hardware-dependent)

**DC Offset Removal & Filtering:**
- Exponential filter: **0.2 weighting** applied each sample
- DC bias correction via `micLev` parameter
- Optional bandpass: 80 Hz – 16 kHz (removes very low rumble + ultrasonic noise)

**Windowing:**
- Flat-top window applied before FFT magnitude computation
- Purpose: reduce spectral leakage from bin-boundary transients

**Library:** ArduinoFFT (publicly available, Espressif-endorsed)

### Frequency Output Mapping

WLED downscales raw 256 FFT bins to **16 output channels (GEQ)** using weighted averaging:

| Channel | Frequency Range | Bin Range | Notes |
|---------|-----------------|-----------|-------|
| 0 | 43–86 Hz | 1–2 | Sub-bass, kick fundamental |
| 1 | 86–129 Hz | 2–3 | Bass fundamental |
| 2 | 129–172 Hz | 3–4 | Upper bass |
| 3 | 172–215 Hz | 4–5 | Bass warmth |
| 4 | 260–303 Hz | 6–7 | Lower mids |
| ... | ... | ... | ... |
| 15 | 7,106–9,259 Hz | 165–215 (×0.70) | High treble |

**Mapping formula:** Multiple adjacent FFT bins averaged into each output channel via `fftAddAvg(lowBin, highBin)`.

**Frequency resolution at 22.05 kHz:**
- Bin spacing: 22050 Hz ÷ 256 ≈ **86 Hz per bin**
- Highest bin: ~5,100 Hz (Nyquist limit)

### Pink Noise Equalization

WLED applies a frequency-dependent amplitude correction to compensate for natural microphone + acoustic response:

```cpp
static float fftResultPink[NUM_GEQ_CHANNELS] =
  { 1.70f, 1.71f, 1.73f, 1.78f, 1.82f, 2.10f, 2.35f, 3.00f,
    3.93f, 5.12f, 6.70f, 8.37f, 10.00f, 11.22f, 11.90f, 9.55f };
```

**Interpretation:**
- Low frequencies (0–2): 1.70–1.78× gain (microphone attenuation at very low freq)
- Mid frequencies (3–7): 1.82–3.00× gain (linear response flattening)
- High frequencies (8–14): 5.12–11.90× gain (aggressive HF boost)
- Ultra-high (15): 9.55× (slight rollback to prevent noise floor artifacts)

**Effect:** Produces a perceptually flat response across audible spectrum despite microphone + acoustic roll-off.

---

## Automatic Gain Control (AGC)

WLED implements a **PI (proportional-integral) feedback controller** with three preset tunings: Normal, Vivid, Lazy.

### AGC Loop Parameters

| Parameter | Normal | Vivid | Lazy | Notes |
|-----------|--------|-------|------|-------|
| `agcSampleDecay` | 0.9994 | 0.9985 | 0.9997 | Envelope averaging (slower at extremes) |
| `agcZoneLow` | 32 | 28 | 36 | Low emergency threshold (dB equiv ~5) |
| `agcZoneHigh` | 240 | 240 | 248 | High emergency threshold (dB equiv ~60) |
| `agcZoneStop` | 336 | 448 | 304 | Saturation limit (no further gain) |
| `agcTarget0` | 112 | 144 | 164 | Setpoint for low signal (~60% scale) |
| `agcTarget1` | 220 | 224 | 216 | Setpoint for mid signal (~85% scale) |
| `agcFollowFast` | 1/192 | 1/128 | 1/256 | Attack time constant (fast tracking) |
| `agcFollowSlow` | 1/6144 | 1/4096 | 1/8192 | Decay time constant (smooth release) |
| `agcControlKp` | 0.6 | 1.5 | 0.65 | Proportional gain (responds to error) |
| `agcControlKi` | 1.7 | 1.85 | 1.2 | Integral gain (eliminates steady-state error) |
| `agcSampleSmooth` | 1/12 | 1/6 | 1/16 | Sample averaging smoothing |

### Preset Characteristics

- **Normal:** Balanced responsiveness. Suitable for variety of music genres.
- **Vivid:** Faster tracking (Kp=1.5 vs 0.6). More "pop" on transients, more noise on quiet passages.
- **Lazy:** Slower response. Smoother, more stable. May miss quick dynamics in fast-tempo tracks.

### Amplification Limits

- Minimum gain: **1/64** (−36 dB, audio attenuates to whisper levels)
- Maximum gain: **32** (+30 dB, overload protection)
- Integration time constant: **2ms** (updates once per 2ms audio chunk)
- Damping factor: **0.25** (feedback loop stability margin)

**Why separate fast/slow paths?** Attack (fast rise to capture transients) and decay (slow fall to prevent "pumping" chop) are decoupled. This mimics dynamic range processors in analog audio gear.

---

## Noise Floor & Silence Gating

### Squelch (Noise Gate) Threshold

```cpp
uint8_t soundSquelch = 10;  // 0–255 range, typical 8–15
```

**Logic:**
```cpp
// Linearised comparison after AGC
if (fabsf(sampleAvg) > soundSquelch) {
  // Gate OPEN: run FFT, update bins
} else {
  // Gate CLOSED: decay spectral bins exponentially
  fftCalc[i] *= 0.85f;  // 15% falloff per cycle
}
```

**Effect:**
- When gate is closed, FFT output decays at 85% per update cycle (~45ms), creating smooth visual fade-out during silence
- Prevents noise floor from triggering beat detection during quiet passages
- Typical squelch setting: **8–15** (user-configurable via web interface)

### Alternative Gate: Sample-Based Threshold

```cpp
if (fabsf(sampleAvg) > 0.25f) {  // ~4% of max int16
  // Process FFT
} else {
  // Decay
  fftCalc[i] *= 0.85f;
}
```

**Interpretation:** Any audio sample exceeding 0.25 units (raw I2S value) keeps gate open. Quieter than typical speaking voice.

---

## Beat & Peak Detection

### "Simple Beat Detection" via Spectral Peaks

WLED implements **NOT a true beat detection algorithm** but rather a **spectral peak detector** in user-selected frequency bins:

```cpp
static void detectSamplePeak(void) {
  bool havePeak = false;

  // Conditions for peak:
  // 1. sampleAvg > 1 (signal above noise floor)
  // 2. maxVol > 0 (threshold configured)
  // 3. binNum > 4 (valid bin selected, typically 8–14)
  // 4. vReal[binNum] > maxVol (spectral magnitude exceeds threshold)
  // 5. (millis() - timeOfPeak) > 100 (minimum 100ms gap between peaks)

  if ((sampleAvg > 1) && (maxVol > 0) && (binNum > 4) &&
      (vReal[binNum] > maxVol) && ((millis() - timeOfPeak) > 100)) {
    havePeak = true;
  }

  if (havePeak) {
    samplePeak = true;
    timeOfPeak = millis();
    udpSamplePeak = true;  // UDP broadcast for networked devices
  }
}
```

**Key Parameters:**
- `binNum`: User-selected frequency bin (default 14 ≈ 280 Hz in 22 kHz mode)
- `maxVol`: Amplitude threshold in that bin (default 31)
- `timeOfPeak`: 100ms minimum debounce (prevents rapid re-triggers on noise)

**Limitations:**
- Only monitors ONE frequency bin at a time
- Does NOT implement kick/snare/hihat separation
- Does NOT track tempo or beat grid
- Returns a single boolean: beat present or not

**User control:** Effects can select which bin to monitor via `binNum` slider. Common selections:
- 5–8: Bass/kick region (~100–200 Hz)
- 8–14: Mid-bass to lower-mid (~280–500 Hz)
- 15+: Treble/high frequencies (> 1 kHz)

---

## Dynamics Limiting

### limitSampleDynamics()

Prevents violent amplitude swings between update cycles:

```cpp
// Configuration
constexpr float bigChange = 196;  // Threshold for "large" change
uint16_t attackTime = 80;         // Default: 80ms rise time
uint16_t decayTime = 1400;        // Default: 1400ms fall time
```

**Behaviour:**
- If sample increases by > `bigChange`, limit rise rate to ~80ms envelope
- If sample decreases, apply slower decay (1400ms) to smooth volume drop
- Prevents "digital clipping" visual artifacts from sudden level jumps

**Effect on real audio:** Transient attack is slightly delayed (~80ms); bass hits lose some punch but gain smoothness.

---

## Post-Processing & Output Scaling

### postProcessFFTResults()

Applied AFTER bin mapping and pink noise correction:

**Three scaling modes:**

| Mode | Formula | Use Case |
|------|---------|----------|
| Logarithmic | `log10(x + 1) * scale` | Musical perception (human hearing is log) |
| Linear | `x * scale` | Energy visualization |
| Square-root | `sqrt(x) * scale` | Middle ground (default in WLED 0.15+) |

**Downscaling factor (22 kHz mode):** 0.46

**Falloff coefficients:** [0.78–0.90] depending on decay time setting. Creates exponential fade of bins during silence.

---

## Summary: Why This Design Works for WLED

1. **Robustness:** FFT + AGC + gating handles a wide range of microphone placements + audio sources without manual calibration
2. **Low computational cost:** 512-point FFT every ~45ms (22 MIPS peak on dual-core ESP32) leaves time for LED rendering
3. **Perceived flatness:** Pink noise correction + logarithmic scaling match human hearing
4. **Beat detection simplicity:** Single-bin threshold requires no sophisticated signal processing; works adequately for musical rhythm
5. **User control:** `soundSquelch`, `AGC preset`, `binNum`, and decay time give non-expert users knobs to tune

---

## What WLED Does NOT Do (Gap Analysis for K1)

| Capability | WLED | K1 Opportunity |
|-----------|------|-----------------|
| Kick/snare/hihat separation | ✗ Single bin only | ✓ Multi-band onset detection |
| Tempo/BPM tracking | ✗ No beat grid | ✓ Beatroot or Kalman filter |
| Pitch/chroma detection | ✗ Only magnitude FFT | ✓ Phase vocoder or constant-Q |
| Percussive vs harmonic | ✗ Not separated | ✓ Onset detector with harmonic masking |
| Silence detection quality | Basic (single threshold) | ✓ Schmitt trigger + hysteresis (K1 v3) |

---

## Implementation References

**WLED Audio Reactive on GitHub:**
- Official: [WLED/usermods/audioreactive/audio_reactive.cpp](https://github.com/WLED/WLED/blob/main/usermods/audioreactive/audio_reactive.cpp)
- Community: [MoonModules/WLED-MM](https://github.com/MoonModules/WLED-MM) (Sound Reactive fork)
- Atuline archive: [atuline/WLED](https://github.com/atuline/WLED)

**Related ESP32 Beat Detection:**
- [ESP32 FFT Beat Detection Forum](https://forum.arduino.cc/t/solved-fft-beat-detection-esp32/660828)
- [ESP32 Music Beat Sync](https://github.com/blaz-r/ESP32-music-beat-sync)
- [ofxBeat — Kick/Snare/Hihat Separation](https://github.com/darrenmothersele/ofxBeat) (openFrameworks, but algorithm transferable)

**Frequency Reference:**
- [Drum Frequencies](https://www.audiorecording.me/drum-frequencies-of-kick-bass-drum-hi-hats-snare-and-crash-cymbals.html/2)
  - Kick: 50–60 Hz fundamental; 100–200 Hz attack
  - Snare: 100–500 Hz body; 5–10 kHz snap
  - Hi-hat: 7–10 kHz (closed); 200 Hz (open, attack)

---

## Practical Takeaways for K1 Development

1. **Use 512-point FFT** at 22.05 kHz (matching WLED) for ~86 Hz bin resolution
2. **Pink noise correction mandatory** — raw microphone response is heavily roll-off
3. **Schmitt trigger gating** (not single threshold) prevents chatter during soft passages
4. **Multi-band onset detection** separates drums better than single-bin beat detection
5. **AGC with separate attack/decay** is essential for responsive-but-smooth audio reaction
6. **100–150 ms debounce** on beat/onset to avoid re-triggers from ringing transients

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-25 | agent:research | Created analysis of WLED Sound Reactive audio processing for K1 reference |
