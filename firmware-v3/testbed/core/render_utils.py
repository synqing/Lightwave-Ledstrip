"""
BeatPulseRenderUtils: Shared utilities for Beat Pulse effect family

Ports C++ helper functions from BeatPulseRenderUtils.h to Python/PyTorch.

Provides:
- Basic utilities: clamp01, lerp, floatToByte, scaleBrightness
- RingProfile: Gaussian, tent, cosine, glow, hardEdge ring shape functions
- BlendMode: softAccumulate, screen blend modes
- BeatPulseHTML: HTML demo parity functions for beat-driven pulse effects
- BeatPulseTiming: Tempo-confidence gated beat detection with fallback metronome

All functions work with both scalar Python floats and PyTorch tensors, with full
gradient support for differentiable operations (e.g., for DIFNO training).
"""

import math
from typing import Union

try:
    import torch
except ImportError:
    torch = None


# ============================================================================
# Basic Utilities
# ============================================================================


def clamp01(v: Union[float, 'torch.Tensor']) -> Union[float, 'torch.Tensor']:
    """Clamp value to [0, 1] range.

    Args:
        v: Input value (scalar float or torch tensor)

    Returns:
        Clamped value in [0, 1]
    """
    if isinstance(v, float) or isinstance(v, int):
        if v < 0.0:
            return 0.0
        if v > 1.0:
            return 1.0
        return float(v)
    else:
        # torch.Tensor
        return torch.clamp(v, min=0.0, max=1.0)


def lerp(a: Union[float, 'torch.Tensor'],
         b: Union[float, 'torch.Tensor'],
         t: Union[float, 'torch.Tensor']) -> Union[float, 'torch.Tensor']:
    """Linear interpolation between two values.

    Args:
        a: Start value
        b: End value
        t: Interpolation factor [0, 1]

    Returns:
        Interpolated value: a + (b - a) * t
    """
    # Check if any input is a tensor
    if isinstance(a, float) and isinstance(b, float) and isinstance(t, float):
        return a + (b - a) * t
    else:
        # Convert to tensors and compute
        a_t = a if torch.is_tensor(a) else torch.tensor(a, dtype=torch.float32)
        b_t = b if torch.is_tensor(b) else torch.tensor(b, dtype=torch.float32)
        t_t = t if torch.is_tensor(t) else torch.tensor(t, dtype=torch.float32)
        return a_t + (b_t - a_t) * t_t


def floatToByte(v: Union[float, 'torch.Tensor']) -> Union[int, 'torch.Tensor']:
    """Convert float [0, 1] to uint8 [0, 255] with 0.5 rounding.

    Args:
        v: Input value in [0, 1]

    Returns:
        Byte value [0, 255]
    """
    if isinstance(v, float) or isinstance(v, int):
        v_f = float(v)
        if v_f <= 0.0:
            return 0
        if v_f >= 1.0:
            return 255
        return int(v_f * 255.0 + 0.5)
    else:
        # torch.Tensor - clamp and scale
        v_clamped = torch.clamp(v, min=0.0, max=1.0)
        return torch.round(v_clamped * 255.0)


def scaleBrightness(baseBrightness: Union[int, 'torch.Tensor'],
                    factor: Union[float, 'torch.Tensor']) -> Union[int, 'torch.Tensor']:
    """Scale brightness by a factor.

    Args:
        baseBrightness: Original brightness [0, 255]
        factor: Scale factor [0, 1]

    Returns:
        Scaled brightness [0, 255]
    """
    if isinstance(baseBrightness, (int, float)) and isinstance(factor, (int, float)):
        base_f = float(baseBrightness)
        factor_clamped = clamp01(float(factor))
        scaled = base_f * factor_clamped

        if scaled <= 0.0:
            return 0
        if scaled >= 255.0:
            return 255
        return int(scaled + 0.5)
    else:
        # torch.Tensor path
        base_t = baseBrightness if torch.is_tensor(baseBrightness) else torch.tensor(baseBrightness, dtype=torch.float32)
        factor_t = factor if torch.is_tensor(factor) else torch.tensor(factor, dtype=torch.float32)
        factor_clamped = clamp01(factor_t)
        scaled = base_t * factor_clamped
        scaled_clamped = torch.clamp(scaled, min=0.0, max=255.0)
        return torch.round(scaled_clamped)


# ============================================================================
# Ring Profile Functions
# ============================================================================


class RingProfile:
    """Ring shape profile functions for beat pulse rendering."""

    @staticmethod
    def gaussian(distance: Union[float, 'torch.Tensor'],
                 sigma: Union[float, 'torch.Tensor']) -> Union[float, 'torch.Tensor']:
        """Gaussian ring profile (soft, natural falloff).

        Formula: exp(-0.5 * (distance / sigma)^2)

        At distance = sigma, intensity is ~0.6
        At distance = 2*sigma, intensity is ~0.13
        At distance = 3*sigma, intensity is ~0.01

        Args:
            distance: Distance from ring centre
            sigma: Gaussian sigma (controls ring spread)

        Returns:
            Intensity [0, 1] (differentiable for tensors)
        """
        if isinstance(distance, float) and isinstance(sigma, float):
            ratio = distance / sigma
            return math.exp(-0.5 * ratio * ratio)
        else:
            # torch.Tensor path - fully differentiable
            dist_t = distance if torch.is_tensor(distance) else torch.tensor(distance, dtype=torch.float32)
            sigma_t = sigma if torch.is_tensor(sigma) else torch.tensor(sigma, dtype=torch.float32)
            ratio = dist_t / sigma_t
            return torch.exp(-0.5 * ratio * ratio)

    @staticmethod
    def tent(distance: Union[float, 'torch.Tensor'],
             width: Union[float, 'torch.Tensor']) -> Union[float, 'torch.Tensor']:
        """Tent (linear) ring profile.

        Args:
            distance: Distance from ring centre
            width: Half-width of the tent

        Returns:
            Intensity [0, 1] (differentiable for tensors)
        """
        if isinstance(distance, float) and isinstance(width, float):
            if distance >= width:
                return 0.0
            return 1.0 - (distance / width)
        else:
            # torch.Tensor path - differentiable
            dist_t = distance if torch.is_tensor(distance) else torch.tensor(distance, dtype=torch.float32)
            width_t = width if torch.is_tensor(width) else torch.tensor(width, dtype=torch.float32)
            return torch.clamp(1.0 - (dist_t / width_t), min=0.0)

    @staticmethod
    def cosine(distance: Union[float, 'torch.Tensor'],
               width: Union[float, 'torch.Tensor']) -> Union[float, 'torch.Tensor']:
        """Cosine ring profile (smooth falloff, zero derivative at edges).

        Formula: 0.5 * (1 + cos(π * distance / width))

        Args:
            distance: Distance from ring centre
            width: Half-width of the ring

        Returns:
            Intensity [0, 1] (differentiable for tensors)
        """
        if isinstance(distance, float) and isinstance(width, float):
            if distance >= width:
                return 0.0
            return 0.5 * (1.0 + math.cos(math.pi * distance / width))
        else:
            # torch.Tensor path - differentiable
            dist_t = distance if torch.is_tensor(distance) else torch.tensor(distance, dtype=torch.float32)
            width_t = width if torch.is_tensor(width) else torch.tensor(width, dtype=torch.float32)
            ratio = dist_t / width_t
            result = 0.5 * (1.0 + torch.cos(math.pi * ratio))
            return torch.where(dist_t >= width_t, torch.zeros_like(result), result)

    @staticmethod
    def glow(distance: Union[float, 'torch.Tensor'],
             coreWidth: Union[float, 'torch.Tensor'],
             haloWidth: Union[float, 'torch.Tensor']) -> Union[float, 'torch.Tensor']:
        """Glow ring profile (bright core + soft halo) for water-like spread.

        Creates a brighter core with smooth quadratic falloff in the halo.
        Ideal for water ripple or light diffusion effects.

        Args:
            distance: Distance from ring centre
            coreWidth: Width of bright core
            haloWidth: Width of soft halo beyond core

        Returns:
            Intensity [0, 1] (differentiable for tensors)
        """
        if isinstance(distance, float) and isinstance(coreWidth, float) and isinstance(haloWidth, float):
            # Scalar path
            if distance <= coreWidth:
                # Core: high intensity with slight rolloff at edge
                t = distance / coreWidth
                return 1.0 - (t * t * 0.2)  # 1.0 at centre, 0.8 at core edge

            # Halo: smooth quadratic decay beyond core
            haloPos = distance - coreWidth
            if haloPos >= haloWidth:
                return 0.0

            t = haloPos / haloWidth
            return 0.8 * (1.0 - t) * (1.0 - t)  # Quadratic falloff from 0.8 to 0
        else:
            # torch.Tensor path - differentiable
            dist_t = distance if torch.is_tensor(distance) else torch.tensor(distance, dtype=torch.float32)
            core_t = coreWidth if torch.is_tensor(coreWidth) else torch.tensor(coreWidth, dtype=torch.float32)
            halo_t = haloWidth if torch.is_tensor(haloWidth) else torch.tensor(haloWidth, dtype=torch.float32)

            # Core branch
            t_core = dist_t / core_t
            core_result = 1.0 - (t_core * t_core * 0.2)

            # Halo branch
            haloPos = dist_t - core_t
            t_halo = haloPos / halo_t
            halo_result = 0.8 * (1.0 - t_halo) * (1.0 - t_halo)

            # Combine with conditions
            result = torch.where(
                dist_t <= core_t,
                core_result,
                torch.where(
                    haloPos >= halo_t,
                    torch.zeros_like(halo_result),
                    halo_result
                )
            )
            return torch.clamp(result, min=0.0, max=1.0)

    @staticmethod
    def hardEdge(diff: Union[float, 'torch.Tensor'],
                 width: Union[float, 'torch.Tensor'],
                 softness: Union[float, 'torch.Tensor']) -> Union[float, 'torch.Tensor']:
        """Hard-edged ring with minimal soft boundary for pressure wave fronts.

        Creates a sharp pressure wave front with subtle anti-aliasing at edges.
        Ideal for shockwave and detonation effects.

        Args:
            diff: Absolute distance from ring centre
            width: Ring half-width
            softness: Anti-aliasing transition width

        Returns:
            Intensity [0, 1] (differentiable for tensors)
        """
        if isinstance(diff, float) and isinstance(width, float) and isinstance(softness, float):
            # Scalar path
            if diff >= width + softness:
                return 0.0
            if diff <= width - softness:
                return 1.0
            # Smooth transition at edge
            return 1.0 - (diff - (width - softness)) / (2.0 * softness)
        else:
            # torch.Tensor path - differentiable
            diff_t = diff if torch.is_tensor(diff) else torch.tensor(diff, dtype=torch.float32)
            width_t = width if torch.is_tensor(width) else torch.tensor(width, dtype=torch.float32)
            soft_t = softness if torch.is_tensor(softness) else torch.tensor(softness, dtype=torch.float32)

            result = torch.where(
                diff_t >= width_t + soft_t,
                torch.zeros_like(diff_t),
                torch.where(
                    diff_t <= width_t - soft_t,
                    torch.ones_like(diff_t),
                    1.0 - (diff_t - (width_t - soft_t)) / (2.0 * soft_t)
                )
            )
            return torch.clamp(result, min=0.0, max=1.0)


# ============================================================================
# Blend Mode Functions
# ============================================================================


class BlendMode:
    """Blend mode functions for graceful multi-layer compositing."""

    @staticmethod
    def softAccumulate(accumulated: Union[float, 'torch.Tensor'],
                       knee: Union[float, 'torch.Tensor'] = 1.5) -> Union[float, 'torch.Tensor']:
        """Soft accumulation for graceful multi-layer handling.

        Maps [0, inf) to [0, 1) with configurable knee. Multiple overlapping
        rings accumulate without harsh clipping.

        Formula: x / (x + knee)

        Args:
            accumulated: Sum of all layer contributions
            knee: Softness of curve (higher = softer saturation, default 1.5)

        Returns:
            Blended intensity [0, 1) (differentiable for tensors)
        """
        if isinstance(accumulated, float) and isinstance(knee, float):
            if accumulated <= 0.0:
                return 0.0
            return accumulated / (accumulated + knee)
        else:
            # torch.Tensor path - differentiable
            acc_t = accumulated if torch.is_tensor(accumulated) else torch.tensor(accumulated, dtype=torch.float32)
            knee_t = knee if torch.is_tensor(knee) else torch.tensor(knee, dtype=torch.float32)
            return torch.where(
                acc_t <= 0.0,
                torch.zeros_like(acc_t),
                acc_t / (acc_t + knee_t)
            )

    @staticmethod
    def screen(a: Union[float, 'torch.Tensor'],
               b: Union[float, 'torch.Tensor']) -> Union[float, 'torch.Tensor']:
        """Screen blend: graceful additive that avoids clipping.

        Multiple overlapping layers blend gracefully without harsh saturation.

        Formula: 1 - (1 - a) * (1 - b)

        Args:
            a: First value [0, 1]
            b: Second value [0, 1]

        Returns:
            Blended intensity [0, 1] (differentiable for tensors)
        """
        if isinstance(a, float) and isinstance(b, float):
            ca = clamp01(a)
            cb = clamp01(b)
            return 1.0 - (1.0 - ca) * (1.0 - cb)
        else:
            # torch.Tensor path - differentiable
            a_t = a if torch.is_tensor(a) else torch.tensor(a, dtype=torch.float32)
            b_t = b if torch.is_tensor(b) else torch.tensor(b, dtype=torch.float32)
            ca = clamp01(a_t)
            cb = clamp01(b_t)
            return 1.0 - (1.0 - ca) * (1.0 - cb)


# ============================================================================
# BeatPulseHTML: HTML Beat Pulse Parity Core
# ============================================================================


class BeatPulseHTML:
    """HTML Beat Pulse demo parity functions.

    Locks the exact mathematics used by the HTML Beat Pulse demo.
    All functions are dt-correct for stable visual timing at 60/120/etc FPS.
    """

    @staticmethod
    def decayMul(dtSeconds: Union[float, 'torch.Tensor']) -> Union[float, 'torch.Tensor']:
        """dt-correct multiplier to match beatIntensity *= 0.94 at ~60 FPS.

        Formula: pow(0.94, dt * 60)

        Args:
            dtSeconds: Delta time in seconds

        Returns:
            Decay multiplier (differentiable for tensors)
        """
        if isinstance(dtSeconds, float):
            return pow(0.94, dtSeconds * 60.0)
        else:
            # torch.Tensor path - differentiable
            dt_t = dtSeconds if torch.is_tensor(dtSeconds) else torch.tensor(dtSeconds, dtype=torch.float32)
            return torch.pow(torch.tensor(0.94, dtype=torch.float32), dt_t * 60.0)

    @staticmethod
    def updateBeatIntensity(beatIntensity: float,
                           beatTick: bool,
                           dtSeconds: float) -> float:
        """Update beatIntensity using the exact HTML behaviour.

        Args:
            beatIntensity: Current intensity state [0..1]
            beatTick: True on beat (slam to 1.0)
            dtSeconds: Delta time in seconds

        Returns:
            Updated beatIntensity
        """
        if beatTick:
            beatIntensity = 1.0

        beatIntensity *= BeatPulseHTML.decayMul(dtSeconds)

        if beatIntensity < 0.0005:
            beatIntensity = 0.0

        return beatIntensity

    @staticmethod
    def ringCentre01(beatIntensity: Union[float, 'torch.Tensor']) -> Union[float, 'torch.Tensor']:
        """Ring centre position in dist01 units [0..1] per HTML maths.

        Formula: wavePos = beatIntensity * 1.2, centre = wavePos * 0.5 = beatIntensity * 0.6

        Args:
            beatIntensity: Global beat intensity state [0..1]

        Returns:
            Ring centre position [0..1] (differentiable for tensors)
        """
        if isinstance(beatIntensity, float):
            return beatIntensity * 0.6
        else:
            # torch.Tensor path - differentiable
            bi_t = beatIntensity if torch.is_tensor(beatIntensity) else torch.tensor(beatIntensity, dtype=torch.float32)
            return bi_t * 0.6

    @staticmethod
    def intensityAtDist(dist01: Union[float, 'torch.Tensor'],
                        beatIntensity: Union[float, 'torch.Tensor']) -> Union[float, 'torch.Tensor']:
        """Compute per-LED pulse intensity at a given centre-distance.

        HTML formula: waveHit = 1 - min(1, abs(dist - wavePos*0.5) * 3)
                      intensity = max(0, waveHit) * beatIntensity

        Args:
            dist01: Centre-origin distance [0..1]
            beatIntensity: Global beat intensity state [0..1]

        Returns:
            Intensity [0..1] (differentiable for tensors)
        """
        if isinstance(dist01, float) and isinstance(beatIntensity, float):
            centre = BeatPulseHTML.ringCentre01(beatIntensity)
            waveHit = 1.0 - min(1.0, abs(dist01 - centre) * 3.0)
            hit = max(0.0, waveHit)
            return hit * beatIntensity
        else:
            # torch.Tensor path - differentiable
            dist_t = dist01 if torch.is_tensor(dist01) else torch.tensor(dist01, dtype=torch.float32)
            bi_t = beatIntensity if torch.is_tensor(beatIntensity) else torch.tensor(beatIntensity, dtype=torch.float32)
            centre = BeatPulseHTML.ringCentre01(bi_t)
            waveHit = 1.0 - torch.min(
                torch.ones_like(dist_t),
                torch.abs(dist_t - centre) * 3.0
            )
            hit = torch.clamp(waveHit, min=0.0)
            return hit * bi_t

    @staticmethod
    def brightnessFactor(intensity: Union[float, 'torch.Tensor']) -> Union[float, 'torch.Tensor']:
        """Brightness factor per HTML maths.

        Formula: brightness = 0.5 + intensity * 0.5

        Args:
            intensity: [0..1]

        Returns:
            brightnessFactor [0.5..1.0] (differentiable for tensors)
        """
        if isinstance(intensity, float):
            return 0.5 + clamp01(intensity) * 0.5
        else:
            # torch.Tensor path - differentiable
            intens_t = intensity if torch.is_tensor(intensity) else torch.tensor(intensity, dtype=torch.float32)
            return 0.5 + clamp01(intens_t) * 0.5

    @staticmethod
    def whiteMix(intensity: Union[float, 'torch.Tensor']) -> Union[float, 'torch.Tensor']:
        """White mix per HTML maths.

        Formula: whiteMix = intensity * 0.3

        Args:
            intensity: [0..1]

        Returns:
            whiteMix [0..0.3] (differentiable for tensors)
        """
        if isinstance(intensity, float):
            return clamp01(intensity) * 0.3
        else:
            # torch.Tensor path - differentiable
            intens_t = intensity if torch.is_tensor(intensity) else torch.tensor(intensity, dtype=torch.float32)
            return clamp01(intens_t) * 0.3


# ============================================================================
# BeatPulseTiming: Tempo-Confidence Gated Beat Detection
# ============================================================================


class BeatPulseTiming:
    """Beat pulse timing helpers with tempo-confidence gate and fallback."""

    TEMPO_CONF_MIN = 0.25  # Minimum tempo confidence threshold

    @staticmethod
    def computeBeatTick(audio_context,
                       fallbackBpm: float,
                       lastBeatMs: int) -> tuple[bool, int]:
        """Compute beat tick with tempo-confidence gate and fallback metronome.

        If audio is available and tempo confidence is high, use audio beat ticks.
        Otherwise, use a fallback metronome driven by raw (unscaled) time.

        Args:
            audio_context: EffectContext.audio object (from C++ EffectContext)
                Must have:
                - available: bool
                - tempoConfidence(): float
                - isOnBeat(): bool
            fallbackBpm: Fallback BPM for metronome mode [30..300]
            lastBeatMs: Last recorded beat time in milliseconds

        Returns:
            Tuple of (beatTick: bool, lastBeatMs_updated: int)
            - beatTick: True if this is a beat frame
            - lastBeatMs_updated: Updated last beat time
        """
        # Simulate audio context availability for testing
        beatLocked = (
            hasattr(audio_context, 'available')
            and audio_context.available
            and hasattr(audio_context, 'tempoConfidence')
            and audio_context.tempoConfidence() >= BeatPulseTiming.TEMPO_CONF_MIN
        )

        if beatLocked:
            # Use audio beat ticks
            if hasattr(audio_context, 'isOnBeat') and audio_context.isOnBeat():
                lastBeatMs = audio_context.rawTotalTimeMs
                return True, lastBeatMs
            return False, lastBeatMs

        # Fallback metronome
        if hasattr(audio_context, 'rawTotalTimeMs'):
            nowMs = audio_context.rawTotalTimeMs
        else:
            # For testing without audio context
            import time
            nowMs = int(time.time() * 1000)

        fallbackBpmClamped = max(30.0, fallbackBpm)
        beatIntervalMs = 60000.0 / fallbackBpmClamped

        if lastBeatMs == 0 or (nowMs - lastBeatMs) >= beatIntervalMs:
            return True, nowMs

        return False, lastBeatMs


# ============================================================================
# Testing and Validation
# ============================================================================


def _run_tests():
    """Run comprehensive tests on all functions.

    Tests:
    - Scalar float inputs for all functions
    - Tensor inputs with gradient computation
    - Boundary conditions
    - Type consistency
    """
    print("=" * 70)
    print("BeatPulseRenderUtils Tests")
    print("=" * 70)

    if torch is None:
        print("WARNING: PyTorch not available. Running scalar tests only.")
        use_tensors = False
    else:
        use_tensors = True

    # Track results
    passed = 0
    failed = 0

    def test(name, condition, expected=True):
        nonlocal passed, failed
        result = "PASS" if condition == expected else "FAIL"
        status = "✓" if condition == expected else "✗"
        print(f"  {status} {name}")
        if condition == expected:
            passed += 1
        else:
            failed += 1

    # Basic Utilities
    print("\n[Basic Utilities]")

    test("clamp01(0.5) == 0.5", clamp01(0.5) == 0.5)
    test("clamp01(-0.5) == 0.0", clamp01(-0.5) == 0.0)
    test("clamp01(1.5) == 1.0", clamp01(1.5) == 1.0)

    test("lerp(0, 1, 0.5) == 0.5", abs(lerp(0.0, 1.0, 0.5) - 0.5) < 1e-6)
    test("lerp(0, 10, 0.2) == 2.0", abs(lerp(0.0, 10.0, 0.2) - 2.0) < 1e-6)

    test("floatToByte(0.0) == 0", floatToByte(0.0) == 0)
    test("floatToByte(1.0) == 255", floatToByte(1.0) == 255)
    test("floatToByte(0.5) == 128", floatToByte(0.5) == 128)

    test("scaleBrightness(100, 0.5) == 50", scaleBrightness(100, 0.5) == 50)
    test("scaleBrightness(255, 1.0) == 255", scaleBrightness(255, 1.0) == 255)

    # Ring Profiles
    print("\n[Ring Profiles - Scalars]")

    g = RingProfile.gaussian(0.0, 1.0)
    test("gaussian(0, 1) == 1.0", abs(g - 1.0) < 1e-6)

    g = RingProfile.gaussian(1.0, 1.0)
    test("gaussian(1, 1) ≈ 0.6065", abs(g - 0.6065) < 0.001)

    t = RingProfile.tent(0.0, 1.0)
    test("tent(0, 1) == 1.0", abs(t - 1.0) < 1e-6)

    t = RingProfile.tent(1.0, 1.0)
    test("tent(1, 1) == 0.0", abs(t - 0.0) < 1e-6)

    c = RingProfile.cosine(0.0, 1.0)
    test("cosine(0, 1) == 1.0", abs(c - 1.0) < 1e-6)

    c = RingProfile.cosine(1.0, 1.0)
    test("cosine(1, 1) == 0.0", abs(c - 0.0) < 1e-6)

    g = RingProfile.glow(0.0, 0.5, 0.5)
    test("glow(0, 0.5, 0.5) == 1.0", abs(g - 1.0) < 1e-6)

    h = RingProfile.hardEdge(0.0, 1.0, 0.1)
    test("hardEdge(0, 1, 0.1) == 1.0", abs(h - 1.0) < 1e-6)

    h = RingProfile.hardEdge(2.0, 1.0, 0.1)
    test("hardEdge(2, 1, 0.1) == 0.0", abs(h - 0.0) < 1e-6)

    # Blend Modes
    print("\n[Blend Modes - Scalars]")

    s = BlendMode.softAccumulate(0.0)
    test("softAccumulate(0) == 0.0", abs(s - 0.0) < 1e-6)

    s = BlendMode.softAccumulate(1.5, knee=1.5)
    test("softAccumulate(1.5, 1.5) == 0.5", abs(s - 0.5) < 1e-6)

    sc = BlendMode.screen(0.0, 0.0)
    test("screen(0, 0) == 0.0", abs(sc - 0.0) < 1e-6)

    sc = BlendMode.screen(1.0, 1.0)
    test("screen(1, 1) == 1.0", abs(sc - 1.0) < 1e-6)

    sc = BlendMode.screen(0.5, 0.5)
    test("screen(0.5, 0.5) == 0.75", abs(sc - 0.75) < 1e-6)

    # BeatPulseHTML
    print("\n[BeatPulseHTML - Scalars]")

    d = BeatPulseHTML.decayMul(0.0)
    test("decayMul(0) == 1.0", abs(d - 1.0) < 1e-6)

    bi = BeatPulseHTML.updateBeatIntensity(0.5, beatTick=True, dtSeconds=0.016)
    test("updateBeatIntensity with beatTick=True resets to 1.0", abs(bi - 1.0 * 0.94 ** (0.016 * 60)) < 1e-6)

    centre = BeatPulseHTML.ringCentre01(1.0)
    test("ringCentre01(1.0) == 0.6", abs(centre - 0.6) < 1e-6)

    intens = BeatPulseHTML.intensityAtDist(0.6, 1.0)
    test("intensityAtDist(0.6, 1.0) == 1.0", abs(intens - 1.0) < 1e-6)

    bf = BeatPulseHTML.brightnessFactor(0.5)
    test("brightnessFactor(0.5) == 0.75", abs(bf - 0.75) < 1e-6)

    wm = BeatPulseHTML.whiteMix(1.0)
    test("whiteMix(1.0) == 0.3", abs(wm - 0.3) < 1e-6)

    # Tensor Tests
    if use_tensors:
        print("\n[Tensor Tests - Ring Profiles]")

        dist_t = torch.tensor([0.0, 1.0, 2.0], requires_grad=True)
        sigma = torch.tensor(1.0, requires_grad=True)
        result = RingProfile.gaussian(dist_t, sigma)
        test("gaussian tensor shape preserved", result.shape == dist_t.shape)
        test("gaussian tensor gradients enabled", result.requires_grad)

        # Verify gradient flow
        loss = result.sum()
        loss.backward()
        test("gaussian tensor backward pass", dist_t.grad is not None)

        print("\n[Tensor Tests - Blend Modes]")

        acc_t = torch.tensor([0.0, 1.0, 2.0], requires_grad=True)
        result = BlendMode.softAccumulate(acc_t)
        test("softAccumulate tensor shape preserved", result.shape == acc_t.shape)
        test("softAccumulate tensor gradients enabled", result.requires_grad)

        loss = result.sum()
        loss.backward()
        test("softAccumulate tensor backward pass", acc_t.grad is not None)

        print("\n[Tensor Tests - BeatPulseHTML]")

        bi_t = torch.tensor([0.0, 0.5, 1.0], requires_grad=True)
        result = BeatPulseHTML.ringCentre01(bi_t)
        test("ringCentre01 tensor shape preserved", result.shape == bi_t.shape)
        test("ringCentre01 tensor values correct", torch.allclose(result, bi_t * 0.6))

        dist_t = torch.tensor([0.3, 0.6, 0.9], requires_grad=True)
        bi_t = torch.tensor(1.0, requires_grad=True)
        result = BeatPulseHTML.intensityAtDist(dist_t, bi_t)
        test("intensityAtDist tensor shape preserved", result.shape == dist_t.shape)
        test("intensityAtDist tensor gradients enabled", result.requires_grad)

    # Summary
    print("\n" + "=" * 70)
    print(f"Results: {passed} passed, {failed} failed")
    print("=" * 70)

    return failed == 0


if __name__ == "__main__":
    success = _run_tests()
    exit(0 if success else 1)
