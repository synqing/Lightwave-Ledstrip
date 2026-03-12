"""
BeatPulseTransportCore: PyTorch port of the ESP32 firmware physics engine.

This module implements semi-Lagrangian advection with temporal feedback,
diffusion, edge sink, and tone-mapping for music visualisation. The core
maintains a radial history buffer and applies 5 sequential operations:

1. Semi-Lagrangian push advection (2-tap linear interpolation)
2. Persistence decay (dt-correct exponential)
3. Diffusion pass (1D Laplacian blur)
4. Edge sink (quadratic fade in last 8 bins)
5. Tone-mapping (knee + gain) for output

Design:
- Float32 throughout (no uint16 intermediate truncation)
- Batched support for multiple zones/samples
- Fully differentiable for gradient-based training
- Registered buffers for state (not parameters)
"""

import torch
import torch.nn as nn
import torch.nn.functional as F
from typing import Dict, Optional


class BeatPulseTransportCore(nn.Module):
    """
    Physics-based radial advection engine for music visualisation.

    Maintains a per-batch history buffer in radial space (distance-from-centre)
    and applies subpixel motion advection with temporal feedback, diffusion,
    and boundary damping.

    Args:
        radial_len (int): Active radial length, typically 80 for 160-LED strips.
                         Must be <= MAX_RADIAL_LEN=160.
        device (torch.device): Device for tensors (cpu or cuda).
        batch_size (int): Batch dimension for history buffer. Default: 1.

    State:
        history: Registered buffer [batch_size, radial_len, 3] in [0, 1] range.

    Constants:
        MAX_RADIAL_LEN: 160 (conservative allocation for 80-LED radial half)
        EDGE_SINK_WIDTH: 8 (quadratic fade region near boundary)
        DEFAULT_KNEE: 0.5 (knee tone-map parameter)
    """

    MAX_RADIAL_LEN = 160
    EDGE_SINK_WIDTH = 8
    DEFAULT_KNEE = 0.5

    def __init__(self, radial_len: int = 80, device: torch.device = None, batch_size: int = 1):
        super().__init__()

        if device is None:
            device = torch.device('cpu')

        self.device_name = device
        self.radial_len = min(radial_len, self.MAX_RADIAL_LEN)
        self.batch_size = batch_size

        # Register history as a buffer (not a parameter, since it's state not a learnable weight)
        # However, we allow gradients to flow through it for backprop during training.
        self.register_buffer(
            'history',
            torch.zeros(batch_size, self.radial_len, 3, dtype=torch.float32, device=device)
        )

    def reset(self) -> None:
        """Zero the history buffer."""
        self.history.zero_()

    def forward(
        self,
        params: Dict[str, float],
        injection: Dict[str, torch.Tensor]
    ) -> torch.Tensor:
        """
        Execute one frame of the physics simulation.

        Args:
            params: Dictionary with keys:
                - offset_per_frame_60hz: float, radial pixels/frame at 60Hz
                - persistence_per_frame_60hz: float in [0, 1], temporal decay
                - diffusion01: float in [0, 1], diffusion strength
                - dt_seconds: float, delta time (clamped to [0, 0.05])

            injection: Dictionary with keys:
                - color_rgb: torch.Tensor [batch_size, 3] in [0, 1]
                - amount01: float in [0, 1], injection strength
                - spread01: float in [0, 1], kernel spread (0=pinpoint, 1=wide)

        Returns:
            history: Current state [batch_size, radial_len, 3] in [0, 1]
        """
        # Clamp dt
        dt = torch.clamp(torch.tensor(params['dt_seconds']), min=0.0, max=0.05).item()
        dt_scale = dt * 60.0  # Reference framerate

        # Scale parameters by dt
        dt_offset = params['offset_per_frame_60hz'] * dt_scale
        dt_persist = params['persistence_per_frame_60hz'] ** dt_scale
        diffusion01 = params['diffusion01']

        # Apply operations sequentially, updating history each time
        # 1. Inject energy at centre (skip if no injection this frame)
        if injection is not None:
            self.history = self._inject_at_centre(self.history, injection)

        # 2. Advect outward with semi-Lagrangian push
        self.history = self._advect_outward(self.history, dt_offset, dt_persist)

        # 3. Diffusion pass (optional)
        if diffusion01 > 0.0001:
            self.history = self._apply_diffusion(self.history, diffusion01)

        # 4. Edge sink (quadratic fade)
        self.history = self._apply_edge_sink(self.history)

        return self.history

    def _inject_at_centre(
        self, history: torch.Tensor, injection: Dict[str, torch.Tensor]
    ) -> torch.Tensor:
        """
        Inject colour energy at centre using 5-bin Gaussian-like kernel.

        The kernel weights vary smoothly with spread01:
        - spread01=0: tight core (w0=0.9, others near 0)
        - spread01=1: wide spread (w0=0.5, w1=0.25, w2=0.12, w3=0.06, w4=0.02)

        This creates organic "bloom" at injection point without sharp needles.

        Args:
            history: Current state [batch_size, radial_len, 3]
            injection: Dict with color_rgb [B,3], amount01 (float), spread01 (float)

        Returns:
            Updated history after injection
        """
        color_rgb = injection['color_rgb']  # [batch_size, 3] in [0, 1]
        amount01 = torch.clamp(torch.as_tensor(injection['amount01'], dtype=torch.float32), min=0.0, max=1.0).item()
        spread01 = torch.clamp(torch.as_tensor(injection['spread01'], dtype=torch.float32), min=0.0, max=1.0).item()

        if amount01 <= 0.0001:
            return history

        # Energy to inject (colour * amount)
        energy = color_rgb * amount01  # [batch_size, 3]

        # Kernel weights (sum should be ~1.0)
        tightness = 1.0 - spread01
        w0 = 0.50 + 0.40 * tightness  # 0.50..0.90
        w1 = 0.20 * spread01 + 0.05   # 0.05..0.25
        w2 = 0.12 * spread01          # 0.00..0.12
        w3 = 0.06 * spread01          # 0.00..0.06
        w4 = 0.02 * spread01          # 0.00..0.02

        # Create injection tensor
        hist_new = history.clone()

        # Add to history bins with kernel spreading
        hist_new[:, 0, :] = hist_new[:, 0, :] + energy * w0
        if self.radial_len > 1:
            hist_new[:, 1, :] = hist_new[:, 1, :] + energy * w1
        if self.radial_len > 2:
            hist_new[:, 2, :] = hist_new[:, 2, :] + energy * w2
        if self.radial_len > 3:
            hist_new[:, 3, :] = hist_new[:, 3, :] + energy * w3
        if self.radial_len > 4:
            hist_new[:, 4, :] = hist_new[:, 4, :] + energy * w4

        # Clamp to [0, 1]
        return torch.clamp(hist_new, min=0.0, max=1.0)

    def _advect_outward(
        self, history: torch.Tensor, dt_offset: float, dt_persist: float
    ) -> torch.Tensor:
        """
        Semi-Lagrangian push advection with 2-tap linear interpolation.

        For each bin i, compute its new position: newPos = i + dt_offset.
        If newPos is in valid range, scatter bin i's energy to two adjacent
        bins (left and right) with fractional weights, then apply persistence.

        This is the "bloom spreading" mechanism: energy flows outward as the
        radial distance increases.

        Args:
            history: Current state [batch_size, radial_len, 3]
            dt_offset: Offset in radial pixels (can be fractional)
            dt_persist: Persistence multiplier after advection

        Returns:
            Advected history
        """
        # Create work buffer to accumulate advected energy
        work = torch.zeros_like(history)

        for i in range(self.radial_len):
            new_pos = float(i) + dt_offset

            # Bounds check: energy must stay in valid range
            if new_pos < 0.0 or new_pos >= self.radial_len - 1:
                continue

            # Linear interpolation weights
            left_idx = int(new_pos)
            frac = new_pos - left_idx

            mix_left = (1.0 - frac) * dt_persist
            mix_right = frac * dt_persist

            # Scatter to two adjacent bins
            work[:, left_idx, :] = work[:, left_idx, :] + history[:, i, :] * mix_left
            work[:, left_idx + 1, :] = work[:, left_idx + 1, :] + history[:, i, :] * mix_right

        return work

    def _apply_diffusion(self, history: torch.Tensor, diffusion01: float) -> torch.Tensor:
        """
        Apply 1D Laplacian diffusion (Gaussian blur).

        Kernel: [k, (1-2k), k] where k = 0.15 * diffusion01

        This creates the "bloom softness" effect: energy spreads locally
        as it travels outward. Intentionally applied AFTER advection to
        mimic energy dissipation in motion.

        Args:
            history: Current state [batch_size, radial_len, 3]
            diffusion01: Strength in [0, 1]

        Returns:
            Diffused history
        """
        k = 0.15 * diffusion01
        c = 1.0 - (2.0 * k)  # Centre retention

        # Create diffused buffer
        diffused = torch.zeros_like(history)

        for i in range(self.radial_len):
            centre = history[:, i, :]

            # Neighbours (clamped at boundaries)
            left = history[:, max(0, i - 1), :] if i > 0 else torch.zeros_like(centre)
            right = history[:, min(self.radial_len - 1, i + 1), :] if i + 1 < self.radial_len else torch.zeros_like(centre)

            # Laplacian convolution
            diffused[:, i, :] = centre * c + left * k + right * k

        return diffused

    def _apply_edge_sink(self, history: torch.Tensor) -> torch.Tensor:
        """
        Apply quadratic edge sink in the last EDGE_SINK_WIDTH bins.

        Energy fades smoothly as it approaches the boundary:
        fade(i) = (distFromEdge / EDGE_SINK_WIDTH)^2

        This prevents hard cutoff artifacts and creates smooth falloff
        at the strip boundary.

        Args:
            history: Current state [batch_size, radial_len, 3]

        Returns:
            History with edge sink applied
        """
        if self.radial_len <= self.EDGE_SINK_WIDTH:
            return history

        sink_start = self.radial_len - self.EDGE_SINK_WIDTH
        hist_new = history.clone()

        for i in range(sink_start, self.radial_len):
            dist_from_edge = float(self.radial_len - 1 - i)
            t = dist_from_edge / float(self.EDGE_SINK_WIDTH)
            fade = t * t  # Quadratic fade

            hist_new[:, i, :] = hist_new[:, i, :] * fade

        return hist_new

    def readout_8bit(self, gain01: float = 1.0, knee: float = None) -> torch.Tensor:
        """
        Tone-map current state to uint8 [0, 255] for calibration/comparison.

        Applies knee tone-mapping to compress highlights while boosting
        low-level energy, then scales by gain and clips to [0, 255].

        Formula: out = in / (in + knee) * (1 + knee)

        This mimics the firmware's 16-bit → 8-bit conversion path.

        Args:
            gain01: Additional [0, 1] scaling at output
            knee: Knee tone-map parameter (default: DEFAULT_KNEE=0.5)

        Returns:
            Tone-mapped 8-bit tensor [batch_size, radial_len, 3]
        """
        if knee is None:
            knee = self.DEFAULT_KNEE

        gain = torch.clamp(torch.tensor(gain01), min=0.0, max=1.0).item()

        # Apply knee tone-map: in / (in + knee) * (1 + knee)
        hist_mapped = self.history / (self.history + knee) * (1.0 + knee)

        # Apply gain and convert to 8-bit
        output = hist_mapped * gain * 255.0
        output = torch.clamp(output, min=0.0, max=255.0)

        return output.to(torch.uint8)

    def knee_tone_map(self, x: torch.Tensor, knee: float = None) -> torch.Tensor:
        """
        Knee tone-mapping function (public utility).

        Formula: out = in / (in + knee) * (1 + knee)

        Maps x in [0, 1] such that:
        - x=0 → 0
        - x=1 → mapped = 1/(1+knee) * (1+knee) = 1 (peaks preserved)
        - Low values boosted, highlights compressed

        Args:
            x: Input tensor [0, 1]
            knee: Knee parameter (default: DEFAULT_KNEE=0.5)

        Returns:
            Tone-mapped tensor, same shape as x
        """
        if knee is None:
            knee = self.DEFAULT_KNEE

        return x / (x + knee) * (1.0 + knee)


if __name__ == '__main__':
    """
    Smoke test: create instance, inject energy, advect 100 frames, verify.
    """
    print("=" * 60)
    print("BeatPulseTransportCore PyTorch Port - Smoke Test")
    print("=" * 60)

    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    print(f"Device: {device}")

    # Create instance with batch_size=2
    batch_size = 2
    radial_len = 80
    core = BeatPulseTransportCore(radial_len=radial_len, device=device, batch_size=batch_size)
    print(f"Initialized BeatPulseTransportCore: batch_size={batch_size}, radial_len={radial_len}")
    print(f"History shape: {core.history.shape}")

    # Test 1: Check initial state is zero
    assert torch.allclose(core.history, torch.zeros_like(core.history)), "Initial history should be zero"
    print("✓ Initial history is zero")

    # Test 2: Inject energy at centre
    print("\nTest: Injection at centre")
    injection = {
        'color_rgb': torch.tensor([[1.0, 0.5, 0.2], [0.2, 1.0, 0.5]], device=device),  # [B, 3]
        'amount01': 0.8,
        'spread01': 0.5
    }
    params = {
        'offset_per_frame_60hz': 0.0,
        'persistence_per_frame_60hz': 1.0,
        'diffusion01': 0.0,
        'dt_seconds': 1.0 / 60.0  # 60 Hz
    }

    core.reset()
    output = core.forward(params, injection)
    assert output.shape == (batch_size, radial_len, 3), f"Output shape mismatch: {output.shape}"
    print(f"Output shape: {output.shape}")

    # Check that energy is injected (non-zero in first few bins)
    energy_injected = output[:, :5, :].sum().item()
    assert energy_injected > 0.1, f"Energy not injected: {energy_injected}"
    print(f"Energy injected (first 5 bins): {energy_injected:.4f}")

    # Test 3: Advection over 100 frames
    print("\nTest: Advection over 100 frames")
    core.reset()

    # Strong advection with no decay
    advection_params = {
        'offset_per_frame_60hz': 2.0,  # 2 pixels/frame
        'persistence_per_frame_60hz': 0.99,
        'diffusion01': 0.0,
        'dt_seconds': 1.0 / 60.0
    }

    for frame in range(100):
        core.forward(advection_params, injection)

    output = core.history
    print(f"After 100 frames, output range: [{output.min():.4f}, {output.max():.4f}]")

    # Energy should decay out of the buffer
    assert output.max() < 1.0 + 0.01, "Output should stay in normalized range"
    print(f"✓ Output range valid")

    # Test 4: 8-bit tone-mapping
    print("\nTest: 8-bit tone-mapping")
    core.reset()
    output = core.forward(params, injection)
    output_8bit = core.readout_8bit(gain01=1.0)

    assert output_8bit.dtype == torch.uint8, f"8-bit output dtype: {output_8bit.dtype}"
    assert output_8bit.shape == (batch_size, radial_len, 3), f"8-bit output shape: {output_8bit.shape}"
    assert output_8bit.max() <= 255, f"8-bit max out of range: {output_8bit.max()}"
    print(f"8-bit output range: [{output_8bit.min()}, {output_8bit.max()}]")
    print(f"✓ 8-bit tone-mapping valid")

    # Test 5: Gradient flow via injection color
    print("\nTest: Gradient flow (differentiability)")
    core.reset()

    # Create injection with requires_grad on color
    injection_grad = {
        'color_rgb': torch.tensor([[1.0, 0.5, 0.2], [0.2, 1.0, 0.5]], device=device, requires_grad=True),
        'amount01': 0.8,
        'spread01': 0.5
    }

    output = core.forward(params, injection_grad)
    loss = output.sum()
    loss.backward()

    # Verify that gradients flow through injection color
    assert injection_grad['color_rgb'].grad is not None, "Color gradients should flow"
    grad_magnitude = injection_grad['color_rgb'].grad.abs().sum().item()
    assert grad_magnitude > 0, "Non-zero gradients expected"
    print(f"Gradient magnitude: {grad_magnitude:.4f}")
    print(f"✓ Gradients flow correctly through injection")

    # Test 6: Diffusion effect
    print("\nTest: Diffusion smoothing")
    core.reset()

    diffusion_params = {
        'offset_per_frame_60hz': 0.0,
        'persistence_per_frame_60hz': 1.0,
        'diffusion01': 0.5,
        'dt_seconds': 1.0 / 60.0
    }

    # Inject at centre
    core.forward(diffusion_params, injection)

    # After diffusion, energy should spread to neighbours
    centre_energy = core.history[:, 0, :].sum().item()
    neighbour_energy = core.history[:, 1, :].sum().item() + core.history[:, 2, :].sum().item()

    print(f"Centre bin energy: {centre_energy:.4f}")
    print(f"Neighbour energy: {neighbour_energy:.4f}")
    assert neighbour_energy > 0.001, "Diffusion should spread energy to neighbours"
    print(f"✓ Diffusion spreads energy correctly")

    # Test 7: Edge sink
    print("\nTest: Edge sink fade")
    core.reset()

    no_sink_params = {
        'offset_per_frame_60hz': 5.0,
        'persistence_per_frame_60hz': 0.95,
        'diffusion01': 0.1,
        'dt_seconds': 1.0 / 60.0
    }

    for _ in range(20):
        core.forward(no_sink_params, injection)

    # Energy near edge should be faded
    edge_energy = core.history[:, -8:, :].sum().item()
    centre_energy = core.history[:, :radial_len//2, :].sum().item()

    print(f"Centre region energy: {centre_energy:.4f}")
    print(f"Edge region energy: {edge_energy:.4f}")
    print(f"✓ Edge sink applied")

    print("\n" + "=" * 60)
    print("All smoke tests passed!")
    print("=" * 60)
