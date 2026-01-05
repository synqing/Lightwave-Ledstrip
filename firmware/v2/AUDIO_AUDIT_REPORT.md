# Audio Pipeline Audit Report & Roadmap
## 1. Executive Summary
This document tracks the audit of the LightwaveOS v2 audio pipeline, specifically focusing on the integration of the SPH0645 I2S microphone and the Goertzel algorithm for frequency analysis.

**Current Status:**  
- **I2S Capture:** Verified working with SPH0645 (MSB alignment fixed, WS polarity fixed).
- **Sample Conversion:** **Major Update (Jan 2025):** Implemented Emotiscope 2.0 exact conversion pipeline for SPH0645 microphone samples (>>14 shift, +7000 offset, clip to ±131072, -360 offset, 4x fixed gain).
- **DSP Pipeline:** Goertzel algorithm implemented and normalized.
- **RMS/AGC:** **Major Update (Dec 25):** Tuned AGC to target -12dB (0.25 RMS) with 300x max gain to address low sensitivity.
- **AGC Toggle:** **Major Update (Jan 2025):** Runtime AGC enable/disable toggle via REST API (`PUT /api/v1/audio/agc`) and WebSocket (`audio.parameters.set`). When disabled, uses fixed 4x gain (Emotiscope mode) with no DC removal.
- **Architecture:** `AudioActor` (Core 0) -> `ControlBus` (Lock-free) -> `Renderer` (Core 1) matches Archive architecture.

## 2. Findings & Fixes

### 2.1. SPH0645 I2S Timing
- **Issue:** SPH0645 requires specific I2S timing (WS polarity inverted, data delayed).
- **Fix:** Applied `REG_SET_BIT(I2S_RX_CONF_REG(0), I2S_RX_WS_IDLE_POL)` in `AudioCapture.cpp` to invert WS polarity.
- **Verification:** Audio samples are now coherent (sine waves look like sine waves).

### 2.1a. SPH0645 Sample Conversion (Jan 2025)
- **Issue:** Need exact parity with Emotiscope 2.0 microphone conversion pipeline for consistent audio processing.
- **Fix:** Implemented Emotiscope 2.0 exact conversion in `AudioCapture.cpp`:
  1. Extract 18-bit value: `raw >> 14`
  2. Add pre-clip offset: `+7000`
  3. Clip to 18-bit range: `±131072`
  4. Subtract post-clip offset: `-360`
  5. Scale to float [-1.0, 1.0]: `* (1.0 / 131072.0)`
  6. Apply fixed 4x amplification: `* 4.0`
  7. Convert back to int16: `* 32768.0` and round
- **Vectorization:** Processes 4 samples at a time for efficiency, with remainder handling for non-divisible HOP_SIZE.
- **Verification:** Matches Emotiscope 2.0 `microphone.h:104-114` conversion pipeline exactly.

### 2.2. RMS & Gain Staging (Dec 25)
- **Issue:** User reported "unmanageably low" RMS levels and "bands[0]=0.005".
- **Root Cause:** 
    - Previous `agcTargetRms` was 0.010 (-40dB), which is too low for visual mapping.
    - `agcMaxGain` was 80x, insufficient for quiet SPH0645 mics.
    - `mapLevelDb` range (-65 to -18) was mismatched with the low target.
- **Fix:**
    - Increased `agcTargetRms` to **0.25** (-12dB).
    - Increased `agcMaxGain` to **300.0f**.
    - Adjusted `mapLevelDb` ceiling to **-12.0f** (matches target).
    - Set `agcMinGain` to 1.0f (unity) to prevent attenuation of quiet signals.
- **Expected Outcome:** `ControlBus` values (RMS and Bands) should now hit 1.0 (full scale) when audio is present.

### 2.3. Architecture & Contracts
- **Comparison with Archive:**
    - **Archive:** `spectra_audio_interface` queue carried `vu_level_normalized` (0..1).
    - **v2:** `ControlBus` carries `rms` (0..1).
    - **Alignment:** The v2 `ControlBus` is the direct equivalent. The "normalization" logic is now robustly handled by the AGC in `AudioActor`, ensuring the `rms` value sent to `ControlBus` is "visual-ready" (0..1).

### 2.4. Goertzel Normalization
- **Issue:** Goertzel magnitudes were potentially unnormalized.
- **Analysis:** `GoertzelAnalyzer` inputs normalized floats (-1..1). Output is proportional to Amplitude * N / 2.
- **Fix:** Normalization factor `1.0f / 250.0f` ensures output is ~Amplitude (0..1).
- **Verification:** With AGC boosting Amplitude to ~0.01 (raw) -> ~0.25 (boosted), Goertzel output will track this boost.

### 2.5. AGC Runtime Toggle (Jan 2025)
- **Feature:** Runtime AGC enable/disable toggle for compatibility with Emotiscope 2.0 fixed-gain mode.
- **Implementation:**
  - Added `agcEnabled` field to `AudioPipelineTuning` struct (defaults to `true` for backward compatibility).
  - When `agcEnabled = true`: Normal AGC path with DC removal and dynamic gain adjustment.
  - When `agcEnabled = false`: Emotiscope mode - no DC removal, no AGC, fixed 4x gain (already applied in AudioCapture).
  - Activity calculation: `1.0f` when AGC disabled (no noise floor gating), gated when enabled.
  - `m_lastAgcGain = 1.0f` when disabled (4x gain already applied, so AGC gain is unity).
- **API Endpoints:**
  - REST: `PUT /api/v1/audio/agc` with `{"enabled": true/false}`
  - WebSocket: `audio.parameters.set` with `{"pipeline": {"agcEnabled": true/false}}`
  - WebSocket: `audio.parameters.get` returns `agcEnabled` in pipeline object
- **Use Case:** Allows switching between dynamic AGC (better for varying input levels) and fixed gain (matches Emotiscope 2.0 behavior exactly).

## 3. Next Steps
1. **User Verification:** Upload firmware and verify serial logs show `rmsMapped` and `raw.bands` hitting ~0.5 to 1.0.
2. **Visual Verification:** If possible, observe LEDs (requires renderer implementation).
3. **Fine Tuning:** Adjust `agcAttack`/`agcRelease` if levels pump too much.

## 4. Development Log
- **Dec 25:** 
    - Fixed Goertzel linker errors.
    - Added `m_lastBands` persistence to fix "picket fence" dropouts.
    - Implemented robust AGC and Gain Staging in `AudioActor`.
    - Aligned `ControlBus` contract with Archive expectations.
- **Jan 2025:**
    - Implemented Emotiscope 2.0 exact conversion pipeline for SPH0645 microphone samples.
    - Added runtime AGC toggle feature (REST API and WebSocket support).
    - Vectorized sample conversion loop (4 samples at a time) with remainder handling.
    - Removed unused DC_BIAS constants, replaced with Emotiscope conversion constants.
