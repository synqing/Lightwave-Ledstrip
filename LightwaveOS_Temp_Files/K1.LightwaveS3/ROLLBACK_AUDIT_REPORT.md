# V2 Firmware Rollback Audit Report

**Comparison:** `0c9cac5` (before rollback) → `db47998` (after rollback)  
**Date:** 2025-01-XX  
**Purpose:** Track down bugs/crashes caused by rollback

## Executive Summary

The rollback removed the **Emotiscope 2.0 TempoEngine** implementation and restored the original **TempoTracker** (Goertzel-based). This was a major architectural change affecting beat tracking, audio processing, and actor system timing.

---

## 1. Beat Tracking System (MAJOR CHANGE)

### 1.1 Removed Components

**Files Deleted:**
- `firmware/v2/src/audio/tempo/TempoEngine.h` (303 lines)
- `firmware/v2/src/audio/tempo/TempoEngine.cpp` (530 lines)

**Key Features Removed:**
- FFT-based novelty calculation (128-bin spectrum)
- Emotiscope 2.0 parity beat tracking algorithm
- 50Hz novelty update rate (TEMPO_UPDATE_INTERVAL_US = 20000us)
- FFT analyzer integration (`FFTAnalyzer` class)
- Sample history buffers (4096-sample full-rate, 2048-sample half-rate)
- 64-bin Goertzel analysis for beat tracking (separate from visual effects)

### 1.2 Restored Components

**TempoTracker (Original K1 Implementation):**
- Goertzel-based 8-band spectral flux
- VU derivative from RMS
- Dual-rate novelty input (31.25 Hz from Goertzel, 62.5 Hz from RMS)
- Phase advancement in AudioNode (not RendererNode)

**Key Differences:**
- **Before (TempoEngine):** FFT 256 → 128-bin spectrum, 50Hz novelty updates, phase advanced in RendererNode at 120 FPS
- **After (TempoTracker):** 8-band Goertzel + RMS derivative, novelty updates per hop (~62.5 Hz), phase advanced in AudioNode per hop

---

## 2. Audio Processing Pipeline Changes

### 2.1 AudioNode.cpp Changes

**Removed:**
- FFT analyzer initialization and history buffers
- `m_fftAnalyzer` member usage
- `m_sampleHistory` and `m_sampleHistoryHalfRate` buffers
- `m_fftSpectrumCached` buffer
- `m_sampleHistoryWriteIndex`, `m_sampleHistoryHalfRateWriteIndex`, `m_sampleHistoryHalfRateAvailable` tracking
- `m_fftSpectrumReady` flag
- `m_lastTempoUpdateUs` timing for 50Hz throttle
- `TEMPO_UPDATE_INTERVAL_US` constant usage
- 64-bin Goertzel analysis (`m_analyzerBeatTracking`)
- Pre-AGC audio buffer (`m_hopBufferPreAgc`) - separate path for beat tracking
- TempoEngine update logic with FFT spectrum input

**Restored:**
- `TempoTracker m_tempo` member
- `m_tempo.init()` in `onStart()`
- `m_tempo.advancePhase(delta_sec)` in `onTick()` (per hop)
- `m_tempo.updateNovelty()` and `m_tempo.updateTempo()` in `processHop()`
- Single audio path (no pre-AGC separation)

**Added (Watchdog Fixes):**
- Multiple `taskYIELD()` calls:
  - After `captureHop()` (line ~352)
  - After Goertzel analysis (line ~737)
  - After Chroma analysis (line ~837)
  - After ControlBus update (line ~872)
  - At end of `onTick()` (line ~256)

### 2.2 AudioNode.h Changes

**Removed Members:**
- `TempoEngine m_tempoEngine`
- `FFTAnalyzer m_fftAnalyzer`
- `float m_sampleHistory[SAMPLE_HISTORY_LENGTH]`
- `float m_sampleHistoryHalfRate[SAMPLE_HISTORY_LENGTH]`
- `float m_fftSpectrumCached[128]`
- `uint16_t m_sampleHistoryWriteIndex`
- `uint16_t m_sampleHistoryHalfRateWriteIndex`
- `uint16_t m_sampleHistoryHalfRateAvailable`
- `bool m_fftSpectrumReady`
- `uint64_t m_lastTempoUpdateUs`
- `int16_t m_hopBufferPreAgc[HOP_SIZE]`
- `GoertzelAnalyzer m_analyzerBeatTracking`
- `float m_bins64Raw[GoertzelAnalyzer::NUM_BINS]`
- `float m_bands64Folded[8]`
- `float m_lastBands64[8]`
- `bool m_analyze64Ready`
- `uint32_t m_goertzel64LogCounter`

**Restored Members:**
- `TempoTracker m_tempo`
- `TempoOutput m_lastTempoOutput`

**Removed Methods:**
- `TempoEngine& getTempoEngineMut()`
- `TempoEngine* getTempoEngine()`

**Restored Methods:**
- `TempoOutput getTempo() const` (now returns `m_lastTempoOutput` directly)

### 2.3 AudioCapture.cpp Changes

**Added:**
- `#include <cstdio>` (minor, likely for logging)

---

## 3. Actor System Changes

### 3.1 Node.cpp (Queue Saturation Prevention)

**Added:**
- `m_nextTick` member variable (TickType_t) for tick deadline tracking
- Queue saturation prevention logic:
  - `DRAIN_THRESHOLD = 50%` (starts draining at 50% full)
  - `MAX_MESSAGES_PER_TICK = 8` (processes up to 8 messages per tick when queue is saturated)
  - Non-blocking message drain loop when queue utilization > 50%

**Changed Tick Timing:**
- **Before:** Simple timeout-based tick (wait for message with timeout = tickInterval, call onTick on timeout)
- **After:** Deadline-based tick (check if tick is due, run immediately, then process messages)

**Potential Issue:** The new tick timing pattern may have broken RendererNode's 120 FPS rendering if tick scheduling is incorrect.

### 3.2 Node.h Changes

**Added:**
- `TickType_t m_nextTick` member variable

---

## 4. Renderer Integration Changes

### 4.1 RendererNode.cpp Changes

**Removed:**
- `#include "../../audio/tempo/TempoEngine.h"`
- TempoEngine phase advancement in `renderFrame()`:
  - `m_tempo->advancePhase(deltaSec)` call
  - `m_tempo->getOutput()` call
  - MusicalGrid tempo feeding from TempoEngine
- ~40 lines of tempo integration code

**Restored:**
- MusicalGrid tempo feeding from `ControlBus` snapshot:
  - Reads `m_lastControlBus.tempo` (thread-safe snapshot)
  - Feeds `OnTempoEstimate()` and `OnBeatObservation()` from snapshot
  - Moved inside the `if (seq != m_lastControlBusSeq)` block (only on new audio frame)

**Key Difference:**
- **Before:** TempoEngine phase advanced at 120 FPS in RendererNode, beat ticks detected in render thread
- **After:** TempoTracker phase advanced per hop in AudioNode, beat ticks read from ControlBus snapshot

### 4.2 RendererNode.h Changes

**Removed:**
- `#include "../../audio/tempo/TempoEngine.h"`
- `lightwaveos::audio::TempoEngine* m_tempo` member
- `setTempo(TempoEngine*)` method
- `isTempoEnabled()` method
- `getTempoOutput()` method
- ~44 lines of TempoEngine integration code

---

## 5. Zone Composer Changes

### 5.1 ZoneComposer.cpp Changes

**Removed:**
- `#include "../../audio/tempo/TempoEngine.h"`
- Beat trigger processing:
  - `m_tempoEngine->getOutput()` calls
  - Beat tick edge detection (`currentBeatTick && !m_lastBeatTick`)
  - `processBeatTrigger()` calls for all zones
- Tempo-based speed scaling:
  - BPM factor calculation (`tempoOut.bpm / 120.0f`)
  - Speed modulation based on tempo
- Beat-based brightness modulation:
  - Beat envelope calculation from `tempoOut.beat_strength`
  - Brightness modulation scaling

**Restored:**
- Empty tempo sync block (no-op when `zone.audio.tempoSync` is true)
- `m_lastBeatTick = false` (beat tracking disabled)

### 5.2 ZoneComposer.h Changes

**Removed:**
- Forward declaration: `namespace lightwaveos { namespace audio { class TempoEngine; } }`
- `audio::TempoEngine* m_tempoEngine` member
- `setTempoEngine(TempoEngine*)` method

---

## 6. Node Orchestrator Changes

### 6.1 NodeOrchestrator.cpp Changes

**Removed:**
- `#include "../../audio/tempo/TempoEngine.h"`
- `m_renderer->setTempo(&m_audio->getTempoEngineMut())` call
- `zoneComposer->setTempoEngine(&m_audio->getTempoEngineMut())` call
- Log message: "Audio integration enabled - ControlBus + TempoEngine"

**Restored:**
- Comment: "TempoTracker is integrated in AudioNode (no external wiring needed)"
- Comment: "ZoneComposer (no tempo engine wiring needed)"
- Log message: "Audio integration enabled - ControlBus"

---

## 7. WebSocket/Network Changes

### 7.1 WsZonesCommands.cpp Changes

**Added (Tab5 Compatibility):**
- Field name compatibility: `zone.setBrightness` now accepts both `"brightness"` and `"value"` fields
- Line 97: `uint8_t brightness = doc["brightness"] | doc["value"] | 128;`

**Purpose:** Fixes Tab5 encoder sending `{"type": "zone.setBrightness", "zoneId": X, "value": Y}` instead of `{"brightness": Y}`

---

## 8. Main.cpp Changes

### 8.1 Serial Command Changes

**Removed:**
- `#include "audio/tempo/TempoEngine.h"` (if present)

**Changed:**
- Serial command `tempo` output:
  - Label changed from "TempoEngine Status" to "TempoTracker Status"
  - Error message changed from "TempoEngine not available" to "TempoTracker not available"

---

## 9. Configuration Changes

### 9.1 features.h Changes

**Changed:**
- Comment: "K1 beat tracker has been replaced by TempoEngine" → "K1 beat tracker has been replaced by TempoEngine"
- Note: Comment still says "TempoEngine" but code uses TempoTracker (inconsistency)

### 9.2 platformio.ini Changes

**Added:**
- Comment about ESP-DSP for SIMD-accelerated DSP operations (matching Emotiscope 2.0)
- Note: ESP-DSP is an ESP-IDF component, available via framework

---

## 10. Documentation Changes

### 10.1 LEGACY_COMPATIBILITY_INVENTORY.md

**Changed:**
- Build target names updated:
  - `esp32dev` → `esp32dev_audio` (default)
  - `esp32dev_wifi` → removed
  - Added `esp32dev_audio_benchmark` and `esp32dev_audio_trace`

### 10.2 OTA_UPDATE_GUIDE.md

**Changed:**
- Build environment table simplified:
  - Removed `esp32dev` and `esp32dev_wifi` rows
  - Updated `esp32dev_audio` description to "Standard v2 build (web + OTA + audio)"

---

## 11. Potential Issues from Rollback

### 11.1 Critical Issues

1. **Node.cpp Tick Timing Change:**
   - **Risk:** RendererNode may not maintain 120 FPS if tick scheduling is incorrect
   - **Symptom:** Visualizations hang on first frame, FPS drops to 0
   - **Status:** Recently fixed (restored timeout-based tick pattern)

2. **TempoTracker Phase Advancement:**
   - **Risk:** Phase advanced in AudioNode (per hop) instead of RendererNode (per frame)
   - **Impact:** Beat ticks may be less smooth, phase may drift
   - **Status:** Functional but different timing characteristics

3. **Zone Beat Modulation Removed:**
   - **Risk:** Zone effects no longer respond to beat ticks
   - **Impact:** Audio-reactive zone features disabled
   - **Status:** Intentionally disabled (beat tracking removed)

### 11.2 Moderate Issues

4. **MusicalGrid Tempo Feeding:**
   - **Risk:** Tempo observations only fed on new ControlBus frames (not every render frame)
   - **Impact:** MusicalGrid may have less frequent updates
   - **Status:** Functional but different update cadence

5. **Style Detection Beat Confidence:**
   - **Risk:** `beatConfidence` hardcoded to `0.0f` (line ~882 in AudioNode.cpp)
   - **Impact:** Style detection may be less accurate without beat confidence
   - **Status:** Functional but degraded

6. **64-bin Goertzel Removed:**
   - **Risk:** No high-resolution spectral analysis for beat tracking
   - **Impact:** Beat tracking may be less accurate
   - **Status:** Intentionally removed

### 11.3 Minor Issues

7. **Documentation Inconsistencies:**
   - `features.h` comment still references "TempoEngine" instead of "TempoTracker"
   - Some log messages may still reference TempoEngine

8. **Pre-AGC Audio Buffer Removed:**
   - **Risk:** Beat tracking uses same audio as visual effects (AGC-processed)
   - **Impact:** May reduce dynamic range for tempo detection
   - **Status:** Functional but potentially less accurate

---

## 12. Files Modified Summary

**Total Files Changed:** 19 files in `firmware/v2/`

**Core Audio:**
- `src/audio/AudioNode.cpp` (218 lines removed, ~50 lines added)
- `src/audio/AudioNode.h` (61 lines removed)
- `src/audio/AudioCapture.cpp` (1 line added)

**Actor System:**
- `src/core/actors/Node.cpp` (86 lines added - queue saturation prevention)
- `src/core/actors/Node.h` (12 lines added)
- `src/core/actors/RendererNode.cpp` (55 lines removed)
- `src/core/actors/RendererNode.h` (44 lines removed)
- `src/core/actors/NodeOrchestrator.cpp` (11 lines removed)

**Zone System:**
- `src/effects/zones/ZoneComposer.cpp` (54 lines removed)
- `src/effects/zones/ZoneComposer.h` (9 lines removed)

**Network:**
- `src/network/webserver/ws/WsZonesCommands.cpp` (3 lines changed)

**Main:**
- `src/main.cpp` (7 lines changed)

**Config:**
- `src/config/features.h` (4 lines changed)

**Deleted Files:**
- `src/audio/tempo/TempoEngine.h` (303 lines)
- `src/audio/tempo/TempoEngine.cpp` (530 lines)

**Documentation:**
- `docs/OTA_UPDATE_GUIDE.md` (5 lines changed)
- `LEGACY_COMPATIBILITY_INVENTORY.md` (7 lines changed)
- `platformio.ini` (2 lines added)
- `wifi_credentials.ini.template` (3 lines added)

---

## 13. Recommendations

### 13.1 Immediate Actions

1. **Verify Node.cpp Tick Timing:**
   - Confirm RendererNode maintains 120 FPS
   - Check that `onTick()` is called on schedule
   - Monitor for frame hangs or FPS drops

2. **Test Beat Tracking:**
   - Verify TempoTracker produces reasonable BPM estimates
   - Check that beat ticks are detected correctly
   - Compare accuracy with previous TempoEngine implementation

3. **Test Zone Audio Features:**
   - Verify zones still render correctly (beat modulation is intentionally disabled)
   - Check that tempo sync zones still work (if applicable)

### 13.2 Code Quality

4. **Fix Documentation:**
   - Update `features.h` comment to reference "TempoTracker" instead of "TempoEngine"
   - Review all log messages for consistency

5. **Review Watchdog Fixes:**
   - Verify `taskYIELD()` calls are sufficient to prevent watchdog timeouts
   - Monitor Audio task CPU usage

### 13.3 Future Considerations

6. **Consider Re-adding Pre-AGC Audio Path:**
   - If beat tracking accuracy degrades, consider restoring `m_hopBufferPreAgc`
   - Evaluate trade-off between memory usage and accuracy

7. **Evaluate MusicalGrid Update Frequency:**
   - Consider feeding tempo observations every frame (not just on new ControlBus frames)
   - Balance between update frequency and CPU usage

---

## 14. Testing Checklist

- [ ] RendererNode maintains 120 FPS (no frame hangs)
- [ ] Beat tracking produces reasonable BPM estimates
- [ ] Beat ticks are detected and logged correctly
- [ ] Zone effects render correctly (without beat modulation)
- [ ] WebSocket zone commands work (brightness field name compatibility)
- [ ] Serial `tempo` command displays correct status
- [ ] No watchdog timeouts in Audio task
- [ ] Queue saturation prevention works (rapid encoder inputs don't saturate)
- [ ] MusicalGrid receives tempo observations
- [ ] Style detection still functions (with reduced beat confidence)

---

**End of Audit Report**

