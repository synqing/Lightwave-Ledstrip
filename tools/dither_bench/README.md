# DitherBench: Quantitative Dithering Assessment Framework

## Overview

DitherBench is a reproducible, audit-grade harness for comparing dithering algorithms used in LED controllers. It implements exact replicas of three systems:

- **LightwaveOS v2**: Bayer 4×4 ordered spatial dithering + optional FastLED temporal dithering
- **SensoryBridge 4.1.1**: 4-phase threshold temporal quantiser with per-frame noise offsets
- **Emotiscope (1.1/1.2/2.0)**: Sigma-delta error-accumulation temporal quantiser with deadband

## Purpose

To provide **data-driven evidence** for:
- Which algorithm best reduces banding at equal flicker levels
- Which minimises shimmer near black
- Spatial patterning characteristics (Bayer vs temporal phase offsets)
- Trade-offs for porting algorithms into LightwaveOS v2

## Quick Start

### Simulation-Only Analysis

```bash
# Create virtual environment
python -m venv .venv
source .venv/bin/activate  # On Windows: .venv\Scripts\activate

# Install dependencies
pip install -r requirements.txt

# Run unit tests
pytest tests -v

# Run full benchmark
python run_bench.py --output reports/$(date +%Y%m%d_%H%M%S) --frames 512 --seed 123

# Generate comparison report
python generate_report.py --input reports/<timestamp>
```

### Hardware Validation Workflow

```bash
# 1. Capture frames from firmware via serial
python serial_frame_capture.py \
  --port /dev/ttyUSB0 \
  --baudrate 115200 \
  --output hardware_captures/effect_42 \
  --duration 10

# 2. Run simulation for same effect
python run_bench.py \
  --output simulation_runs/effect_42_sim \
  --frames 150 \
  --seed 123

# 3. Compare hardware vs simulation
python analyze_captured_frames.py \
  --hardware hardware_captures/effect_42 \
  --simulation simulation_runs/effect_42_sim \
  --pipeline lwos \
  --output validation_results/effect_42

# Results include:
#   - Side-by-side frame comparisons
#   - Per-frame metrics (MAE, RMSE, correlation)
#   - Aggregate statistics
#   - Error distribution plots
```

## Project Structure

```
tools/dither_bench/
├── README.md                      # This file
├── requirements.txt               # Pinned Python dependencies
├── run_bench.py                  # Main benchmark orchestrator
├── generate_report.py            # Report generation script
├── serial_frame_capture.py       # Hardware frame capture receiver
├── analyze_captured_frames.py    # Hardware vs simulation comparison
├── src/
│   └── dither_bench/             # Main package
│       ├── __init__.py
│       ├── quantisers/           # Dithering algorithm implementations
│       │   ├── __init__.py
│       │   ├── sensorybridge.py  # SB 4-phase temporal
│       │   ├── emotiscope.py     # Emotiscope sigma-delta
│       │   └── lwos.py           # LWOS Bayer + temporal model
│       ├── pipelines/            # Full rendering pipeline simulators
│       │   ├── __init__.py
│       │   ├── base.py
│       │   ├── lwos_pipeline.py
│       │   ├── sb_pipeline.py
│       │   └── emo_pipeline.py
│       ├── stimuli/              # Test pattern generators
│       │   ├── __init__.py
│       │   └── generators.py
│       ├── metrics/              # Analysis and measurement tools
│       │   ├── __init__.py
│       │   ├── banding.py
│       │   ├── temporal.py
│       │   └── accuracy.py
│       └── utils/                # Helper functions
│           ├── __init__.py
│           └── config.py
├── tests/                        # pytest unit tests
│   ├── __init__.py
│   ├── test_quantisers.py
│   ├── test_emotiscope.py
│   ├── test_lwos.py
│   ├── test_pipelines.py
│   └── test_metrics.py
├── datasets/                     # Small deterministic test data
│   └── .gitkeep
├── hardware_captures/            # Captured frames from firmware (gitignored)
│   └── .gitignore
├── simulation_runs/              # Simulation outputs (gitignored)
│   └── .gitignore
├── validation_results/           # Hardware vs sim analysis (gitignored)
│   └── .gitignore
└── reports/                      # Generated outputs (timestamped)
    └── .gitignore                # Exclude large artefacts
```

## Reproducibility

All runs are:
- **Deterministic**: Controlled by `--seed` parameter
- **Versioned**: Each report includes git SHA, config snapshot, and timestamps
- **Auditable**: Full parameter logs and intermediate data preserved

## Reference Sources

Implementation is directly derived from:
- SensoryBridge: `/Users/spectrasynq/Workspace_Management/Software/K1.node1/references/Sensorybridge.sourcecode/SensoryBridge-4.1.1/SENSORY_BRIDGE_FIRMWARE/led_utilities.h`
- Emotiscope: `/Users/spectrasynq/Workspace_Management/Software/K1.node1/references/Emotiscope.sourcecode/Emotiscope-1.2/src/led_driver.h`
- LightwaveOS v2: `firmware/v2/src/effects/enhancement/ColorCorrectionEngine.cpp`

## Output Artifacts

Each benchmark run produces:
- `results.csv` - One row per test scenario
- `run_config.json` - Complete parameter snapshot
- `timings.json` - Per-stage performance metrics
- `frames.npz` - Optional frame captures (gated by flag)
- `plots/` - Matplotlib/Plotly visualizations
- `report.md` - Executive summary with conclusions

## Development

```bash
# Run tests with coverage
pytest tests --cov=src/dither_bench --cov-report=html

# Type checking
mypy src/dither_bench

# Formatting
black src tests
```

## Hardware Validation

### Firmware Integration

To enable frame capture from firmware:

1. **Add serial output header**: `firmware/v2/src/core/actors/SerialFrameOutput.h` (provided)
2. **Add serial commands**: See `docs/analysis/SERIAL_FRAME_CAPTURE_GUIDE.md`
3. **Flash firmware** with capture support
4. **Capture frames** using `serial_frame_capture.py`

### Serial Commands (Firmware)

```
capture on              # Enable all taps
capture on 2            # Enable TAP_B only (post-correction)
capture status          # Show capture state
capture send 1          # Send single frame from TAP_B
capture stream 1        # Stream TAP_B continuously
capture off             # Disable capture
```

### Typical Workflow

```bash
# Terminal 1: Start Python receiver
python serial_frame_capture.py \
  --port /dev/ttyUSB0 \
  --output hw_captures \
  --duration 30

# Terminal 2: Firmware serial console
> capture on 2          # Enable TAP_B
> <let effects run>
> capture off

# Analysis: Compare captured frames with simulation
python analyze_captured_frames.py \
  --hardware hw_captures \
  --simulation sim_runs/lwos_test \
  --pipeline lwos \
  --tap TAP_B_POST_CORRECTION \
  --output validation_results
```

### Expected Outcomes

- **Perfect match (MAE < 1.0)**: Simulation accurately models hardware
- **Close match (MAE < 5.0)**: Minor discrepancies (timing, floating-point precision)
- **Poor match (MAE > 10.0)**: Algorithm mismatch or configuration error

## License

Part of the LightwaveOS project. See repository root for license.
