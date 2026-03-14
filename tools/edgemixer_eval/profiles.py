"""
EdgeMixer evaluation profiles and test corpus definitions.

Defines named parameter profiles for the EdgeMixer post-processor and
stimulus categories for operator-guided evaluation sessions.

EdgeMixer parameters:
    mode:     0=mirror, 1=analogous, 2=complementary,
              3=split_complementary, 4=saturation_veil
    spread:   0-60 (hue spread in degrees)
    strength: 0-255 (mix strength)
    spatial:  0=uniform, 1=centre_gradient
    temporal: 0=static, 1=rms_gate
"""

from dataclasses import dataclass
from typing import Dict, List, Optional


@dataclass(frozen=True)
class Profile:
    """Immutable EdgeMixer parameter profile."""

    name: str
    mode: int           # 0-4
    spread: int         # 0-60 degrees
    strength: int       # 0-255
    spatial: int        # 0=uniform, 1=centre_gradient
    temporal: int       # 0=static, 1=rms_gate
    description: str

    def __post_init__(self):
        if not 0 <= self.mode <= 4:
            raise ValueError(f"mode must be 0-4, got {self.mode}")
        if not 0 <= self.spread <= 60:
            raise ValueError(f"spread must be 0-60, got {self.spread}")
        if not 0 <= self.strength <= 255:
            raise ValueError(f"strength must be 0-255, got {self.strength}")
        if self.spatial not in (0, 1):
            raise ValueError(f"spatial must be 0 or 1, got {self.spatial}")
        if self.temporal not in (0, 1):
            raise ValueError(f"temporal must be 0 or 1, got {self.temporal}")

    def as_dict(self) -> dict:
        """Return profile parameters as a dict suitable for JSON serialisation."""
        return {
            "mode": self.mode,
            "spread": self.spread,
            "strength": self.strength,
            "spatial": self.spatial,
            "temporal": self.temporal,
        }


# ---------------------------------------------------------------------------
# Named profiles
# ---------------------------------------------------------------------------

PROFILES: Dict[str, Profile] = {
    # Baseline
    "mirror": Profile(
        "mirror", 0, 30, 255, 0, 0,
        "Control baseline -- no processing",
    ),

    # Single-mode, default settings
    "analogous": Profile(
        "analogous", 1, 30, 255, 0, 0,
        "Analogous hue shift (+30deg)",
    ),
    "complementary": Profile(
        "complementary", 2, 30, 255, 0, 0,
        "Complementary (+180deg, -15% sat)",
    ),
    "split_comp": Profile(
        "split_comp", 3, 30, 255, 0, 0,
        "Split complementary (+150deg, -10% sat)",
    ),
    "sat_veil": Profile(
        "sat_veil", 4, 30, 255, 0, 0,
        "Saturation veil (desaturate by spread)",
    ),

    # Spatial variants
    "analogous_gradient": Profile(
        "analogous_gradient", 1, 30, 255, 1, 0,
        "Analogous + centre gradient",
    ),
    "split_comp_gradient": Profile(
        "split_comp_gradient", 3, 30, 255, 1, 0,
        "Split comp + centre gradient",
    ),
    "sat_veil_gradient": Profile(
        "sat_veil_gradient", 4, 30, 255, 1, 0,
        "Sat veil + centre gradient",
    ),

    # Temporal variants (audio-reactive)
    "analogous_rms": Profile(
        "analogous_rms", 1, 30, 255, 0, 1,
        "Analogous + RMS gate",
    ),
    "split_comp_rms": Profile(
        "split_comp_rms", 3, 30, 255, 0, 1,
        "Split comp + RMS gate",
    ),

    # Combined (spatial + temporal)
    "split_comp_full": Profile(
        "split_comp_full", 3, 30, 255, 1, 1,
        "Split comp + gradient + RMS gate",
    ),

    # Spread variants
    "analogous_wide": Profile(
        "analogous_wide", 1, 45, 255, 0, 0,
        "Analogous wide (+45deg)",
    ),
    "analogous_narrow": Profile(
        "analogous_narrow", 1, 15, 255, 0, 0,
        "Analogous narrow (+15deg)",
    ),
    "sat_veil_heavy": Profile(
        "sat_veil_heavy", 4, 60, 255, 0, 0,
        "Saturation veil heavy (max desat)",
    ),

    # Strength variants
    "split_comp_subtle": Profile(
        "split_comp_subtle", 3, 30, 128, 0, 0,
        "Split comp at 50% strength",
    ),
}


# ---------------------------------------------------------------------------
# Shootout default list -- the most important profiles to compare
# ---------------------------------------------------------------------------

SHOOTOUT_DEFAULT: List[str] = [
    "mirror",
    "analogous",
    "complementary",
    "split_comp",
    "sat_veil",
    "analogous_gradient",
    "split_comp_gradient",
    "split_comp_full",
]


# ---------------------------------------------------------------------------
# Stimulus categories (operator labels, not automated)
# ---------------------------------------------------------------------------

STIMULUS_CATEGORIES: Dict[str, str] = {
    "silence":          "No audio input -- tests near-black skip and null-audio",
    "bass_dark":        "Bass-heavy content with dark palette -- tests sub-bass reactivity",
    "bright_saturated": "High-energy bright content -- tests colour preservation",
    "pastel":           "Soft, desaturated content -- tests subtle hue shifts",
    "warm_dominant":    "Warm-tone content (reds/oranges) -- tests analogous warmth",
    "cool_dominant":    "Cool-tone content (blues/greens) -- tests complementary coolness",
}


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def get_profile(name: str) -> Optional[Profile]:
    """Look up a profile by name.  Returns None if not found."""
    return PROFILES.get(name)


def list_profiles() -> List[str]:
    """Return all profile names in definition order."""
    return list(PROFILES.keys())
