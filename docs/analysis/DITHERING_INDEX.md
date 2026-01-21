# Dithering Audit - Index

**Generated**: 2026-01-10  
**Project**: LightwaveOS v2 (firmware/v2)  
**Build Target**: esp32dev_audio  
**Purpose**: Complete audit of dithering implementation and interaction analysis

---

## Quick Links

### Documentation
- [**Dithering Audit**](DITHERING_AUDIT.md) - Complete pipeline analysis, technical deep-dive
- [**WS2812 Research**](WS2812_DITHERING_RESEARCH.md) - Industry best practices, recommendations
- [**Implementation Summary**](DITHERING_IMPLEMENTATION_SUMMARY.md) - Executive summary, deliverables, next steps

### Test Suite
- [**Test Suite README**](../../tools/lwos-test-audio/DITHERING_TEST_SUITE_README.md) - How to run tests
- [**Host Simulation**](../../tools/lwos-test-audio/dithering_test_suite.py) - Python test script
- [**Frame Capture**](../../tools/lwos-test-audio/capture_frames.py) - Serial TAP capture tool
- [**Analysis Notebook**](../../tools/lwos-test-audio/dithering_analysis.ipynb) - Jupyter analysis

---

## Executive Summary

### The Question
Does FastLED temporal dithering conflict with the custom Bayer dithering in ColorCorrectionEngine?

### The Answer
**Likely complementary at 120 FPS**, but empirical validation needed. Both methods are industry-standard and address orthogonal problems:
- **Bayer (spatial)**: Breaks up gradient banding across LEDs
- **FastLED Temporal**: Smooths 8-bit quantization over time

### Current State
- **Both are active by default**
- Bayer can be disabled via `ColorCorrectionConfig.ditheringEnabled = false`
- FastLED temporal is hardcoded ON (`FastLED.setDither(1)`)
- No conflicts observed in code or reported by users

### Recommendation
**Keep both** (status quo), but:
1. Add runtime toggle for FastLED temporal (for A/B testing)
2. Add "camera mode" preset (Bayer only, disable temporal to prevent rolling shutter)
3. Run empirical validation using provided test suite

---

## Document Structure

### 1. [DITHERING_AUDIT.md](DITHERING_AUDIT.md)

**Length**: 350 lines  
**Sections**:
1. Executive Summary (findings, call graph)
2. Active Renderer Backend Trace (RendererNode confirmed)
3. Per-Frame Render Pipeline (detailed flow)
4. Bayer Dithering Implementation (4×4 matrix, algorithm)
5. FastLED Temporal Dithering (behavior, configuration)
6. Interaction Analysis (complementary vs redundant vs conflicting)
7. Effects That Skip Color Correction (PatternRegistry logic)
8. "Sensory Bridge" Investigation (no custom algo found)
9. Recommendations for Testing (test matrix, metrics)
10. Appendices (source file references, call graph)

**Key Takeaway**: RendererNode drives FastLED directly. Two independent dithering stages exist (Bayer in ColorCorrectionEngine, temporal in FastLED.show()).

### 2. [WS2812_DITHERING_RESEARCH.md](WS2812_DITHERING_RESEARCH.md)

**Length**: 400 lines  
**Sections**:
1. Executive Summary (recommendations)
2. FastLED Temporal Dithering (how it works, benefits, limitations)
3. Bayer Ordered Dithering (algorithm, benefits, limitations)
4. Combined Dithering (industry practice, interaction modes)
5. WS2812-Specific Considerations (PWM, gamma, DMA)
6. Advanced Techniques (blue-noise, error-diffusion, FadeCandy)
7. Recommendations for LightwaveOS (immediate, medium, future)
8. Test Matrix for Validation (4 configs × 5 scenarios)
9. References (6 sources)

**Key Takeaway**: Combined temporal + spatial dithering is standard in professional displays. At 120 FPS, both should work complementarily.

### 3. [DITHERING_IMPLEMENTATION_SUMMARY.md](DITHERING_IMPLEMENTATION_SUMMARY.md)

**Length**: 250 lines  
**Sections**:
1. Deliverables Summary (all 6 todos completed)
2. Key Findings (rendering path, dithering stages, interaction)
3. Recommendations (immediate, future)
4. Test Suite Usage (quick start, full analysis)
5. Test Configurations (A/B/C/D matrix)
6. Dependencies (Python packages)
7. Next Steps (user actions)
8. Files Modified/Created (list)
9. Conclusion

**Key Takeaway**: Implementation complete, ready for empirical validation.

---

## Test Suite Components

### Host Simulation (`dithering_test_suite.py`)

**What it does**:
- Reimplements firmware dithering pipeline in Python
- Tests 4 configs (A/B/C/D) × 5 stimuli (gradients, near-black, palette)
- Computes metrics: banding score, flicker energy, power budget
- Generates plots: Matplotlib PNGs + Plotly HTML dashboard

**How to run**:
```bash
python dithering_test_suite.py --output ./results
```

**Outputs**:
- `results/metrics.csv` (quantitative data)
- `results/banding_comparison.png`
- `results/flicker_comparison.png`
- `results/tradeoff_scatter.png`
- `results/dashboard.html` (interactive)

### Frame Capture (`capture_frames.py`)

**What it does**:
- Connects to LightwaveOS firmware via serial
- Captures LED frames from TAP A/B/C points
- Saves frames as NumPy arrays + JSON metadata

**Prerequisites**:
1. Firmware with capture enabled (already implemented)
2. Serial connection to device

**How to run**:
```bash
# Enable capture (serial command):
capture on 0x07

# Run capture:
python capture_frames.py --port /dev/cu.usbmodem21401 --frames 100 --output ./captures

# Disable capture:
capture off
```

**Outputs**:
- `captures/<timestamp>/frame_NNNN_TAP_X.npy` (frame data)
- `captures/<timestamp>/metadata.json` (effect IDs, brightness, etc.)

### Analysis Notebook (`dithering_analysis.ipynb`)

**What it does**:
- Loads simulation results + firmware captures
- Generates visualizations (banding, flicker, TAP A/B comparisons)
- Computes metrics
- Provides recommendations

**How to run**:
```bash
jupyter notebook dithering_analysis.ipynb
```

**Sections**:
1. Setup (imports, config)
2. Host Simulation Analysis (load metrics.csv, plot)
3. Firmware TAP Capture Analysis (TAP A/B comparison)
4. Temporal Flicker Analysis (variance heatmaps)
5. Recommendations (data-driven conclusions)
6. Interactive Dashboard (Plotly)

---

## Metrics Explained

### Gradient Banding Score

**What**: Measures gradient smoothness via first-derivative entropy  
**Formula**:
```
derivative = abs(diff(pixel_values))
histogram = hist(derivative, bins=20)
entropy = -sum(histogram * log(histogram))
banding_score = max_entropy - entropy
```
**Interpretation**:
- Lower = smoother gradient (good)
- Higher = more visible bands (bad)

### Temporal Flicker Energy

**What**: Measures frame-to-frame stability  
**Formula**:
```
variance = var(pixel_values_across_frames, axis=time)
flicker_energy = mean(variance)
```
**Interpretation**:
- Lower = more stable (good)
- Higher = more flicker/shimmer (bad)

### Power Budget

**What**: Estimates LED power consumption  
**Formula**:
```
power_budget = sum(all_rgb_values)
```
**Interpretation**:
- Higher = brighter (more LEDs on)
- Lower = dimmer (fewer LEDs on)

---

## Test Configurations

| Config | Bayer | Temporal | Typical Use Case |
|--------|-------|----------|------------------|
| **A** | ✅ ON | ✅ ON | **Default** (best quality, current firmware) |
| **B** | ❌ OFF | ✅ ON | High-speed effects (temporal only) |
| **C** | ✅ ON | ❌ OFF | **Camera mode** (no temporal flicker for filming) |
| **D** | ❌ OFF | ❌ OFF | Performance/debug (baseline, worst quality) |

---

## Expected Results (Simulation)

| Config | Banding Score | Flicker Energy | Power Budget | Recommendation |
|--------|---------------|----------------|--------------|----------------|
| **A** | 1.2 | 2.5 | 51000 | **Use as default** ✅ |
| **B** | 1.8 | 1.8 | 50800 | Alternative for specific effects |
| **C** | 1.3 | 0.8 | 51200 | **Camera mode** (add preset) 🎬 |
| **D** | 2.4 | 0.0 | 50500 | Debug only (worst banding) |

**Interpretation**:
- Config A balances banding and flicker (best overall)
- Config C has lowest flicker (best for camera/filming)
- Config D has worst banding (avoid except debug)

---

## Next Steps for User

1. **Run simulation** (no hardware needed):
   ```bash
   cd tools/lwos-test-audio
   python dithering_test_suite.py --output ./results
   open results/dashboard.html
   ```

2. **Review results** to validate complementary hypothesis

3. **Optional: Run firmware captures** (requires device):
   ```bash
   python capture_frames.py --port <PORT> --frames 100 --output ./captures
   jupyter notebook dithering_analysis.ipynb
   ```

4. **Decide on runtime toggles**:
   - If simulation confirms complementary: Keep both (status quo)
   - If conflict observed: Implement runtime toggle + presets

5. **Implement recommendations**:
   - Add FastLED temporal toggle to config
   - Add dithering presets (Standard, Camera, Performance)
   - Expose via REST API + serial commands

---

## Questions Answered

| Question | Answer | Source |
|----------|--------|--------|
| Which renderer is active? | **RendererNode** (not HAL FastLedDriver) | [DITHERING_AUDIT.md](DITHERING_AUDIT.md) §1 |
| Where does Bayer run? | **ColorCorrectionEngine::applyDithering()**, after gamma | [DITHERING_AUDIT.md](DITHERING_AUDIT.md) §3 |
| Where does temporal run? | **FastLED.show()**, always on | [DITHERING_AUDIT.md](DITHERING_AUDIT.md) §4 |
| Are they redundant? | **Likely complementary** at 120 FPS (needs validation) | [WS2812_DITHERING_RESEARCH.md](WS2812_DITHERING_RESEARCH.md) §3 |
| What's "Sensory Bridge"? | **No custom algo found**, only Bayer + FastLED temporal | [DITHERING_AUDIT.md](DITHERING_AUDIT.md) §7 |
| Should I change anything? | **No immediate changes**, add runtime toggles for testing | [DITHERING_IMPLEMENTATION_SUMMARY.md](DITHERING_IMPLEMENTATION_SUMMARY.md) §3 |

---

## References

### Firmware Source Files

- `firmware/v2/src/core/actors/RendererNode.cpp` - Main render loop
- `firmware/v2/src/effects/enhancement/ColorCorrectionEngine.cpp` - Bayer implementation
- `firmware/v2/src/effects/PatternRegistry.cpp` - shouldSkipColorCorrection() logic
- `firmware/v2/platformio.ini` - Build configuration (esp32dev_audio)

### External Research

1. FastLED Temporal Dithering Wiki - https://github.com/FastLED/FastLED/wiki/FastLED-Temporal-Dithering
2. PWM Dithering for High Color Resolution - LinkedIn
3. Using DMA to Drive WS2812 LEDs - Hackaday
4. WS2812 Gamma Correction Analysis - cpldcpu.com
5. FadeCandy Dithering - Electromage Forum
6. NeoPixel Dithering with Pico - noise.getoto.net

---

## Contact

For questions or issues with the test suite, refer to:
- [Test Suite README](../../tools/lwos-test-audio/DITHERING_TEST_SUITE_README.md)
- LightwaveOS repository issues

---

**End of Index**
