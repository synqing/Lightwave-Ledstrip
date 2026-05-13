---
id: 2026-05-07--firmware-v3--vp-stack-introspection-command
date_utc: 2026-05-07
agent: Codex
scope: firmware-v3
type: feature
summary: Add serial VP stack introspection for current renderer topology, surfaces, colour layers, silence policy, EdgeMixer, capture, and LED transport state.
files_changed:
  - firmware-v3/src/core/diagnostics/VpStackIntrospection.h
  - firmware-v3/src/core/actors/RendererActor.h
  - firmware-v3/src/core/actors/RendererActor.cpp
  - firmware-v3/src/core/SystemInit.cpp
  - firmware-v3/src/serial/SerialCLI.cpp
  - firmware-v3/test/test_vp_stack_introspection/test_main.cpp
  - firmware-v3/platformio.ini
  - firmware-v3/scripts/native_harness_matrix.py
validation: pio test -e native_test_vp_stack_introspection; python3 scripts/native_harness_matrix.py; pio run -e esp32dev_audio_esv11_k1v2_32khz; MAC-verified upload to K1v2 b4:3a:45:a5:87:f8 on /dev/cu.usbmodem2101; serial `vp stack`, `dbg memory`, and `s`.
breaking_change: false
follow_ups: []
---

## Details

Implements the serial-first, read-only `vp stack` command from the VP stack introspection spec. The command reports the current effect, palette, renderer topology, authored/correction/output surfaces, colour correction/gamma state, tone-map and silence-policy state, EdgeMixer settings, capture taps, LED show statistics, dithering, and the expected S3 wire-time guard. No REST, WebSocket, WiFi mode, visual default, or render-path behaviour was changed.
