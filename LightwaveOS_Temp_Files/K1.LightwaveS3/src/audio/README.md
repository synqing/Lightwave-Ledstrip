# LightwaveOS v2 Audio Subsystem

## Overview

The audio subsystem provides real-time frequency analysis and musical grid tracking for audio-reactive LED effects. It consists of three main components:

1. **Contracts** - Data structures and lock-free communication primitives
2. **GoertzelAnalyzer** - Frequency band analyzer using the Goertzel algorithm
3. **AudioActor** - FreeRTOS task orchestrating audio capture and analysis

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    AUDIO CORE (Core 0)                       │
│                                                               │
│  ┌──────────────┐    ┌─────────────────┐    ┌────────────┐ │
│  │ I2S Capture  │───▶│ GoertzelAnalyzer│───▶│ ControlBus │ │
│  │ (PDM/I2S)    │    │ (8 freq bands)  │    │ (Smoothing)│ │
│  └──────────────┘    └─────────────────┘    └──────┬─────┘ │
│                                                      │       │
│                      ┌──────────────────────────────┘       │
│                      │                                       │
│                      ▼                                       │
│            ┌──────────────────┐                              │
│            │ SnapshotBuffer<T>│ (Lock-free double-buffer)    │
│            └────────┬─────────┘                              │
└─────────────────────┼─────────────────────────────────────┘
                      │
                      │ Cross-core communication
                      │
┌─────────────────────▼─────────────────────────────────────┐
│                   RENDER CORE (Core 1)                      │
│                                                               │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐ │
│  │ MusicalGrid  │    │ Effect Layer │    │ LED Renderer │ │
│  │ (PLL, BPM)   │───▶│ (Drive,      │───▶│ (FastLED)    │ │
│  │              │    │  Punch, etc) │    │              │ │
│  └──────────────┘    └──────────────┘    └──────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

## Contracts (v2/src/audio/contracts/)

### AudioTime.h
Sample-index monotonic clock for audio synchronization.

```cpp
struct AudioTime {
    uint64_t sample_index;      // Monotonic sample counter
    uint32_t sample_rate_hz;    // Sample rate (16000 Hz)
    uint64_t monotonic_us;      // Wall clock reference
};
```

**Key functions:**
- `AudioTime_SamplesToSeconds()` - Convert sample delta to seconds
- `AudioTime_SecondsBetween()` - Compute time delta between AudioTime instances

### SnapshotBuffer.h
Lock-free double-buffer template for cross-core communication.

```cpp
template <typename T>
class SnapshotBuffer {
    void Publish(const T& snapshot);        // Writer (audio core)
    uint32_t ReadLatest(T& out) const;      // Reader (render core)
    uint32_t GetSequence() const;           // Check for updates
};
```

**Usage:**
```cpp
// Audio thread (Core 0)
SnapshotBuffer<ControlBusFrame> frameBuffer;
ControlBusFrame frame = {...};
frameBuffer.Publish(frame);

// Render thread (Core 1)
ControlBusFrame latest;
uint32_t seq = frameBuffer.ReadLatest(latest);
```

### ControlBus.h
DSP signals container with attack/release envelope smoothing.

```cpp
struct ControlBusFrame {
    AudioTime t;                    // Timestamp
    float rms;                      // Root-mean-square energy [0,1]
    float flux;                     // Spectral flux [0,1]
    float bands[8];                 // 8 frequency bands [0,1]
    float chroma[12];               // 12 pitch class bins [0,1]
    float drive;                    // Smoothed energy [0,1]
    float punch;                    // Transient detector [0,1]
    bool beat_detected;
    float beat_strength;
};

class ControlBus {
    void UpdateFromHop(const AudioTime& now, const ControlBusRawInput& raw);
    const ControlBusFrame& GetFrame() const;
    void SetAttack(float attack);    // 0-1, lower = faster
    void SetRelease(float release);  // 0-1, lower = faster
};
```

**Usage:**
```cpp
ControlBus bus;
bus.SetAttack(0.3f);   // Fast attack
bus.SetRelease(0.85f); // Slow release

ControlBusRawInput raw = {...};
bus.UpdateFromHop(now, raw);
const ControlBusFrame& frame = bus.GetFrame();
```

### MusicalGrid.h
PLL-style beat/tempo tracker.

```cpp
struct MusicalGridSnapshot {
    AudioTime t;
    float bpm_smoothed;             // Smoothed BPM estimate
    float tempo_confidence;         // Confidence [0,1]
    uint64_t beat_index;            // Total beats since start
    float beat_phase01;             // Phase within beat [0,1)
    bool beat_tick;                 // True on beat transition
    uint64_t bar_index;
    float bar_phase01;              // Phase within bar [0,1)
    bool downbeat_tick;
    uint8_t beat_in_bar;            // 0-3 for 4/4
    uint8_t beats_per_bar;
};

class MusicalGrid {
    void Tick(const AudioTime& now);                    // RENDER thread, 120 FPS
    void OnTempoEstimate(float bpm, float confidence);  // AUDIO thread
    void OnBeatObservation(const AudioTime& now,
                          float strength,
                          bool is_downbeat);            // AUDIO thread
    void ReadLatest(MusicalGridSnapshot& out) const;
    void SetBeatsPerBar(uint8_t beats);
};
```

**Usage:**
```cpp
MusicalGrid grid;

// Audio thread: Feed tempo/beat observations
grid.OnTempoEstimate(124.5f, 0.8f);
grid.OnBeatObservation(now, 0.9f, true);

// Render thread: Advance PLL and read state
grid.Tick(renderTime);
MusicalGridSnapshot snap;
grid.ReadLatest(snap);

if (snap.beat_tick) {
    // Trigger beat-synced effect
}
```

## GoertzelAnalyzer (v2/src/audio/GoertzelAnalyzer.h)

Frequency analyzer using the Goertzel algorithm for efficient single-frequency DFT.

### Target Frequencies (16kHz sample rate)
| Band | Frequency | Description |
|------|-----------|-------------|
| 0    | 60 Hz     | Sub-bass    |
| 1    | 120 Hz    | Bass        |
| 2    | 250 Hz    | Low-mid     |
| 3    | 500 Hz    | Mid         |
| 4    | 1000 Hz   | High-mid    |
| 5    | 2000 Hz   | Presence    |
| 6    | 4000 Hz   | Brilliance  |
| 7    | 8000 Hz   | Air         |

### API

```cpp
class GoertzelAnalyzer {
public:
    static constexpr size_t WINDOW_SIZE = 512;  // 2 hops @ 256 samples
    static constexpr uint8_t NUM_BANDS = 8;

    GoertzelAnalyzer();

    // Accumulate samples into rolling window
    void accumulate(const int16_t* samples, size_t count);

    // Compute magnitudes when window is full
    // Returns true if new results ready, false otherwise
    bool analyze(float* bandsOut);

    // Reset accumulator
    void reset();
};
```

### Usage Example

```cpp
GoertzelAnalyzer analyzer;
float bands[8];

// In I2S read callback (every 256 samples = 1 hop @ 16kHz)
int16_t samples[256];
i2s_read(I2S_NUM_0, samples, sizeof(samples), &bytesRead, portMAX_DELAY);

analyzer.accumulate(samples, 256);

// Every 2 hops (512 samples), analyze() returns true
if (analyzer.analyze(bands)) {
    // Fresh band magnitudes available
    for (int i = 0; i < 8; ++i) {
        Serial.printf("Band %d: %.2f\n", i, bands[i]);
    }

    // Feed to ControlBus
    ControlBusRawInput raw;
    memcpy(raw.bands, bands, sizeof(bands));
    controlBus.UpdateFromHop(now, raw);
}
```

## Goertzel Algorithm Details

The Goertzel algorithm efficiently computes the DFT for a single frequency:

```
Input: N samples, target frequency f, sample rate Fs
Coefficient: coeff = 2 * cos(2π * k/N), where k = (f/Fs) * N

For each sample x[n]:
    s[n] = x[n] + coeff * s[n-1] - s[n-2]

Magnitude = sqrt(s[N-1]^2 + s[N-2]^2 - coeff * s[N-1] * s[N-2])
```

### Advantages over FFT
- **Lower memory** - No complex buffer allocation
- **Single-frequency** - Only compute bins we need
- **Fixed-point friendly** - Coefficient precomputation
- **Deterministic timing** - No bit-reversal or twiddle lookups

### Performance (ESP32-S3 @ 240MHz)
- **Window size**: 512 samples
- **Compute time**: ~1.5 ms for 8 bands
- **Memory**: 1 KB accumulation buffer + 32 bytes coefficients
- **Overhead**: Negligible compared to I2S DMA

## Integration with AudioActor

The AudioActor FreeRTOS task (Core 0) orchestrates:

1. **I2S Capture** - Read 256 samples (16ms @ 16kHz) via DMA
2. **Goertzel Analysis** - Accumulate samples, analyze every 2 hops
3. **ControlBus Update** - Smooth raw analysis with attack/release
4. **Snapshot Publish** - Push frame to render core via SnapshotBuffer

```cpp
// Pseudocode for AudioActor loop
void AudioActor::run() {
    while (true) {
        // 1. Read from I2S
        i2s_read(I2S_NUM_0, samples, HOP_SIZE * sizeof(int16_t), ...);

        // 2. Update time
        AudioTime now(sampleIndex, 16000, esp_timer_get_time());
        sampleIndex += HOP_SIZE;

        // 3. Accumulate for Goertzel
        m_goertzel.accumulate(samples, HOP_SIZE);

        // 4. Analyze when window full (every 2 hops)
        float bands[8];
        if (m_goertzel.analyze(bands)) {
            ControlBusRawInput raw;
            memcpy(raw.bands, bands, sizeof(bands));
            raw.rms = computeRMS(samples, HOP_SIZE);

            m_controlBus.UpdateFromHop(now, raw);
        }

        // 5. Publish to render core
        m_frameBuffer.Publish(m_controlBus.GetFrame());
    }
}
```

## Thread Safety

All cross-core communication uses lock-free atomics:

- **SnapshotBuffer** - Atomic index swap, no mutex
- **MusicalGrid::OnTempoEstimate()** - Atomic BPM update
- **MusicalGrid::OnBeatObservation()** - Phase correction (lock-free)

**CRITICAL**:
- Audio thread (Core 0) only calls `Publish()` and `OnXxx()` methods
- Render thread (Core 1) only calls `ReadLatest()` and `Tick()`

## Memory Budget

| Component          | Size   | Location |
|--------------------|--------|----------|
| GoertzelAnalyzer   | ~1 KB  | DRAM     |
| ControlBus         | 256 B  | DRAM     |
| SnapshotBuffer (2x)| 512 B  | DRAM     |
| MusicalGrid        | 128 B  | DRAM     |
| **Total**          | **2 KB**| DRAM    |

## Feature Flags

All audio components are guarded by:

```cpp
#if FEATURE_AUDIO_SYNC
// ... audio code ...
#endif
```

Set in `src/config/features.h`:
```cpp
#define FEATURE_AUDIO_SYNC 1  // Enable audio subsystem
```

## Testing

Unit tests are located in `v2/test/audio/`:
- `test_goertzel.cpp` - Goertzel algorithm correctness
- `test_controlbus.cpp` - Attack/release smoothing
- `test_musicalgrid.cpp` - PLL phase tracking

Run tests:
```bash
pio test -e native  # Run on host (native)
```

## References

- **Goertzel Algorithm**: https://en.wikipedia.org/wiki/Goertzel_algorithm
- **PLL Theory**: "Digital Phase-Locked Loops" by Roland E. Best
- **Lock-Free Programming**: "C++ Concurrency in Action" by Anthony Williams
