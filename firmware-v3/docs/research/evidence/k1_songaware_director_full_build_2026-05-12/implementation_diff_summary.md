RBDO label: GROUNDED

Core implementation:
- Added/expanded SongAwareDirector state machine, policy snapshots, selection snapshots, allowlist snapshots, runtime status, debug status, counters, restore/reset, transition telemetry, and classification reason telemetry.
- Added modes: off, parameter, director. Existing on aliases parameter.
- Added constrained Director policy and allowlist.
- Added dwell/cooldown/rate-limit/anti-thrash/ownership/health/enable-grace gates.

Renderer:
- Director switch requests queue one Renderer-owned transition.
- Director transition snapshots source LEDs, brackets target effect lifecycle, renders a target frame under a Director guard, then starts existing TransitionEngine.
- No broad transition-system refactor.

Control surface:
- Serial: status, off, on, mode, switching, reset, restore, debug, policy, allowlist, health, counters reset.
- Serial JSON, REST, WS parity added where SongAware routes already existed.

Tests:
- Added native SongAware Director test coverage.
- Added SongAware native env to native harness matrix.
