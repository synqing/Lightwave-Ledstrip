# Colour Pipeline Testbed

**Purpose:** Definitive, reproducible testing framework for measuring colour-processing impact on WS2812 output and LGP interference physics.

---

## Overview

This testbed captures **direct-source** frame data at three pipeline stages:
- **Tap A (Pre-Correction):** After `renderFrame()`, before `processBuffer()`
- **Tap B (Post-Correction):** After `processBuffer()`, before `showLeds()`
- **Tap C (Pre-WS2812):** After `showLeds()` copy, before `FastLED.show()`

Captures are exported via:
1. **Serial/USB binary dump** (ground-truth, deterministic)
2. **WebSocket `led_stream`** (convenient, real-time)

---

## Installation

```bash
cd tools/colour_testbed
pip install -r requirements.txt
```

**Requirements:**
- Python 3.8+
- numpy
- matplotlib
- scipy
- pyserial (for serial capture)

---

## Usage

### 1. Capture Frames from Device

**Serial Method (Recommended):**
```bash
# Enable capture mode (all taps)
# Send via serial: capture on

# Wait for frames to be captured, then dump:
# Send via serial: capture dump a  (for Tap A)
# Send via serial: capture dump b  (for Tap B)
# Send via serial: capture dump c  (for Tap C)

# Save serial output to file:
pyserial-capture /dev/tty.usbmodem1101 115200 > baseline_tap_a.bin
```

**WebSocket Method:**
```python
from capture_led_stream import capture_frames
capture_frames(host="192.168.1.100", effect_id=3, num_frames=100, output_file="confetti_tap_b.bin")
```

### 2. Parse Captured Frames

```python
from frame_parser import parse_serial_dump, parse_led_stream

# Parse serial dump
frames = parse_serial_dump("baseline_tap_a.bin")

# Parse led_stream capture
frames = parse_led_stream("confetti_tap_b.bin")
```

### 3. Analyze Frames

```python
from analyse import compare_baseline_candidate, generate_report

# Compare baseline vs candidate
results = compare_baseline_candidate(
    baseline_dir="captures/baseline",
    candidate_dir="captures/candidate",
    effect_id=3
)

# Generate comprehensive report
generate_report(results, output_dir="reports/effect_3")
```

### 4. Automated Test Run

```python
from test_runner import run_full_test_suite

# Capture all effects (0-67) for baseline and candidate
run_full_test_suite(
    baseline_firmware="baseline.bin",
    candidate_firmware="candidate.bin",
    output_dir="test_results"
)
```

---

## Frame Format

### Serial Dump Format

**Header (17 bytes):**
```
[0xFD]                    // Magic byte
[0x01]                    // Version
[tap_id]                  // 0=Tap A, 1=Tap B, 2=Tap C
[effect_id]               // Effect ID (0-67)
[palette_id]              // Palette ID
[brightness]              // Brightness (0-255)
[speed]                   // Speed (0-50)
[frame_index_0-3]         // Frame index (uint32_t, little-endian)
[timestamp_0-3]           // Timestamp in microseconds (uint32_t, little-endian)
[frame_len_0-1]           // Frame length in bytes (uint16_t, little-endian) = 320*3 = 960
```

**Payload (960 bytes):**
```
[RGB×320]                 // RGB data for 320 LEDs (interleaved: strip1[0..159], strip2[0..159])
```

**Total:** 977 bytes per frame

### Led Stream Format

See `v2/src/network/WebServer.h:LedStreamConfig` for frame format v1 specification.

---

## Metrics

### Per-Pixel Metrics
- **L1 Delta:** Sum of absolute differences per channel
- **L2 Delta:** Euclidean distance in RGB space
- **Max Error:** Maximum per-channel difference
- **PSNR:** Peak Signal-to-Noise Ratio

### Edge Amplitude Metrics
- **I₁/I₂ Ratio:** Mean amplitude ratio between strips
- **Ratio Stability:** Coefficient of variation over time
- **Spatial Symmetry:** Correlation between left/right halves

### Temporal Metrics
- **Frame-to-Frame Delta:** Energy of frame differences
- **Trail Persistence:** Quantiles of low-value pixel lifetimes

### Channel Transfer Curves
- **Empirical LUT:** Input→output mapping per channel
- **Gamma Estimation:** Fitted gamma value from transfer curve

---

## Output

### Visual Comparisons
- Side-by-side strip renderings (PNG)
- Difference heatmaps (PNG)
- Time-series plots (ratio, luma) (PNG)

### Metrics Report
- Per-effect summary table (Markdown/HTML)
- Pass/fail status per metric
- Root cause analysis

### Performance Benchmarks
- FPS comparison
- Memory usage
- Capture overhead

---

## Files

- `frame_parser.py` - Parse serial dumps and led_stream captures
- `analyse.py` - Core analysis engine (metrics, comparisons)
- `visualize.py` - Generate visualizations (plots, heatmaps)
- `test_runner.py` - Automated test suite runner
- `report.py` - Generate comprehensive reports
- `capture_led_stream.py` - WebSocket capture utility

---

## Example Workflow

```bash
# 1. Flash baseline firmware
pio run -e esp32dev -t upload

# 2. Enable capture, set effect 3 (Confetti), wait 5 seconds
# Serial: capture on
# Serial: 3
# (wait 5 seconds)

# 3. Dump Tap A
# Serial: capture dump a
# Save output: baseline_confetti_tap_a.bin

# 4. Flash candidate firmware
pio run -e esp32dev -t upload

# 5. Repeat steps 2-3
# Save output: candidate_confetti_tap_a.bin

# 6. Analyze
python analyse.py baseline_confetti_tap_a.bin candidate_confetti_tap_a.bin

# 7. Generate report
python report.py --baseline baseline_confetti_tap_a.bin --candidate candidate_confetti_tap_a.bin --output reports/
```

---

**See individual script documentation for detailed usage.**

