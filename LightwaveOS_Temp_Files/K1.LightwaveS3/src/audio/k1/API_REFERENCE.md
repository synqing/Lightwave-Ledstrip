# K1 Front-End API Reference

## Core Types

### AudioChunk
```cpp
struct AudioChunk {
    int16_t samples[128];        // Mono audio samples
    uint32_t n;                  // Always 128
    uint64_t sample_counter_end; // Inclusive end sample index
};
```

### AudioFeatureFrame
```cpp
struct AudioFeatureFrame {
    // Timestamps (sample counter)
    uint64_t t_samples;          // End sample index
    uint32_t hop_index;           // Increments each hop
    float t_us;                   // Derived: (t_samples * 1000000ULL) / 16000

    // Rhythm (every hop, 125 Hz)
    float rhythm_bins[24];        // Post-noise, post-AGC
    float rhythm_energy;          // RMS
    float rhythm_novelty;          // Scale-invariant flux

    // Harmony (every 2 hops, 62.5 Hz)
    bool harmony_valid;           // True only on harmony ticks
    float harmony_bins[64];       // Post-noise, post-AGC
    float chroma12[12];           // Sum-normalized chroma
    float chroma_stability;        // Rolling cosine similarity
    float key_clarity;             // Peakiness metric

    // Quality flags
    bool is_silence;
    bool is_clipping;
    bool overload;                // Compute overrun
};
```

## Main API

### K1AudioFrontEnd
```cpp
class K1AudioFrontEnd {
public:
    bool init();
    AudioFeatureFrame processHop(const AudioChunk& chunk, bool is_clipping);
    const AudioFeatureFrame& getCurrentFrame() const;
    bool isInitialized() const;
};
```

### FeatureBus
```cpp
class FeatureBus {
public:
    void publish(const AudioFeatureFrame& frame);
    void getSnapshot(AudioFeatureFrame* frame_out) const;
    const AudioFeatureFrame* getCurrentFrame() const;
};
```

## TempoTracker Integration

### updateFromFeatures()
```cpp
void TempoTracker::updateFromFeatures(const k1::AudioFeatureFrame& frame);
```

Consumes K1 features:
- `frame.rhythm_novelty` → primary onset evidence
- `frame.rhythm_energy` → optional VU derivative
- `frame.t_samples` → timing (sample counter)

## Sample Counter Timebase

All timestamps use sample counter:
- `t_samples = sample_index` (monotonic, deterministic)
- `t_us = (t_samples * 1000000ULL) / FS_HZ`
- Native-safe (no system timers)

## Constants

See `K1Spec.h`:
- `FS_HZ = 16000`
- `HOP_SAMPLES = 128`
- `HOP_HZ = 125`
- `HARMONY_TICK_DIV = 2`
- `HARMONY_BINS = 64`
- `RHYTHM_BINS = 24`
- `N_MIN = 256`, `N_MAX = 1536`

