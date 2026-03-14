# EdgeMixer Evaluation Harness

Repeatable evaluation workflow for the EdgeMixer post-processing engine. Captures LED frames from real K1 hardware, computes colour transform metrics, and generates comparison reports.

## Quick Start

```bash
cd tools

# List available profiles
python -m edgemixer_eval list

# Single profile evaluation
python -m edgemixer_eval single --port /dev/cu.usbmodem21401 --profile split_comp

# Profile shootout (8 default profiles, each 5s capture)
python -m edgemixer_eval shootout --port /dev/cu.usbmodem21401

# Dual-K1 control vs treatment
python -m edgemixer_eval dual \
    --port-control /dev/cu.usbmodem21401 \
    --port-treatment /dev/cu.usbmodem212401 \
    --profile-treatment split_comp_gradient
```

## Dependencies

Requires `numpy` and `pyserial`. Both available in `tools/led_capture/.venv/`:

```bash
source tools/led_capture/.venv/bin/activate
```

## Subcommands

### `single` — Single Profile Evaluation

Captures frames with one EdgeMixer profile and reports metrics + safety gates.

```
python -m edgemixer_eval single --port PORT --profile PROFILE [--frames 300] [--fps 30] [--output DIR] [--stimulus LABEL]
```

### `shootout` — Multi-Profile Comparison

Cycles through profiles on one device, always including MIRROR as baseline. Produces a ranked comparison table and per-profile delta reports.

```
python -m edgemixer_eval shootout --port PORT [--profiles mirror,analogous,split_comp] [--frames 150] [--output DIR]
```

Default profiles: mirror, analogous, complementary, split_comp, sat_veil, analogous_gradient, split_comp_gradient, split_comp_full.

### `dual` — Dual-K1 Control vs Treatment

Two K1 devices: control (MIRROR) and treatment (specified profile). Captures from both and produces a side-by-side comparison.

```
python -m edgemixer_eval dual --port-control PORT_A --port-treatment PORT_B --profile-treatment PROFILE [--frames 300] [--output DIR]
```

### `list` — List Profiles

Shows all 15 named profiles with settings and descriptions, plus the 6 stimulus categories.

## Profiles

| Profile | Mode | Spread | Spatial | Temporal | Notes |
|---------|------|--------|---------|----------|-------|
| mirror | 0 | 30 | uniform | static | Control baseline |
| analogous | 1 | 30 | uniform | static | +30deg hue shift |
| complementary | 2 | 30 | uniform | static | +180deg, -15% sat |
| split_comp | 3 | 30 | uniform | static | +150deg, -10% sat |
| sat_veil | 4 | 30 | uniform | static | Desaturate by spread |
| analogous_gradient | 1 | 30 | gradient | static | Analogous, zero at centre |
| split_comp_gradient | 3 | 30 | gradient | static | Split comp, zero at centre |
| split_comp_full | 3 | 30 | gradient | rms_gate | Full composable stack |
| analogous_rms | 1 | 30 | uniform | rms_gate | Audio-reactive analogous |
| split_comp_subtle | 3 | 30 | uniform | static | Half strength (128/255) |

## Metrics

### Behavioural Metrics

| Metric | Meaning | Higher = |
|--------|---------|----------|
| Strip divergence L2 | RGB distance between strip1 and strip2 | More visible effect |
| Hue shift mean | Circular hue rotation (degrees) | More colour separation |
| Saturation delta | strip2 sat - strip1 sat | More saturated |
| Brightness ratio | strip2 brightness / strip1 brightness | 1.0 = ideal |
| Spatial gradient effectiveness | edge divergence / centre divergence | Better gradient |
| Temporal responsiveness | Pearson(RMS, divergence) | Stronger audio coupling |

### Safety Metrics

| Gate | Threshold | Meaning |
|------|-----------|---------|
| Near-black leaks | Must be 0 | No colour injected into dark pixels |
| Brightness preservation | 0.70 - 1.30 | No extreme brightness change |
| Mirror identity | divergence < 1.0 | MIRROR mode produces identical strips |

## Output

Text reports go to stdout. JSON export via `--output DIR`:

```bash
python -m edgemixer_eval shootout --port /dev/cu.usbmodem21401 --output ./eval_results/
```

## Capture Details

- Uses TAP C (post-EdgeMixer, pre-WS2812) for accurate evaluation
- Frame format: serial v2 (1009 bytes: 17 header + 960 RGB + 32 metrics trailer)
- Strip layout in frame: strip1 = indices 0-159, strip2 = indices 160-319
- Default: 30 FPS capture, 0.5s settle time between profile changes

## Limitations

- Capture is sequential in dual mode (control first, then treatment). True simultaneous capture would require threading.
- Stimulus categories are operator labels only, not automated. The evaluation measures whatever content is currently playing.
- Firmware version and git commit are not automatically harvested from the device. Tag your runs with `--stimulus` labels.
- The serial port open may reset the ESP32-S3 board (USB CDC DTR/RTS behaviour). A 3-second settle time is applied.
- Aesthetic judgement (depth perception, elegance, musical legibility) cannot be inferred from metrics alone. Where visual review is required, the report states this explicitly.
