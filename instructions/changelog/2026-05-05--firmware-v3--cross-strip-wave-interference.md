---
title: "firmware-v3: Synergy-Topology Phase 3 Move 3.2 Cross-Strip Wave Interference"
date: 2026-05-05
area: firmware-v3
kind: added
files:
  - firmware-v3/src/effects/ieffect/CrossStripWaveInterferenceEffect.h
  - firmware-v3/src/effects/ieffect/CrossStripWaveInterferenceEffect.cpp
  - firmware-v3/src/hal/esp32s3/LedDriver_S3.cpp
  - firmware-v3/src/network/webserver/ws/WsRenderCommands.cpp
  - docs/protocol/k1-rest-contract.yaml
  - docs/protocol/k1-ws-contract.yaml
  - BACKLOG.md
validation:
  - Captain K1v2 hardware visual sign-off on /dev/tty.usbmodem2101
  - python3 scripts/check_native_harness_routes.py
  - python3 scripts/native_harness_matrix.py
  - pio run -e esp32dev_audio_esv11_k1v2_32khz
refs:
  - BACKLOG.md § Synergy-Topology Programme Phase 3 Move 3.2
  - firmware-v3/docs/research/synergy-topology/Topology_Reconciliation.md §5
---

Adds `CrossStripWaveInterferenceEffect` as the Phase 3 Move 3.2 F4 dual-strip
moat effect. Captain's visual preflight selected the `3pi/4` default phase
offset because it gave the clearest top-strip tooth against bottom-strip trough
separation through the LGP.

Hardware testing found white vertical flashes were not caused by FastLED
dithering. The causal fix is a full post-`FastLED.show()` WS2812 wire-time
fence on K1v2 because the patched RMT path returns before the frame has
physically shifted out.

Also exposes LED dithering as a runtime diagnostic control over serial, REST,
and WebSocket while keeping FastLED dithering enabled by default.
