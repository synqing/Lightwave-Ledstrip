---
abstract: "Extracted WLED Sound Reactive algorithm parameters, thresholds, and constants. Ready-reference for replicating WLED behavior or porting algorithms to K1."
---

# WLED Sound Reactive — Parameter & Constant Extract

## Sampling & FFT Configuration

```cpp
// Digitisation
const uint16_t samplesFFT = 512;           // FFT block size (power of 2)
const SRate_t SAMPLE_RATE = 22050;        // Hz (alternative: 10240, 20480)
const uint16_t FFT_MIN_CYCLE = 21;        // ms, minimum before repeat

// FFT computation
const uint16_t samples = 512;              // Window size
const uint16_t samplesPerSecond = 22050;  // Sample rate

// Time windows
const uint16_t AUDIO_BUFFER_SIZE = 512;   // Samples per buffer
const uint16_t AUDIO_UPDATE_FREQ = 21;    // ms cycle time
const uint16_t AUDIO_MIN_CYCLE = 21;      // Minimum cycle time
```

## ADC & Preprocessing

```cpp
// Sample preprocessing
const float filterConstant = 0.2f;        // Exponential filter weighting
const uint8_t dcRemovalFactor = 128;      // DC offset correction

// Gate threshold
const float SQUELCH_RANGE = 255;          // 0–255 scale
uint8_t soundSquelch = 10;                // Default noise gate, 0–255

// Hard gate alternative
const float HARD_GATE_THRESHOLD = 0.25f;  // |sampleAvg| > 0.25 opens gate
const float GATE_DECAY_RATE = 0.85f;      // FFT bins decay at 85% per cycle when gate closed
```

## AGC Configuration

### Preset Selectors (Normal=0, Vivid=1, Lazy=2)

```cpp
// Three tunings available
enum AGC_MODE {
  AGC_OFF = 0,
  AGC_NORMAL = 1,
  AGC_VIVID = 2,
  AGC_LAZY = 3
};

// AGC Sample Decay (per-sample exponential averaging)
// Higher = slower averaging, more stability
const float agcSampleDecay[AGC_NUM_PRESETS] = {
  0.9994f,     // Normal: slower averaging
  0.9985f,     // Vivid: more responsive
  0.9997f      // Lazy: slowest response
};

// Emergency Zone Low (dB-equiv threshold)
// Below this, AGC applies maximum gain
const uint8_t agcZoneLow[AGC_NUM_PRESETS] = {
  32,          // Normal
  28,          // Vivid (more aggressive)
  36           // Lazy
};

// Emergency Zone High (dB-equiv threshold)
// Above this, AGC applies braking
const uint8_t agcZoneHigh[AGC_NUM_PRESETS] = {
  240,         // Normal
  240,         // Vivid
  248          // Lazy
};

// Saturation Stop (absolute limit)
// AGC stops gaining above this
const uint8_t agcZoneStop[AGC_NUM_PRESETS] = {
  336,         // Normal
  448,         // Vivid (allows more headroom)
  304          // Lazy
};

// Target amplitude (setpoint for PI controller)
// agcTarget0 = "comfortable" level for quiet passages
const uint8_t agcTarget0[AGC_NUM_PRESETS] = {
  112,         // Normal (~60% of scale, ~0.44 V RMS)
  144,         // Vivid (more "pop")
  164          // Lazy (more conservative)
};

// agcTarget1 = "comfortable" level for louder passages
const uint8_t agcTarget1[AGC_NUM_PRESETS] = {
  220,         // Normal (~85% of scale)
  224,         // Vivid
  216          // Lazy
};

// Fast follow time constant (attack)
// Lower = faster attack (catches transients quickly)
const double agcFollowFast[AGC_NUM_PRESETS] = {
  1.0 / 192.0,   // Normal: 192ms rise
  1.0 / 128.0,   // Vivid: 128ms rise (faster)
  1.0 / 256.0    // Lazy: 256ms rise (slower)
};

// Slow follow time constant (release/decay)
// Lower = faster decay (releases quicker after peak)
const double agcFollowSlow[AGC_NUM_PRESETS] = {
  1.0 / 6144.0,    // Normal: 6144ms fall
  1.0 / 4096.0,    // Vivid: 4096ms fall (snappier)
  1.0 / 8192.0     // Lazy: 8192ms fall (smoother)
};

// PI Controller Proportional Gain
// Higher = faster response to error
const float agcControlKp[AGC_NUM_PRESETS] = {
  0.6f,      // Normal
  1.5f,      // Vivid (more responsive)
  0.65f      // Lazy
};

// PI Controller Integral Gain
// Eliminates steady-state offset
const float agcControlKi[AGC_NUM_PRESETS] = {
  1.7f,      // Normal
  1.85f,     // Vivid
  1.2f       // Lazy
};

// Sample averaging smoothing (0–1)
// Exponential averaging window for sample values
const float agcSampleSmooth[AGC_NUM_PRESETS] = {
  1.0f / 12.0f,    // Normal: 12-sample window
  1.0f / 6.0f,     // Vivid: 6-sample window (less smoothing)
  1.0f / 16.0f     // Lazy: 16-sample window (more smoothing)
};

// AGC Integration & Control Loop
const uint16_t AGC_UPDATE_FREQ = 2;       // ms, control loop update interval
const float agcDampingFactor = 0.25f;     // Feedback loop stability damping

// Amplification Range
const float AGC_AMP_MIN = 1.0f / 64.0f;   // −36 dB floor
const float AGC_AMP_MAX = 32.0f;          // +30 dB ceiling
```

## Dynamics Limiting

```cpp
// limitSampleDynamics() — prevents violent amplitude jumps

// Threshold for "large" change detection
const float bigChange = 196.0f;

// Default timing (user-configurable via UI)
uint16_t attackTime = 80;                 // ms, onset rise time limit
uint16_t decayTime = 1400;                // ms, release fall time limit

// Envelope scaling factors (derived from timing)
// These are pre-computed coefficients that limit dB/ms rate
const float attackScale = 196.0f / 80.0f;     // ~2.45 units/ms
const float decayScale = 196.0f / 1400.0f;    // ~0.14 units/ms
```

## FFT Postprocessing

### Pink Noise Correction Table

```cpp
// Frequency-domain equalization table for 16 output bands
// Values are multiplicative gains to flatten perceived frequency response
// Lower freq (<200 Hz) has less energy due to mic roll-off → boost
// Higher freq has more noise → moderate boost but not excessive

static float fftResultPink[NUM_GEQ_CHANNELS] = {
  1.70f,     // Ch 0:  43–86 Hz (sub-bass kick) — 1.7× boost
  1.71f,     // Ch 1:  86–129 Hz (bass fundamental)
  1.73f,     // Ch 2: 129–172 Hz
  1.78f,     // Ch 3: 172–215 Hz (bass warmth)
  1.82f,     // Ch 4: 215–260 Hz
  2.10f,     // Ch 5: 260–303 Hz (low-mid) — 2.1× boost
  2.35f,     // Ch 6: 303–346 Hz
  3.00f,     // Ch 7: 346–389 Hz (presence) — 3.0× boost
  3.93f,     // Ch 8: 389–432 Hz (upper-mid)
  5.12f,     // Ch 9: 432–475 Hz — 5.1× boost
  6.70f,     // Ch 10: 475–518 Hz
  8.37f,     // Ch 11: 518–561 Hz
  10.00f,    // Ch 12: 561–604 Hz (treble) — max 10× boost
  11.22f,    // Ch 13: 604–647 Hz — 11.2× boost
  11.90f,    // Ch 14: 647–690 Hz (bright)
  9.55f      // Ch 15: 7–9 kHz (high treble) — rollback to 9.55×
};
```

**Interpretation:** Boost factors are applied post-FFT to compensate for:
- Microphone flat-response assumption vs actual roll-off
- Acoustic coupling loss (enclosure effects)
- I2S amplifier response curve

### Scaling Modes

```cpp
// Three post-processing scaling options (user-selectable)

enum SCALING_MODE {
  LOGARITHMIC = 0,   // log10(x + 1) — musical perception
  LINEAR = 1,        // x — physical energy
  SQRT = 2           // sqrt(x) — compromise (new default)
};

// Mode-specific formulas
float scaledValue;

if (mode == LOGARITHMIC) {
  scaledValue = log10(binValue + 1.0f) * logScale;
} else if (mode == LINEAR) {
  scaledValue = binValue * linearScale;
} else if (mode == SQRT) {
  scaledValue = sqrt(binValue) * sqrtScale;
}

// Downscaling factor (applied after scaling)
const float DOWNSCALE_FACTOR_22K = 0.46f;   // 22 kHz mode
const float DOWNSCALE_FACTOR_10K = 0.30f;   // 10 kHz mode (higher noise)

// Falloff decay (during silence gate)
// Higher = faster fade
const float falloffDecay[AGC_NUM_PRESETS] = {
  0.78f,     // Normal: 22% falloff per cycle
  0.85f,     // Vivid: 15% falloff (longer sustain)
  0.90f      // Lazy: 10% falloff (smoothest)
};
```

## Beat & Peak Detection

```cpp
// Peak Detection for Beat Output

// Parameter constants
uint8_t maxVol = 31;                       // Amplitude threshold in selected bin
uint8_t binNum = 8;                        // FFT bin to monitor (0–255)
uint16_t timeOfPeak = 0;                   // Last peak timestamp
const uint16_t PEAK_MIN_INTERVAL = 100;    // ms, minimum debounce between peaks

// Detection logic
bool samplePeak = false;                   // Output flag

void detectSamplePeak(void) {
  bool havePeak = false;

  // All conditions required:
  // 1. sampleAvg > 1 — signal above noise floor
  // 2. maxVol > 0 — user has configured threshold
  // 3. binNum > 4 — valid bin selected
  // 4. vReal[binNum] > maxVol — current FFT magnitude exceeds threshold
  // 5. (millis() - timeOfPeak) > 100 — minimum 100ms gap

  if ((sampleAvg > 1) && (maxVol > 0) && (binNum > 4) &&
      (vReal[binNum] > maxVol) && ((millis() - timeOfPeak) > PEAK_MIN_INTERVAL)) {
    havePeak = true;
  }

  if (havePeak) {
    samplePeak = true;
    timeOfPeak = millis();
    udpSamplePeak = true;    // Network broadcast flag
  }
}

// User-selectable bin assignments (frequency zones)
// Typical selections:
//   5–7:  Bass/kick region (130–172 Hz)
//   8–14: Mid-bass to lower-mid (215–647 Hz)
//   15+:  Treble (> 1 kHz)
```

## Bin Mapping Constants (22 kHz Mode)

```cpp
// Downscaling from 256 FFT bins to 16 GEQ channels
// Each entry sums N adjacent FFT bins

const uint8_t binLow[16] = {
  1,    // Ch 0: FFT[1]
  2,    // Ch 1: FFT[2]
  3,    // ...
  4,
  6,    // Ch 4: start at FFT[6] (skip 5)
  8,
  11,
  15,
  20,
  27,   // Ch 9: start at FFT[27]
  36,
  48,
  64,
  85,
  114,
  151   // Ch 15: start at FFT[151]
};

const uint8_t binHigh[16] = {
  2,    // Ch 0: sum FFT[1..2]
  3,
  4,
  5,
  8,    // Ch 4: sum FFT[6..8]
  11,
  15,
  20,
  27,
  36,   // Ch 9: sum FFT[27..36]
  48,
  64,
  85,
  114,
  151,
  215   // Ch 15: sum FFT[151..215]
};

// Downscaling weights (optional per-channel scaling post-sum)
const float binScale[16] = {
  1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
  1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.70f  // Ch 15: slight roll-off
};
```

## Hardware Constraints (ESP32)

```cpp
// I2S DMA configuration
const uint8_t I2S_PORT = I2S_NUM_0;
const uint16_t I2S_BUFFER_SIZE = 256;      // Samples per DMA buffer
const uint8_t I2S_NUM_BUFFERS = 2;         // Ping-pong buffering

// Memory limits
const uint16_t MAX_FFT_SIZE = 512;         // Maximum FFT block
const uint16_t MAX_CHANNELS = 16;          // GEQ output channels
const uint16_t UDP_BUFFER_SIZE = 512;      // Bytes for network payload

// Dual-core task affinity
const uint8_t AUDIO_CORE = 0;              // Core 0: audio processing
const uint8_t RENDER_CORE = 1;             // Core 1: LED rendering + effects
```

## UDP Network Format

```cpp
// AudioSync packet format (GEQ + beat)

struct AudioPacket {
  uint8_t geq[16];               // 16 GEQ channels (0–255 each)
  uint8_t beat;                  // Beat flag (0 or 1)
  uint8_t reserved[3];           // Padding
} __attribute__((packed));       // 20 bytes total

// Typical broadcast frequency: ~22 Hz (synchronized with FFT update)
```

---

## Porting Checklist: Replicating WLED in C++

**If you want to implement WLED's algorithm in K1 for comparison:**

- [ ] Use ArduinoFFT library or equivalent (512-point, 22 kHz mode)
- [ ] Implement DC removal with 0.2 exponential filter
- [ ] Apply Flat-top window before FFT
- [ ] Downscale 256 bins to 16 channels using `fftAddAvg()` with provided bin ranges
- [ ] Apply `fftResultPink[16]` equalization gains
- [ ] Implement one of three AGC presets (easiest: Normal mode)
- [ ] Add Squelch gate with 85% decay during gate-closed
- [ ] Implement beat detection on selected bin with 100ms debounce
- [ ] Test at 22 kHz first (cleanest); 10.24 kHz requires different pink table

**Estimated implementation time:** ~2–3 hours (AGC is most complex).

---

## Reference & Sources

- WLED official: https://github.com/WLED/WLED/blob/main/usermods/audioreactive/
- Audio Reactive docs: https://kno.wled.ge/advanced/audio-reactive/
- ArduinoFFT library: https://github.com/kosme/ArduinoFFT
- ESP32 I2S: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2s.html

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-25 | agent:research | Extracted WLED Sound Reactive parameters and constants for reference |
