---
abstract: "SensoryBridge (Lixie Labs) audio silence detection and noise gating implementation. Covers calibration algorithm, per-bin noise subtraction, RMS-based VU calculation, 10-second silence timeout, novelty detection, and sweet spot fade logic. Research source: GitHub connornishijima/SensoryBridge."
---

# SensoryBridge Audio Processing: Silence Detection & Noise Gating Research

**Source Repository:** [GitHub - connornishijima/SensoryBridge](https://github.com/connornishijima/SensoryBridge)

**Research Date:** 2026-03-25

---

## 1. Architecture Overview

SensoryBridge uses a **Goertzel-based Discrete Fourier Transform (GDFT)** architecture, NOT FFT:

- **64 frequency bins** across an 88-key piano range (110 Hz to 4186 Hz, by default)
- **Sliding window:** 256 new samples per frame
- **Operating mode:** ESP32-S2 with I2S microphone input (SPH0645)
- **Sample rate:** 12,800 Hz (configurable, `DEFAULT_SAMPLE_RATE`)

The audio pipeline executes in this order each frame:

1. `acquire_sample_chunk()` — I2S DMA capture + DC offset removal
2. `calculate_vu()` — RMS loudness + noise floor subtraction
3. `process_GDFT()` — Goertzel on 64 frequencies + per-bin noise subtraction
4. `calculate_novelty()` — Spectral change detection (positive deltas only)
5. `run_sweet_spot()` — Silence-aware LED indicator dimming

---

## 2. Noise Gating & Calibration (GDFT.h)

### 2.1 Calibration Phase (256 frames ≈ 2 seconds)

**Trigger:** User presses NOISE BUTTON or firmware auto-calls `start_noise_cal()`.

**Duration & Collection:**
```cpp
// From GDFT.h process_GDFT()
if (noise_complete == false) {
    for (uint8_t i = 0; i < NUM_FREQS; i += 1) {
        if (magnitudes_normalized_avg[i] > noise_samples[i]) {
            noise_samples[i] = magnitudes_normalized_avg[i];
        }
    }
    noise_iterations++;
    if (noise_iterations >= 256) {  // Calibration complete (~2 seconds @ 100 FPS)
        noise_complete = true;
        // Store DC offset and save to flash
    }
}
```

**Key Logic:**
- Runs 256 iterations at system FPS (≈100 FPS default)
- **Per-bin collection:** Stores the MAXIMUM magnitude observed in each bin across all 256 frames
- **Storage:** `noise_samples[NUM_FREQS]` array holds peak noise floor per frequency bin
- **Context:** Must be performed with ONLY ambient room noise (no music, no talking)

### 2.2 Noise Reduction (Per-Bin Subtraction)

Once calibration is complete, the firmware applies **aggressive noise gating** per bin:

```cpp
// From GDFT.h process_GDFT()
if (noise_complete == true) {
    magnitudes_normalized_avg[i] -= float(noise_samples[i] * SQ15x16(1.5));
    if (magnitudes_normalized_avg[i] < 0.0) {
        magnitudes_normalized_avg[i] = 0.0;
    }
}
```

**Key Parameters:**
- **Safety margin:** 1.5× the calibrated noise floor is subtracted (not just 1×)
  - Treats calibrated noise as if it were 50% louder than actual measurement
  - Prevents false positives from ambient noise fluctuations
- **Floor clamping:** Negative results are hard-clamped to 0.0 (silence)
- **Per-bin independent:** Each of 64 frequencies is gated independently

**Rationale:** The 1.5× multiplier provides hysteresis — if room noise varies by ±30%, the gate remains stable.

---

## 3. VU Calculation & RMS-Based Loudness (i2s_audio.h)

### 3.1 RMS Computation

```cpp
void calculate_vu() {
    float sum = 0.0;

    // Compute sum of squares
    for (uint16_t i = 0; i < CONFIG.SAMPLES_PER_CHUNK; i++) {
        sum += float(waveform_fixed_point[i] * waveform_fixed_point[i]);
    }

    // Compute RMS
    SQ15x16 rms = sqrt(float(sum / CONFIG.SAMPLES_PER_CHUNK));
    audio_vu_level = rms;

    // ... noise floor logic follows
}
```

**Fixed-Point Arithmetic:** Uses `SQ15x16` (signed Q15.16 fixed-point) for precision without floating-point overhead.

### 3.2 Noise Floor Calibration (During Phase 1)

```cpp
if (noise_complete == false) {
    // During calibration, find RMS noise floor
    if (float(audio_vu_level * 1.5) > CONFIG.VU_LEVEL_FLOOR) {
        CONFIG.VU_LEVEL_FLOOR = float(audio_vu_level * 1.5);
    }
}
```

- Observes RMS during the 256-frame calibration window
- **Stores:** The peak RMS value × 1.5 as the VU floor
- **Same 1.5× strategy** as per-bin noise gating (conservative, with hysteresis)

### 3.3 RMS Noise Subtraction (Post-Calibration)

```cpp
else {
    // Subtract noise floor from volume level
    audio_vu_level -= CONFIG.VU_LEVEL_FLOOR;

    // Zero out negative volume
    if (audio_vu_level < 0.0) {
        audio_vu_level = 0.0;
    }

    // Normalize volume level
    CONFIG.VU_LEVEL_FLOOR = min(0.99f, CONFIG.VU_LEVEL_FLOOR);
    audio_vu_level /= (1.0 - CONFIG.VU_LEVEL_FLOOR);
}
```

**Normalization:** Divides by `(1.0 - noise_floor)` to re-normalize so that the quietest detectable signal maps to ~0 and louder signals spread proportionally.

---

## 4. Silence Detection & Timeout Logic

### 4.1 Three-State Sweet Spot Detection

Silence is detected using a **three-state system** based on raw waveform amplitude (NOT RMS):

```cpp
// From i2s_audio.h acquire_sample_chunk()
if (max_waveform_val_raw <= CONFIG.SWEET_SPOT_MIN_LEVEL * 1.10) {
    sweet_spot_state = -1;  // SILENT
    if (sweet_spot_state_last != -1) {
        silence_temp = true;
        silence_switched = t_now;
    }
} else if (max_waveform_val_raw >= CONFIG.SWEET_SPOT_MAX_LEVEL) {
    sweet_spot_state = 1;   // LOUD
    if (sweet_spot_state_last != 1) {
        silence_temp = false;
        silence_switched = t_now;
    }
} else {
    sweet_spot_state = 0;   // MODERATE
    if (sweet_spot_state_last != 0) {
        silence_temp = false;
        silence_switched = t_now;
    }
}
```

**Key Parameters:**
- **Silent threshold:** `SWEET_SPOT_MIN_LEVEL × 1.10` (110% of minimum)
- **Loud threshold:** `SWEET_SPOT_MAX_LEVEL` (default: 30,000)
- **Default min:** `SWEET_SPOT_MIN_LEVEL = 750` (set during calibration, updated per frame if lower audio detected)
- **State space:** {-1: silent, 0: moderate, 1: loud}

### 4.2 Persistent Silence (10-Second Timeout)

```cpp
if (silence_temp == true) {
    if (t_now - silence_switched >= 10000) {  // 10,000 milliseconds
        silence = true;
    }
} else {
    silence = false;
}
```

**Timeout Logic:**
- `silence_temp` is a **temporary flag** that triggers immediately when amplitude drops below 110% of min
- `silence` is the **persistent flag** that only becomes true after 10+ consecutive seconds of `silence_temp`
- **Hysteresis:** If audio rises above threshold at any point, both flags reset to false immediately

This prevents brief silences (like a breath between phrases) from triggering the full silence state.

---

## 5. Silence-Aware Dimming (led_utilities.h)

### 5.1 Silent Scale Fade

Once the 10-second silence condition is true, the firmware dims all LEDs using an **asymmetric exponential smoothing filter**:

```cpp
if (CONFIG.STANDBY_DIMMING == true) {
    float silent_scale_raw = 1.0 - silence;  // 0 when silent, 1.0 when not
    silent_scale = silent_scale_raw * 0.1 + silent_scale_last * 0.9;
    silent_scale_last = silent_scale;
} else {
    silent_scale = 1.0;  // No dimming if feature disabled
}
```

**Fade Behavior:**
- **Raw value:** 1.0 (full brightness) when `silence == false`, 0.0 (off) when `silence == true`
- **Smoothing factor:** 10% new, 90% history — very slow fade (≈180 frames to fully dim at 100 FPS ≈ 1.8 seconds)
- **Feature flag:** `CONFIG.STANDBY_DIMMING` can disable this entirely

### 5.2 Sweet Spot Indicator Dimming

The center "Sweet Spot" LED (indicating audio level appropriateness) is dimmed to 10% brightness during silence:

```cpp
// From led_utilities.h run_sweet_spot()
led_power[uint8_t(i + 1)] = 256 * led_level * (0.1 + silent_scale * 0.9) * sweet_spot_brightness * ...
```

**Formula breakdown:**
- Base factor: `0.1` (10% minimum)
- Additional brightness: `silent_scale * 0.9` (up to 90% more when not silent)
- **Result:** 10% brightness when silent, 100% brightness when audio is active

---

## 6. Novelty Detection (GDFT.h)

### 6.1 Positive Spectral Change Only

Novelty measures **only upward spectral changes** (onsets, transients):

```cpp
void calculate_novelty(uint32_t t_now) {
    SQ15x16 novelty_now = 0.0;
    for (uint16_t i = 0; i < NUM_FREQS; i++) {
        int16_t rounded_index = spectral_history_index - 1;
        while (rounded_index < 0) {
            rounded_index += SPECTRAL_HISTORY_LENGTH;
        }
        SQ15x16 novelty_bin = spectrogram[i] - spectral_history[rounded_index][i];

        if (novelty_bin < 0.0) {
            novelty_bin = 0.0;  // IGNORE decreases, only count increases
        }

        novelty_now += novelty_bin;
    }
    novelty_now /= NUM_FREQS;  // Normalize by bin count
    novelty_curve[spectral_history_index] = sqrt(float(novelty_now));
}
```

**Key Characteristics:**
- **Direction:** One-way detection (positive delta only, negative = clamped to 0)
- **Aggregation:** Sums all 64 bins, then divides by 64 for per-bin average
- **Final metric:** Square root of the sum (non-linear scaling to emphasize small changes)
- **Storage:** Stores novelty value per frame in `novelty_curve[]` for display/logic

### 6.2 Spectral History (FIFO Window)

```cpp
// Store current spectrogram in circular history
for (uint16_t b = 0; b < NUM_FREQS; b += 8) {
    spectral_history[spectral_history_index][b + 0] = spectrogram[b + 0];
    spectral_history[spectral_history_index][b + 1] = spectrogram[b + 1];
    // ... loop unroll for efficiency
}

spectral_history_index++;
if (spectral_history_index >= SPECTRAL_HISTORY_LENGTH) {
    spectral_history_index -= SPECTRAL_HISTORY_LENGTH;  // Circular wrap
}
```

The circular buffer stores the last N frames' spectrograms (exact length `SPECTRAL_HISTORY_LENGTH` undefined in excerpt, likely 64-256 frames).

---

## 7. User-Accessible Parameters

From `constants.h`, `globals.h`, and config structures:

| Parameter | Default | Purpose | User-Tunable |
|-----------|---------|---------|--------------|
| `SAMPLE_RATE` | 12,800 Hz | I2S audio sampling rate | Via config |
| `SAMPLES_PER_CHUNK` | 256 | Samples per frame | Build-time only |
| `SWEET_SPOT_MIN_LEVEL` | 750 | Silence threshold (raw amplitude) | Auto-calibrated during noise cal |
| `SWEET_SPOT_MAX_LEVEL` | 30,000 | Loudness ceiling | Via config |
| `VU_LEVEL_FLOOR` | Calibrated | RMS noise floor | Auto-calibrated |
| `DC_OFFSET` | Calibrated | DC bias removal | Auto-calibrated |
| `SENSITIVITY` | Via config | Mic gain multiplier | Yes, user-facing setting |
| `MOOD` | Via config | Controls spectral smoothing decay rates | Yes |
| `PHOTONS` (brightness) | Via config | LED intensity multiplier | Yes |
| `STANDBY_DIMMING` | Boolean | Enable/disable silence fade | Via config |
| Noise gate multiplier | 1.5× | Safety margin on gated bins | Firmware constant |
| Silence timeout | 10,000 ms | Duration before LED dimming | Firmware constant |

---

## 8. Comparison with LightwaveOS

### SensoryBridge Approach (Referenced)
- **Per-bin noise subtraction:** Each of 64 frequency bins independently gated
- **1.5× safety margin:** Aggressive, conservative gating
- **RMS-based VU floor:** Single global RMS threshold
- **10-second silence timeout:** Persistent silence flagged only after extended quiet
- **Sweet spot as feedback:** Visual indication of audio level appropriateness
- **Novelty as onset detection:** Tracks positive spectral changes for transient triggers

### LightwaveOS (Current)
- **Schmitt trigger hysteresis:** Open at RMS ≥ 0.02, close at < 0.005 with 25-hop hold (~200 ms)
- **Global silence gate:** Uniform threshold across all frequencies via ControlBus
- **No persistence timeout:** Silence detection is immediate (no 10-second delay)
- **ControlBus snapshot gating:** External network code calls snapshot API, no live access
- **Onset detector:** Rewritten March 2026 for onset hardening (research-backed, not novelty-based)

### Key Differences
1. **Bin-level granularity:** SensoryBridge gates per-frequency; LightwaveOS gates RMS globally
2. **Hysteresis strategy:** SensoryBridge uses 1.5× multiplier; LightwaveOS uses Schmitt trigger
3. **Silence UX:** SensoryBridge dims LEDs after 10 seconds; LightwaveOS has no built-in dimming
4. **Novelty use:** SensoryBridge uses for visual feedback; LightwaveOS onset detector is separate

---

## 9. Calibration User Experience

From `sensorybridge.rocks/initial_calibration.html`:

**User Steps:**
1. Ensure only ambient room noise (no music, no talking)
2. Press NOISE BUTTON
3. Wait ~3 seconds (256 frames at 100 FPS)
4. LED strip displays pink graph during measurement
5. After completion, LED strip stays nearly off during silence

**Recalibration Triggers:**
- Room relocation
- Environmental condition changes (fans on/off, etc.)

No manual threshold tuning required — fully automatic per-bin calibration.

---

## 10. Silence Gate State Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│ Frame N: Acquire I2S samples                                        │
│          Compute max_waveform_val_raw                              │
└────────────────────────┬────────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────────┐
│ State Machine (3-state sweet spot):                                 │
│                                                                     │
│ if (max_waveform_val_raw <= SWEET_SPOT_MIN × 1.10):               │
│     sweet_spot_state = -1  (SILENT)                               │
│     silence_temp = true                                            │
│     silence_switched = t_now                                       │
│                                                                     │
│ else if (max_waveform_val_raw >= SWEET_SPOT_MAX):                │
│     sweet_spot_state = 1   (LOUD)                                 │
│     silence_temp = false                                           │
│                                                                     │
│ else:                                                              │
│     sweet_spot_state = 0   (MODERATE)                             │
│     silence_temp = false                                           │
└────────────────────────┬────────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────────┐
│ Persistence Check (10-second timeout):                              │
│                                                                     │
│ if (silence_temp == true):                                          │
│     if (t_now - silence_switched >= 10000):                        │
│         silence = true  ✓ (10+ seconds of quiet detected)         │
│                                                                     │
│ else:                                                               │
│     silence = false  (audio detected, reset immediately)           │
└────────────────────────┬────────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────────┐
│ Dimming (Exponential Smoothing):                                    │
│                                                                     │
│ silent_scale_raw = 1.0 - silence  (0 when silent, 1.0 active)     │
│ silent_scale = 0.1 * silent_scale_raw + 0.9 * silent_scale_last   │
│                                                                     │
│ Result: Smooth 1.8-second fade from full brightness to 10%        │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 11. Confidence & Limitations

**High Confidence:**
- Per-bin noise floor subtraction (1.5× multiplier) ✓ Confirmed in GDFT.h
- RMS calculation + VU floor logic ✓ Confirmed in i2s_audio.h
- 10-second silence timeout ✓ Exact code in acquire_sample_chunk()
- Novelty as positive spectral deltas ✓ Full calculate_novelty() visible
- Sweet spot 3-state machine ✓ Complete logic extracted

**Medium Confidence:**
- SPECTRAL_HISTORY_LENGTH exact size (referenced but not defined in excerpt)
- Per-effect novelty usage (lightshow_modes.h referenced but not fully analyzed)

**Not Addressed (Out of Scope):**
- Per-effect light mode implementations (64+ visualizers)
- Calibration stability across temperature/humidity variations
- Real-world dB SPL mapping (firmware uses raw amplitude units)

---

## 12. References & Source Code

**Key Source Files (GitHub connornishijima/SensoryBridge):**
- `SENSORY_BRIDGE_FIRMWARE/GDFT.h` — Goertzel algorithm + per-bin noise gating
- `SENSORY_BRIDGE_FIRMWARE/i2s_audio.h` — I2S capture + RMS VU + silence state machine
- `SENSORY_BRIDGE_FIRMWARE/led_utilities.h` — Silent scale dimming + sweet spot indicator
- `SENSORY_BRIDGE_FIRMWARE/noise_cal.h` — Calibration start/clear
- `SENSORY_BRIDGE_FIRMWARE/constants.h` — Build-time constants
- `SENSORY_BRIDGE_FIRMWARE/globals.h` — Runtime calibration storage

**Documentation:**
- [SensoryBridge GitHub](https://github.com/connornishijima/SensoryBridge)
- [SensoryBridge Docs](https://github.com/connornishijima/sensory_bridge_docs)
- [sensorybridge.rocks](https://sensorybridge.rocks/)
- [Initial Calibration Guide](https://sensorybridge.rocks/initial_calibration.html)
- [Sweet Spot & Chroma Docs](https://sensorybridge.rocks/sweet_spot_and_chroma.html)

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-25 | claude:research | Initial research synthesis: silence detection algorithm, per-bin gating, novelty detection, user parameters |

