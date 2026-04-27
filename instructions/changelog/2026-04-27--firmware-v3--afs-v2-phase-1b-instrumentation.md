---
id: 2026-04-27--firmware-v3--afs-v2-phase-1b-instrumentation
date_utc: 2026-04-27
agent: codex
scope: firmware-v3
type: feature
summary: Add Audio Feature Surface v2 Phase 1B trace instrumentation and health gauges.
files_changed:
  - firmware-v3/src/audio/AudioActor.cpp
  - firmware-v3/src/audio/AudioActor.h
  - firmware-v3/src/audio/contracts/SnapshotBuffer.h
  - firmware-v3/src/audio/onset/OnsetDetector.cpp
  - firmware-v3/src/audio/onset/OnsetDetector.h
  - firmware-v3/src/core/actors/RendererActor.cpp
  - firmware-v3/src/hal/interface/ILedDriver.h
  - firmware-v3/src/hal/esp32s3/LedDriver_S3.cpp
  - firmware-v3/src/main.cpp
  - firmware-v3/src/network/WebServer.h
  - firmware-v3/src/network/WiFiManager.cpp
  - firmware-v3/src/network/webserver/WsGateway.cpp
  - firmware-v3/src/network/webserver/WsGateway.h
  - firmware-v3/src/network/webserver/ws/WsOtaCommands.cpp
  - firmware-v3/src/config/features.h
validation: git diff --check; python3 firmware-v3/tools/check_effect_contracts.py; pio run -e esp32dev_audio_esv11_k1v2_32khz_trace
breaking_change: false
follow_ups: []
---

## Details

Adds low-cost MabuTrace counters and instants for audio hop timing, onset subspans, snapshot handoff cost, render-frame work time, LED output health, heap/stack health, AP/WebSocket state, and OTA/WiFi markers.
