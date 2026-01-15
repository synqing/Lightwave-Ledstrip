# K1.Lightwave vs LightwaveOS v2 Audio Pipeline Review

## 1. Objective
Compare the SPH0645 microphone implementation in the legacy `K1.Lightwave` firmware with the new `LightwaveOS v2` implementation to ensure technical correctness and identify any missed optimizations.

## 2. I2S Configuration & Timing
**Findings:** Both systems address the SPH0645's non-standard I2S timing (MSB alignment), but use different methods.

| Feature | K1.Lightwave (Legacy) | LightwaveOS v2 (Current) | Analysis |
| :--- | :--- | :--- | :--- |
| **Timing Fix** | `REG_SET_BIT(RX_TIMING, BIT(9))` | `REG_SET_BIT(RX_CONF, WS_IDLE_POL)` | **Equivalent Outcome.** K1 delays the data line by 1 cycle. v2 inverts the WS polarity. Both effectively align the MSB to the correct clock edge. |
| **MSB Shift** | `REG_CLR_BIT(RX_CONF, MSB_SHIFT)` | Defaults (Set) | K1 disables the hardware Philips shift. v2 relies on the standard shift. Both work in their respective contexts. |
| **Sample Rate** | 16,000 Hz | 16,000 Hz | Identical. v2 uses 16kHz for Tab5 parity and standard voice band coverage. |

**Verdict:** The v2 implementation (WS Polarity Inversion) is a verified, standard fix for SPH0645 on ESP32. No changes needed.

## 3. Signal Processing & Gain
**Findings:** The primary discrepancy (low levels in v2) was due to a difference in how gain/normalization is handled.

| Feature | K1.Lightwave (Legacy) | LightwaveOS v2 (Current) | Analysis |
| :--- | :--- | :--- | :--- |
| **Input Levels** | Raw Integer (+/- 32767) | Raw Integer (+/- 32767) | Identical raw capture. |
| **DC Removal** | Static `CONFIG.DC_OFFSET` | Dynamic IIR Filter | v2's dynamic filter adapts to drift/temperature better than K1's static calibration. |
| **Gain Control** | **"Sweet Spot" Auto-Ranging** | **AGC (Automatic Gain Control)** | K1 tracks `max_val` and scales visuals relative to it. v2 boosts signal to a fixed target RMS (0.25). |
| **Normalization** | Implicit (in visual mapping) | Explicit (0.0 - 1.0) | v2 provides a cleaner contract: `ControlBus` always carries 0..1 normalized data. |

**The "Low Level" Explanation:**
- K1 works by saying "The max I saw was 500, so 500 is 100% brightness."
- v2 (initially) said "The max is 32767. You gave me 500. That is 1.5% brightness."
- **The Fix:** v2 now amplifies that 500 to ~8000 (Target RMS) so it *becomes* ~25-50% brightness naturally.

## 4. Conclusion
The v2 implementation is **technically superior** for a modular OS because it decouples the "Audio Engine" from the "Visual Renderer".
- **K1:** The visualizer needs to know about "Sweet Spots" and raw levels.
- **v2:** The visualizer just asks for "RMS (0..1)" and gets a usable value, because the Audio Actor handled the dirty work (AGC).

**Recommendation:** Proceed with the current v2 fixes (High Gain AGC). It aligns with the goal of a robust, normalized Control Bus.
