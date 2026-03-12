# BeatPulseTransportCore Reference Pair Generation

This directory contains the test harness for generating ground-truth reference (parameter, output) pairs from the **REAL firmware** BeatPulseTransportCore. These pairs calibrate the PyTorch port.

## Quick Start

### 1. Generate Reference Data

```bash
cd /sessions/adoring-festive-clarke/mnt/firmware-v3
pio run -e native_test
```

This runs the native test suite, including `test_transport_reference.cpp`, which:
- Creates a BeatPulseTransportCore instance
- Iterates through 576 parameter combinations (4 × 3 × 4 × 2 × 2 × 3)
- For each combination:
  - Resets the transport core
  - Injects a white (255, 255, 255) pulse at centre
  - Runs 100 frames of advection with the given parameters
  - Records the final 16-bit HDR state and 8-bit tone-mapped output
- Writes all pairs to `testbed/data/reference_pairs.bin`

### 2. Inspect Reference Data

```bash
cd /sessions/adoring-festive-clarke/mnt/firmware-v3/testbed/data
python3 read_reference.py
```

Output:
```
Loaded 576 reference pairs
State shape (16-bit): (576, 80, 3)
State shape (8-bit): (576, 80, 3)

Parameter ranges:
  offset       :  0.500000 to  4.000000 (  4 unique values)
  persistence  :  0.900000 to  0.990000 (  3 unique values)
  diffusion    :  0.000000 to  1.000000 (  4 unique values)
  dt           :  0.008000 to  0.016000 (  2 unique values)
  amount       :  0.500000 to  1.000000 (  2 unique values)
  spread       :  0.000000 to  1.000000 (  3 unique values)

16-bit state statistics:
  Mean:   845.3
  Std:    2104.5
  Min:    0
  Max:    65535

8-bit state statistics:
  Mean:    89.2
  Std:     58.4
  Min:    0
  Max:    255
```

## File Format

### Binary Structure

Header (20 bytes):
```c
uint32_t magic = 0x4C575246  // "LWRF" in ASCII
uint32_t version = 1
uint32_t num_pairs           // 576
uint32_t radial_len          // 80
uint32_t num_frames          // 100
```

Per pair (1444 bytes):
```c
float params[6]                 // 24 bytes: offset, persistence, diffusion, dt, amount, spread
uint16_t state_16bit[80*3]      // 480 bytes: radial distance × RGB (uint16)
uint8_t state_8bit[80*3]        // 240 bytes: radial distance × RGB (uint8 tone-mapped)
```

Total file size: `20 + 576 * 1444 = 831,744 bytes`

## Python Reader API

```python
import read_reference

# Load data
data = read_reference.read_reference_file('reference_pairs.bin')

# Access arrays
params = data['params']                # [576, 6] float32
state_16bit = data['state_16bit']      # [576, 80, 3] uint16
state_8bit = data['state_8bit']        # [576, 80, 3] uint8

# Get parameter ranges
ranges = data['param_ranges']
print(ranges['offset'])  # {'min': 0.5, 'max': 4.0, 'unique': 4}

# Print stats
read_reference.print_stats(data)
```

## Parameter Grid

**4 × 3 × 4 × 2 × 2 × 3 = 576 combinations**

| Parameter | Values | Count |
|-----------|--------|-------|
| `offsetPerFrameAt60Hz` | 0.5, 1.0, 2.0, 4.0 | 4 |
| `persistencePerFrame60Hz` | 0.90, 0.95, 0.99 | 3 |
| `diffusion01` | 0.0, 0.3, 0.6, 1.0 | 4 |
| `dt_seconds` | 0.008 (120 FPS), 0.016 (60 FPS) | 2 |
| injection `amount01` | 0.5, 1.0 | 2 |
| injection `spread01` | 0.0, 0.5, 1.0 | 3 |

## Test Flow

1. **Reset**: `core.resetAll()`, `core.setNowMs(0)`
2. **Inject**: White (255, 255, 255) pulse at centre with given `amount` and `spread`
3. **Advect**: 100 frames of `advectOutward()` with given parameters
4. **Extract**: Read `m_ps->hist[0]` (16-bit HDR state)
5. **Tone-map**: Apply knee tone-mapping (firmware formula) to produce 8-bit output
6. **Write**: Binary pair to file

## Tone-Mapping Formula

The 8-bit output applies the same knee tone-mapping used in `BeatPulseTransportCore::toCRGB8()`:

```cpp
float kneeToneMap(float in01, float knee = 0.5f) {
    if (in01 <= 0.0f) return 0.0f;
    const float mapped = in01 / (in01 + knee);
    return mapped * (1.0f + knee);
}
```

This formula:
- Boosts low levels (low-light visibility)
- Compresses highlights (prevents harsh clipping)
- At `in01=1.0, knee=0.5`: maps to exactly 1.0 (preserves peak brightness)

## C++ Test Details

File: `test/test_native/test_transport_reference.cpp`

Key implementation notes:
- Uses `#define private public` to access `BeatPulseTransportCore::m_ps` and `BeatPulseTransportCore::RGB16`
- Allocates transport core state via `malloc()` (native build, no PSRAM)
- Validates output is non-zero (injection should produce energy)
- Spot-checks file structure in separate test

## Common Issues

### File not found
**Symptom**: `FileNotFoundError: reference_pairs.bin`

**Solution**: Run the test harness:
```bash
pio run -e native_test
```

### Wrong file size
**Symptom**: File exists but is truncated or wrong size

**Solution**: Check for build failures:
```bash
pio run -e native_test -vv  # Verbose output
```

### Python import error
**Symptom**: `ModuleNotFoundError: numpy`

**Solution**:
```bash
pip3 install numpy
```

## Using Reference Data in PyTorch

```python
import torch
import read_reference

# Load reference
data = read_reference.read_reference_file('reference_pairs.bin')
params_tensor = torch.from_numpy(data['params']).float()  # [576, 6]
state_tensor = torch.from_numpy(data['state_16bit']).float() / 65535.0  # [576, 80, 3] normalized

# Train PyTorch model to predict state from params...
```

## Architecture

The reference generation is designed for **single-threaded, deterministic output**:
- No randomness (same seed produces identical results)
- No external I/O (only file write at end)
- Matches firmware math exactly (same tone-mapping, advection formulas)

## Next Steps

1. Verify `reference_pairs.bin` exists and is ~832 KB
2. Use `read_reference.py` to validate format
3. Feed into PyTorch training pipeline
4. Compare PyTorch predictions against ground truth
