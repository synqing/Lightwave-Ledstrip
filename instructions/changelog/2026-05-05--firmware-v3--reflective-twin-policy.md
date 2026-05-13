---
id: 2026-05-05--firmware-v3--reflective-twin-policy
date_utc: 2026-05-05
agent: codex
scope: firmware-v3
type: feature
summary: Close Synergy-Topology Phase 3 Move 3.1 with a Reflective Twin policy gate for direct dual-strip rendering.
files_changed:
  - BACKLOG.md
  - CHANGELOG.md
  - firmware-v3/platformio.ini
  - firmware-v3/scripts/native_harness_matrix.py
  - firmware-v3/src/core/actors/RendererActor.cpp
  - firmware-v3/src/effects/ReflectiveTwinPolicy.h
  - firmware-v3/test/test_native/test_effect_role_flags.cpp
  - firmware-v3/test/test_reflective_twin_policy/test_reflective_twin_policy.cpp
validation: python3 scripts/check_native_harness_routes.py; git diff --check; pio test -e native_test_reflective_twin_policy; python3 scripts/native_harness_matrix.py; pio run -e esp32dev_audio_esv11_k1v2_32khz
breaking_change: false
follow_ups:
  - Phase 3 Move 3.3 GEO-13 InterStripPhaseDelay infrastructure remains next while Move 3.2 stays gated on LGP fringe-visibility data.
---

## Details

Adds a small metadata-backed guard so only effects declaring `EffectRoleFlags::DUAL_CHANNEL` can preserve `ctx.dualChannelMode=true` after render. Default and legacy effects continue through the mirrored unified framebuffer path, preserving the Reflective Twin contract.
