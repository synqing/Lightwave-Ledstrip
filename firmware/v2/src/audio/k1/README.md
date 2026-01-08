# K1 Dual-Bank Goertzel Front-End

## Phase Acceptance Gates

### Build Gates
- ✅ Build succeeds for ESP32-S3 target (`esp32dev_audio` environment)
- ✅ Build succeeds for native target (`native` environment)
- ✅ No compilation errors or warnings

### Timebase Gates
- ✅ Can print single hop timestamp from sample counter
- ✅ No timing derived from `esp_timer_get_time()` in algorithmic code
- ✅ All timestamps use sample counter: `t_us = (sampleIndex * 1000000ULL) / FS_HZ`

### Determinism Gates
- ✅ Native builds produce identical results for identical inputs
- ✅ No system timer dependencies in DSP/tempo paths
- ✅ Sample counter is monotonic and deterministic

## Architecture Overview

The K1 front-end provides:
- **RhythmBank**: 24 bins computed every hop (125 Hz)
- **HarmonyBank**: 64 semitone bins computed every 2 hops (62.5 Hz)
- **Sample-counter timebase**: Deterministic, native-safe timing
- **Group-by-N processing**: Efficient CPU usage via shared history buffers

## Critical Correctness Requirements

1. **Goertzel coeff from k/N**: `ω = 2πk/N`, NOT `2πf/FS`
2. **Per-N windows**: Each unique N gets its own Hann LUT
3. **Magnitude normalization**: Normalize by window energy and N
4. **Group processing**: Copy/window once per N, not per bin

