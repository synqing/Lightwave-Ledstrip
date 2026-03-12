# BeatPulseTransportCore - Quick Start

## Installation

No external dependencies beyond PyTorch:

```bash
pip install torch
```

## Basic Usage

```python
import torch
from core.transport_core import BeatPulseTransportCore

# Create physics engine
device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
core = BeatPulseTransportCore(radial_len=80, device=device, batch_size=1)

# One simulation frame
params = {
    'offset_per_frame_60hz': 2.0,          # Pixels per frame
    'persistence_per_frame_60hz': 0.99,    # 99% retention
    'diffusion01': 0.1,                    # Slight blur
    'dt_seconds': 1.0 / 60.0               # 60 Hz
}

injection = {
    'color_rgb': torch.tensor([[1.0, 0.5, 0.2]], device=device),  # Red-ish
    'amount01': 0.8,                       # 80% strength
    'spread01': 0.5                        # Medium spread
}

state = core.forward(params, injection)
# state shape: [1, 80, 3] in [0, 1]

# Get 8-bit output (for LEDs)
leds_8bit = core.readout_8bit(gain01=1.0)
# leds_8bit shape: [1, 80, 3] uint8 [0-255]

# Reset for next episode
core.reset()
```

## Key Methods

| Method | Purpose | Returns |
|--------|---------|---------|
| `forward(params, injection)` | Run one physics frame | `[B, radial_len, 3]` float32 [0, 1] |
| `readout_8bit(gain, knee)` | Tone-map for output | `[B, radial_len, 3]` uint8 [0, 255] |
| `reset()` | Zero history | None |
| `knee_tone_map(x, knee)` | Utility tone-map | Same shape as input |

## Batched Simulation

Process multiple samples in parallel:

```python
core = BeatPulseTransportCore(radial_len=80, batch_size=4)

# Batch of 4 injections
injection = {
    'color_rgb': torch.tensor([
        [1.0, 0.5, 0.2],  # Sample 0: red
        [0.2, 1.0, 0.5],  # Sample 1: green
        [0.2, 0.5, 1.0],  # Sample 2: blue
        [1.0, 1.0, 1.0],  # Sample 3: white
    ], device=device),
    'amount01': 0.8,
    'spread01': 0.5
}

state = core.forward(params, injection)
# state shape: [4, 80, 3] - all 4 samples processed together
```

## Training with Gradients

```python
# Enable gradient tracking on injection
injection_color = torch.tensor([[1.0, 0.5, 0.2]], requires_grad=True)

injection = {
    'color_rgb': injection_color,
    'amount01': 0.8,
    'spread01': 0.5
}

# Simulate
state = core.forward(params, injection)

# Compute loss (e.g., match target pattern)
target = torch.tensor([...])  # Your target state
loss = ((state - target) ** 2).mean()

# Backprop
loss.backward()

# Update injection_color
with torch.no_grad():
    injection_color -= 0.01 * injection_color.grad
    injection_color.grad.zero_()
```

## Constants

| Name | Value | Use |
|------|-------|-----|
| `MAX_RADIAL_LEN` | 160 | Max buffer size |
| `EDGE_SINK_WIDTH` | 8 | Boundary fade region |
| `DEFAULT_KNEE` | 0.5 | Tone-map curvature |

## Parameter Ranges

```python
params = {
    'offset_per_frame_60hz': 0.5,      # 0.5 - 5.0 (pixels/frame)
    'persistence_per_frame_60hz': 0.95, # 0.8 - 0.99 (decay)
    'diffusion01': 0.1,                # 0.0 - 1.0 (blur strength)
    'dt_seconds': 1.0/60.0             # Auto-clamped to [0, 0.05]
}

injection = {
    'color_rgb': torch.tensor([[1.0, 0.5, 0.2]]),  # [0, 1] per channel
    'amount01': 0.8,                   # [0, 1] (inject strength)
    'spread01': 0.5                    # [0, 1] (kernel width)
}
```

## Performance Tips

1. **GPU acceleration**: Use `device=torch.device('cuda')`
2. **Batch simulation**: Higher `batch_size` for better GPU utilization
3. **Avoid readout_8bit() in loop**: Only call when needed (expensive due to tone-mapping)
4. **Reuse core instance**: Don't recreate for each frame

## Debugging

```python
# Check state range
print(f"History range: [{state.min():.3f}, {state.max():.3f}]")

# Verify shapes
print(f"State shape: {state.shape}")
print(f"Output 8bit: {leds_8bit.dtype}")

# Monitor gradients
for param in [injection['color_rgb']]:
    if param.grad is not None:
        print(f"Grad norm: {param.grad.norm().item():.4f}")
```

## Common Issues

**"memory error"**: Increase `radial_len` gradually or reduce `batch_size`

**"gradient is None"**: Ensure input tensors have `requires_grad=True`

**"output out of range"**: Values should stay in [0, 1] before `readout_8bit()`

## Physics Intuition

- **Advection**: Energy moves outward, persistence controls how much stays
- **Injection**: 5-bin kernel creates smooth "bloom" at centre
- **Diffusion**: Blurs energy as it spreads (optional)
- **Edge sink**: Prevents hard boundary artifacts
- **Tone-map**: Compresses HDR → 8-bit, boosting low levels

## See Also

- `README_TRANSPORT_CORE.md` - Full API documentation
- `transport_core.py` - Source code with detailed comments
- Firmware source: `src/effects/ieffect/BeatPulseTransportCore.h`
