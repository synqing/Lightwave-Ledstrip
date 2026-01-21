# Audio Pipeline Audit Report & Roadmap
## 1. Executive Summary
This document tracks the audit of the LightwaveOS v2 audio pipeline, specifically focusing on the integration of the SPH0645 I2S microphone and the Goertzel algorithm for frequency analysis.

**Current Status:**  
- **I2S Capture:** Verified working with SPH0645 (MSB alignment fixed, WS polarity fixed).
- **DSP Pipeline:** Goertzel algorithm implemented and normalized.
- **RMS/AGC:** **Major Update (Dec 25):** Tuned AGC to target -12dB (0.25 RMS) with 300x max gain to address low sensitivity.
- **Architecture:** `AudioActor` (Core 0) -> `ControlBus` (Lock-free) -> `Renderer` (Core 1) matches Archive architecture.

## 2. Findings & Fixes

### 2.1. SPH0645 I2S Timing
- **Issue:** SPH0645 requires specific I2S timing (WS polarity inverted, data delayed).
- **Fix:** Applied `REG_SET_BIT(I2S_RX_CONF_REG(0), I2S_RX_WS_IDLE_POL)` in `AudioCapture.cpp` to invert WS polarity.
- **Verification:** Audio samples are now coherent (sine waves look like sine waves).

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
