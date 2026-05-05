---
id: 2026-05-06--firmware-v3--fade-override-helper
date_utc: 2026-05-06
agent: codex
scope: firmware-v3
type: fix
summary: Added the Surface 7 fade-to-black bench helper substrate
files_changed:
  - BACKLOG.md
  - firmware-v3/src/effects/FadeOverride.h
  - firmware-v3/test/test_fade_override/test_fade_override.cpp
  - firmware-v3/platformio.ini
  - firmware-v3/scripts/native_harness_matrix.py
validation: pio test -e native_test_fade_override; python3 scripts/check_native_harness_routes.py; python3 scripts/native_harness_matrix.py; pio run -e esp32dev_audio_esv11_k1v2_32khz
breaking_change: false
follow_ups:
  - Effects must opt in explicitly before `effect.fade_to_black` can change their persistence behaviour at runtime.
---

## Details

Added `effects/FadeOverride.h` with `fadeToBlackByGated()`, which performs one bench toggle read and delegates to the existing dt-correct fade helper only when `effect.fade_to_black` is enabled. No existing effect call sites were migrated, so the helper is validation substrate only until a specific effect is selected for A/B testing.
