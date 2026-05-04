---
id: 2026-05-05--firmware-v3--native-harness-hardening
date_utc: 2026-05-05
agent: codex
scope: firmware-v3
type: bugfix
summary: Harden native Unity harness routing by retiring the stale broad native aggregate and adding a scoped matrix.
files_changed:
  - docs/superpowers/plans/2026-05-05-native-harness-hardening.md
  - firmware-v3/platformio.ini
  - firmware-v3/REFERENCE_HARNESS.md
  - firmware-v3/scripts/check_native_harness_routes.py
  - firmware-v3/scripts/native_harness_matrix.py
  - firmware-v3/src/config/audio_config.h
  - firmware-v3/src/effects/ieffect/BeatParitySpriteEffect.cpp
  - firmware-v3/test/test_audio_benchmark/test_pipeline_benchmark.cpp
  - firmware-v3/test/test_native/mocks/ESPAsyncWebServer.h
  - firmware-v3/test/test_native/test_ws_effects_codec.cpp
  - firmware-v3/test/test_ws_effects_codec/test_ws_effects_codec.cpp
  - firmware-v3/test/test_ws_zones_codec/test_ws_zones_codec.cpp
  - firmware-v3/test/tools/README.md
  - .github/workflows/audio_benchmark.yml
  - .github/workflows/codec_boundary_check.yml
  - .github/workflows/firmware_build_check.yml
  - CHANGELOG.md
validation: python3 scripts/check_native_harness_routes.py; python3 scripts/native_harness_matrix.py; pio run -e esp32dev_audio_esv11_k1v2_32khz; git diff --check
breaking_change: false
follow_ups: []
---

## Details

Retires the stale broad `native_test` aggregate and routes native host verification through scoped PlatformIO environments so unrelated codec, effect, router, and benchmark tests cannot be linked into an incoherent object graph again.
