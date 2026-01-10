# Beat/Tempo Tracking System - Consolidated Source Files

This directory contains the top 10 most critical source files for the beat/tempo tracking system, consolidated from the K1 migration session.

## File List

1. **01_TempoTracker.h** - Core tempo tracker header
   - Onset detection, beat tracking, PLL phase lock
   - TempoDensityBuffer (60-180 BPM bins)
   - 2nd-order PLL for tempo correction

2. **02_TempoTracker.cpp** - Core tempo tracker implementation
   - K1 feature consumption (`updateFromFeatures`)
   - Beat-candidate gating (minDt check)
   - Density buffer voting with octave folding
   - PLL phase/tempo correction

3. **03_K1AudioFrontEnd.h** - K1 orchestrator header
   - Coordinates all K1 modules
   - Produces `AudioFeatureFrame` every hop

4. **04_K1AudioFrontEnd.cpp** - K1 orchestrator implementation
   - RhythmBank processing (every hop)
   - HarmonyBank processing (every 2 hops)
   - Novelty flux computation
   - Signal conditioning (NoiseFloor, AGC)

5. **05_NoveltyFlux.h** - Novelty flux detector header
   - Half-wave rectified spectral flux
   - Scale-invariant normalization

6. **06_NoveltyFlux.cpp** - Novelty flux implementation
   - Frame-to-frame energy increase detection
   - Baseline EMA for normalization

7. **07_K1Types.h** - Core data structures
   - `AudioChunk` - Single hop of audio samples
   - `AudioFeatureFrame` - Complete feature output
   - `GoertzelBinSpec` - Goertzel bin specification

8. **08_K1Spec.h** - Locked configuration constants
   - FS_HZ = 16000 (sample rate)
   - HOP_SAMPLES = 128 (8ms hop)
   - HARMONY_BINS = 64, RHYTHM_BINS = 24
   - N_MIN = 256, N_MAX = 1536

9. **09_FeatureBus.h** - Cross-core communication
   - Lock-free snapshot buffer
   - Single-writer, multi-reader pattern

10. **10_TempoOutput.h** - Output contract
    - BPM, phase, confidence, beat_tick
    - Effect consumption interface

11. **11_AudioNode_Integration.cpp** - Integration point
    - K1 front-end initialization
    - Feature frame processing
    - TempoTracker update call

## Architecture Overview

```
AudioNode (AudioNode.cpp)
    ↓
K1AudioFrontEnd (K1AudioFrontEnd.cpp)
    ├─ RhythmBank (24 bins, every hop)
    ├─ NoveltyFlux (onset detection)
    └─ AudioFeatureFrame output
        ↓
TempoTracker (TempoTracker.cpp)
    ├─ updateFromFeatures() - consumes rhythm_novelty
    ├─ detectOnset() - peak detection
    ├─ updateBeat() - beat-candidate gating + density voting
    ├─ updateTempo() - density buffer peak finding + PLL
    └─ TempoOutput - BPM, phase, confidence, beat_tick
```

## Key Features

- **Sample Counter Timebase**: All timing uses `uint64_t t_samples` (deterministic, native-safe)
- **K1 Integration**: TempoTracker consumes `rhythm_novelty` from K1 front-end
- **TempoDensityBuffer**: 121 BPM bins (60-180), octave folding (0.5×, 1×, 2×)
- **2nd-Order PLL**: Phase correction (fast) + tempo correction (slow)
- **Beat-Candidate Gating**: Rejects fast onsets (< minDt) to prevent hat interference

## Migration Status

✅ **Complete**: All K1 migration phases completed
- Phase 0-10: All acceptance gates passed
- TempoTracker refactored to use K1 features
- Sample counter timebase unified
- Beat-candidate gating implemented
- TempoDensityBuffer + PLL implemented

## Current Debug Status

🔍 **In Progress**: Debugging zero novelty issue
- Hypothesis F: K1 initialization/audio chunk
- Hypothesis G: Rhythm bank producing zeros
- Hypothesis H: NoveltyFlux receiving zeros

Instrumentation added to trace the pipeline from audio input → rhythm processing → novelty output.

