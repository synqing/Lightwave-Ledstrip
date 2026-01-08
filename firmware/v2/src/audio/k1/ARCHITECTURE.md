# K1 Dual-Bank Goertzel Front-End Architecture

## Overview

The K1 front-end provides a deterministic, scale-invariant audio feature extraction pipeline optimized for beat/tempo tracking and harmonic analysis.

## Key Design Principles

1. **Sample Counter Timebase**: All timestamps use sample counter (no system timers)
2. **Scale-Invariant Features**: Normalized magnitudes and novelty flux
3. **Group-by-N Processing**: Efficient CPU usage via shared history buffers
4. **Correct Goertzel Math**: Coeff from k/N (not f directly)

## Architecture

```
AudioNode (125 Hz)
  │
  ├─> AudioCapture (128 samples @ 16kHz)
  │
  ├─> K1AudioFrontEnd
  │   │
  │   ├─> AudioRingBuffer (4096 samples)
  │   │
  │   ├─> RhythmBank (every hop, 125 Hz)
  │   │   ├─> GoertzelBank (24 bins, group-by-N)
  │   │   ├─> NoiseFloor
  │   │   ├─> AGC (attenuation-only)
  │   │   └─> NoveltyFlux
  │   │
  │   └─> HarmonyBank (every 2 hops, 62.5 Hz)
  │       ├─> GoertzelBank (64 bins, group-by-N)
  │       ├─> NoiseFloor
  │       ├─> AGC (mild boost)
  │       ├─> ChromaExtractor (64→12)
  │       └─> ChromaStability
  │
  └─> FeatureBus (publish AudioFeatureFrame)
      │
      └─> TempoTracker (consume K1 features)
```

## Data Flow

1. **Audio Capture**: 128 samples @ 16kHz → `AudioChunk`
2. **Ring Buffer**: Push chunk, maintain history
3. **RhythmBank**: Process every hop (125 Hz)
   - CopyLast + window (per unique N)
   - Goertzel kernel (all bins with same N)
   - Noise floor subtraction
   - AGC (attenuation-only)
   - Novelty flux calculation
4. **HarmonyBank**: Process every 2 hops (62.5 Hz)
   - Same processing as RhythmBank
   - Chroma extraction (64→12)
   - Chroma stability tracking
5. **Feature Frame**: Publish `AudioFeatureFrame` to `FeatureBus`
6. **Tempo Tracker**: Consume `rhythm_novelty` for onset detection

## Contracts

### AudioFeatureFrame
- Published every hop (125 Hz)
- `harmony_valid` true only on harmony ticks (62.5 Hz)
- All timestamps in samples (sample counter)
- Features normalized and scale-invariant

### Sample Counter Timebase
- `t_samples`: End sample index (monotonic, deterministic)
- `t_us = (t_samples * 1000000ULL) / 16000`
- No `esp_timer_get_time()` in algorithmic code
- Native-safe (deterministic)

## Performance

- **Memory**: <32 KB total
- **CPU**: Rhythm ≤0.6ms, Harmony ≤1.0ms
- **Hot Path**: No heap allocs, no memmove, O(1) window lookup

## Integration

### TempoTracker
- Consumes `AudioFeatureFrame::rhythm_novelty` as primary onset
- Uses `t_samples` for timing (no system timers)
- Beat-candidate gating prevents hat dominance
- Tempo density buffer + 2nd-order PLL for robust tracking

### Effects
- Can consume `AudioFeatureFrame` for harmonic/chroma features
- Backward compatible with legacy `ControlBus` output

