## 1. AUDIO CAPTURE SOURCE - COMPLETE SPECIFICATION

 ### 1.1 Hardware Configuration

 **Primary Configuration File**:
 `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/v2/src/config/audio_config.h`

 | Parameter | Value | File Location (Line) |
 |-----------|-------|----------------------|
 | Microphone | SPH0645 MEMS I2S | Lines 36-39 |
 | Sample Rate | 16,000 Hz | Line 53 |
 | Bit Depth | 32-bit I2S slots (18-bit actual data) | Lines 95, 80-93 |
 | Hop Size | 256 samples | Line 54 |
 | DMA Buffer Count | 4 | Line 72 |
 | DMA Buffer Samples | 512 | Line 73 |
 | I2S Port | I2S_NUM_0 | AudioCapture.h:171 |

 ### 1.2 I2S Pin Configuration

 | Pin | GPIO | Function |
 |-----|------|----------|
 | BCLK | GPIO14 | Bit Clock |
 | WS (LRCL) | GPIO12 | Word Select |
 | DIN (DOUT) | GPIO13 | Data In from mic |

 **Evidence**: AudioCapture.cpp Lines 309-315

 ### 1.3 SPH0645 Sample Conversion Chain

 **File**: `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/v2/src/audio/AudioCapture.cpp`

 ```
 Raw 32-bit I2S DMA Sample
         |
         v
 Shift >> 14 to extract 18-bit signed value    (Line 237)
         |
         v
 Add DC_BIAS_ADD (+7000) for bias correction   (Line 238)
         |
         v
 Clip to 18-bit range [-131072, +131071]       (Lines 239-240)
         |
         v
 Subtract DC_BIAS_SUB (-360)                   (Line 241)
         |
         v
 Scale to 16-bit (>> 2)                        (Line 242)
         |
         v
 Output: int16_t sample [-32768, 32767]
 ```

 ### 1.4 ESP32-S3 Register Fixes (Critical for SPH0645)

 **File**: AudioCapture.cpp Lines 289-302

 ```cpp
 REG_SET_BIT(I2S_RX_TIMING_REG(I2S_PORT), BIT(9));      // Delay sampling by 1 BCLK
 REG_SET_BIT(I2S_RX_CONF_REG(I2S_PORT), I2S_RX_MSB_SHIFT);  // Enable MSB shift
 REG_SET_BIT(I2S_RX_CONF_REG(I2S_PORT), I2S_RX_WS_IDLE_POL); // WS polarity inversion
 ```

 ---

 ## 2. CONTROLBUS STRUCTURE - COMPLETE SPECIFICATION

 ### 2.1 File Locations

 | File | Purpose |
 |------|---------|
 | `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/v2/src/audio/contracts/ControlBus.h` |
 Structure definitions |
 | `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/v2/src/audio/contracts/ControlBus.cpp` |
 Smoothing implementation |

 ### 2.2 Constants (ControlBus.h Lines 8-10)

 ```cpp
 CONTROLBUS_NUM_BANDS  = 8    // Frequency bands (60-7800 Hz)
 CONTROLBUS_NUM_CHROMA = 12   // Pitch classes (C, C#, D, ... B)
 CONTROLBUS_WAVEFORM_N = 128  // Time-domain sample points
 ```

 ### 2.3 ControlBusRawInput Structure (Lines 15-22)

 | Field | Type | Range | Description |
 |-------|------|-------|-------------|
 | `rms` | float | [0.0, 1.0] | RMS energy level |
 | `flux` | float | [0.0, 1.0] | Spectral flux / novelty |
 | `bands[8]` | float[8] | [0.0, 1.0] | 8 frequency band energies |
 | `chroma[12]` | float[12] | [0.0, 1.0] | 12 pitch class energies |
 | `waveform[128]` | int16_t[128] | [-32768, 32767] | Time-domain samples |

 ### 2.4 ControlBusFrame Structure (Lines 27-37)

 | Field | Type | Description |
 |-------|------|-------------|
 | `t` | AudioTime | Timestamp (sample_index, sample_rate, monotonic_us) |
 | `hop_seq` | uint32_t | **Monotonic hop counter - increments ONCE per 16ms audio hop** |
 | `rms` | float | Smoothed RMS [0.0-1.0] |
 | `flux` | float | Smoothed spectral flux [0.0-1.0] |
 | `bands[8]` | float[8] | Smoothed 8 frequency bands [0.0-1.0] |
 | `chroma[12]` | float[12] | Smoothed 12 pitch classes [0.0-1.0] |
 | `waveform[128]` | int16_t[128] | Time-domain waveform (unsmoothed) |

 ### 2.5 hop_seq - The Change Detection Mechanism

 **Definition**: A monotonically increasing counter that increments ONCE per audio processing hop.

 **Increment Location**: ControlBus.cpp Line 37
 ```cpp
 void ControlBus::UpdateFromHop(const AudioTime& now, const ControlBusRawInput& raw) {
     m_frame.t = now;
     m_frame.hop_seq++;  // <-- HERE
     ...
 }
 ```

 **Rate**: 62.5 Hz (every 16 ms when audio is processing)

 **Usage by Effects**: Effects compare `ctx.audio.controlBus.hop_seq` against their stored `m_lastHopSeq` to detect
 new audio data:
 ```cpp
 // From AudioWaveformEffect.cpp Lines 43-46
 bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
 if (newHop) {
     m_lastHopSeq = ctx.audio.controlBus.hop_seq;
     // Process new audio data...
 }
 ```

 ### 2.6 Smoothing Parameters (ControlBus.h Lines 70-71)

 ```cpp
 m_alpha_fast = 0.35f  // Fast response for RMS
 m_alpha_slow = 0.12f  // Slower response for flux, bands, chroma
 ```

 **Smoothing Formula** (ControlBus.cpp Lines 39-57):
 ```cpp
 m_rms_s = lerp(m_rms_s, clamp01(raw.rms), m_alpha_fast);        // Fast
 m_flux_s = lerp(m_flux_s, clamp01(raw.flux), m_alpha_slow);     // Slow
 m_bands_s[i] = lerp(m_bands_s[i], clamp01(raw.bands[i]), m_alpha_slow);  // Slow
 m_chroma_s[i] = lerp(m_chroma_s[i], clamp01(raw.chroma[i]), m_alpha_slow); // Slow
 // Waveform: copied directly (no smoothing) - Line 59-63
 ```

 ---

 ## 3. AUDIO PROCESSING PIPELINE - COMPLETE TRACE

 ### 3.1 AudioActor Overview

 **Files**:
 - `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/v2/src/audio/AudioActor.h`
 - `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/v2/src/audio/AudioActor.cpp`

 **Actor Configuration** (AudioActor.h Lines 406-415):
 - **Name**: "Audio"
 - **Core**: 0
 - **Priority**: 4 (below Renderer at 5)
 - **Tick Interval**: 16 ms (62.5 Hz)
 - **Stack Size**: AUDIO_ACTOR_STACK_WORDS (16KB)

 ### 3.2 Processing Flow (AudioActor.cpp `processHop()` Lines 280-541)

 ```
 AUDIO PROCESSING PIPELINE (Every 16ms hop)
 ==========================================

 STEP 1: BUILD AUDIOTIME (Lines 311-316)
    - AudioTime(sample_index, SAMPLE_RATE=16000, monotonic_us)
    - m_sampleIndex += 256 (HOP_SIZE)
    - m_hopCount++

 STEP 2: DC REMOVAL (Lines 350-354)
    - m_dcEstimate += dcAlpha * (x - m_dcEstimate)
    - dcAlpha default = 0.001

 STEP 3: AGC (Automatic Gain Control) (Lines 402-415)
    - Target RMS: 0.25
    - Min/Max Gain: 1.0 / 100.0
    - Attack: 0.08, Release: 0.02
    - Clip detection reduces gain by 0.90

 STEP 4: RMS CALCULATION (Lines 417-421)
    - computeRMS(m_hopBufferCentered, 256)
    - dB mapping: floor=-65dB, ceil=-12dB

 STEP 5: SPECTRAL FLUX (Lines 423-427)
    - flux = max(0, rmsMapped - prevRMS) * fluxScale

 STEP 6: GOERTZEL BAND ANALYSIS (Lines 450-516)
    - Window: 512 samples (2 hops accumulated)
    - Target Frequencies: 60, 120, 250, 500, 1000, 2000, 4000, 7800 Hz
    - Updates every 2 hops (~32ms)
    - Persists last valid bands between updates

 STEP 7: CHROMAGRAM ANALYSIS (Lines 518-533)
    - Window: 512 samples (2 hops accumulated)
    - 48 note frequencies across 4 octaves
    - Folded into 12 pitch classes
    - Updates every 2 hops (~32ms)

 STEP 8: WAVEFORM DOWNSAMPLING (Lines 461-482)
    - 256 samples -> 128 points (peak of each pair)

 STEP 9: CONTROLBUS UPDATE (Lines 535-540)
    - m_controlBus.UpdateFromHop(now, raw)
    - Applies smoothing (alpha_fast, alpha_slow)
    - hop_seq incremented here

 STEP 10: PUBLISH TO SNAPSHOTBUFFER (Line 540)
    - m_controlBusBuffer.Publish(m_controlBus.GetFrame())
    - Lock-free cross-core transfer
 ```

 ### 3.3 Goertzel Analyzer Details

 **File**: `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/v2/src/audio/GoertzelAnalyzer.h`

 | Parameter | Value | Line |
 |-----------|-------|------|
 | Window Size | 512 samples | Line 29 |
 | Number of Bands | 8 | Line 30 |
 | Sample Rate | 16,000 Hz | Line 31 |
 | Target Frequencies | 60, 120, 250, 500, 1000, 2000, 4000, 7800 Hz | Lines 66-68 |

 **Algorithm** (GoertzelAnalyzer.cpp Lines 56-78):
 ```cpp
 // Goertzel recursion
 s0 = sample + coeff * s1 - s2;
 s2 = s1;
 s1 = s0;

 // Magnitude computation
 magnitude = sqrt(s1*s1 + s2*s2 - coeff*s1*s2);
 ```

 ### 3.4 Chromagram Analyzer Details

 **File**: `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/v2/src/audio/ChromaAnalyzer.h`

 | Parameter | Value | Line |
 |-----------|-------|------|
 | Window Size | 512 samples | Line 31 |
 | Pitch Classes | 12 | Line 32 |
 | Octaves Analyzed | 4 (C2-B5) | Line 33 |
 | Total Note Frequencies | 48 (4 x 12) | Lines 70-79 |

 **Note Frequency Ranges**:
 - Octave 2: 65.41 Hz (C2) - 123.47 Hz (B2)
 - Octave 3: 130.81 Hz (C3) - 246.94 Hz (B3)
 - Octave 4: 261.63 Hz (C4) - 493.88 Hz (B4)
 - Octave 5: 523.25 Hz (C5) - 987.77 Hz (B5)

 ---

 ## 4. DATA DELIVERY TO EFFECTS - COMPLETE TRACE

 ### 4.1 Cross-Core Architecture

 ```
          CORE 0                              CORE 1
     ┌───────────────┐                   ┌───────────────┐
     │  AudioActor   │                   │RendererActor  │
     │  Priority: 4  │                   │  Priority: 5  │
     │  Tick: 16ms   │                   │  Tick: ~8ms   │
     │   (62.5 Hz)   │                   │   (120 FPS)   │
     └───────┬───────┘                   └───────┬───────┘
             │                                   │
             │  SnapshotBuffer<ControlBusFrame>  │
             │  (Lock-free double buffer)        │
             │───────────────────────────────────>│
             │                                   │
             │                                   ▼
             │                         ┌───────────────────┐
             │                         │ EffectContext.audio │
             │                         │ - controlBus      │
             │                         │ - musicalGrid     │
             │                         │ - available       │
             │                         └───────────────────┘
             │                                   │
             │                                   ▼
             │                         ┌───────────────────┐
             │                         │  IEffect::render() │
             │                         │  (e.g., AudioBloom)│
             │                         └───────────────────┘
 ```

 ### 4.2 SnapshotBuffer (Lock-Free Cross-Core Communication)

 **File**: 
 `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/v2/src/audio/contracts/SnapshotBuffer.h`

 **Implementation**: Double-buffer with atomic sequence counter

 ```cpp
 // Publisher (AudioActor - Core 0) - Line 27-38
 void Publish(const T& v) {
     const uint32_t cur = m_active.load(std::memory_order_relaxed);
     const uint32_t nxt = cur ^ 1U;
     m_buf[nxt] = v;
     std::atomic_thread_fence(std::memory_order_release);
     m_active.store(nxt, std::memory_order_release);
     m_seq.fetch_add(1U, std::memory_order_release);
 }

 // Consumer (RendererActor - Core 1) - Lines 45-61
 uint32_t ReadLatest(T& out) const {
     uint32_t s0 = m_seq.load(std::memory_order_acquire);
     uint32_t idx = m_active.load(std::memory_order_acquire);
     out = m_buf[idx];  // BY-VALUE copy for thread safety
     // Retry once if seq changed during copy
     ...
     return s1;
 }
 ```

 ### 4.3 RendererActor Audio Integration

 **File**: 
 `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/v2/src/core/actors/RendererActor.cpp`
 **Lines 710-756**

 ```cpp
 // 1. Read latest ControlBusFrame BY VALUE (thread-safe)
 uint32_t seq = m_controlBusBuffer->ReadLatest(m_lastControlBus);

 // 2. Check if new frame arrived
 if (seq != m_lastControlBusSeq) {
     // Resync extrapolation base
     m_lastAudioTime = m_lastControlBus.t;
     m_lastAudioMicros = now_us;
     m_lastControlBusSeq = seq;
 }

 // 3. Build extrapolated render-time AudioTime
 // Render runs at 120 FPS, audio at 62.5 Hz - extrapolate samples
 uint64_t dt_us = now_us - m_lastAudioMicros;
 uint64_t extrapolated_samples = m_lastAudioTime.sample_index +
     (dt_us * m_lastAudioTime.sample_rate_hz / 1000000);

 // 4. Tick MusicalGrid at 120 FPS (PLL freewheel pattern)
 m_musicalGrid.Tick(render_now);
 m_musicalGrid.ReadLatest(m_lastMusicalGrid);

 // 5. Compute staleness for ctx.audio.available
 float age_s = AudioTime_SecondsBetween(m_lastControlBus.t, render_now);
 float staleness_s = m_audioContractTuning.audioStalenessMs / 1000.0f;  // 100ms default
 bool is_fresh = (age_s >= 0.0f && age_s < staleness_s);

 // 6. Populate AudioContext (by-value copies)
 ctx.audio.controlBus = m_lastControlBus;
 ctx.audio.musicalGrid = m_lastMusicalGrid;
 ctx.audio.available = is_fresh;
 ```

 ### 4.4 AudioContext Structure

 **File**: `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/v2/src/plugins/api/EffectContext.h`
 **Lines 61-137**

 | Field | Type | Access |
 |-------|------|--------|
 | `controlBus` | ControlBusFrame | Direct field |
 | `musicalGrid` | MusicalGridSnapshot | Direct field |
 | `available` | bool | True if audio < 100ms old |

 **Convenience Methods**:
 ```cpp
 float rms()                    // RMS energy [0.0-1.0]
 float flux()                   // Spectral flux [0.0-1.0]
 float getBand(uint8_t i)       // Band energy [0.0-1.0]
 float bass()                   // Bands 0-1 averaged
 float mid()                    // Bands 2-4 averaged
 float treble()                 // Bands 5-7 averaged
 float beatPhase()              // Beat phase [0.0-1.0]
 bool isOnBeat()                // True on beat boundary
 bool isOnDownbeat()            // True on bar boundary
 float bpm()                    // Current BPM estimate
 float tempoConfidence()        // [0.0-1.0]
 int16_t getWaveformSample(i)   // Raw waveform sample
 float getWaveformAmplitude(i)  // Normalized [0.0-1.0]
 float getWaveformNormalized(i) // Signed normalized [-1.0, 1.0]
 ```

 ---

 ## 5. TIMING ANALYSIS - COMPLETE SPECIFICATION

 ### 5.1 Critical Timing Parameters

 | Stage | Rate | Interval | Location |
 |-------|------|----------|----------|
 | Audio Capture | 16,000 Hz | 62.5 us/sample | audio_config.h:53 |
 | Audio Hop | 62.5 Hz | 16 ms | audio_config.h:59-60 |
 | Goertzel Analysis | 31.25 Hz | 32 ms | GoertzelAnalyzer.h:29 |
 | Chroma Analysis | 31.25 Hz | 32 ms | ChromaAnalyzer.h:31 |
 | MusicalGrid Tick | 120 Hz | ~8.3 ms | RendererActor.cpp:737 |
 | Effect Render | 120 Hz | ~8.3 ms | IEffect::render() |
 | Staleness Threshold | N/A | 100 ms | audio_config.h:127 |

 ### 5.2 Timing Flow Diagram

 ```
 Time (ms)  0    16    32    48    64    80    96
            |     |     |     |     |     |     |
 Audio:     H1----H2----H3----H4----H5----H6----
            |     |     |     |     |     |
 Goertzel:  .....G1.....G2.....G3.....G4.....
            |     |     |     |     |     |
 Chroma:    .....C1.....C2.....C3.....C4.....
            |     |     |     |     |     |
 Render:    RRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRR
            (~12 render frames per audio hop)

 H = Hop (256 samples captured, ControlBus published)
 G = Goertzel analysis (every 2 hops when 512-sample window full)
 C = Chroma analysis (every 2 hops when 512-sample window full)
 R = Render frame (IEffect::render() called at ~120 FPS)
 ```

 ### 5.3 Latency Analysis

 | Path | Latency | Notes |
 |------|---------|-------|
 | I2S DMA to AudioCapture | < 1 ms | DMA buffer fill |
 | AudioCapture to ControlBus | ~0.5 ms | processHop() |
 | ControlBus to SnapshotBuffer | < 0.1 ms | Atomic publish |
 | SnapshotBuffer to RendererActor | < 0.1 ms | ReadLatest() |
 | **Total Audio-to-Effect (typical)** | **~1-2 ms** | Best case |
 | **Worst Case (Goertzel stale)** | **~32 ms** | Waiting for 2 hops |

 ### 5.4 Band/Chroma Persistence Logic

 **Problem**: Goertzel/Chroma only update every 2 hops (32ms), but effects render at 120 FPS (8.3ms). Without
 persistence, bands would be 0 every other hop causing "picket fence" flickering.

 **Solution** (AudioActor.cpp Lines 510-516, 528-532):
 ```cpp
 // When Goertzel completes (every 2 hops)
 if (m_analyzer.analyze(bandsRaw)) {
     for (int i = 0; i < NUM_BANDS; ++i) {
         m_lastBands[i] = band;  // Persist unscaled value
         raw.bands[i] = band * activity;
     }
 } else {
     // No new analysis - reuse last known bands
     for (int i = 0; i < NUM_BANDS; ++i) {
         raw.bands[i] = m_lastBands[i] * activity;  // Apply activity gate
     }
 }
 ```

 ---

 ## 6. EFFECT AUDIO CONSUMPTION EXAMPLES

 ### 6.1 AudioBloomEffect

 **File**:
 `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/v2/src/effects/ieffect/AudioBloomEffect.cpp`

 **New Hop Detection** (Lines 40-44):
 ```cpp
 bool newHop = (ctx.audio.controlBus.hop_seq != m_lastHopSeq);
 if (newHop) {
     m_lastHopSeq = ctx.audio.controlBus.hop_seq;
     m_iter++;
 }
 ```

 **Chroma Usage** (Lines 54-76):
 ```cpp
 for (uint8_t i = 0; i < 12; ++i) {
     float bin = ctx.audio.controlBus.chroma[i];
     float bright = bin * bin * 1.5f;  // Square and boost
     ...
 }
 ```

 ### 6.2 AudioWaveformEffect

 **File**: `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/v2/src/effects/ieffect/AudioWaveform
 Effect.cpp`

 **Waveform Sample Access** (Lines 50-52):
 ```cpp
 for (uint8_t i = 0; i < ctx.audio.waveformSize(); ++i) {
     int16_t sample = ctx.audio.getWaveformSample(i);
     ...
 }
 ```

 **Chroma Color Mapping** (Lines 92-105):
 ```cpp
 for (uint8_t c = 0; c < 12; ++c) {
     float chromaVal = ctx.audio.controlBus.chroma[c];
     if (chromaVal > maxChroma) {
         maxChroma = chromaVal;
         dominantPitch = c;
     }
 }
 ```

 ---

 ## 7. TUNING PARAMETERS

 ### 7.1 AudioPipelineTuning

 **File**: `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/v2/src/audio/AudioTuning.h`
 **Lines 16-46**

 | Parameter | Default | Range | Description |
 |-----------|---------|-------|-------------|
 | `dcAlpha` | 0.001 | [0.000001, 0.1] | DC removal time constant |
 | `agcTargetRms` | 0.25 | [0.01, 1.0] | AGC target RMS level |
 | `agcMinGain` | 1.0 | [0.1, 50] | Minimum AGC gain |
 | `agcMaxGain` | 100.0 | [1.0, 500] | Maximum AGC gain |
 | `agcAttack` | 0.08 | [0.0, 1.0] | AGC attack rate |
 | `agcRelease` | 0.02 | [0.0, 1.0] | AGC release rate |
 | `rmsDbFloor` | -65.0 | [-120, 0] | RMS dB floor |
 | `rmsDbCeil` | -12.0 | [-120, 0] | RMS dB ceiling |
 | `controlBusAlphaFast` | 0.35 | [0.0, 1.0] | Fast smoothing |
 | `controlBusAlphaSlow` | 0.12 | [0.0, 1.0] | Slow smoothing |

 ### 7.2 AudioContractTuning

 **Lines 48-60**

 | Parameter | Default | Range | Description |
 |-----------|---------|-------|-------------|
 | `audioStalenessMs` | 100.0 | [10, 1000] | Staleness threshold |
 | `bpmMin` | 30.0 | [20, 200] | Min BPM for tempo detection |
 | `bpmMax` | 300.0 | [60, 400] | Max BPM for tempo detection |
 | `beatsPerBar` | 4 | [1, 12] | Time signature numerator |
 | `beatUnit` | 4 | [1, 16] | Time signature denominator |

 ---

 ## 8. VERIFICATION EVIDENCE

 ### 8.1 Cross-Reference Validation

 **HOP_SIZE Consistency**:
 - audio_config.h:54 = 256 VERIFIED
 - AudioCapture.h:178 uses HOP_SIZE VERIFIED
 - AudioActor.cpp:315 uses HOP_SIZE VERIFIED
 - AudioActor.cpp:451 uses HOP_SIZE VERIFIED

 **SAMPLE_RATE Consistency**:
 - audio_config.h:53 = 16000 VERIFIED
 - GoertzelAnalyzer.h:31 = 16000 VERIFIED
 - ChromaAnalyzer.h:34 = 16000 VERIFIED

 **WINDOW_SIZE Consistency**:
 - GoertzelAnalyzer.h:29 = 512 VERIFIED
 - ChromaAnalyzer.h:31 = 512 VERIFIED

 ### 8.2 Key Evidence Trail

 | Finding | Evidence Location | Verified |
 |---------|-------------------|----------|
 | SPH0645 MEMS mic | AudioCapture.cpp:69 | YES |
 | 16 kHz sample rate | audio_config.h:53 | YES |
 | 256-sample hop | audio_config.h:54 | YES |
 | 16 ms hop duration | Calculated: 256/16000 = 0.016s | YES |
 | 62.5 Hz hop rate | audio_config.h:60 | YES |
 | 8 frequency bands | ControlBus.h:8 | YES |
 | 12 chroma bins | ControlBus.h:9 | YES |
 | 128-point waveform | ControlBus.h:10 | YES |
 | hop_seq increments per hop | ControlBus.cpp:37 | YES |
 | Lock-free cross-core | SnapshotBuffer.h:26-61 | YES |
 | 120 FPS render target | RendererActor.h:77 | YES |
 | 100 ms staleness threshold | audio_config.h:127 | YES |
 | Band persistence | AudioActor.cpp:510-516 | YES |

 ---

 ## 9. SUMMARY - KEY CONSTANTS

 ```
 AUDIO CAPTURE
   Sample Rate:        16,000 Hz
   Bit Depth:          18-bit (in 32-bit I2S slot)
   I2S Port:           I2S_NUM_0

 AUDIO PROCESSING
   Hop Size:           256 samples
   Hop Duration:       16 ms
   Hop Rate:           62.5 Hz
   Goertzel Window:    512 samples (32 ms, every 2 hops)
   Frequency Bands:    8 (60, 120, 250, 500, 1000, 2000, 4000, 7800 Hz)
   Chroma Bins:        12 (C, C#, D, D#, E, F, F#, G, G#, A, A#, B)
   Octaves:            4 (C2-B5, 65.41-987.77 Hz)
   Waveform Points:    128

 RENDERING
   Target FPS:         120
   Frame Time:         ~8.33 ms
   Staleness:          100 ms

 SMOOTHING
   RMS Alpha (fast):   0.35
   Bands Alpha (slow): 0.12
   Chroma Alpha:       0.12
 ```

 ---

 ## VERIFICATION STATUS: VERIFIED

 All findings are based on direct code examination with exact file paths and line number references. The audio 
 pipeline is well-architected with:

 1. **Clean separation of concerns** - Capture, Processing, and Delivery are isolated
 2. **Thread-safe cross-core communication** - SnapshotBuffer double-buffer pattern
 3. **Proper timing hierarchy** - Audio at 62.5 Hz, Render at 120 FPS with extrapolation
 4. **Staleness detection** - Effects gracefully degrade when audio is stale
 5. **Band/Chroma persistence** - Prevents "picket fence" dropouts between Goertzel updates
 6. **hop_seq change detection** - Effects can detect new audio vs stale data