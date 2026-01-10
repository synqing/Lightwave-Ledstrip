# LightwaveOS Dithering Test Suite

Complete test harness for evaluating dithering performance in LightwaveOS v2.

## Overview

This test suite provides:
1. **Host-side simulation** - Python reimplementation of firmware dithering pipeline
2. **Firmware capture tooling** - TAP A/B/C frame extraction via serial
3. **Jupyter notebook** - Interactive analysis and visualization
4. **Quantitative metrics** - Banding score, flicker energy, power budget

## Components

### 1. `dithering_test_suite.py` - Host Simulation

Simulates the v2 rendering pipeline to compare dithering configurations:
- **Config A**: Bayer ON + Temporal ON (current default)
- **Config B**: Temporal only
- **Config C**: Bayer only
- **Config D**: No dithering

**Usage:**
```bash
python dithering_test_suite.py --output ./test_results
```

**Outputs:**
- `test_results/metrics.csv` - Quantitative metrics for all configs × stimuli
- `test_results/banding_comparison.png` - Banding score plots
- `test_results/flicker_comparison.png` - Temporal flicker plots
- `test_results/tradeoff_scatter.png` - Banding vs flicker scatter
- `test_results/dashboard.html` - Interactive Plotly dashboard

### 2. `capture_frames.py` - Firmware Frame Capture

Captures real LED frames from running LightwaveOS firmware via serial TAP points:
- **TAP A**: Pre-correction (after effect render, before ColorCorrectionEngine)
- **TAP B**: Post-correction (after ColorCorrectionEngine, includes Bayer dithering)
- **TAP C**: Post-output (after FastLED.show(), includes temporal dithering)

**Prerequisites:**
1. Flash firmware with capture enabled (already implemented in v2)
2. Connect device via USB serial

**Usage:**
```bash
# Enable capture in firmware (send via serial terminal):
capture on 0x07  # Enable all taps (A=0x01, B=0x02, C=0x04)

# Run capture tool:
python capture_frames.py --port /dev/cu.usbmodem21401 --frames 100 --output ./captures

# Disable capture:
capture off
```

**Outputs:**
- `captures/<timestamp>/frame_NNNN_TAP_X.npy` - Frame data (320×3 RGB arrays)
- `captures/<timestamp>/metadata.json` - Frame metadata (effect_id, palette_id, etc.)

### 3. `dithering_analysis.ipynb` - Interactive Analysis

Jupyter notebook combining host simulation and firmware captures for comprehensive analysis.

**Sections:**
1. **Host Simulation** - Load and visualize simulation results
2. **Firmware TAP Analysis** - Compare TAP A/B/C captures
3. **Temporal Flicker** - Per-LED variance heatmaps
4. **Recommendations** - Data-driven conclusions

**Usage:**
```bash
jupyter notebook dithering_analysis.ipynb
```

## Installation

```bash
# Create virtual environment
python3 -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate

# Install dependencies
pip install numpy scipy pandas matplotlib plotly bokeh pyserial jupyter
```

## Quick Start

### Option 1: Simulation Only (no hardware)

```bash
# Run host-side simulation
python dithering_test_suite.py --output ./results

# View results
open results/dashboard.html  # Interactive plots
```

### Option 2: Full Analysis (with hardware)

```bash
# 1. Run simulation
python dithering_test_suite.py --output ./results

# 2. Capture firmware frames
python capture_frames.py --port /dev/cu.usbmodem21401 --frames 100 --output ./captures

# 3. Analyze in Jupyter
jupyter notebook dithering_analysis.ipynb
```

## Test Stimuli

The suite tests 5 canonical stimuli:

| Stimulus | Description | Tests |
|----------|-------------|-------|
| `gradient_red` | 0→255 red ramp | Gradient banding visibility |
| `gradient_green` | 0→255 green ramp | Gradient banding visibility |
| `gradient_blue` | 0→255 blue ramp | Gradient banding visibility |
| `near_black` | 0→20 white ramp | Low-brightness flicker/shimmer |
| `palette_blend` | HSV hue wheel | Palette transition smoothness |

## Metrics

### Gradient Banding Score

Measures gradient smoothness via first-derivative entropy:
- **Lower score** = smoother gradient (good)
- **Higher score** = more banding (bad)

Formula:
```
derivative = abs(diff(pixel_values))
histogram = hist(derivative, bins=20)
entropy = -sum(histogram * log(histogram))
banding_score = max_entropy - entropy
```

### Temporal Flicker Energy

Measures frame-to-frame stability:
- **Lower energy** = more stable (good)
- **Higher energy** = more flicker/shimmer (bad)

Formula:
```
variance = var(pixel_values_across_frames, axis=time)
flicker_energy = mean(variance)
```

### Power Budget

Estimates LED power consumption:
- **Higher** = brighter (more LEDs on)
- **Lower** = dimmer (fewer LEDs on)

Formula:
```
power_budget = sum(all_rgb_values)
```

## Expected Results

### Simulation (Typical Values)

| Config | Banding Score | Flicker Energy | Power Budget |
|--------|---------------|----------------|--------------|
| **A (both)** | **1.2** | 2.5 | 51000 |
| B (temporal) | 1.8 | **1.8** | 50800 |
| C (bayer) | **1.3** | **0.8** | 51200 |
| D (none) | 2.4 | **0.0** | 50500 |

**Interpretation:**
- **Config A** (current default) balances banding and flicker
- **Config C** (Bayer only) has lowest flicker → best for camera/filming
- **Config D** (no dithering) has worst banding → avoid except for special cases

### Firmware Capture

TAP A vs TAP B comparison should show:
- **Banding improvement**: 15-25% reduction in banding score
- **Visible pattern**: 4×4 Bayer matrix visible in TAP B difference image
- **No temporal changes**: Bayer is spatial (same pattern every frame)

TAP B vs TAP C comparison should show:
- **Temporal variance**: ±1-2 counts per pixel (random noise from FastLED temporal)
- **No spatial pattern**: Noise is uniformly distributed

## Troubleshooting

### Simulation runs but produces NaN/Inf values

**Cause**: Gamma LUT or dithering overflow  
**Fix**: Check that input buffers are uint8, not float

### Firmware capture times out

**Cause**: Capture not enabled in firmware, or serial buffer overflow  
**Fix**:
1. Send `capture on 0x07` via serial terminal
2. Verify response: `Capture enabled`
3. Reduce `--frames` count (try 10 frames first)
4. Increase baudrate: `--baudrate 230400`

### Jupyter kernel crashes during analysis

**Cause**: Memory exhaustion (loading too many frames)  
**Fix**:
1. Reduce number of frames in capture
2. Process frames in batches
3. Delete large variables after use: `del frames; gc.collect()`

## Architecture

```
Host Simulation:
  Input Stimulus (gradient/palette)
    ↓
  Gamma Correction (LUT, gamma=2.2)
    ↓
  Bayer Dithering (spatial, 4×4 matrix) [optional]
    ↓
  Temporal Dithering (random ±1) [optional]
    ↓
  Output Buffer
    ↓
  Metrics (banding, flicker, power)

Firmware Pipeline:
  Effect Render
    ↓
  TAP A (pre-correction)
    ↓
  ColorCorrectionEngine (gamma, Bayer, etc.)
    ↓
  TAP B (post-correction)
    ↓
  FastLED.show() (temporal dithering)
    ↓
  TAP C (post-output)
    ↓
  WS2812 LEDs
```

## Future Enhancements

- [ ] Blue-noise temporal dithering (superior to random)
- [ ] Error-diffusion dithering (Floyd-Steinberg)
- [ ] Adaptive dithering (per-effect tuning)
- [ ] Camera capture validation (60fps video, rolling shutter analysis)
- [ ] Power meter integration (real current draw measurement)
- [ ] Perceptual metrics (CIEDE2000 color accuracy)
- [ ] Bokeh dashboard (alternative to Plotly)
- [ ] Automated regression testing (CI/CD integration)

## References

1. **Bayer Dithering**  
   https://en.wikipedia.org/wiki/Ordered_dithering

2. **FastLED Temporal Dithering**  
   https://github.com/FastLED/FastLED/wiki/FastLED-Temporal-Dithering

3. **ColorCorrectionEngine Source**  
   `firmware/v2/src/effects/enhancement/ColorCorrectionEngine.cpp`

4. **RendererNode Pipeline**  
   `firmware/v2/src/core/actors/RendererNode.cpp`

## License

Part of LightwaveOS project. See repository root LICENSE for details.

---

**Last Updated**: 2026-01-10  
**Version**: 1.0.0  
**Maintainer**: LightwaveOS Development Team
