---
id: 2026-04-27--firmware-v3--afs-v2-effect-api-lock
date_utc: 2026-04-27
agent: codex
scope: firmware-v3
type: chore
summary: Lock the Audio Feature Surface v2 effect-facing API and checker policy.
files_changed:
  - firmware-v3/src/plugins/api/EffectContext.h
  - firmware-v3/tools/check_effect_contracts.py
  - firmware-v3/src/effects/ieffect/LGPBeatPrismOnsetAdvectEffect.cpp
  - firmware-v3/src/effects/ieffect/LGPBeatPrismOnsetDriftEffect.cpp
  - firmware-v3/src/effects/ieffect/LGPBeatPrismOnsetEffect.cpp
  - firmware-v3/src/effects/ieffect/LGPBeatPrismOnsetIgniteEffect.cpp
  - firmware-v3/src/effects/ieffect/LGPBeatPrismOnsetRotateEffect.cpp
validation: git diff --check; python3 firmware-v3/tools/check_effect_contracts.py
breaking_change: false
follow_ups: []
---

## Details

Adds musical-range helpers and rhythm accessors to `EffectContext`, migrates the Beat Prism onset family away from raw `controlBus` reads, and gates new raw `bins256` effect access through the contract checker.
