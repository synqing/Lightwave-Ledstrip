---
title: "firmware-v3: Synergy-Topology Phase 3 Move 3.3 InterStripPhaseDelay substrate"
date: 2026-05-05
area: firmware-v3
kind: added
files:
  - firmware-v3/src/effects/persistence/InterStripPhaseDelay.h
  - firmware-v3/test/test_interstrip_phase_delay/test_interstrip_phase_delay.cpp
  - firmware-v3/platformio.ini
  - firmware-v3/scripts/native_harness_matrix.py
  - BACKLOG.md
  - CHANGELOG.md
validation:
  - pio test -e native_test_interstrip_phase_delay
  - python3 scripts/check_native_harness_routes.py
  - git diff --check
  - python3 scripts/native_harness_matrix.py
  - pio run -e esp32dev_audio_esv11_k1v2_32khz
refs:
  - BACKLOG.md § Synergy-Topology Programme Phase 3 Move 3.3
  - firmware-v3/docs/research/synergy-topology/Topology_Reconciliation.md §5
---

Adds `InterStripPhaseDelay`, a paired dual-strip delay-line helper over
`PSRAMFrameRing`, so future `DUAL_CHANNEL` effects can sample strip A/B frame
history at explicit offsets without render-path allocation. This is substrate
only; it does not add a visible effect or expose any runtime control surface.

Follow-up: Phase 3 Move 3.2 remains gated on the M1 LGP fringe-visibility
measurement and must not start until Captain provides the measurement verdict.
