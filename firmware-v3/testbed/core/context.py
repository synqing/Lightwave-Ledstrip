"""
Python dataclass equivalents of EffectContext and AudioContext from ESP32 firmware.

These dataclasses mirror the C++ structs in EffectContext.h and are designed
to feed the PyTorch PDE testbed for music visualisation systems.
"""

from __future__ import annotations

from dataclasses import dataclass, field, asdict
from typing import Optional, Generator, Tuple
import numpy as np
from math import sin, pi, exp, sqrt


# ============================================================================
# Audio Context - mirrors ControlBusFrame + MusicalGridSnapshot + chord/saliency
# ============================================================================

@dataclass(slots=True)
class AudioContext:
    """
    Audio context passed to effects (by-value copies for thread safety).

    Contains copies of audio data from the audio pipeline including DSP signals,
    beat/tempo tracking, and musical analysis features.

    Attributes:
        rms: RMS energy level (0.0-1.0)
        fast_rms: Fast-smoothed RMS (more responsive)
        flux: Spectral flux / onset detection signal (0.0-1.0)
        fast_flux: Fast-smoothed flux (0.0-1.0)
        bpm: BPM estimate (typically 60-180)
        tempo_confidence: Beat detection confidence (0.0-1.0)
        beat_phase: Phase within current beat (0.0-1.0)
        beat_strength: Beat detection strength (0.0-1.0), decays after beat
        beat_tick: True if currently on a beat (single-frame pulse)
        downbeat_tick: True if on downbeat (beat 1 of measure)
        available: True if audio data is fresh (<100ms old)
        liveliness: Audio-driven liveliness scalar (0.0-1.0)
        silent_scale: Silence fade scale (1.0=active, 0.0=silent)
        is_silent: True if audio is currently silent
        bands: 8-bin frequency bands (bass to treble), shape (8,)
        heavy_bands: 8-bin smoothed bands, shape (8,)
        chroma: 12-bin chroma/pitch histogram, shape (12,)
        heavy_chroma: 12-bin smoothed chroma, shape (12,)
        waveform: 128-sample int16 waveform, shape (128,)
        bins64: 64-bin FFT, shape (64,)
        bins64_adaptive: 64-bin adaptive FFT (Sensory Bridge), shape (64,)
        snare_energy: Snare frequency band energy (150-300 Hz)
        hihat_energy: Hi-hat frequency band energy (6-12 kHz)
        snare_trigger: True if snare onset detected this frame
        hihat_trigger: True if hi-hat onset detected this frame
        overall_saliency: Overall saliency score (0.0-1.0)
        harmonic_saliency: Harmonic novelty/saliency (0.0-1.0)
        rhythmic_saliency: Rhythmic novelty/saliency (0.0-1.0)
        timbral_saliency: Timbral novelty/saliency (0.0-1.0)
        dynamic_saliency: Dynamic novelty/saliency (0.0-1.0)
        music_style: Detected music style (int enum)
        style_confidence: Style detection confidence (0.0-1.0)
        chord_type: Detected chord type (int enum)
        root_note: Root note (0-11: C=0, C#=1, ..., B=11)
        chord_confidence: Chord detection confidence (0.0-1.0)
    """

    # Scalar energy fields
    rms: float = 0.0
    fast_rms: float = 0.0
    flux: float = 0.0
    fast_flux: float = 0.0
    bpm: float = 120.0
    tempo_confidence: float = 0.0
    beat_phase: float = 0.0
    beat_strength: float = 0.0
    beat_tick: bool = False
    downbeat_tick: bool = False
    available: bool = False
    liveliness: float = 0.0
    silent_scale: float = 1.0
    is_silent: bool = False

    # Frequency band arrays (numpy arrays, defaults to zeros)
    bands: np.ndarray = field(default_factory=lambda: np.zeros(8, dtype=np.float32))
    heavy_bands: np.ndarray = field(default_factory=lambda: np.zeros(8, dtype=np.float32))

    # Chroma/pitch arrays
    chroma: np.ndarray = field(default_factory=lambda: np.zeros(12, dtype=np.float32))
    heavy_chroma: np.ndarray = field(default_factory=lambda: np.zeros(12, dtype=np.float32))

    # Waveform (128 int16 samples)
    waveform: np.ndarray = field(default_factory=lambda: np.zeros(128, dtype=np.int16))

    # FFT bins
    bins64: np.ndarray = field(default_factory=lambda: np.zeros(64, dtype=np.float32))
    bins64_adaptive: np.ndarray = field(default_factory=lambda: np.zeros(64, dtype=np.float32))

    # Percussion/onset detection
    snare_energy: float = 0.0
    hihat_energy: float = 0.0
    snare_trigger: bool = False
    hihat_trigger: bool = False

    # Musical saliency / novelty
    overall_saliency: float = 0.0
    harmonic_saliency: float = 0.0
    rhythmic_saliency: float = 0.0
    timbral_saliency: float = 0.0
    dynamic_saliency: float = 0.0

    # Music style classification
    music_style: int = 0  # Enum value (UNKNOWN, RHYTHMIC_DRIVEN, etc.)
    style_confidence: float = 0.0

    # Chord detection
    chord_type: int = 0  # Enum value (NONE, MAJOR, MINOR, etc.)
    root_note: int = 0
    chord_confidence: float = 0.0

    # ========================================================================
    # Convenience Accessors - mirroring C++ methods
    # ========================================================================

    def bass(self) -> float:
        """Get bass energy (bands 0-1 averaged)."""
        return float((self.bands[0] + self.bands[1]) * 0.5)

    def mid(self) -> float:
        """Get mid energy (bands 2-4 averaged)."""
        return float((self.bands[2] + self.bands[3] + self.bands[4]) / 3.0)

    def treble(self) -> float:
        """Get treble energy (bands 5-7 averaged)."""
        return float((self.bands[5] + self.bands[6] + self.bands[7]) / 3.0)

    def heavy_bass(self) -> float:
        """Get heavy-smoothed bass energy (bands 0-1 averaged)."""
        return float((self.heavy_bands[0] + self.heavy_bands[1]) * 0.5)

    def heavy_mid(self) -> float:
        """Get heavy-smoothed mid energy (bands 2-4 averaged)."""
        return float((self.heavy_bands[2] + self.heavy_bands[3] + self.heavy_bands[4]) / 3.0)

    def heavy_treble(self) -> float:
        """Get heavy-smoothed treble energy (bands 5-7 averaged)."""
        return float((self.heavy_bands[5] + self.heavy_bands[6] + self.heavy_bands[7]) / 3.0)

    def to_tensor(self) -> np.ndarray:
        """
        Convert AudioContext to a flat float32 tensor suitable for DIFNO input.

        Flattens scalar fields and array fields into a single 1D vector.
        Order: scalars, bands, heavy_bands, chroma, heavy_chroma, waveform (normalized),
               bins64, bins64_adaptive, remaining scalars.

        Returns:
            np.ndarray: Flat float32 tensor
        """
        # Normalize waveform from int16 to float32 (-1.0 to 1.0)
        waveform_norm = self.waveform.astype(np.float32) / 32768.0

        # Scalars as list
        scalar_features = np.array([
            self.rms, self.fast_rms, self.flux, self.fast_flux,
            self.bpm, self.tempo_confidence, self.beat_phase, self.beat_strength,
            float(self.beat_tick), float(self.downbeat_tick), float(self.available),
            self.liveliness, self.silent_scale, float(self.is_silent),
            self.snare_energy, self.hihat_energy, float(self.snare_trigger),
            float(self.hihat_trigger), self.overall_saliency, self.harmonic_saliency,
            self.rhythmic_saliency, self.timbral_saliency, self.dynamic_saliency,
            float(self.music_style), self.style_confidence, float(self.chord_type),
            float(self.root_note), self.chord_confidence
        ], dtype=np.float32)

        # Concatenate all features
        return np.concatenate([
            scalar_features,
            self.bands.astype(np.float32),
            self.heavy_bands.astype(np.float32),
            self.chroma.astype(np.float32),
            self.heavy_chroma.astype(np.float32),
            waveform_norm,
            self.bins64.astype(np.float32),
            self.bins64_adaptive.astype(np.float32),
        ])


# ============================================================================
# Effect Context - mirrors EffectContext struct
# ============================================================================

@dataclass(slots=True)
class EffectContext:
    """
    Effect rendering context with all dependencies.

    This is the Python equivalent of the C++ EffectContext struct. All effect
    implementations receive this context and should NOT access any other
    global state.

    Attributes:
        led_count: Total LED count (320 for standard config)
        center_point: CENTER ORIGIN point for position-based effects (79 for standard)
        brightness: Master brightness (0-255)
        speed: Animation speed (1-100)
        g_hue: Auto-incrementing hue value (0-255)
        mood: Sensory Bridge mood (0-255): low=reactive, high=smooth
        intensity: Effect intensity (0-255)
        saturation: Color saturation (0-255)
        complexity: Pattern complexity (0-255)
        variation: Random variation (0-255)
        fade_amount: Trail fade (0-255): 0=no fade, higher=faster
        delta_time_ms: Time since last frame (ms)
        delta_time_seconds: Time since last frame (seconds, high precision)
        raw_delta_time_ms: Unscaled time since last frame (ms)
        raw_delta_time_seconds: Unscaled time since last frame (seconds)
        frame_number: Frame counter
        total_time_ms: Total effect runtime (ms)
        raw_total_time_ms: Unscaled total effect runtime (ms)
        zone_id: Current zone ID (0-3, or 0xFF if global)
        zone_start: Zone start index in global buffer
        zone_length: Zone length
        audio: AudioContext instance
    """

    # LED configuration
    led_count: int = 320
    center_point: int = 79

    # Color/brightness parameters
    brightness: int = 255
    speed: int = 15
    g_hue: int = 0
    mood: int = 128  # Default 0.5 normalized: balanced reactive/smooth

    # Visual enhancement parameters
    intensity: int = 128
    saturation: int = 255
    complexity: int = 128
    variation: int = 64
    fade_amount: int = 20

    # Timing
    delta_time_ms: int = 8
    delta_time_seconds: float = 0.008
    raw_delta_time_ms: int = 8
    raw_delta_time_seconds: float = 0.008

    # Counters
    frame_number: int = 0
    total_time_ms: int = 0
    raw_total_time_ms: int = 0

    # Zone information
    zone_id: int = 0xFF  # 0xFF = global, not a zone
    zone_start: int = 0
    zone_length: int = 0

    # Audio context
    audio: AudioContext = field(default_factory=AudioContext)

    # ========================================================================
    # Helper Methods
    # ========================================================================

    def get_distance_from_center(self, index: int) -> float:
        """
        Calculate normalized distance from center (CENTER ORIGIN pattern).

        Args:
            index: LED index (0 to led_count-1)

        Returns:
            Distance from center: 0.0 at center, 1.0 at edges
        """
        if self.led_count == 0 or self.center_point == 0:
            return 0.0

        distance_from_center = abs(index - self.center_point)
        max_distance = float(self.center_point)

        return float(distance_from_center) / max_distance

    def get_signed_position(self, index: int) -> float:
        """
        Get signed position from center (-1.0 to +1.0).

        Args:
            index: LED index

        Returns:
            -1.0 at start, 0.0 at center, +1.0 at end
        """
        if self.led_count == 0 or self.center_point == 0:
            return 0.0

        offset = index - self.center_point
        max_offset = float(self.center_point)

        return float(offset) / max_offset

    def mirror_index(self, index: int) -> int:
        """
        Map strip index to mirror position (for symmetric effects).

        Args:
            index: LED index on one side

        Returns:
            Corresponding index on the other side
        """
        if index >= self.led_count:
            return 0

        if index < self.center_point:
            # Left side -> right side
            return self.center_point + (self.center_point - 1 - index)
        else:
            # Right side -> left side
            return self.center_point - 1 - (index - self.center_point)

    def get_phase(self, frequency_hz: float) -> float:
        """
        Get time-based phase for smooth animations.

        Args:
            frequency_hz: Oscillation frequency in Hz

        Returns:
            Phase value (0.0 to 1.0)
        """
        if frequency_hz <= 0:
            return 0.0
        period = 1000.0 / frequency_hz
        return (self.total_time_ms % int(period)) / period

    def get_sine_wave(self, frequency_hz: float) -> float:
        """
        Get sine wave value based on time.

        Args:
            frequency_hz: Oscillation frequency in Hz

        Returns:
            Sine value (-1.0 to +1.0)
        """
        phase = self.get_phase(frequency_hz)
        return sin(phase * 2.0 * pi)

    def is_zone_render(self) -> bool:
        """Check if this is a zone render (not full strip)."""
        return self.zone_id != 0xFF

    def get_mood_normalized(self) -> float:
        """
        Get normalized mood value (Sensory Bridge pattern).

        Returns:
            0.0 (reactive) to 1.0 (smooth)
        """
        return float(self.mood) / 255.0

    def get_mood_smoothing(self) -> Tuple[float, float]:
        """
        Get smoothing follower coefficients (Sensory Bridge pattern).

        Returns:
            Tuple of (rise_coefficient, fall_coefficient) both in [0.0, 1.0]
            - Low mood: Fast rise (0.3), faster fall (0.5) - reactive
            - High mood: Slow rise (0.7), slower fall (0.8) - smooth
        """
        mood_norm = self.get_mood_normalized()
        rise = 0.3 + 0.4 * mood_norm  # 0.3 (reactive) to 0.7 (smooth)
        fall = 0.5 + 0.3 * mood_norm   # 0.5 (reactive) to 0.8 (smooth)
        return (rise, fall)

    def get_safe_delta_seconds(self) -> float:
        """
        Get safe delta time in seconds (clamped for physics stability).

        Returns:
            Delta time in seconds, clamped to [0.0001, 0.05]

        Prevents physics explosion on frame drops (>50ms) and ensures
        a minimum timestep for stability (0.1ms).
        """
        dt = self.delta_time_seconds
        if dt < 0.0001:
            dt = 0.0001
        if dt > 0.05:
            dt = 0.05
        return dt

    def get_safe_raw_delta_seconds(self) -> float:
        """
        Get safe unscaled delta time in seconds (clamped for stability).

        Returns:
            Delta time in seconds, clamped to [0.0001, 0.05]

        Uses raw_delta_time_seconds so beat timing stays independent of SPEED.
        """
        dt = self.raw_delta_time_seconds
        if dt < 0.0001:
            dt = 0.0001
        if dt > 0.05:
            dt = 0.05
        return dt


# ============================================================================
# AudioContextGenerator - generates synthetic audio for training
# ============================================================================

class AudioContextGenerator:
    """
    Generates synthetic audio contexts for training data.
    """

    @staticmethod
    def random_static() -> AudioContext:
        """
        Generate a random but temporally uncorrelated audio frame.

        Returns:
            AudioContext with random values
        """
        ctx = AudioContext()
        ctx.rms = float(np.random.uniform(0.0, 1.0))
        ctx.fast_rms = float(np.random.uniform(0.0, 1.0))
        ctx.flux = float(np.random.uniform(0.0, 0.5))
        ctx.fast_flux = float(np.random.uniform(0.0, 0.5))
        ctx.bpm = float(np.random.uniform(60, 180))
        ctx.tempo_confidence = float(np.random.uniform(0.0, 1.0))
        ctx.beat_phase = float(np.random.uniform(0.0, 1.0))
        ctx.beat_strength = float(np.random.uniform(0.0, 1.0))
        ctx.beat_tick = bool(np.random.rand() < 0.1)  # 10% chance
        ctx.downbeat_tick = bool(np.random.rand() < 0.02)  # 2% chance
        ctx.available = True
        ctx.liveliness = float(np.random.uniform(0.0, 1.0))
        ctx.silent_scale = float(np.random.uniform(0.5, 1.0))
        ctx.is_silent = bool(np.random.rand() < 0.1)

        # Random bands (correlated)
        ctx.bands = np.random.uniform(0.0, 1.0, 8).astype(np.float32)
        ctx.heavy_bands = np.random.uniform(0.0, 1.0, 8).astype(np.float32)

        # Random chroma
        ctx.chroma = np.random.uniform(0.0, 1.0, 12).astype(np.float32)
        ctx.heavy_chroma = np.random.uniform(0.0, 1.0, 12).astype(np.float32)

        # Random waveform
        ctx.waveform = np.random.randint(-32768, 32767, 128, dtype=np.int16)

        # Random FFT bins
        ctx.bins64 = np.random.uniform(0.0, 1.0, 64).astype(np.float32)
        ctx.bins64_adaptive = np.random.uniform(0.0, 1.0, 64).astype(np.float32)

        # Percussion/onset
        ctx.snare_energy = float(np.random.uniform(0.0, 0.5))
        ctx.hihat_energy = float(np.random.uniform(0.0, 0.5))
        ctx.snare_trigger = bool(np.random.rand() < 0.05)  # 5% chance
        ctx.hihat_trigger = bool(np.random.rand() < 0.05)

        # Saliency
        ctx.overall_saliency = float(np.random.uniform(0.0, 0.3))
        ctx.harmonic_saliency = float(np.random.uniform(0.0, 0.3))
        ctx.rhythmic_saliency = float(np.random.uniform(0.0, 0.3))
        ctx.timbral_saliency = float(np.random.uniform(0.0, 0.3))
        ctx.dynamic_saliency = float(np.random.uniform(0.0, 0.3))

        # Style
        ctx.music_style = np.random.randint(0, 5)
        ctx.style_confidence = float(np.random.uniform(0.0, 0.5))

        # Chord
        ctx.chord_type = np.random.randint(0, 4)
        ctx.root_note = np.random.randint(0, 12)
        ctx.chord_confidence = float(np.random.uniform(0.0, 0.3))

        return ctx

    @staticmethod
    def random_beat_sequence(n_frames: int, bpm: float, dt: float) -> list[AudioContext]:
        """
        Generate n_frames of temporally coherent audio with beat ticks at the right intervals.

        Creates smooth RMS/flux curves and correlated band energies across frames.

        Args:
            n_frames: Number of frames to generate
            bpm: BPM for beat grid
            dt: Delta time per frame in seconds

        Returns:
            List of AudioContext instances with temporal coherence
        """
        results: list[AudioContext] = []
        beat_interval_frames = int((60.0 / bpm) / dt)  # Frames per beat
        downbeat_interval = 4 * beat_interval_frames    # Frames per downbeat (4/4 measure)

        # Initialize smoothed state
        rms_state = 0.5
        flux_state = 0.2
        band_state = np.random.uniform(0.3, 0.7, 8).astype(np.float32)

        for frame_idx in range(n_frames):
            ctx = AudioContext()

            # Smooth RMS evolution
            rms_target = 0.3 + 0.5 * (1.0 + sin(2 * pi * frame_idx / (beat_interval_frames * 8))) / 2
            rms_state += 0.1 * (rms_target - rms_state)
            ctx.rms = float(np.clip(rms_state + np.random.normal(0, 0.05), 0, 1))
            ctx.fast_rms = ctx.rms

            # Flux spikes near beats
            beat_phase = (frame_idx % beat_interval_frames) / beat_interval_frames
            flux_target = 0.1 + 0.3 * exp(-20 * (beat_phase - 0.1)**2)
            flux_state += 0.15 * (flux_target - flux_state)
            ctx.flux = float(np.clip(flux_state + np.random.normal(0, 0.02), 0, 0.5))
            ctx.fast_flux = ctx.flux

            # Beat ticks
            ctx.beat_tick = (frame_idx % beat_interval_frames) == 0
            ctx.downbeat_tick = (frame_idx % downbeat_interval) == 0
            ctx.beat_phase = beat_phase
            ctx.beat_strength = float(exp(-10 * beat_phase**2))

            # Temporal metadata
            ctx.bpm = bpm
            ctx.tempo_confidence = 0.9
            ctx.available = True
            ctx.liveliness = float(np.clip(rms_state * 1.2, 0, 1))
            ctx.silent_scale = 1.0 if ctx.rms > 0.1 else 0.5
            ctx.is_silent = ctx.rms < 0.1

            # Correlated band evolution
            band_state += np.random.normal(0, 0.03, 8).astype(np.float32)
            band_state = np.clip(band_state, 0, 1).astype(np.float32)
            ctx.bands = band_state.copy()
            ctx.heavy_bands = band_state + np.random.normal(0, 0.02, 8).astype(np.float32)
            ctx.heavy_bands = np.clip(ctx.heavy_bands, 0, 1).astype(np.float32)

            # Random but stable other fields
            ctx.chroma = np.random.uniform(0.2, 0.8, 12).astype(np.float32)
            ctx.heavy_chroma = ctx.chroma + np.random.normal(0, 0.05, 12).astype(np.float32)
            ctx.heavy_chroma = np.clip(ctx.heavy_chroma, 0, 1).astype(np.float32)

            # Waveform proportional to RMS
            scale = int(ctx.rms * 20000)
            ctx.waveform = np.random.randint(-scale, scale, 128, dtype=np.int16)

            # FFT bins
            ctx.bins64 = (band_state[:8] if len(band_state) > 0 else np.zeros(8)).astype(np.float32)
            ctx.bins64_adaptive = ctx.bins64.copy()

            # Onset triggers on beat
            ctx.snare_trigger = ctx.beat_tick and np.random.rand() < 0.5
            ctx.hihat_trigger = ctx.beat_tick and np.random.rand() < 0.3
            ctx.snare_energy = 0.3 if ctx.snare_trigger else 0.1
            ctx.hihat_energy = 0.2 if ctx.hihat_trigger else 0.05

            # Saliency
            ctx.overall_saliency = float(np.clip(flux_state * 0.5, 0, 0.3))
            ctx.harmonic_saliency = float(np.random.uniform(0, 0.2))
            ctx.rhythmic_saliency = 0.2 if ctx.beat_tick else 0.05
            ctx.timbral_saliency = float(np.random.uniform(0, 0.2))
            ctx.dynamic_saliency = float(np.random.uniform(0, 0.2))

            # Style
            ctx.music_style = np.random.randint(0, 5)
            ctx.style_confidence = 0.7

            # Chord (randomly)
            ctx.chord_type = np.random.randint(0, 4)
            ctx.root_note = np.random.randint(0, 12)
            ctx.chord_confidence = 0.5 if ctx.harmonic_saliency > 0.15 else 0.2

            results.append(ctx)

        return results

    @staticmethod
    def from_numpy(array: np.ndarray) -> AudioContext:
        """
        Construct AudioContext from a numpy array (for replaying recorded audio).

        Assumes array is in the same order as produced by AudioContext.to_tensor().

        Args:
            array: 1D numpy array of shape (256 + 128,) = (384,) approximately

        Returns:
            AudioContext reconstructed from the array

        Raises:
            ValueError: If array shape is incorrect
        """
        if array.ndim != 1:
            raise ValueError(f"Expected 1D array, got shape {array.shape}")

        # Expected sizes: 28 scalars + 8 + 8 + 12 + 12 + 128 + 64 + 64 = 324
        expected_size = 28 + 8 + 8 + 12 + 12 + 128 + 64 + 64
        if len(array) != expected_size:
            raise ValueError(
                f"Expected array of size {expected_size}, got {len(array)}. "
                f"Array should be output of AudioContext.to_tensor()."
            )

        idx = 0
        ctx = AudioContext()

        # Scalars
        ctx.rms = float(array[idx]); idx += 1
        ctx.fast_rms = float(array[idx]); idx += 1
        ctx.flux = float(array[idx]); idx += 1
        ctx.fast_flux = float(array[idx]); idx += 1
        ctx.bpm = float(array[idx]); idx += 1
        ctx.tempo_confidence = float(array[idx]); idx += 1
        ctx.beat_phase = float(array[idx]); idx += 1
        ctx.beat_strength = float(array[idx]); idx += 1
        ctx.beat_tick = bool(array[idx]); idx += 1
        ctx.downbeat_tick = bool(array[idx]); idx += 1
        ctx.available = bool(array[idx]); idx += 1
        ctx.liveliness = float(array[idx]); idx += 1
        ctx.silent_scale = float(array[idx]); idx += 1
        ctx.is_silent = bool(array[idx]); idx += 1
        ctx.snare_energy = float(array[idx]); idx += 1
        ctx.hihat_energy = float(array[idx]); idx += 1
        ctx.snare_trigger = bool(array[idx]); idx += 1
        ctx.hihat_trigger = bool(array[idx]); idx += 1
        ctx.overall_saliency = float(array[idx]); idx += 1
        ctx.harmonic_saliency = float(array[idx]); idx += 1
        ctx.rhythmic_saliency = float(array[idx]); idx += 1
        ctx.timbral_saliency = float(array[idx]); idx += 1
        ctx.dynamic_saliency = float(array[idx]); idx += 1
        ctx.music_style = int(array[idx]); idx += 1
        ctx.style_confidence = float(array[idx]); idx += 1
        ctx.chord_type = int(array[idx]); idx += 1
        ctx.root_note = int(array[idx]); idx += 1
        ctx.chord_confidence = float(array[idx]); idx += 1

        # Arrays
        ctx.bands = array[idx:idx+8].astype(np.float32); idx += 8
        ctx.heavy_bands = array[idx:idx+8].astype(np.float32); idx += 8
        ctx.chroma = array[idx:idx+12].astype(np.float32); idx += 12
        ctx.heavy_chroma = array[idx:idx+12].astype(np.float32); idx += 12

        # Waveform (denormalize from -1.0 to 1.0 back to int16)
        waveform_norm = array[idx:idx+128].astype(np.float32)
        ctx.waveform = (waveform_norm * 32768.0).astype(np.int16); idx += 128

        ctx.bins64 = array[idx:idx+64].astype(np.float32); idx += 64
        ctx.bins64_adaptive = array[idx:idx+64].astype(np.float32); idx += 64

        return ctx


# ============================================================================
# EffectContextSweeper - generates parameter sweeps
# ============================================================================

class EffectContextSweeper:
    """
    Generates parameter sweeps for training data (grid and Sobol sequences).
    """

    @staticmethod
    def grid_sweep(param_ranges: dict) -> Generator[EffectContext, None, None]:
        """
        Generate Cartesian product over parameter ranges.

        Args:
            param_ranges: Dict mapping param names to lists of values
                         e.g., {'speed': [1, 5, 10], 'mood': [0, 128, 255]}

        Yields:
            EffectContext instances with each parameter combination
        """
        import itertools

        param_names = list(param_ranges.keys())
        param_lists = [param_ranges[name] for name in param_names]

        for values in itertools.product(*param_lists):
            ctx = EffectContext()
            for name, value in zip(param_names, values):
                if hasattr(ctx, name):
                    setattr(ctx, name, value)
            yield ctx

    @staticmethod
    def sobol_sweep(param_ranges: dict, n_samples: int) -> Generator[EffectContext, None, None]:
        """
        Generate Sobol quasi-random sequence over parameter ranges.

        Uses the Sobol low-discrepancy sequence for better coverage than
        random sampling. Falls back to numpy random if scipy unavailable.

        Args:
            param_ranges: Dict mapping param names to (min, max) tuples
                         e.g., {'speed': (1, 100), 'mood': (0, 255)}
            n_samples: Number of samples to generate

        Yields:
            EffectContext instances with Sobol-sampled parameters
        """
        try:
            from scipy.stats import qmc
            sampler = qmc.Sobol(d=len(param_ranges), scramble=True)
            # Generate n_samples points in [0, 1)^d
            samples = sampler.random(n_samples)
        except ImportError:
            # Fallback: use uniform random if scipy unavailable
            samples = np.random.uniform(0, 1, (n_samples, len(param_ranges)))

        param_names = list(param_ranges.keys())

        for sample in samples:
            ctx = EffectContext()
            for i, name in enumerate(param_names):
                min_val, max_val = param_ranges[name]
                # Scale from [0, 1) to [min_val, max_val]
                value = min_val + sample[i] * (max_val - min_val)
                if hasattr(ctx, name):
                    # Convert to int if the default is int
                    default_type = type(getattr(ctx, name))
                    if default_type == int:
                        value = int(value)
                    setattr(ctx, name, value)
            yield ctx


# ============================================================================
# Main block - test suite
# ============================================================================

if __name__ == "__main__":
    import sys

    test_results = []

    # Test 1: Create default AudioContext
    try:
        audio = AudioContext()
        assert audio.rms == 0.0
        assert audio.bpm == 120.0
        assert audio.bands.shape == (8,)
        assert audio.waveform.shape == (128,)
        test_results.append(("Default AudioContext", "PASS"))
    except Exception as e:
        test_results.append(("Default AudioContext", f"FAIL: {e}"))

    # Test 2: Create default EffectContext
    try:
        effect = EffectContext()
        assert effect.led_count == 320
        assert effect.center_point == 79
        assert effect.brightness == 255
        test_results.append(("Default EffectContext", "PASS"))
    except Exception as e:
        test_results.append(("Default EffectContext", f"FAIL: {e}"))

    # Test 3: AudioContext convenience accessors
    try:
        audio = AudioContext()
        audio.bands = np.array([0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8], dtype=np.float32)
        bass = audio.bass()
        mid = audio.mid()
        treble = audio.treble()
        assert isinstance(bass, float)
        assert isinstance(mid, float)
        assert isinstance(treble, float)
        test_results.append(("AudioContext accessors (bass/mid/treble)", "PASS"))
    except Exception as e:
        test_results.append(("AudioContext accessors", f"FAIL: {e}"))

    # Test 4: Generate 100 random audio frames
    try:
        frames = []
        for _ in range(100):
            ctx = AudioContextGenerator.random_static()
            frames.append(ctx)
        assert len(frames) == 100
        assert all(isinstance(f, AudioContext) for f in frames)
        test_results.append(("Generate 100 random audio frames", "PASS"))
    except Exception as e:
        test_results.append(("Random audio frames", f"FAIL: {e}"))

    # Test 5: Generate beat sequence (10 frames at 120 BPM)
    try:
        beat_frames = list(AudioContextGenerator.random_beat_sequence(10, 120.0, 0.008))
        assert len(beat_frames) == 10
        # Check that beats occur periodically
        beat_count = sum(1 for f in beat_frames if f.beat_tick)
        assert beat_count >= 1
        test_results.append(("10-frame beat sequence at 120 BPM", "PASS"))
    except Exception as e:
        test_results.append(("Beat sequence", f"FAIL: {e}"))

    # Test 6: AudioContext.to_tensor()
    try:
        audio = AudioContext()
        audio.rms = 0.5
        audio.bands = np.ones(8, dtype=np.float32) * 0.3
        tensor = audio.to_tensor()
        assert isinstance(tensor, np.ndarray)
        assert tensor.dtype == np.float32
        assert tensor.ndim == 1
        # Expected size: 28 + 8 + 8 + 12 + 12 + 128 + 64 + 64 = 324
        assert len(tensor) == 324
        test_results.append(("AudioContext.to_tensor()", "PASS"))
    except Exception as e:
        test_results.append(("to_tensor", f"FAIL: {e}"))

    # Test 7: from_numpy round-trip
    try:
        audio1 = AudioContext()
        audio1.rms = 0.7
        audio1.bpm = 130.0
        audio1.bands = np.array([0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8], dtype=np.float32)

        tensor = audio1.to_tensor()
        audio2 = AudioContextGenerator.from_numpy(tensor)

        assert abs(audio2.rms - audio1.rms) < 1e-5
        assert abs(audio2.bpm - audio1.bpm) < 1e-5
        assert np.allclose(audio2.bands, audio1.bands)
        test_results.append(("AudioContext from_numpy round-trip", "PASS"))
    except Exception as e:
        test_results.append(("from_numpy round-trip", f"FAIL: {e}"))

    # Test 8: EffectContext helper methods
    try:
        effect = EffectContext(led_count=320, center_point=79)

        # Test distance from center
        dist_center = effect.get_distance_from_center(79)
        dist_edge = effect.get_distance_from_center(0)
        assert dist_center == 0.0
        assert dist_edge > 0.9

        # Test signed position
        pos_left = effect.get_signed_position(0)
        pos_right = effect.get_signed_position(319)
        assert pos_left < 0
        assert pos_right > 0

        test_results.append(("EffectContext helpers (distance/position)", "PASS"))
    except Exception as e:
        test_results.append(("EffectContext helpers", f"FAIL: {e}"))

    # Test 9: Grid sweep
    try:
        param_ranges = {
            'speed': [1, 5, 10],
            'mood': [0, 128, 255]
        }
        sweep_contexts = list(EffectContextSweeper.grid_sweep(param_ranges))
        # 3 x 3 = 9 combinations
        assert len(sweep_contexts) == 9
        test_results.append(("Grid sweep (3 x 3)", "PASS"))
    except Exception as e:
        test_results.append(("Grid sweep", f"FAIL: {e}"))

    # Test 10: Sobol sweep
    try:
        param_ranges = {
            'speed': (1, 100),
            'mood': (0, 255),
            'intensity': (0, 255)
        }
        sweep_contexts = list(EffectContextSweeper.sobol_sweep(param_ranges, 64))
        assert len(sweep_contexts) == 64
        test_results.append(("Sobol sweep (3 params, 64 samples)", "PASS"))
    except Exception as e:
        test_results.append(("Sobol sweep", f"FAIL: {e}"))

    # Test 11: Mood smoothing
    try:
        effect = EffectContext(mood=0)
        rise_low, fall_low = effect.get_mood_smoothing()

        effect.mood = 255
        rise_high, fall_high = effect.get_mood_smoothing()

        assert rise_low < rise_high
        assert fall_low < fall_high
        test_results.append(("Mood smoothing coefficients", "PASS"))
    except Exception as e:
        test_results.append(("Mood smoothing", f"FAIL: {e}"))

    # Test 12: Phase and sine wave
    try:
        effect = EffectContext(total_time_ms=0)
        phase = effect.get_phase(1.0)  # 1 Hz = 1000ms period
        assert 0.0 <= phase <= 1.0

        sine = effect.get_sine_wave(1.0)
        assert -1.0 <= sine <= 1.0
        test_results.append(("Phase and sine wave calculations", "PASS"))
    except Exception as e:
        test_results.append(("Phase/sine", f"FAIL: {e}"))

    # Print results
    print("\n" + "=" * 70)
    print("CONTEXT.PY TEST SUITE RESULTS")
    print("=" * 70)

    passed = 0
    failed = 0
    for test_name, result in test_results:
        status_str = f"[{result}]"
        print(f"{test_name:<50} {status_str}")
        if result == "PASS":
            passed += 1
        else:
            failed += 1

    print("=" * 70)
    print(f"Total: {passed} passed, {failed} failed")
    print("=" * 70)

    sys.exit(0 if failed == 0 else 1)
