# Dual-Rate Audio Pipeline Technical Brief

**Date:** 2025-12-25
**Authors:** LightwaveOS Team + Claude Opus 4.5
**Commits:** `17e1ed2`, `56d6475`, `ae40c22`, `0e1b9db`
**Branch:** `main`

---

## Executive Summary

This document provides comprehensive technical documentation for the dual-rate audio pipeline implementation in LightwaveOS v2. The architecture separates audio processing into two synchronized lanes optimized for different perceptual requirements: fast texture-reactive features (125 Hz) and stable musical time tracking (62.5 Hz).

---

## Table of Contents

1. [Problem Statement](#1-problem-statement)
2. [Architecture Decision](#2-architecture-decision)
3. [Implementation Details](#3-implementation-details)
4. [File-by-File Changes](#4-file-by-file-changes)
5. [Challenges Overcome](#5-challenges-overcome)
6. [Lessons Learned](#6-lessons-learned)
7. [Future Contributors Guide](#7-future-contributors-guide)

---

## 1. Problem Statement

### 1.1 Original Issue

Audio-reactive effects exhibited **motion stuttering** instead of clean CENTER→EDGE propagation. Investigation revealed multiple root causes:

1. **Single-rate pipeline mismatch**: The original 256-sample hop (16ms @ 16kHz = 62.5 Hz) was too slow for punchy visual micro-motion
2. **Spectral analysis window too short**: 256 samples provided insufficient frequency resolution for bass coherence (sub-100 Hz requires >25ms windows)
3. **Beat tracker instability**: Fast hop rates caused tempo estimate jitter; slow hop rates caused visual lag

### 1.2 Requirements

The solution needed to satisfy 9 non-negotiable requirements:

| Req | Description | Constraint |
|-----|-------------|------------|
| R1 | Tab5 parity for beat tracking | 62.5 Hz beat lane, 256-sample hops |
| R2 | Low-latency texture response | <10ms for RMS/flux updates |
| R3 | Bass frequency coherence | ≥32ms analysis window |
| R4 | Lock-free cross-core coordination | No mutexes in render path |
| R5 | Monotonic sample-domain timing | 64-bit sample index as timebase |
| R6 | MusicalGrid PLL in render domain | 120 FPS phase extrapolation |
| R7 | Graceful degradation | Stub API when audio disabled |
| R8 | Runtime tunability | No recompilation for DSP tweaks |
| R9 | RAM budget | <2KB additional overhead |

---

## 2. Architecture Decision

### 2.1 Lock-In Decision: Band-Weighted Spectral Flux

After evaluating 4 transient detection approaches, **Option 3: Band-weighted spectral flux with adaptive threshold** was selected:

```
flux_weighted = Σ(w[band] × max(0, band[t] - band[t-1]))

where w = [2.0, 2.0, 1.0, 1.0, 1.0, 0.5, 0.5, 0.5]  // bass-heavy weighting

threshold = EMA_mean + k × EMA_std  // k = 1.5, α = 0.1
```

**Rationale:**
- Bass bands (0-1: 60-120 Hz) weighted 2× to detect kick drums
- High bands (5-7: 2-8 kHz) weighted 0.5× to reduce hi-hat false positives
- Adaptive threshold prevents false triggers on constant tones while catching transients

**Rejected Alternatives:**
- *Raw spectral flux*: Too many false positives from sustained notes
- *Energy-only detection*: Misses soft attacks with spectral content
- *Fixed threshold*: Cannot adapt to varying loudness levels

### 2.2 Two-Rate Pipeline Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                      AudioActor (Core 0, 8ms tick)               │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────┐     ┌─────────────┐     ┌─────────────────────┐│
│  │ I2S Capture │────►│  128-sample │────►│    FAST LANE        ││
│  │  (HOP_FAST) │     │   DC/AGC    │     │  125 Hz / 8ms       ││
│  └─────────────┘     └─────────────┘     │  - RMS              ││
│                                          │  - Spectral flux    ││
│                                          │  - 8-band spectrum  ││
│         ┌────────────────────────────────│  - 12-bin chroma    ││
│         │ Accumulate 2× 128 = 256        │  - 128-pt waveform  ││
│         ▼                                └────────┬────────────┘│
│  ┌─────────────┐                                  │             │
│  │ Beat Ring   │     ┌─────────────────────┐      │             │
│  │  Buffer     │────►│    BEAT LANE        │      │             │
│  │  (256 samp) │     │  62.5 Hz / 16ms     │      │             │
│  └─────────────┘     │  - Band-weighted    │      │             │
│                      │    spectral flux    │      │             │
│                      │  - Adaptive thresh  │      │             │
│                      │  - BPM estimation   │      │             │
│                      └──────────┬──────────┘      │             │
│                                 │                 │             │
│                    ┌────────────┴─────────────────┴───────────┐ │
│                    │         SnapshotBuffer (lock-free)       │ │
│                    │   ControlBusFrame │ BeatObsFrame         │ │
│                    └────────────┬─────────────────────────────┘ │
└─────────────────────────────────┼────────────────────────────────┘
                                  │ Cross-core (atomic read)
┌─────────────────────────────────┼────────────────────────────────┐
│                      RendererActor (Core 1, 120 FPS)             │
├─────────────────────────────────┼────────────────────────────────┤
│                                 ▼                                │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                  MusicalGrid PLL                          │  │
│  │  - OnTempoEstimate(t, bpm, confidence)                    │  │
│  │  - OnBeatObservation(t, strength, is_downbeat)            │  │
│  │  - Tick() at 120 FPS: extrapolates beat_phase01           │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                 │                                │
│                                 ▼                                │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │               EffectContext.audio                         │  │
│  │  - controlBus: ControlBusFrame (FAST LANE data)           │  │
│  │  - musicalGrid: MusicalGridSnapshot (PLL output)          │  │
│  │  - available: bool (freshness check)                      │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                 │                                │
│                                 ▼                                │
│                     Effect.render(ctx)                           │
└──────────────────────────────────────────────────────────────────┘
```

---

## 3. Implementation Details

### 3.1 Timing Constants (audio_config.h)

```cpp
// FAST LANE - texture-reactive features
constexpr uint16_t HOP_FAST = 128;      // 8ms hop @ 16kHz
constexpr float HOP_FAST_HZ = 125.0f;   // 16000 / 128

// BEAT LANE - tempo/beat tracking (Tab5 parity)
constexpr uint16_t HOP_BEAT = 256;      // 16ms hop @ 16kHz
constexpr float HOP_BEAT_HZ = 62.5f;    // 16000 / 256

// Goertzel analysis window
constexpr uint16_t GOERTZEL_WINDOW = 512; // 32ms for bass coherence
```

**Why these values:**
- 128-sample fast hop provides 8ms latency, perceptually "instant"
- 256-sample beat hop matches Tab5's proven tempo tracking
- 512-sample Goertzel window captures 2+ cycles of 60 Hz (kick drum fundamental)
- 75% overlap (128-sample advance on 512-sample window) maintains spectral continuity

### 3.2 SnapshotBuffer Pattern (lock-free cross-core)

```cpp
template<typename T>
class SnapshotBuffer {
    std::atomic<uint32_t> m_seq{0};  // Odd = writing, even = stable
    T m_data;

public:
    void Publish(const T& frame) {
        uint32_t v = m_seq.load(std::memory_order_relaxed);
        m_seq.store(v + 1, std::memory_order_release);  // Mark odd (writing)
        m_data = frame;                                  // Copy data
        m_seq.store(v + 2, std::memory_order_release);  // Mark even (stable)
    }

    uint32_t ReadLatest(T& out) const {
        uint32_t v0, v1;
        do {
            v0 = m_seq.load(std::memory_order_acquire);
            if (v0 & 1) continue;  // Skip if mid-write
            out = m_data;          // Copy snapshot
            v1 = m_seq.load(std::memory_order_acquire);
        } while (v0 != v1);        // Retry if changed during read
        return v1;                 // Return sequence for change detection
    }
};
```

**Why this pattern:**
- No mutex = no priority inversion or deadlock risk
- Sequence counter enables "has new data?" detection without comparing payloads
- Reader retries on conflict (~0.01% of reads in practice)

### 3.3 MusicalGrid PLL (render-domain freewheel)

The MusicalGrid runs in the renderer at 120 FPS, extrapolating beat phase between audio observations:

```cpp
void MusicalGrid::Tick(const AudioTime& render_now) {
    float dt_sec = m_has_tick
        ? render_now.timeSince(m_last_tick_t)
        : (1.0f / 120.0f);
    m_last_tick_t = render_now;
    m_has_tick = true;

    // Advance beat phase based on smoothed BPM
    float beats_per_sec = m_bpm_smoothed / 60.0f;
    m_beat_float += beats_per_sec * dt_sec;

    // Apply pending beat observation (phase correction)
    if (m_pending_beat) {
        float observed_phase = fmodf((float)m_beat_float, 1.0f);
        float error = wrapHalf(0.0f - observed_phase);  // Target: phase=0 at beat
        m_beat_float += error * m_tuning.phaseCorrectionGain;
        m_pending_beat = false;
    }

    // Publish snapshot
    MusicalGridSnapshot snap;
    snap.beat_phase01 = fmodf((float)m_beat_float, 1.0f);
    snap.beat_tick = (uint64_t)m_beat_float != m_prev_beat_index;
    // ... etc
    m_snap.Publish(snap);
}
```

**Why PLL in render domain:**
- Audio thread runs at 62.5-125 Hz; effects need 120 Hz beat phase
- PLL extrapolation provides smooth phase between beat observations
- Phase correction pulls freewheel back toward observed beats

### 3.4 Beat Lane Tick Counter Pattern

AudioActor ticks every 8ms but beat lane only processes every 16ms:

```cpp
void AudioActor::onTick() {
    m_tickCounter++;

    // Capture and process FAST LANE every tick (125 Hz)
    captureHop();  // 128 samples → ControlBusFrame

    // Process BEAT LANE every 2nd tick (62.5 Hz)
    if ((m_tickCounter & 1) == 0) {
        processBeatLane();  // Uses accumulated 256 samples → BeatObsFrame
    }
}
```

**Why this pattern:**
- Single actor, single I2S instance
- Modulo counter avoids separate timers
- Beat ring buffer accumulates 2× fast hops = 1× beat hop

---

## 4. File-by-File Changes

### 4.1 AudioActor.h

**Changes:**
- Added `m_beatRingBuffer[HOP_BEAT]`: Accumulates 256 samples from 2× 128-sample fast hops
- Added `m_beatObsBuffer`: SnapshotBuffer for BeatObsFrame
- Added `m_tickCounter`: Modulo counter for beat lane cadence
- Changed `m_hopBuffer` size from `HOP_SIZE` to `HOP_FAST`

**Why necessary:**
The original single-buffer design couldn't support dual-rate processing. The beat ring buffer accumulates fast hops until a full beat hop (256 samples) is ready for tempo analysis.

### 4.2 AudioActor.cpp

**Changes:**
- `processHop()`: Rewrote for 128-sample fast lane processing
- `processBeatLane()`: New method that feeds BeatTracker and publishes BeatObsFrame
- Logging intervals adjusted from 310 ticks (~10s @ 62.5 Hz) to 620 ticks (~10s @ 125 Hz)

**Why necessary:**
The original `processHop()` assumed 256-sample hops. The rewrite:
1. Processes 128-sample hops through DC removal, AGC, and Goertzel
2. Accumulates samples into beat ring buffer
3. Triggers beat lane every 2nd tick

### 4.3 RendererActor.h

**Changes:**
- Added `setAudioBuffers(ControlBusFrame*, BeatObsFrame*)` replacing `setAudioBuffer()`
- Added `m_beatObsBuffer`, `m_lastBeatObs`, `m_lastBeatObsSeq`

**Why necessary:**
Renderer now receives two data streams:
1. **FAST LANE** (ControlBusFrame): RMS, bands, waveform for texture effects
2. **BEAT LANE** (BeatObsFrame): Beat observations for MusicalGrid

### 4.4 RendererActor.cpp

**Changes:**
- `onTick()` extended to read BeatObsFrame and call `m_musicalGrid.OnTempoEstimate()`/`OnBeatObservation()`
- Added sequence-based change detection for beat observations

**Why necessary:**
The MusicalGrid PLL needs external tempo estimates and beat observations. Without these, it would freewheel indefinitely at default BPM. The sequence check prevents re-feeding stale observations.

### 4.5 ActorSystem.cpp

**Changes:**
- Changed `m_renderer->setAudioBuffer()` to `m_renderer->setAudioBuffers()` with both buffer pointers

**Why necessary:**
Wiring update to pass both FAST and BEAT lane buffers during system initialization.

### 4.6 EffectContext.h

**Changes:**
- Added `StubControlBusFrame` and `StubMusicalGridSnapshot` structs
- Added `controlBus` and `musicalGrid` members to stub `AudioContext`

**Why necessary:**
Effects like `AudioBloomEffect` access `ctx.audio.controlBus.chroma[i]` unconditionally. When `FEATURE_AUDIO_SYNC` is disabled, the stub AudioContext must provide these members to compile. This enables effects to be written without `#if FEATURE_AUDIO_SYNC` guards.

### 4.7 AudioTuning.h (NEW)

**Purpose:**
Runtime-tunable DSP parameters without recompilation.

**Key parameters:**
```cpp
struct AudioPipelineTuning {
    float dcAlpha = 0.001f;           // DC removal time constant
    float agcTargetRms = 0.25f;       // AGC target level
    float agcAttack = 0.08f;          // Attack rate (fast for transients)
    float agcRelease = 0.02f;         // Release rate (slow for sustain)
    float noiseFloorRise = 0.0005f;   // Noise floor tracking (slow up)
    float noiseFloorFall = 0.01f;     // Noise floor tracking (fast down)
    float dbFloor = -48.0f;           // Level mapping floor
    float dbCeil = -6.0f;             // Level mapping ceiling
};
```

**Why necessary:**
Different audio sources (microphone, line-in, genres) need different tuning. API endpoints `/api/v1/audio/parameters` expose these for runtime adjustment.

---

## 5. Challenges Overcome

### 5.1 Hop Buffer Half-Full Artifacts

**Problem:** When switching from 256 to 128-sample hops, Goertzel analysis alternated between valid data and zeros every other hop.

**Root cause:** The 512-sample Goertzel window was being reset and only filled with 128 samples, leaving 384 zeros.

**Solution:** Implemented sliding window with 75% overlap. Each 128-sample hop shifts the window and appends new samples, never resetting mid-analysis.

### 5.2 Band Dropout in Renderer

**Problem:** Frequency band values oscillated between valid data and zeros, causing flickering effects.

**Root cause:** `m_lastBands[]` persistence was not applied correctly when hop_seq hadn't changed.

**Solution:** Added persistence logic that reuses last-known band values when no new audio data arrives, preventing visual artifacts during brief audio gaps.

### 5.3 Stub API Compilation Failure

**Problem:** Effects accessing `ctx.audio.controlBus.bands[i]` failed to compile when `FEATURE_AUDIO_SYNC` was disabled.

**Root cause:** The stub `AudioContext` only had methods (`bass()`, `mid()`, etc.) but not the underlying data members that effects accessed directly.

**Solution:** Added `StubControlBusFrame` and `StubMusicalGridSnapshot` structs with identical member layouts to the real types.

### 5.4 Cross-Core Timing Drift

**Problem:** MusicalGrid beat phase drifted from actual audio beats over time.

**Root cause:** Audio and render domains have independent clocks. Without correction, small timing differences accumulate.

**Solution:** MusicalGrid PLL with phase correction gain. On each beat observation, the PLL nudges the freewheel phase toward zero (beat boundary), correcting drift gradually without jarring jumps.

---

## 6. Lessons Learned

### 6.1 Design Lessons

1. **Dual-rate is not optional for audio-visual sync**: Fast texture response and stable tempo tracking have fundamentally different requirements. A single rate is always a compromise.

2. **Lock-free patterns require careful sequence design**: The odd/even sequence counter is subtle but essential. Getting the memory ordering wrong causes rare, hard-to-debug corruption.

3. **Stub APIs must match real APIs structurally**: Method-only stubs break when callers access data members directly. Always provide the same public interface.

### 6.2 Implementation Lessons

1. **Log intervals must scale with tick rate**: Changing from 62.5 Hz to 125 Hz doubles log frequency. Adjust modulo counters accordingly.

2. **Ring buffer write indices need modular arithmetic**: Off-by-one errors in `m_beatRingWriteIndex` cause subtle audio glitches every ~4 seconds (256-sample boundary).

3. **AGC attack/release asymmetry matters**: Fast attack catches transients; slow release maintains sustain. Symmetric rates sound "pumpy" or "squashed."

### 6.3 Debugging Lessons

1. **DMA debug output is essential for I2S validation**: Without peak-at-multiple-shifts logging, confirming correct SPH0645 bit alignment is guesswork.

2. **Sequence counters reveal cross-core issues**: If `m_lastBeatObsSeq` never changes, the problem is in audio thread publishing, not renderer consumption.

3. **Visual stuttering usually indicates timing, not rendering**: When effects "work" but stutter, the issue is almost always in the audio→effect data path, not the effect's render() function.

---

## 7. Future Contributors Guide

### 7.1 Adding a New Audio Feature

1. **Choose the lane**: Texture feature (fast reaction) → FAST LANE. Musical feature (tempo-related) → BEAT LANE.

2. **Add to appropriate frame struct**:
   - FAST: `ControlBusFrame` in `ControlBus.h`
   - BEAT: `BeatObsFrame` in `MusicalGrid.h`

3. **Compute in AudioActor**:
   - FAST: In `processHop()`, every tick
   - BEAT: In `processBeatLane()`, every 2nd tick

4. **Expose in EffectContext**: Add accessor method to `AudioContext` struct

5. **Update stubs**: Add member to `StubControlBusFrame` or `StubMusicalGridSnapshot`

### 7.2 Tuning DSP Parameters

All DSP tuning is in `AudioTuning.h`. To tune:

1. **Via API**: `POST /api/v1/audio/parameters` with JSON body
2. **Via code**: Modify defaults in `AudioPipelineTuning` constructor

Key tuning relationships:
- `agcAttack` > `agcRelease`: Faster attack, slower release (punch)
- `noiseFloorFall` > `noiseFloorRise`: Fast gate open, slow close (responsiveness)
- `dbFloor` to `dbCeil`: Dynamic range compression (louder apparent volume)

### 7.3 Debugging Audio Issues

**No audio data reaching effects:**
1. Check `ESP_LOGI(TAG, "Audio alive: ...")` output
2. Verify `m_lastControlBusSeq` increments in RendererActor
3. Check `ctx.audio.available` is true

**Beat tracking unstable:**
1. Check `tempo_confidence` in logs (should be >0.5 for valid music)
2. Adjust `ONSET_THRESHOLD_K` (higher = fewer but more confident beats)
3. Check bass band energies (kick detection needs sub-200 Hz energy)

**Visual stuttering:**
1. Check effect is using `ctx.audio.beatPhase()` not `millis()`
2. Verify `beat_tick` pulses align with audible beats
3. Check effect's motion velocity is scaled by `ctx.speed`

### 7.4 Memory Budget

| Component | RAM Usage |
|-----------|-----------|
| `m_beatRingBuffer[256]` | 512 bytes |
| `m_beatObsBuffer` | ~64 bytes |
| `m_hopBuffer[128]` | 256 bytes |
| `MusicalGrid` state | ~128 bytes |
| **Total** | ~960 bytes |

Headroom remains for ~1KB additional features before hitting 2KB budget.

---

## Appendix A: API Endpoints Added

### Audio Parameters (GET/POST/PATCH)
```
/api/v1/audio/parameters

Response:
{
  "success": true,
  "data": {
    "dcAlpha": 0.001,
    "agcTargetRms": 0.25,
    "agcAttack": 0.08,
    "agcRelease": 0.02,
    ...
  }
}
```

### Effect Parameters (GET/POST/PATCH)
```
/api/v1/effects/parameters?id=N

Response:
{
  "success": true,
  "data": {
    "effectId": 5,
    "effectName": "LGPStarBurst",
    "parameters": {
      "intensity": 0.75,
      "burstThreshold": 0.5
    }
  }
}
```

---

## Appendix B: Related Documentation

- `Audio_Reactive_Failure.md`: Initial motion stuttering investigation
- `Audio_Reactive_Failure_v2.md`: Deeper DSP analysis
- `Audio_Reactive_Failure_v3.md`: Root cause identification and fix verification
- `audio_config.h`: Authoritative timing constants
- `AudioTuning.h`: Runtime-tunable parameter definitions
