# BeatPulseTransportCore: PyTorch Physics Engine

## Overview

This module provides a faithful PyTorch port of the ESP32 firmware's `BeatPulseTransportCore` physics engine. It implements semi-Lagrangian advection with temporal feedback, diffusion, and edge damping for music visualisation systems driving 320 WS2812 LEDs via a light guide plate.

### Key Characteristics

- **Float32 throughout**: No uint16 intermediate truncation (intentionally different from firmware)
- **Fully batched**: Support for batch dimension `[B, radial_len, 3]`
- **Differentiable**: All operations support PyTorch autograd for gradient-based training
- **Physics-based**: Implements 5 sequential PDEs matching the firmware semantics exactly

## Core Operations

### 1. Semi-Lagrangian Advection (`_advect_outward`)

**Physics**: Transport energy outward in radial space with subpixel motion.

```
For each bin i:
  newPos = i + dt_offset
  if newPos in valid range:
    scatter to bins floor(newPos) and ceil(newPos)
    apply persistence: energy *= exp(dt * log(persistence_per_frame))
```

**2-tap linear interpolation**: Energy at position i is split between two adjacent bins based on fractional distance.

**Persistence**: Exponential decay corrected for framerate: `persistence = p^(dt * 60)`

### 2. Injection at Centre (`_inject_at_centre`)

**Physics**: Gaussian-like 5-bin kernel injection for organic bloom.

```
Kernel weights (sum ≈ 1.0):
  w0 = 0.50 + 0.40 * (1 - spread)    [0.50 to 0.90]
  w1 = 0.20 * spread + 0.05           [0.05 to 0.25]
  w2 = 0.12 * spread                  [0.00 to 0.12]
  w3 = 0.06 * spread                  [0.00 to 0.06]
  w4 = 0.02 * spread                  [0.00 to 0.02]
```

**Spread01**: Controls kernel width
- 0 = tight core (most energy in bin 0)
- 1 = wide distribution (energy spreads to bins 0–4)

### 3. Diffusion Pass (`_apply_diffusion`)

**Physics**: 1D Laplacian blur mimicking energy dissipation.

```
Kernel: [k, (1-2k), k] where k = 0.15 * diffusion01
```

**Applied after advection** to create the "bloom softness" effect as energy travels outward.

### 4. Edge Sink (`_apply_edge_sink`)

**Physics**: Quadratic boundary damping in the last 8 bins.

```
For bin i in [radial_len - 8, radial_len):
  distFromEdge = radial_len - 1 - i
  fade = (distFromEdge / 8)^2
  energy *= fade
```

Prevents hard cutoff artifacts and creates smooth falloff at strip boundary.

### 5. Tone-Mapping (`knee_tone_map` + `readout_8bit`)

**Physics**: HDR compression for 8-bit output.

```
Formula: out = in / (in + knee) * (1 + knee)

Default knee = 0.5:
  Maps 1.0 to 1.0 (peaks preserved)
  Boosts low-level energy
  Compresses highlights smoothly
```

## API Reference

### Constructor

```python
BeatPulseTransportCore(
    radial_len: int = 80,
    device: torch.device = None,
    batch_size: int = 1
)
```

**Args:**
- `radial_len`: Active radial length (≤ 160). Typical: 80 for 160-LED strips.
- `device`: torch.device (cpu or cuda)
- `batch_size`: Number of parallel samples in the batch

**State:**
- `history`: Registered buffer `[batch_size, radial_len, 3]` in [0, 1]

### Forward Pass

```python
output = core.forward(params, injection)
```

**Args:**

- `params`: Dict with keys:
  - `offset_per_frame_60hz` (float): Radial pixels/frame at 60Hz
  - `persistence_per_frame_60hz` (float ∈ [0, 1]): Temporal decay
  - `diffusion01` (float ∈ [0, 1]): Diffusion strength (0 disables)
  - `dt_seconds` (float): Delta time (auto-clamped to [0, 0.05])

- `injection`: Dict with keys:
  - `color_rgb` (Tensor [B, 3]): Colour in [0, 1]
  - `amount01` (float ∈ [0, 1]): Injection strength
  - `spread01` (float ∈ [0, 1]): Kernel spread

**Returns:**
- `history`: Updated state `[batch_size, radial_len, 3]` in [0, 1]

### Output Methods

```python
# 8-bit tone-mapped output (for comparison with firmware)
output_8bit = core.readout_8bit(gain01=1.0, knee=0.5)  # Returns [B, radial_len, 3] uint8

# Reset to zero
core.reset()
```

## Design Decisions

### Float32 Throughout (vs Firmware's uint16)

The firmware uses 16-bit RGB internally (0–65535 per channel) then tone-maps to 8-bit (0–255). Our port:
- Uses float32 [0, 1] throughout
- Only tone-maps at readout time (`readout_8bit`)
- Avoids the firmware's truncation artifacts
- Maintains full differentiability for gradient-based training

This intentional divergence improves numerical stability and training dynamics.

### No In-Place Operations

All operations return new tensors to maintain a clean gradient graph:
```python
# ✓ Good: self.history = self._advect_outward(self.history, ...)
# ✗ Bad: self.history[:, i, :] += ... (in-place on leaf tensor)
```

### Batched State

History is batched `[B, radial_len, 3]` to support:
- Multi-zone processing
- Parallel simulation for training
- Efficient GPU utilization

## Calibration Against Firmware

Use `readout_8bit()` to compare against firmware output:

```python
core = BeatPulseTransportCore(radial_len=80)

# ... simulate several frames ...

output_8bit = core.readout_8bit(gain01=1.0)  # [batch, 80, 3] uint8 [0, 255]
# Compare with firmware LED buffer
```

The tone-mapping formula exactly matches the firmware's `kneeToneMap + toCRGB8` pipeline.

## Example Usage

```python
import torch
from core.transport_core import BeatPulseTransportCore

# Create instance
device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
core = BeatPulseTransportCore(radial_len=80, device=device, batch_size=4)

# Simulate 100 frames
for frame in range(100):
    params = {
        'offset_per_frame_60hz': 2.0,      # 2 pixels/frame
        'persistence_per_frame_60hz': 0.99, # 99% retention
        'diffusion01': 0.1,                 # Light blur
        'dt_seconds': 1.0 / 60.0            # 60 Hz
    }
    
    injection = {
        'color_rgb': torch.tensor([[1.0, 0.5, 0.2]] * 4, device=device),
        'amount01': 0.8,
        'spread01': 0.5
    }
    
    output = core.forward(params, injection)
    # output shape: [4, 80, 3] in [0, 1]

# Get 8-bit output for LED display
leds_8bit = core.readout_8bit(gain01=1.0)  # [4, 80, 3] uint8
```

## Testing

Run the smoke tests:

```bash
python transport_core.py
```

Tests verify:
1. Correct tensor shapes
2. Energy injection in first few bins
3. Advection over 100 frames with energy decay
4. 8-bit tone-mapping range and dtype
5. Gradient flow (differentiability)
6. Diffusion spreading
7. Edge sink fading

## Constants

| Constant | Value | Meaning |
|----------|-------|---------|
| `MAX_RADIAL_LEN` | 160 | Conservative allocation |
| `EDGE_SINK_WIDTH` | 8 | Boundary fade region |
| `DEFAULT_KNEE` | 0.5 | Tone-map curvature |

## References

- **Firmware source**: `src/effects/ieffect/BeatPulseTransportCore.h`
- **Physics model**: Semi-Lagrangian advection with temporal feedback + 1D Laplacian diffusion
- **LED configuration**: 320 WS2812B LEDs via light guide plate (160 per side)

## Integration with DIFNO Pipeline

This module forms the foundation of the DIFNO (Differentiable Fourier Neural Operator) training pipeline:

1. **Forward simulation**: Generate training trajectories via `forward()`
2. **Tone-mapping**: Convert to 8-bit via `readout_8bit()` for calibration
3. **Gradient computation**: Autograd flows through injection parameters
4. **Loss computation**: Compare against target (e.g., music features)
5. **Optimization**: Update injection parameters via SGD/Adam

All operations support batching and differentiation for efficient training.
