# Serial Frame Capture - Quick Reference

## Complete System Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                     DitherBench Framework                           │
│                                                                     │
│  ┌────────────────────┐         ┌──────────────────────┐          │
│  │  Simulation Mode   │         │  Hardware Validation │          │
│  │                    │         │                      │          │
│  │  • Pure Python     │         │  • Real ESP32-S3     │          │
│  │  • 3 pipelines     │         │  • Serial capture    │          │
│  │  • Test stimuli    │         │  • 3 tap points      │          │
│  │  • Metrics         │         │  • Frame-accurate    │          │
│  └────────────────────┘         └──────────────────────┘          │
│           │                              │                         │
│           │                              │                         │
│           └──────────────┬───────────────┘                         │
│                          ▼                                         │
│              ┌───────────────────────┐                            │
│              │  Analysis & Reports   │                            │
│              │                       │                            │
│              │  • Side-by-side viz   │                            │
│              │  • MAE/RMSE metrics   │                            │
│              │  • Error distribution │                            │
│              └───────────────────────┘                            │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 📋 Pre-Flight Checklist

### Python Environment Setup

```bash
cd tools/dither_bench

# Create virtual environment
python3 -m venv .venv
source .venv/bin/activate

# Install dependencies
pip install -r requirements.txt

# Verify installation
pytest tests -v
```

### Firmware Preparation

1. **Add SerialFrameOutput.h** to `firmware/v2/src/core/actors/`
2. **Add serial commands** to `main.cpp` (see SERIAL_FRAME_CAPTURE_GUIDE.md)
3. **Build and flash** firmware
4. **Test serial connection**: `screen /dev/ttyUSB0 115200`

---

## 🚀 Usage Scenarios

### Scenario 1: Pure Simulation (No Hardware Required)

**Use Case**: Compare dithering algorithms purely in software

```bash
# Run benchmark with all pipelines
python run_bench.py \
  --output reports/sim_only_$(date +%Y%m%d_%H%M%S) \
  --frames 150 \
  --seed 42 \
  --leds 80

# Results saved to:
#   reports/sim_only_YYYYMMDD_HHMMSS/results.csv
#   reports/sim_only_YYYYMMDD_HHMMSS/run_config.json
```

**Expected Output**:
- CSV with banding/temporal/accuracy metrics for each pipeline
- JSON config snapshot
- Timing data

---

### Scenario 2: Hardware Frame Capture

**Use Case**: Extract real LED data from running firmware

```bash
# Terminal 1: Start Python receiver (BEFORE enabling capture)
python serial_frame_capture.py \
  --port /dev/ttyUSB0 \
  --baudrate 115200 \
  --output hardware_captures/effect_13 \
  --duration 10

# Terminal 2: Firmware serial console
> capture on 2          # Enable TAP_B (post-correction)
> 5                     # Switch to effect 13
> <wait 10 seconds>
> capture off

# Check captured frames
ls -lh hardware_captures/effect_13/
# Should see:
#   frame_000001_TAP_B_POST_CORRECTION.npz
#   frame_000002_TAP_B_POST_CORRECTION.npz
#   ...
#   session_info.json
```

**Expected Output**:
- NPZ files (one per captured frame)
- Session metadata JSON

---

### Scenario 3: Hardware vs Simulation Comparison

**Use Case**: Validate that simulation accurately models hardware

```bash
# Step 1: Capture hardware frames (see Scenario 2)
python serial_frame_capture.py \
  --port /dev/ttyUSB0 \
  --output hw_captures/ramp_test \
  --duration 5

# Step 2: Run simulation with same effect parameters
python run_bench.py \
  --output sim_runs/ramp_test_sim \
  --frames 150 \
  --seed 123

# Step 3: Compare
python analyze_captured_frames.py \
  --hardware hw_captures/ramp_test \
  --simulation sim_runs/ramp_test_sim \
  --pipeline lwos \
  --tap TAP_B_POST_CORRECTION \
  --output validation_results/ramp_test

# Check results
ls validation_results/ramp_test/
# Should see:
#   comparison_frame_000001.png   (side-by-side viz)
#   comparison_frame_000002.png
#   ...
#   metrics_summary.json           (per-frame MAE/RMSE)
#   metrics_over_time.png          (error trends)
```

**Expected Output**:
- Side-by-side visualizations (first 10 frames)
- Per-frame metrics (JSON)
- Aggregate statistics printed to console
- Error trends plot

---

## 🔍 Interpreting Results

### Simulation Metrics

| Metric | Good | Acceptable | Poor | Meaning |
|--------|------|------------|------|---------|
| **Banding Score** | < 5 | 5-15 | > 15 | Number of visible steps in gradient |
| **Temporal Stability** | < 1.0 | 1.0-3.0 | > 3.0 | Frame-to-frame variance (flicker) |
| **Accuracy (MAE)** | < 1.0 | 1.0-3.0 | > 3.0 | Mean absolute error vs ideal |

### Hardware Validation Metrics

| Metric | Perfect Match | Close Match | Mismatch |
|--------|---------------|-------------|----------|
| **MAE** | < 1.0 | 1.0-5.0 | > 10.0 |
| **RMSE** | < 2.0 | 2.0-8.0 | > 15.0 |
| **Correlation** | > 0.99 | 0.95-0.99 | < 0.95 |

**What does MAE < 1.0 mean?**
- Average per-channel difference is less than 1 unit (out of 255)
- Simulation is **bit-accurate** to hardware
- Dithering algorithm is correctly replicated

---

## 🛠️ Troubleshooting

### "No frames received" (serial_frame_capture.py)

```bash
# Check serial port
ls /dev/tty* | grep -i usb

# Test serial connection manually
screen /dev/ttyUSB0 115200
# Press 's' - should see firmware status output

# Verify capture is enabled
> capture status
# Should show: "Capture mode: ENABLED"

# Try higher baud rate (if supported)
python serial_frame_capture.py --port /dev/ttyUSB0 --baudrate 921600 ...
```

### "Sync errors" or frame corruption

**Causes**:
- Serial buffer overrun (firmware sending faster than receiver can read)
- Debug prints interfering with binary stream

**Solutions**:
```cpp
// In firmware main.cpp:
// 1. Reduce frame rate during capture
FastLED.setMaxRefreshRate(30);  // Down from 120 FPS

// 2. Disable debug prints
#define SERIAL_DEBUG 0

// 3. Increase baud rate
Serial.begin(921600);  // Up from 115200
```

### "Shape mismatch" (analyze_captured_frames.py)

**Cause**: Captured frames have different LED count than simulation

**Solution**:
```bash
# Check hardware capture LED count
python -c "import numpy as np; print(np.load('hw_captures/frame_000001_TAP_B_POST_CORRECTION.npz')['rgb'].shape)"

# Run simulation with matching LED count
python run_bench.py --leds <matching_count> ...
```

---

## 📊 Example Analysis Workflow

### Goal: Determine if LWOS Bayer dithering works correctly

```bash
# 1. Generate known gradient stimulus in firmware
#    (e.g., effect that produces smooth 0→255 ramp)

# 2. Capture hardware output
python serial_frame_capture.py \
  --port /dev/ttyUSB0 \
  --output hw_gradient \
  --duration 3

# 3. Simulate same gradient
python run_bench.py \
  --output sim_gradient \
  --frames 90 \
  --seed 123

# 4. Compare TAP_A (pre-correction) vs TAP_B (post-correction)
#    Expected: TAP_B should show +0/+1 deltas from Bayer matrix

python -c "
import numpy as np
import matplotlib.pyplot as plt

# Load frames
tap_a = np.load('hw_gradient/frame_000050_TAP_A_PRE_CORRECTION.npz')['rgb']
tap_b = np.load('hw_gradient/frame_000050_TAP_B_POST_CORRECTION.npz')['rgb']

# Compute Bayer delta
delta = tap_b.astype(np.int16) - tap_a.astype(np.int16)

# Visualize
plt.figure(figsize=(12, 4))
plt.subplot(121)
plt.imshow(delta[:160, :].T, aspect='auto', interpolation='nearest', vmin=0, vmax=1)
plt.colorbar()
plt.title('Bayer Delta (Strip 1)')
plt.subplot(122)
plt.hist(delta.flatten(), bins=range(-5, 6))
plt.title('Delta Distribution')
plt.savefig('bayer_analysis.png')
print('✓ Saved to bayer_analysis.png')
"

# 5. Expected result: Delta should be 0 or +1, following 4x4 pattern
```

---

## 🎯 Next Steps

### Immediate Actions

1. ✅ **Serial capture system** implemented (`serial_frame_capture.py`)
2. ✅ **Firmware output** defined (`SerialFrameOutput.h`)
3. ✅ **Analysis tools** ready (`analyze_captured_frames.py`)
4. ⏳ **Integration**: Add serial commands to `main.cpp`
5. ⏳ **Testing**: Capture first hardware frames and validate

### Future Enhancements

- **WebSocket capture**: Real-time streaming to dashboard
- **Automated test suite**: CI/CD integration with hardware-in-loop
- **Multi-effect validation**: Batch capture all effects
- **Perceptual metrics**: SSIM, MS-SSIM for visual similarity

---

## 📚 Related Documentation

- `docs/analysis/SERIAL_FRAME_CAPTURE_GUIDE.md` - Firmware integration details
- `docs/analysis/DITHERING_COMPARATIVE_REPORT.md` - Algorithm analysis
- `docs/analysis/WS2812_DITHERING_RESEARCH.md` - Literature review
- `tools/dither_bench/README.md` - Main DitherBench documentation

---

**System Status**: ✅ **READY FOR HARDWARE TESTING**

All Python components are implemented and tested. Firmware integration requires adding serial commands to `main.cpp` as documented in SERIAL_FRAME_CAPTURE_GUIDE.md.