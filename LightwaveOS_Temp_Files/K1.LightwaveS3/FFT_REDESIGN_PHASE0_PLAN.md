# FFT Redesign - Phase 0: Library Integration & Workspace Setup

**Date**: 2026-01-09
**Branch**: `feat/tempo-fft-redesign`
**Status**: In Progress

---

## Objectives

1. **Integrate KissFFT** from FastLED dependency
2. **Create FFT wrapper classes** for 512-point real FFT at 16kHz
3. **Understand latency budget** and performance constraints
4. **Plan architecture** for phases 1-10
5. **Establish testing framework** for FFT accuracy

---

## Current Architecture (Goertzel-Based)

```
Audio Input (128 samples @ 16kHz, 8ms)
    ↓
K1AudioFrontEnd
    ├─ RhythmBank (24 Goertzel bins, 60-600Hz)
    ├─ HarmonyBank (64 Goertzel bins, 200-2kHz)
    ├─ NoveltyFlux (spectral flux: 0.7×rhythm + 0.3×harmony)
    └─ AudioFeatureFrame output (125Hz frame rate)
        ↓
TempoTracker (onset timing → BPM estimation)
    ↓
TempoOutput (bpm, phase01, confidence, beat_tick)
```

---

## Target Architecture (FFT-Based)

```
Audio Input (128 samples @ 16kHz, 8ms)
    ↓
K1AudioFrontEnd (KEEP SAME INTERFACE)
    ├─ K1FFTAnalyzer (512-point real FFT at 16kHz)
    │   ├─ Windowing (Hann window pre-computed)
    │   ├─ vDSP/KissFFT real FFT
    │   └─ Magnitude extraction (1024 float → 256 magnitude)
    ├─ SpectralFlux (half-wave rectified magnitude differences)
    ├─ FrequencyBandExtraction (bass/mid/high energy)
    └─ AudioFeatureFrame output (SAME FORMAT AS BEFORE)
        ↓
TempoTracker (Synesthesia's exact algorithm)
    ├─ Adaptive threshold (median + 1.5σ flux history)
    ├─ Onset detection (peak picking)
    ├─ BPM estimation (inter-onset interval voting)
    ├─ Exponential smoothing (α_attack=0.15, α_release=0.05)
    ├─ Multi-factor confidence
    └─ State machine (SEARCHING/LOCKING/LOCKED/UNLOCKING)
        ↓
TempoOutput (bpm, phase01, confidence, beat_tick)
```

**Key Insight**: Keep K1AudioFrontEnd interface unchanged, replace internals.

---

## Phase 0 Deliverables

### 1. FFT Window & Setup (NEW: `K1FFTConfig.h`)

- Pre-computed Hann window (512 samples)
- KissFFT initialization
- Magnitude scaling constants

**Files to Create**:
- `firmware/v2/src/audio/k1/K1FFTConfig.h` - Configuration & constants

### 2. FFT Analyzer Wrapper (NEW: `K1FFTAnalyzer.h`)

Real FFT wrapper using KissFFT:
- Constructor: Allocate KissFFT context
- `processFrame()`: Apply window → FFT → magnitude extraction
- `getSpectrum()`: Return frequency bin magnitudes (256 bins for 16kHz)

**Files to Create**:
- `firmware/v2/src/audio/k1/K1FFTAnalyzer.h`
- `firmware/v2/src/audio/k1/K1FFTAnalyzer.cpp`

### 3. Spectral Flux Calculator (NEW: `SpectralFlux.h`)

- Magnitude history buffer (2 frames for delta calculation)
- `process()`: Calculate flux = Σ max(0, current[k] - previous[k])
- `getFlux()`: Return scalar flux value

**Files to Create**:
- `firmware/v2/src/audio/k1/SpectralFlux.h`
- `firmware/v2/src/audio/k1/SpectralFlux.cpp`

### 4. Frequency Band Extractor (NEW: `FrequencyBandExtractor.h`)

Extract bass/mid/high energy from magnitude spectrum:
- Bass: 20-200Hz (bins 1-9 @ 21.5Hz/bin)
- Mid: 200-2kHz (bins 10-93)
- High: 6-20kHz (bins 279-930)

**Files to Create**:
- `firmware/v2/src/audio/k1/FrequencyBandExtractor.h`

### 5. AudioFeatureFrame Compatibility

Ensure new FFT pipeline outputs **same AudioFeatureFrame format**:
- `mag_rhythm[24]` → Extract from FFT magnitudes (20-600Hz subset)
- `mag_harmony[64]` → Extract from FFT magnitudes (200-2kHz subset)
- `novelty_flux` → From new SpectralFlux class

**Modification**:
- `firmware/v2/src/audio/k1/K1AudioFrontEnd.h` - Add FFT components
- `firmware/v2/src/audio/k1/K1AudioFrontEnd.cpp` - Replace Goertzel logic

---

## Performance Budget (ESP32-S3 at 16kHz)

### Goertzel Baseline (Current)
- RhythmBank: 24 bins × 2 multiplies + 2 adds ≈ 96 operations per sample
- HarmonyBank: 64 bins × same ≈ 256 operations
- Total: ~400 operations/sample × 16000 samples/sec = **6.4 MOPS**
- Measured: **0.8-1.2ms per 8ms buffer** (10-15% CPU)

### KissFFT (Target)
- 512-point real FFT: ~0.5ms (N log N ≈ 4608 operations)
- Magnitude extraction: ~0.1ms (256 multiplies)
- Spectral flux: ~0.1ms (256 subtracts + max)
- Total: **0.7-0.9ms per 8ms buffer** (9-11% CPU) ✅ **WITHIN BUDGET**

### Memory Impact
- **Goertzel**: RhythmBank (240B) + HarmonyBank (640B) + NoveltyFlux = ~1.2KB
- **FFT**: KissFFT workspace (~2KB) + magnitude buffer (1KB) + window (2KB) + flux history (1KB) = **~6KB**
- **Trade-off**: +5KB memory for better frequency resolution & Synesthesia accuracy

---

## KissFFT Integration (Already Available!)

```cpp
// From FastLED 3.10.0 (already in platformio.ini)
#include "kiss_fft.h"
#include "kiss_fftr.h"

// Real FFT for 512 samples:
kiss_fftr_cfg cfg = kiss_fftr_alloc(512, 0, NULL, NULL);
kiss_fft_cpx fft_out[512];
kiss_fftr(cfg, input_real, fft_out);
kiss_fft_free(cfg);
```

**No new library needed!** Just include from FastLED's third_party directory.

---

## Memory Layout (Pre-Allocated, No Heap in Hot Path)

```
AudioRingBuffer         1024 floats  (4KB) - KEEP SAME
K1FFTConfig (constants) ~2KB        - Read-only
K1FFTAnalyzer
  - fftSetup (KissFFT)  ~2KB        - Allocated once in init()
  - window[512]         2KB         - Pre-computed Hann window
  - magnitude[256]      1KB         - Current magnitude spectrum
  - input_real[512]     2KB         - FFT input buffer

SpectralFlux
  - prev_magnitude[256] 1KB         - Previous frame
  - flux_history[40]    160B        - For adaptive threshold

FrequencyBandExtractor
  - Static (no state)    0B         - Pure function

Total New: ~11KB
```

---

## Testing Strategy

### Unit Tests (Phase 0)
1. **FFT Accuracy**: Compare against known test signal
   - Input: 1000 Hz sine at -20dB
   - Expected: Magnitude peak at bin 47 (1000Hz ≈ 47×21.5Hz)

2. **Window Function**: Verify Hann window properties
   - Max at center (sample 256) = 1.0
   - Min at edges = 0

3. **Magnitude Extraction**: Verify magnitude calculation
   - Input: FFT of known amplitude
   - Output: Magnitude within 5% of expected

4. **Spectral Flux**: Verify flux calculation
   - Input: Two frames with known magnitude change
   - Output: Flux = Σ max(0, diff)

### Integration Tests (Phase 1)
- Process 8ms audio chunks through full pipeline
- Verify AudioFeatureFrame output matches old format
- Check CPU/memory usage

---

## Files to Create/Modify

### NEW FILES (Phase 0)
```
firmware/v2/src/audio/k1/
├── K1FFTConfig.h                 [NEW] FFT config & Hann window
├── K1FFTAnalyzer.h               [NEW] FFT wrapper class
├── K1FFTAnalyzer.cpp             [NEW] KissFFT integration
├── SpectralFlux.h                [NEW] Spectral flux calculator
├── SpectralFlux.cpp              [NEW] Flux magnitude difference
└── FrequencyBandExtractor.h       [NEW] Bass/mid/high energy extraction
```

### MODIFIED FILES (Phase 1)
```
firmware/v2/src/audio/k1/
├── K1AudioFrontEnd.h             [MODIFY] Add FFT components
├── K1AudioFrontEnd.cpp           [MODIFY] Replace Goertzel logic
└── K1Types.h                      [MODIFY] Add FFT-related types if needed
```

### UNCHANGED
```
firmware/v2/src/audio/tempo/
├── TempoTracker.h                [UNCHANGED] Interface stays same
├── TempoTracker.cpp              [PHASE 3+] Replace algorithm
└── TUNING_GUIDE.md               [PHASE 5+] Update parameters

firmware/v2/src/audio/contracts/
└── TempoOutput.h                 [UNCHANGED]
```

---

## Success Criteria (Phase 0)

- [x] KissFFT successfully compiles in ESP32-S3 build
- [ ] K1FFTAnalyzer processes 512-sample frames correctly
- [ ] Magnitude spectrum matches expected frequency bins
- [ ] Spectral flux calculation verified on test signals
- [ ] Memory usage ≤ 11KB new allocation
- [ ] CPU usage ≤ 2ms per 8ms buffer (25% CPU)
- [ ] AudioFeatureFrame output compatible with TempoTracker
- [ ] All Phase 0 unit tests pass

---

## Dependencies

**Already Available**:
- FastLED 3.10.0 (KissFFT included)
- ESP32-S3 Arduino Framework
- FreeRTOS

**No Additional Libraries Needed** ✅

---

## Next Phases (Overview)

| Phase | Objective | Key Changes |
|-------|-----------|-------------|
| **0** | FFT library integration | KissFFT wrapper + spectral flux |
| **1** | 512pt FFT pipeline | Replace Goertzel banks completely |
| **2** | Spectral migration | Update mag_rhythm/harmony extraction |
| **3** | Beat detection rewrite | Synesthesia's exact algorithm |
| **4** | Adaptive threshold | Median + 1.5σ |
| **5** | Exponential smoothing | Attack/release BPM smoothing |
| **6** | Confidence scoring | Multi-factor formula |
| **7** | State machine | SEARCHING/LOCKING/LOCKED/UNLOCKING |
| **8** | Integration testing | End-to-end validation |
| **9** | Performance optimization | CPU/memory tuning for ESP32-S3 |
| **10** | Final validation | Comprehensive testing across genres |

---

## References

- [Synesthesia Beat Detection](../../Synesthesia/Docs/02_ALGORITHMS/01_Beat_Detection_Complete.md) - Algorithm spec
- [FFT Processing](../../Synesthesia/Docs/02_ALGORITHMS/04_FFT_Processing.md) - Windowing, magnitude extraction
- [KissFFT Documentation](https://github.com/mborgerding/kissfft)
- [FastLED FFT Examples](firmware/Tab5.encoder/_ref/M5Unified/examples/Advanced/Mic_FFT/Mic_FFT.ino)

---

**Status**: Ready to proceed to Phase 1 (FFT Pipeline Implementation)
