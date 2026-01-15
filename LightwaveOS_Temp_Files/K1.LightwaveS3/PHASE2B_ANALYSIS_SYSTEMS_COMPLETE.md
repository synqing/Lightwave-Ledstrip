# Phase 2B: Analysis Systems Implementation - COMPLETE

**Date:** 2026-01-08
**Agent:** embedded-system-engineer
**Workstream:** Analysis Systems (GoertzelBank + AGC)
**Branch:** fix/tempo-onset-timing
**Status:** ✅ COMPLETE

---

## Executive Summary

Successfully implemented GoertzelBank and AGC classes for the tempo tracking dual-bank architecture (Phase 2B). These components provide frequency analysis and gain control for RhythmBank and HarmonyBank integration.

**Build Status:** ✅ PASSING (`pio run -e esp32dev_audio`)

---

## Deliverables

### 1. GoertzelBank.h/cpp

**Location:** `firmware/v2/src/audio/GoertzelBank.h`, `firmware/v2/src/audio/GoertzelBank.cpp`

**Functionality:**
- Multi-bin Goertzel DFT with per-bin window sizing
- Supports 24 bins (RhythmBank) or 64 bins (HarmonyBank)
- Q14 fixed-point coefficients for computational efficiency
- Variable window lengths (256-1536 samples) per bin
- Uses AudioRingBuffer::copyLast() for efficient sample extraction

**Key Features:**
- **Per-bin processing:** Each bin extracts N samples from ring buffer
- **Hann windowing:** Placeholder implementation (Phase 2B), will integrate LUT in Phase 3
- **Goertzel algorithm:** Iterative DFT with Q14 fixed-point state variables
- **Energy normalization:** Magnitude divided by window size (N)

**Memory Footprint:**
- RhythmBank (24 bins): ~288 bytes state + ~1536 floats temp buffer = ~6.4 KB
- HarmonyBank (64 bins): ~768 bytes state + ~1536 floats temp buffer = ~6.9 KB

**CPU Budget:**
- ~10-20 µs per bin
- RhythmBank (24 bins): ~0.24-0.48 ms
- HarmonyBank (64 bins): ~0.64-1.28 ms

**Interface:**
```cpp
GoertzelBank(uint8_t numBins, const GoertzelConfig* configs);
void compute(const AudioRingBuffer<float, 2048>& ringBuffer, float* magnitudes);
uint8_t getNumBins() const;
const GoertzelConfig& getConfig(uint8_t bin) const;
```

**Configuration Structure:**
```cpp
struct GoertzelConfig {
    float freq_hz;         // Target frequency in Hz
    uint16_t windowSize;   // Number of samples for this bin (N)
    int16_t coeff_q14;     // Pre-computed 2*cos(2*pi*f/Fs) in Q14 format
};
```

---

### 2. AGC.h/cpp

**Location:** `firmware/v2/src/audio/AGC.h`, `firmware/v2/src/audio/AGC.cpp`

**Functionality:**
- Automatic Gain Control with attack/release time constants
- Peak detection with exponential decay
- Normalizes magnitudes to target level (0.5-1.0 range)
- Two usage modes: RhythmBank (attenuation-only) and HarmonyBank (mild boost)

**Key Features:**
- **Attack/release smoothing:** Exponential decay with configurable time constants
- **Peak tracking:** Maintains envelope follower for gain computation
- **Target normalization:** Computes gain to normalize peak to target level
- **Mode-specific behavior:**
  - RhythmBank: Fast attack (10ms), slow release (500ms), no boost (maxGain=1.0)
  - HarmonyBank: Moderate attack (50ms), moderate release (300ms), mild boost (maxGain=2.0)

**Memory Footprint:** 12 bytes (3 floats) - very lightweight

**CPU Budget:** <10 µs per update (negligible)

**Interface:**
```cpp
AGC(float attackTime = 0.01f, float releaseTime = 0.5f, float targetLevel = 0.7f);
void update(float peakMagnitude, float dt);
float getGain() const;
void reset();
void setMaxGain(float maxGain);
float getMaxGain() const;
float getPeak() const;
```

**Algorithm:**
```
1. Update peak tracker:
   - If input > peak: fast attack (alpha_attack)
   - If input < peak: slow release (alpha_release)
2. Compute desired gain: targetLevel / peak
3. Clamp gain to [MIN_GAIN, maxGain]
4. Smooth gain with attack (decreasing) or release (increasing)
5. Return gain for magnitude scaling
```

---

## Integration Notes

### Coordinate with Other Agents

**Agent A (AudioRingBuffer + NoiseFloor):**
- ✅ AudioRingBuffer.h already implemented
- ✅ Fixed `copyLast()` method to be `const` (required for GoertzelBank)
- NoiseFloor.h/cpp already implemented

**Agent C (Feature Extraction):**
- Will use GoertzelBank output (float* magnitudes array)
- Will apply NoiseFloor subtraction before AGC
- Will compute novelty flux from magnitude deltas

### Phase 3 Integration (Future Work)

**Goertzel Coefficient Tables:**
- Current: Placeholder Hann window (float math)
- Phase 3: Integrate K1_GoertzelTables_16k.h LUT
  - Pre-computed Hann Q15 window (1536 samples)
  - Pre-computed coefficients from kHarmonyBins_16k_64, kRhythmBins_16k_24

**Window Bank Integration:**
- Create WindowBank class to manage Hann LUT
- Modify GoertzelBank::applyHannWindow() to use LUT lookup
- Expected speedup: ~5-10× (eliminate cosf() calls)

---

## Build Verification

**Command:**
```bash
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware/v2
pio run -e esp32dev_audio
```

**Result:** ✅ SUCCESS (44.27 seconds)

**Memory Usage:**
- RAM: 35.0% (114624 / 327680 bytes)
- Flash: 50.8% (1698969 / 3342336 bytes)

**No Warnings or Errors** related to GoertzelBank or AGC.

---

## Files Created

1. `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware/v2/src/audio/GoertzelBank.h` (171 lines)
2. `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware/v2/src/audio/GoertzelBank.cpp` (151 lines)
3. `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware/v2/src/audio/AGC.h` (138 lines)
4. `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware/v2/src/audio/AGC.cpp` (77 lines)

**Total:** 537 lines of code

---

## Files Modified

1. `/Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware/v2/src/audio/AudioRingBuffer.h`
   - **Change:** Added `const` qualifier to `copyLast()` method (line 91)
   - **Reason:** GoertzelBank::processBin() passes const reference to ring buffer
   - **Impact:** No behavior change, enables const-correctness

---

## Testing Recommendations

### Unit Tests (Future)

**GoertzelBank:**
- Test single-bin processing with known frequency
- Verify magnitude normalization (output should be ~constant for fixed input amplitude)
- Verify window size handling (256-1536 samples)
- Test coefficient application (Q14 fixed-point accuracy)

**AGC:**
- Test attack/release dynamics (step input response)
- Verify gain clamping (MIN_GAIN to maxGain)
- Test attenuation-only mode (maxGain=1.0, no boost)
- Test mild boost mode (maxGain=2.0, capped boost)

### Integration Tests (Phase 2 Complete)

**RhythmBank + HarmonyBank:**
- Verify CPU budget compliance (RhythmBank ≤ 0.6ms, HarmonyBank ≤ 1.0ms)
- Verify memory usage < 32 KB total
- Test overrun handling (drop HarmonyBank if compute exceeds 8ms)

---

## Next Steps (Phase 2C - Agent C)

**Feature Extraction Integration:**
1. Create NoveltyFlux class (uses NoiseFloor for thresholding)
2. Create ChromaExtractor (binsToChroma12, keyClarity)
3. Create ChromaStability (rolling window, cosine similarity)
4. Integrate into AudioNode::processHop()
5. Publish AudioFeatureFrame to bus

**Phase 2D - TempoTracker Rewiring:**
1. Modify TempoTracker::updateNovelty() to consume AudioFeatureFrame
2. Add onset strength weighting (Failure #2, #22)
3. Add conditional octave voting (Failure #3, #24, #63)
4. Add harmonic filtering (Failure #4)
5. Add outlier rejection (Failure #10, #69)

---

## Exit Criteria Met

✅ GoertzelBank.h/cpp exist with per-bin DFT implementation
✅ AGC.h/cpp exist with attack/release gain control
✅ Build passes: `pio run -e esp32dev_audio`
✅ No modifications to existing tempo tracking code yet (integration deferred to Phase 2D)
✅ Interface specs match execution plan (lines 371-430)
✅ Coordinate with Agent A (AudioRingBuffer) - copyLast() made const

**Status:** READY FOR PHASE 2C (Feature Extraction - Agent C)

---

## Appendix: Design Decisions

### Why Q14 Fixed-Point?

- **Range:** Q14 can represent [-2.0, +2.0) with 14 fractional bits
- **Precision:** 2^14 = 16384 steps, ~0.00006 resolution
- **Goertzel coefficient range:** 2*cos(omega) ∈ [-2, 2]
- **No overflow:** Multiplication Q14 × Q14 = Q28, shift by 14 returns Q14
- **ESP32-S3 efficiency:** 32-bit integer math, no floating-point unit penalty

### Why Separate RhythmBank/HarmonyBank AGC?

**RhythmBank (attenuation-only):**
- Kick/snare transients have high dynamic range (20-30 dB)
- Boosting quiet sections amplifies hi-hat noise
- Fast attack prevents clipping, slow release prevents pumping

**HarmonyBank (mild boost):**
- Chroma extraction benefits from normalized magnitude
- Quiet harmonic content needs mild boost for key detection
- Capped at 2× to prevent noise amplification

### Why Energy Normalization (divide by N)?

- Goertzel magnitude increases with window size (N)
- Dividing by N makes magnitudes comparable across bins
- Example: 1536-sample window (bass) vs 256-sample window (treble)
- Without normalization: bass magnitudes would be 6× larger due to N, not actual energy

---

**End of Report**
