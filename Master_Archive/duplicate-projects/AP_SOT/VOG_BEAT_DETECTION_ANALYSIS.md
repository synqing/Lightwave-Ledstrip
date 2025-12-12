# Voice of God (VoG) and Beat Detection Analysis
**Date**: January 14, 2025  
**System**: AP_SOT (Pluggable Audio Pipeline)  
**Current State**: Beat detection working, VoG showing 0.00, BPM stuck at 141

## Executive Summary

The beat detection system is successfully detecting beats but has two critical issues:
1. VoG (Voice of God) confidence engine shows 0.00 due to improper buffer management
2. BPM gets stuck at 141 even during silence due to missing timeout mechanism

## System Architecture Overview

### Dual-Path Processing
```
I2S Input → DC Offset → Goertzel → [Fork Point]
                                    ├─→ Beat Detector (RAW data)
                                    └─→ AGC → Zone Mapper (Visualization)
                                    
VoG Engine: Async @ 10-12Hz comparing RAW vs AGC spectrum
```

### Key Components

1. **Main Pipeline** (`SpectraSynq_Main`)
   - I2S Input (16kHz, 128 samples)
   - DC Offset Calibration
   - Goertzel Transform (96 bins, /8 scaled)
   - Multiband AGC (4 bands)
   - Zone Mapper (36 zones → 8 legacy zones)

2. **Beat Pipeline** (`SpectraSynq_Beat`)
   - Beat Detector Node (uses RAW Goertzel output)
   - Legacy algorithm: energy threshold + transient detection
   - Requires 4 inter-beat intervals for confidence calculation

3. **VoG Engine** (Asynchronous)
   - Runs at ~11.8Hz (85ms interval)
   - Compares raw spectrum vs AGC spectrum
   - Calculates confidence based on energy ratio
   - NOT part of main pipeline - decoupled oracle

## Current Issues Identified

### 1. VoG Shows 0.00 Confidence

**Root Cause**: Buffer pointer management issue
- VoG is created in `main.cpp` line 93
- Spectrum pointers set in lines 252-269
- Problem: Buffers are pipeline outputs that get reused/overwritten
- By the time VoG reads at 12Hz, data is invalid

**Evidence from main.cpp**:
```cpp
// Line 254-259: Getting pointers to live buffers
AudioBuffer* raw_spectrum = main_pipeline->getNodeOutput("Goertzel");
AudioBuffer* agc_spectrum = main_pipeline->getNodeOutput("MultibandAGC");
vog_node->setSpectrumPointers(raw_spectrum, agc_spectrum);
```

These pointers reference buffers that are actively being modified by the pipeline.

### 2. BPM Stuck at 141

**Root Cause**: No timeout mechanism in legacy beat detector
- Once 4+ beats establish a BPM, it never resets
- No silence detection or timeout
- BPM remains at last calculated value forever

**Evidence from legacy_beat_detector.cpp**:
- No timeout logic in `process()` method
- `current_bpm` only updated when new beats detected
- No mechanism to reset or decay BPM during silence

## Critical System Understanding

### Goertzel /8 Scaling
- **NEVER REMOVE THIS SCALING** - it's calibrated throughout the system
- Output range: 0.0 to ~65536.0 (depends on input amplitude)
- Silence threshold: 50.0f (calibrated for /8 scaled values)
- All thresholds and calculations depend on this scaling

### Beat Detection Flow
1. Goertzel outputs scaled frequency magnitudes
2. Beat detector calculates total energy across bins
3. Checks for transient (energy increase from last frame)
4. If energy > 50.0f AND transient detected AND debounce time passed → BEAT
5. After 4 beats, calculates BPM from inter-beat intervals

### VoG Algorithm Purpose
- **Confidence Engine**: Validates beat significance
- **Hardness Modulator**: Provides intensity metric for visuals
- Compares dynamic range: raw energy vs AGC-normalized energy
- High ratio = significant transient (real beat)
- Low ratio = steady state or compressed audio

## Data Flow Analysis

### Current (Broken) Flow:
1. Pipeline processes frame
2. Goertzel output buffer updated
3. AGC processes and modifies buffer
4. Zone mapper processes
5. VoG tries to read buffers (but they're already modified/reused)
6. VoG calculates ratio of invalid data → 0.00 confidence

### Required (Fixed) Flow:
1. Pipeline processes frame
2. **Snapshot Goertzel RAW output** → stable buffer for VoG
3. AGC processes
4. **Snapshot AGC output** → stable buffer for VoG
5. Zone mapper processes
6. VoG reads stable snapshots at 12Hz → valid confidence

## Debug Output Analysis

From the serial output:
```
BEAT DETECTED! energy=62465.9, BPM will update
Energy: 0.5 | Bass: 0.8 | Mid: 0.6 | High: 0.1 | Beat: 0.0 (120 BPM) | VoG: 0.00
```

- Beat detection working (energy > 50.0 threshold)
- Zone energies normalized correctly (0-1 range)
- Beat confidence shows 0.0 (needs 4 beats for history)
- VoG shows 0.00 (buffer issue)

After sufficient beats:
```
Energy: 0.5 | Bass: 0.7 | Mid: 0.7 | High: 0.1 | Beat: 0.9 (141 BPM) | VoG: 0.00
```

- BPM calculated correctly (140.9)
- Confidence high (0.93)
- But BPM stuck at 141 even during silence
- VoG still 0.00

## Implementation Requirements

### 1. Fix VoG Buffer Management
- Create dedicated snapshot buffers (2 x 96 floats)
- Capture snapshots at correct pipeline stages
- Ensure VoG reads stable, valid data

### 2. Add Beat Detector Timeout
- Track time since last beat
- If no beat for 3+ seconds, reset BPM to 0 or 120
- Decay confidence during silence

### 3. Maintain System Integrity
- NO changes to /8 scaling
- NO changes to core algorithms
- Only fix buffer management and add timeout

## Risk Assessment

### Safe to Change:
- Buffer management (adding snapshots)
- Timeout logic (new feature, doesn't affect detection)
- VoG pointer setup

### NEVER Change:
- Goertzel /8 scaling factor
- Energy thresholds (50.0f)
- Core beat detection algorithm
- Pipeline processing order

## Testing Protocol

1. Verify beat detection still works
2. Confirm VoG shows > 0.00 during beats
3. Test BPM timeout during silence
4. Ensure no performance impact
5. Validate with various music genres

## Live Testing Results - January 14, 2025

### Test Configuration
- Energy threshold increased from 50.0f to 8000.0f
- Transient detection at 50% with 5000 minimum
- Running with live music input

### Key Findings

#### 1. Beat Detection - "Crocodile Lock" Behavior
- Successfully detecting beats (not every frame anymore)
- BPM locked to 140.2 with 96% confidence
- **Issue**: Still detecting too many beats per second
  - Expected: ~2.3 beats/second for 140 BPM
  - Actual: 5-10 detections per second
  - Energy values: 8000-15000 for weak beats, 30000+ for strong beats
  - Background: 1000-5000 range

#### 2. VoG Fundamental Flaw - The WHERE Problem
**Critical Discovery**: VoG is looking at the wrong relationship!

Current VoG logic:
```
if (raw_energy > agc_energy) {
    // Assumes this indicates a transient
    confidence = (ratio - 1.0) * 2.0
}
```

**The Problem**: During normal music playback, AGC is DOING ITS JOB:
- AGC boosts quiet signals → agc_energy > raw_energy
- This is NORMAL behavior, not a failure state
- VoG ratio consistently 0.2-0.5 (AGC 2-5x higher than raw)
- Result: raw_conf = 0.000 always

**The Insight**: It's not WHAT we measure or HOW we measure - it's WHERE we look:
1. Absolute ratio (raw/AGC) is wrong metric
2. We should measure RATE OF CHANGE in the ratio
3. When a transient hits:
   - Raw energy spikes immediately
   - AGC takes time to respond (attack time)
   - The DELTA in ratio reveals the transient

### 3. Revised VoG Algorithm Concept

Instead of:
```cpp
float ratio = raw_energy / agc_energy;
if (ratio > 1.0) confidence = high;
```

We need:
```cpp
float current_ratio = raw_energy / agc_energy;
float ratio_delta = current_ratio - previous_ratio;
float ratio_acceleration = ratio_delta - previous_delta;

// Transients show as positive acceleration in ratio
if (ratio_acceleration > threshold) {
    // This is a divine moment!
    confidence = normalize(ratio_acceleration);
}
```

### 4. Beat Detection Recommendations

Based on testing:
1. **Energy Threshold**: Increase to 12000-15000 (not 8000)
2. **Transient Sensitivity**: Increase to 75% (not 50%)
3. **Temporal Gating**: Add minimum time between beats
   ```cpp
   min_beat_interval = 60000.0f / (current_bpm * 1.5f);
   if (time_since_last < min_beat_interval) skip;
   ```

## Conclusion

The system architecture is sound, but we've been looking in the wrong place:

1. **Beat Detection**: Works but needs temporal gating to prevent "crocodile lock"
2. **VoG Engine**: Fundamentally flawed - comparing absolutes instead of derivatives
3. **The Solution**: Measure CHANGE and ACCELERATION, not static ratios

The Voice of God speaks through dynamics, not statics. We must listen to the rate of change, not the momentary state.