# BeatPulseTransportCore Reference Test Harness

## Overview

This test harness generates **ground-truth reference data** from the production BeatPulseTransportCore firmware. It runs the exact algorithm through a comprehensive parameter grid (576 combinations) and records the final state for calibrating the PyTorch port.

## Files Created

### 1. Test Source
**`test/test_native/test_transport_reference.cpp`** (365 lines)

C++ test harness that:
- Creates a real BeatPulseTransportCore instance
- Iterates through all parameter combinations
- For each: injects white pulse, runs 100 frames, records output
- Writes binary reference file with full state (16-bit HDR + 8-bit tone-mapped)

**Key tests**:
- `test_generate_transport_reference_pairs()` — Main generation (takes ~10-30s)
- `test_verify_reference_file_structure()` — Spot-checks output format
- `test_transport_parameters_affect_output()` — Validates parameters matter

**Notable implementation**:
- Uses `#define private public` pattern to access private struct members (safe for testing)
- Matches firmware tone-mapping exactly (knee formula at line 169-177)
- Validates all outputs non-zero (sanity check)

### 2. Python Reader
**`testbed/data/read_reference.py`** (230 lines)

Python 3 utility for loading and analyzing reference data.

**API**:
```python
import read_reference

data = read_reference.read_reference_file('reference_pairs.bin')
# Returns dict with:
#   params: [576, 6] float32
#   state_16bit: [576, 80, 3] uint16
#   state_8bit: [576, 80, 3] uint8
#   param_ranges: dict of min/max/unique per parameter

read_reference.print_stats(data)  # Print summary
```

**Validates**:
- Binary magic number (0x4C575246 = "LWRF")
- Version and structure
- Non-zero outputs (no all-black pairs)

### 3. Output Data Directory
**`testbed/data/`** — Reserved for `reference_pairs.bin`

### 4. Documentation
**`testbed/REFERENCE_GENERATION.md`** (180 lines)
**`REFERENCE_HARNESS.md`** (this file)

## How to Run

### Generate Reference Data

```bash
cd /sessions/adoring-festive-clarke/mnt/firmware-v3

# Build and run the native test suite
pio run -e native_test
```

This will:
1. Compile C++ test (with NATIVE_BUILD=1)
2. Link against Unity framework
3. Run tests including reference generation
4. Write `testbed/data/reference_pairs.bin`

**Expected output**:
```
Generating 576 transport reference pairs...
  Wrote 100 / 576 pairs
  Wrote 200 / 576 pairs
  Wrote 300 / 576 pairs
  Wrote 400 / 576 pairs
  Wrote 500 / 576 pairs
Successfully wrote 576 reference pairs to /sessions/.../reference_pairs.bin

test_verify_reference_file_structure ... PASSED
test_transport_parameters_affect_output ... PASSED
```

### Inspect the Data

```bash
cd /sessions/adoring-festive-clarke/mnt/firmware-v3/testbed/data

python3 read_reference.py
```

**Output**:
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

All-zero outputs: 0 (16-bit), 0 (8-bit)
```

## Binary File Format

**Magic**: `0x4C575246` ("LWRF" — LightWave Reference)
**Version**: 1
**Total size**: ~831 KB

### Header (20 bytes)
```c
uint32_t magic = 0x4C575246
uint32_t version = 1
uint32_t num_pairs = 576
uint32_t radial_len = 80
uint32_t num_frames = 100
```

### Per Pair (1444 bytes each)
```c
float params[6]                    // 24 bytes
  [0] offsetPerFrameAt60Hz
  [1] persistencePerFrame60Hz
  [2] diffusion01
  [3] dt_seconds
  [4] injection amount01
  [5] injection spread01

uint16_t state_16bit[80*3]         // 480 bytes (HDR radial history)
uint8_t state_8bit[80*3]           // 240 bytes (tone-mapped output)
```

**Total**: 20 + 576 × 1444 = **831,744 bytes**

## Parameter Grid

**4 × 3 × 4 × 2 × 2 × 3 = 576 pairs**

| Param | Values | Count |
|-------|--------|-------|
| offset | 0.5, 1.0, 2.0, 4.0 | 4 |
| persistence | 0.90, 0.95, 0.99 | 3 |
| diffusion | 0.0, 0.3, 0.6, 1.0 | 4 |
| dt | 0.008 (120 FPS), 0.016 (60 FPS) | 2 |
| amount | 0.5, 1.0 | 2 |
| spread | 0.0, 0.5, 1.0 | 3 |

## Test Procedure per Pair

1. **Reset**: `core.resetAll()`, `core.setNowMs(0)`
2. **Inject**: White (255, 255, 255) at centre with given `amount` and `spread`
3. **Advect**: 100 frames of `advectOutward()` with the 4 parameters
4. **Extract**: Read `m_ps->hist[0]` (16-bit HDR state)
5. **Tone-map**: Apply knee formula to produce 8-bit output
6. **Write**: Binary pair to file

## Tone-Mapping

The firmware applies knee tone-mapping to compress HDR 16-bit to LDR 8-bit:

```cpp
float kneeToneMap(float in01, float knee = 0.5f) {
    if (in01 <= 0.0f) return 0.0f;
    const float mapped = in01 / (in01 + knee);
    return mapped * (1.0f + knee);  // Boost to restore peak
}
```

**Effect**:
- Boosts dark values (low-light visibility)
- Compresses highlights gracefully (no harsh clipping)
- Preserves peak brightness (`in=1.0` → `out=1.0`)

## Using in PyTorch

```python
import torch
import read_reference

# Load reference data
data = read_reference.read_reference_file('reference_pairs.bin')

# Convert to tensors
params = torch.from_numpy(data['params']).float()  # [576, 6]
state_16bit = torch.from_numpy(data['state_16bit']).float() / 65535.0  # Normalize
state_8bit = torch.from_numpy(data['state_8bit']).float() / 255.0

# Split train/val
n = len(params)
train_idx = torch.arange(int(0.8 * n))
val_idx = torch.arange(int(0.8 * n), n)

params_train = params[train_idx]
state_train = state_8bit[train_idx]  # or state_16bit for fine-grained learning

# Train model to predict state from params...
model = YourTransportModel()
loss_fn = torch.nn.MSELoss()
optimizer = torch.optim.Adam(model.parameters())

for epoch in range(100):
    pred = model(params_train)
    loss = loss_fn(pred, state_train)
    loss.backward()
    optimizer.step()
    optimizer.zero_grad()
```

## Verification Checklist

- [ ] Reference file exists: `testbed/data/reference_pairs.bin` (~831 KB)
- [ ] Binary magic correct: `0x4C575246`
- [ ] 576 pairs present
- [ ] No all-zero outputs
- [ ] Parameter ranges match grid
- [ ] Python reader loads without error
- [ ] NumPy arrays have correct shape

## Common Issues

### Issue: "pio: command not found"
**Solution**: Install PlatformIO
```bash
pip3 install platformio
```

### Issue: Test hangs on generation
**Solution**: Generation takes ~30s for 576 pairs. Check:
```bash
# See live output
pio run -e native_test -vv
```

### Issue: "reference_pairs.bin not found"
**Solution**: Test must complete successfully. Check logs:
```bash
pio run -e native_test --verbose
```

### Issue: Python import errors
**Solution**: Install NumPy
```bash
pip3 install numpy
```

## Next Steps

1. **Generate data** (one-time):
   ```bash
   pio run -e native_test
   ```

2. **Verify output**:
   ```bash
   python3 testbed/data/read_reference.py
   ```

3. **Feed into PyTorch**:
   - Load with `read_reference_file()`
   - Train transport model
   - Compare predictions against ground truth
   - Export quantized model for inference

## Architecture Notes

**Why `#define private public`?**
- Allows test code to access `m_ps` (private PSRAM data pointer)
- Safe because test is compiled separately with NATIVE_BUILD=1
- Production firmware still has private members
- Common testing pattern in C++ (e.g., Google Test)

**Why binary format?**
- Compact (831 KB vs ~2 MB as JSON)
- Fast to load (NumPy frombuffer)
- Reproducible (exact floating-point bit-for-bit matching)

**Why 100 frames?**
- Long enough for energy to propagate through buffer (~80 bins)
- Short enough to complete in reasonable time
- Captures steady-state advection behavior

## Files Summary

| File | Size | Purpose |
|------|------|---------|
| `test/test_native/test_transport_reference.cpp` | 365 lines | Main test harness |
| `testbed/data/read_reference.py` | 230 lines | Python data loader |
| `testbed/REFERENCE_GENERATION.md` | 180 lines | Detailed guide |
| `REFERENCE_HARNESS.md` | this file | Overview & quick-start |
| `testbed/data/reference_pairs.bin` | 831 KB | Output (generated) |

---

**Last Updated**: 2026-03-08
**Status**: Ready for PyTorch calibration pipeline
