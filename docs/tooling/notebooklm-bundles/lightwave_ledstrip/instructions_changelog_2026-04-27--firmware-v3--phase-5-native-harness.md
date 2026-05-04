---
id: 2026-04-27--firmware-v3--phase-5-native-harness
date_utc: 2026-04-27
agent: codex
scope: firmware-v3
type: chore
summary: Add a focused native harness for Phase 5 effect verification.
files_changed:
  - firmware-v3/platformio.ini
  - firmware-v3/src/config/audio_config.h
  - firmware-v3/src/config/chip_config.h
  - firmware-v3/src/metrics/VRMSMetrics.h
  - firmware-v3/test/test_native/
  - firmware-v3/test/test_native_phase5/
validation: git diff --check; python3 firmware-v3/tools/check_effect_contracts.py; pio test -e native_test_phase5
breaking_change: false
follow_ups: []
---

## Details

Adds a `native_test_phase5` PlatformIO environment and native scaffolding needed to compile and run the focused Phase 5 Unity test suite without pulling the broader stale native-test graph.
