"""
ZoneComposer state snapshot + equivalence comparison (E5 skeleton).

Phase 0 status: SCAFFOLDING. Dataclass shapes defined; field-level comparison logic
implemented. Concrete population from transport responses lands in Phase 1.
"""

from dataclasses import dataclass, field, asdict
from typing import List, Optional
import json


@dataclass
class ZoneSegment:
    """One zone's LED segment definition."""
    zoneId: int
    s1LeftStart: int
    s1LeftEnd: int
    s1RightStart: int
    s1RightEnd: int
    totalLeds: int


@dataclass
class ZoneEntry:
    """Per-zone runtime state."""
    id: int
    enabled: bool
    effectId: int
    effectName: str
    brightness: int
    speed: int
    paletteId: int
    paletteName: str
    blendMode: int
    blendModeName: str
    # Phase 1 extensions:
    audioRoutingMode: Optional[str] = None  # FULL_MIX | BASS | MID | HIGH (D-4)
    expressionOverrides: Optional[dict] = None  # opt-in per A2 (D-2)


@dataclass
class ZoneState:
    """Full ZoneComposer state snapshot — single source of truth for equivalence."""
    enabled: bool
    zoneCount: int
    segments: List[ZoneSegment] = field(default_factory=list)
    zones: List[ZoneEntry] = field(default_factory=list)

    def to_dict(self) -> dict:
        return asdict(self)

    def canonical_json(self) -> str:
        """Stable serialisation for byte-equivalence comparison."""
        return json.dumps(self.to_dict(), sort_keys=True)


def assert_equivalent(snapshots: dict) -> tuple[bool, str]:
    """
    Compare snapshots from multiple transports.

    Args:
        snapshots: {transport_name: ZoneState}

    Returns:
        (True, "") if all snapshots are byte-equivalent.
        (False, diff_message) if any snapshot diverges.
    """
    if len(snapshots) < 2:
        return True, ""

    canonical = {name: state.canonical_json() for name, state in snapshots.items()}
    reference_name = next(iter(canonical))
    reference = canonical[reference_name]

    divergences = []
    for name, json_str in canonical.items():
        if json_str != reference:
            divergences.append(f"{name} differs from {reference_name}")

    if divergences:
        # TODO_PHASE1: emit field-level diff via difflib for actionable output
        return False, "; ".join(divergences)
    return True, ""


def parse_rest_zones(response: dict) -> ZoneState:
    """Parse GET /api/v1/zones response into ZoneState."""
    # TODO_PHASE1: extract enabled, zoneCount, segments, zones from response
    raise NotImplementedError("parse_rest_zones — Phase 1")


def parse_ws_zones_list(response: dict) -> ZoneState:
    """Parse zones.list (or zones.get) WS response into ZoneState."""
    # TODO_PHASE1: extract from data field
    raise NotImplementedError("parse_ws_zones_list — Phase 1")


def parse_serial_zones_list(response: dict) -> ZoneState:
    """Parse zones.list SerialJSON response into ZoneState."""
    # TODO_PHASE1: SerialJSON response shape is similar to WS but may have minor differences
    # Verify against actual response in Phase 1.
    raise NotImplementedError("parse_serial_zones_list — Phase 1")
