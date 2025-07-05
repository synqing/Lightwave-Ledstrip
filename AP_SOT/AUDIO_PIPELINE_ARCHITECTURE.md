# SpectraSynq Audio Pipeline Architecture
**CRITICAL DOCUMENTATION - READ BEFORE MODIFYING**

## Overview

The SpectraSynq audio pipeline implements a sophisticated dual-path architecture to solve the fundamental conflict between beat detection (which requires raw dynamics) and visual effects (which need normalized data).

## Signal Flow

```
SPH0645 I2S Microphone (16kHz, 32-bit)
         ↓
DC Offset Calibration (-5200 for SPH0645)
         ↓
High-Pass Filter (removes DC drift)
         ↓
God-Tier Goertzel Engine (96 musical bins, A0-A7)
         ↓
      [SPLIT]
         ↓                              ↓
    RAW PATH                       AGC PATH
(Preserves dynamics)          (Normalizes for visuals)
         ↓                              ↓
   Beat Detection              Multiband AGC System
   Onset Detection            (4-band cochlear processing)
   Tempo Tracking                       ↓
         ↓                      Zone Energy Calculation
         ↓                      Visual Effects
         ↓                              ↓
      [MERGE]
         ↓
    Audio State
(Global data structure)
```

## Critical Components

### 1. God-Tier Goertzel Engine
- 96 frequency bins (A0-A7, every semitone)
- Compile-time LUTs (zero runtime trig)
- Cache-optimized loop structure
- Raw magnitude output (0-10000 range)

### 2. Dual Data Paths
**RAW PATH:**
- Direct Goertzel output
- NO normalization
- Fed to beat/onset detection
- Preserves transient information

**AGC PATH:**
- Goertzel → Multiband AGC
- 4 perceptual bands
- Normalized output (0-1 range)
- Used for zone calculation and visuals

### 3. Multiband AGC System
- **Bass (20-200Hz)**: Kick drums, bass fundamentals
- **Low-Mid (200-800Hz)**: Vocals, instrument body
- **High-Mid (800-3kHz)**: Presence, clarity
- **Treble (3k-20kHz)**: Cymbals, air, sparkle

Features:
- Independent gain per band
- Cross-band coupling (prevents artifacts)
- Dynamic time constants
- Cochlear-inspired processing

### 4. Beat Detection
- Operates on RAW frequency data
- Energy variance tracking
- Multi-candidate tempo estimation
- Phase tracking for visual sync

## Common Mistakes to Avoid

### ❌ DO NOT: Process beat detection on AGC output
AGC normalizes dynamics, destroying the transient information beat detection needs.

### ❌ DO NOT: Apply normalization before beat detection
Beat detection requires the full dynamic range to detect energy spikes.

### ❌ DO NOT: Mix RAW and AGC paths
Keep the paths separate. Beat detection uses RAW, visuals use AGC.

### ❌ DO NOT: Modify Goertzel frequencies
The 96 bins are musically spaced (A0-A7). Linear spacing breaks musical accuracy.

### ❌ DO NOT: Change the sample rate
16kHz is optimal for SPH0645 and our frequency range.

## Integration Points

### Adding New Audio Effects
1. Effects should read from `audio_state.core.audio_bins` (AGC-processed)
2. Never modify the frequency bins directly
3. Use zone energies for frequency-based effects

### Adding New Beat-Reactive Features
1. Read from `audio_state.ext.beat` structure
2. Use beat confidence and phase for timing
3. Never process AGC data for beat detection

### Modifying the Pipeline
1. Understand both data paths first
2. Test with real music (not just tones)
3. Monitor both RAW and AGC outputs
4. Verify beat detection still works

## Performance Characteristics
- Latency: <8ms (128 samples @ 16kHz)
- Frame rate: 125 FPS capability
- CPU usage: ~15% with full pipeline
- Memory: Static allocation, no heap usage

## Testing the Pipeline

```bash
# Monitor dual paths
python monitor_audio.py

# Test beat detection
python test_beat_detection.py

# Visualize AGC bands
python monitor_agc_bands.py
```

## Historical Context

Previous implementations made these mistakes:
1. FFT replaced Goertzel (lost musical accuracy)
2. AGC before beat detection (broke beat tracking)
3. Linear frequency bins (not musically spaced)
4. Hardcoded 120 BPM (ignored actual tempo)

The current architecture solves all these issues through careful separation of concerns and understanding of psychoacoustic principles.

---
**Remember**: Beat detection needs dynamics. Visual effects need normalization. Never mix the two!