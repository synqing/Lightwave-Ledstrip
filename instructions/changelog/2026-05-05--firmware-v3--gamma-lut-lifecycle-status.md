---
id: 2026-05-05--firmware-v3--gamma-lut-lifecycle-status
date_utc: 2026-05-05
agent: codex
scope: firmware-v3
type: bugfix
summary: Rebuild colour-correction gamma LUTs after config changes and expose diagnostic status proof
files_changed:
  - docs/protocol/k1-rest-contract.yaml
  - docs/protocol/k1-ws-contract.yaml
  - firmware-v3/platformio.ini
  - firmware-v3/scripts/native_harness_matrix.py
  - firmware-v3/src/effects/enhancement/ColorCorrectionEngine.cpp
  - firmware-v3/src/effects/enhancement/ColorCorrectionEngine.h
  - firmware-v3/src/network/webserver/handlers/ColorCorrectionHandlers.cpp
  - firmware-v3/src/network/webserver/ws/WsColorCommands.cpp
  - firmware-v3/src/serial/SerialCLI.cpp
  - firmware-v3/test/test_color_correction_engine/test_color_correction_engine.cpp
  - firmware-v3/test/test_native/mocks/Preferences.h
  - firmware-v3/test/test_native/mocks/esp_log.h
  - firmware-v3/test/test_native/mocks/fastled_mock.h
validation: pio test -e native_test_color_correction_engine; python3 scripts/check_native_harness_routes.py; python3 scripts/native_harness_matrix.py; pio run -e esp32dev_audio_esv11_k1v2_32khz
breaking_change: false
follow_ups:
  - Subjective colour A/B was skipped; do not use this work to change product visual defaults.
---

## Details

`ColorCorrectionEngine::loadFromNVS()` now routes loaded config through `setConfig()` so persisted gamma settings cannot leave a stale LUT behind. Runtime gamma status now exposes `gammaEnabled`, `gammaValue`, a LUT generation id, and samples at `0,32,64,128,192,255` over serial, WebSocket, and REST.

The mutable `getConfig()` escape hatch was removed; existing command paths now copy config, mutate locally, and call `setConfig()` so gamma writes pass through the LUT lifecycle.
