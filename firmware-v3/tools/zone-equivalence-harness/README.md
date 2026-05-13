---
abstract: "E5 cross-transport equivalence harness skeleton. Proves REST/WS/SerialJSON converge to identical ZoneComposer state for equivalent commands. Phase 0 deliverable — framework ready, full test cases land as Phase 1 commands ship. Detects silent transport divergence before it ships."
---

# Zone Composer Cross-Transport Equivalence Harness (E5)

**Status:** Phase 0 skeleton. Framework + transport adapters scaffolded; specific test cases populate as Phase 1 commands ship.

**Purpose.** Three transports (REST, WebSocket, SerialJSON) currently expose overlapping zone-control surface area. Without explicit equivalence testing, transport-specific drift is silent: a field renamed in WS but not in SerialJSON, validation tightened on REST but loose on Serial, payload-shape changes that one transport accepts and another rejects.

This harness detects that drift before it ships.

## The equivalence contract

For every supported zone command:

```
Same logical command → identical final ZoneComposer state, regardless of transport.
```

Test pattern:

1. Reset K1 V2 to a known initial state (factory preset 0 = Unified)
2. Send command X via REST
3. Snapshot full zone state via `GET /api/v1/zones`
4. Reset to known state
5. Send same logical command X via WebSocket
6. Snapshot full zone state via `zones.list`
7. Reset to known state
8. Send same logical command X via SerialJSON
9. Snapshot full zone state via `zones.list`
10. Assert all three snapshots are byte-equivalent

## Architecture

```
zone-equivalence-harness/
├── README.md                       # This file
├── lib/
│   ├── __init__.py
│   ├── transports.py               # REST, WS, SerialJSON adapters (Phase 0 skeleton)
│   └── snapshot.py                 # ZoneComposer state snapshot + comparison (Phase 0 skeleton)
├── tests/
│   ├── test_zone_set_blend.py      # Phase 1: zone.setBlend equivalence
│   ├── test_zones_update.py        # Phase 1: zones.update batch equivalence
│   ├── test_zone_set_layout.py     # Phase 2: zones.setLayout (gated on B5 #1)
│   └── test_zone_effects_params.py # Phase 1 keystone: zone.effects.parameters.set (gated on D-1)
└── run_all.py                      # Test runner; emits pass/fail per command
```

## Usage (when populated)

```bash
# Run all equivalence tests against K1 V2
python3 run_all.py \
    --rest-host 192.168.4.1 \
    --rest-port 80 \
    --ws-host 192.168.4.1 \
    --ws-port 80 \
    --serial-port /dev/cu.usbmodem2101

# Run a specific test
python3 tests/test_zone_set_blend.py \
    --rest-host 192.168.4.1 --serial-port /dev/cu.usbmodem2101
```

Exit code 0 = all transports converged. 1 = divergence detected.

## Phase 0 minimum-viable contents

This document + the directory structure + transport adapter skeletons. Specific test cases populate during Phase 1 as commands stabilise.

**What lives here today:**
- This README
- `lib/transports.py` skeleton (Phase 0; class outlines + import scaffolding, no full request implementation)
- `lib/snapshot.py` skeleton (defines `ZoneState` dataclass + `snapshot()` interface)
- `run_all.py` skeleton (test discovery + report aggregation)

**What lands in Phase 1:**
- `tests/test_zone_set_blend.py` — first concrete equivalence test (blend mode is fully implemented across all three transports today; lowest-risk first test)
- `tests/test_zones_update.py` — batch update equivalence
- Full `lib/transports.py` REST + WS clients

**What lands in Phase 2+:**
- `tests/test_zone_set_layout.py` — needs SerialJSON setLayout (B5 Gap 1)
- `tests/test_zone_effects_params.py` — needs D-1 instance pool + D-3 keystone

## Why this is gated as scaffolding

Captain's Phase 0 directive: "Create the test harness structure now. It does not need full Phase 1 commands yet. It must be ready to prove that REST, WS, and SerialJSON produce identical final ZoneComposer state for the same logical command."

Building the full harness today means writing tests for commands that may still be in flux. The prudent shape is: skeleton ready, ready to receive tests as Phase 1 commands land. A new test file = a new equivalence guarantee shipped.

## Connection to Spike 4

Spike 4 (transport state equivalence) per the Pre-Phase-1 Decisions is the validation that the harness's first real test suite passes. Once Spike 4 demonstrates equivalence on `zone.setBlend` (the simplest fully-implemented command), the harness has proven its wiring works and Phase 1 can populate the rest.

## Connection to D-8

D-8 (single-effect regression gate) is a different test family — it ensures zones-disabled → single-effect rendering still works. E5 is orthogonal — it ensures equivalent commands produce equivalent state across transports. Both gates run on every PR; both must pass.

## Cross-references

- Companion plan: `~/.claude/plans/zone-composer-instrument-program.md`
- Command matrix: `docs/protocol/zones-command-matrix.md`
- SerialJSON parity inventory: `docs/protocol/zones-serial-json-parity.md`
- ADR D-1, D-3, D-8: `docs/adr/zone-composer-architecture-decisions.md`
- D-8 regression gate: `firmware-v3/test/test_zone_regression_gate/README.md`

---
**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-05-01 | Claude (Phase 0 E5) | Created. Harness skeleton + directory structure + test-case sequencing. Test bodies land as Phase 1 commands ship. |
