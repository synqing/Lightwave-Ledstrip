---
id: 2026-04-27--firmware-v3--phase-5-effect-exemplars
date_utc: 2026-04-27
agent: codex
scope: firmware-v3
type: feature
summary: Add Phase 5 K1-native effect exemplars and registration.
files_changed:
  - firmware-v3/src/effects/ieffect/AttackOnlyPitchVelocityFieldEffect.cpp
  - firmware-v3/src/effects/ieffect/AttackOnlyPitchVelocityFieldEffect.h
  - firmware-v3/src/effects/ieffect/BeatParitySpriteEffect.cpp
  - firmware-v3/src/effects/ieffect/BeatParitySpriteEffect.h
  - firmware-v3/src/effects/ieffect/RadialTimeScopeEffect.cpp
  - firmware-v3/src/effects/ieffect/RadialTimeScopeEffect.h
  - firmware-v3/src/effects/persistence/PSRAMFrameRing.h
  - firmware-v3/src/config/display_order.h
  - firmware-v3/src/config/effect_ids.h
  - firmware-v3/src/effects/CoreEffects.cpp
validation: git diff --check; python3 firmware-v3/tools/check_effect_contracts.py; pio test -e native_test_phase5
breaking_change: false
follow_ups: []
---

## Details

Adds centre-origin Phase 5 exemplars for radial history, pitch-class velocity, and beat-parity sprite radiation, plus the PSRAM framebuffer ring substrate and effect registration metadata.
