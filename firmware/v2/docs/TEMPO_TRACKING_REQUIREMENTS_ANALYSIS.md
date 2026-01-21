# Tempo Tracking Requirements Analysis

## Question 1: Desired Tempo Range

### Current Configuration

**Configured Range**: 60-180 BPM (as defined in `TempoTrackerTuning`)

```cpp
// From TempoTracker.h lines 42-43
float minBpm = 60.0f;   // Minimum BPM
float maxBpm = 180.0f;  // Maximum BPM
```

### Technical Context

1. **MusicalGrid Range**: The render-domain PLL (`MusicalGrid`) supports 30-300 BPM, but the audio-domain tracker (`TempoTracker`) clamps to 60-180 BPM for practical music detection.

2. **Why 60-180 BPM?**
   - **60 BPM minimum**: Prevents false positives from slow ambient sounds, room noise, or sub-audio vibrations
   - **180 BPM maximum**: Covers most electronic music (house: 120-130, techno: 130-140, DnB: 160-180)
   - **Practical range**: Most commercial music falls within 60-180 BPM

3. **Edge Cases**:
   - **<60 BPM**: Very slow ambient/classical (e.g., 40-50 BPM adagio) - not currently supported
   - **>180 BPM**: Extreme speed metal (200+ BPM) - would require subdivision detection
   - **Subdivision handling**: Current onset-timing approach can detect 2x/0.5x intervals, but needs refinement (as seen in the half-tempo bug)

### Recommendation

**Keep 60-180 BPM** for the primary tracker, but consider:
- **Add subdivision detection** to handle 2x/0.5x tempo errors (currently being addressed)

---

## Question 2: Real-Time vs Accuracy vs Stability

### Current Architecture: **Real-Time with Stability**

The system prioritizes **real-time responsiveness** for LED synchronization, with stability as a secondary concern.

### Evidence from Codebase

#### Real-Time Requirements

1. **LED Render Rate**: 120 FPS target (8.33ms per frame)
   - From `RendererActor`: "Tick() is called ONLY by the renderer at ~120 FPS"
   - Tempo updates must arrive faster than visual refresh

2. **Audio Processing Latency**: ~16-32ms total
   ```
   I2S DMA: 16ms (256 samples @ 16kHz)
   Goertzel fill: 16ms (512-sample window)
   Beat detection: 16ms (3-point delay)
   Total: ~48ms audio-to-LED
   ```
   - **Acceptable**: Human tolerance for beat sync is ~100ms

3. **Tempo Update Rate**: 62.5 Hz (every 16ms hop)
   - From `audio_config.h`: `HOP_SIZE = 256` → 16ms hop @ 16kHz
   - TempoTracker updates every hop, providing sub-100ms latency

#### Stability Mechanisms

1. **EMA Smoothing**: 
   ```cpp
   // From TempoTracker.cpp line 281-282
   beat_state_.periodSecEma = (1.0f - beat_state_.periodAlpha) * beat_state_.periodSecEma
                              + beat_state_.periodAlpha * onsetDt;
   ```
   - `periodAlpha = 0.15f` (15% new, 85% old) - conservative smoothing

2. **BPM Update Smoothing**:
   ```cpp
   // From TempoTracker.cpp line 294
   beat_state_.bpm = 0.70f * beat_state_.bpm + 0.30f * newBpm;
   ```
   - 70% old, 30% new - prevents jitter

3. **Confidence-Based Locking**:
   ```cpp
   // From TempoOutput.h line 19
   bool locked;  // True if confidence > threshold
   ```
   - Effects only use tempo when `locked=true` (confidence > threshold)
   - Prevents unstable tempo from affecting visuals

### Trade-Off Analysis

| Priority | Current Behavior | Impact |
|----------|------------------|--------|
| **Real-Time** | ✅ Updates every 16ms | LED sync feels responsive |
| **Stability** | ✅ EMA + confidence gating | Prevents jitter, but can be slow to adapt |
| **Accuracy** | ⚠️ Can lock to half-tempo | Current bug: accepts 2x intervals too easily |

### Recommendation

**Current priority is correct**: Real-time > Stability > Accuracy

However, the **half-tempo bug** shows we need better accuracy without sacrificing real-time:
- ✅ **Keep 16ms update rate** (real-time requirement)
- ✅ **Keep EMA smoothing** (stability)
- ⚠️ **Fix interval consistency check** (accuracy - in progress)
- ⚠️ **Add tempo correction mechanism** (accuracy - in progress)

---

## Question 3: Audio Input Sources

### Current Implementation: **I2S Microphone Only**

**Primary Source**: SPH0645 MEMS I2S Microphone

```cpp
// From audio_config.h lines 37-39
constexpr gpio_num_t I2S_BCLK_PIN = GPIO_NUM_14;
constexpr gpio_num_t I2S_DOUT_PIN = GPIO_NUM_13;
constexpr gpio_num_t I2S_LRCL_PIN = GPIO_NUM_12;
```

**Configuration**:
- **Sample Rate**: 16,000 Hz
- **Bit Depth**: 32-bit I2S slots (18-bit actual data from SPH0645)
- **Hop Size**: 256 samples (16ms frames)
- **FFT Size**: 512 samples (32ms window)

### Historical Context

From `docs/archive/audio-analysis/v3-i2s-capture.md`:
- **Tab5 compatibility**: System was designed for Tab5 audio pipeline parity
- **I2S driver**: ESP-IDF 5.x new I2S driver (`driver/i2s_std.h`)
- **Proven sample conversion**: DC bias correction, clipping, scaling chain

### Impact on Tempo Tracking

**Microphone Input Characteristics**:
- ✅ **Pros**: Real-time, no latency from source
- ⚠️ **Cons**: 
  - Room acoustics affect onset detection
  - Background noise can trigger false onsets
  - Distance-dependent sensitivity



---

## Question 4: Architecture Options

### Current Architecture: **FFT + Onset Detection**

**Implementation**: 3-layer onset-timing approach (recently migrated from Goertzel resonator bank)

```
Layer 1: Onset Detection
  - Spectral flux (8-band Goertzel)
  - VU derivative (RMS change)
  - Combined → single onset signal

Layer 2: Beat Tracking
  - Inter-onset interval measurement
  - EMA period estimation
  - PLL-style phase locking

Layer 3: Output Formatting
  - BeatState → TempoOutput
  - Confidence gating
  - Phase [0,1) calculation
```

### Why We Migrated from Goertzel Resonator Bank

**Previous Approach** (Goertzel resonator bank):
- 121 resonators (60-180 BPM @ 1 BPM resolution)
- 22 KB memory footprint
- **Problem**: Harmonic aliasing (detected 2x/3x harmonics instead of fundamental)

**Current Approach** (Onset-timing):
- ~100 lines of code
- <1 KB memory footprint
- **Problem**: Half-tempo locking (accepts 2x intervals too easily)

### Alternative Architectures

#### 1. **Comb Filterbank** (Autocorrelation-based)

**How it works**:
- Bank of comb filters at different delay periods
- Correlates audio with delayed version of itself
- Peak correlation = tempo period

**Pros**:
- ✅ Immune to harmonic aliasing (detects period, not frequency)
- ✅ Handles polyrhythms well
- ✅ Good for complex music

**Cons**:
- ⚠️ Higher CPU cost (autocorrelation is O(N²))
- ⚠️ Memory: ~4-8 KB for delay buffers
- ⚠️ Slower adaptation (needs history)

**Verdict**: **Good alternative** if onset-timing can't be fixed, but higher cost.

#### 2. **Autocorrelation (Single-Pass)**

**How it works**:
- Compute autocorrelation of onset signal (not raw audio)
- Find peak in autocorrelation → tempo period
- Simpler than comb filterbank

**Pros**:
- ✅ Lower CPU than comb filterbank
- ✅ Still immune to harmonics
- ✅ Memory: ~1-2 KB

**Cons**:
- ⚠️ Needs onset signal (already have this)
- ⚠️ Still O(N log N) if using FFT for autocorrelation

**Verdict**: **Viable upgrade path** if we keep onset detection but improve tempo estimation.

#### 3. **ML-Based (Neural Network)**

**How it works**:
- Train CNN/LSTM on onset sequences
- Predict tempo and phase
- Run inference on ESP32-S3

**Pros**:
- ✅ State-of-the-art accuracy
- ✅ Handles complex rhythms

**Cons**:
- ❌ **Too heavy for ESP32-S3** (would need external ML accelerator)
- ❌ Memory: 100+ KB for model
- ❌ CPU: 10-50ms inference time (too slow for real-time)
- ❌ Training data required

**Verdict**: **Not feasible** for current hardware.

#### 4. **Hybrid: Onset + Autocorrelation**

**How it works**:
- Keep current onset detection (Layer 1)
- Replace interval-based tracking (Layer 2) with autocorrelation of onset signal
- Use autocorrelation peak for tempo, PLL for phase

**Pros**:
- ✅ Reuses existing onset detection
- ✅ Autocorrelation immune to harmonics
- ✅ Moderate CPU cost (~2-3ms per hop)
- ✅ Memory: ~2 KB for autocorrelation buffer

**Cons**:
- ⚠️ Needs 2-4 seconds of onset history (acceptable latency)

**Verdict**: **Best upgrade path** if current fixes don't work.


### Decision Matrix

| Scenario | Recommendation |
|----------|----------------|
| **Current fixes work** | ✅ Keep onset-timing, tune parameters |
| **Still locks to half-tempo** | ⚠️ Try hybrid onset + autocorrelation |
| **Need polyrhythm support** | ⚠️ Consider comb filterbank |
| **CPU headroom <10%** | ❌ Don't use comb filterbank |
| **Memory <50 KB free** | ❌ Don't use ML-based |


