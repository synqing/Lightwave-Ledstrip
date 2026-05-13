---
id: 2026-05-06--firmware-v3--k1v2-batch-a-sram-reclaim
date_utc: 2026-05-06
agent: codex
scope: firmware-v3
type: fix
summary: Reclaim K1v2 internal SRAM from cold Batch A buffers and registries
files_changed:
  - BACKLOG.md
  - firmware-v3/scripts/check_k1v2_batch_a_sram_reclaim.py
  - firmware-v3/docs/research/k1v2_sram_psram_reclaim_run_2026-05-06.md
  - firmware-v3/src/serial/CaptureStreamer.h
  - firmware-v3/src/serial/CaptureStreamer.cpp
  - firmware-v3/src/network/webserver/StaticAssetRoutes.cpp
  - firmware-v3/src/network/webserver/WsCommandRouter.h
  - firmware-v3/src/network/webserver/WsCommandRouter.cpp
  - firmware-v3/src/plugins/BuiltinEffectRegistry.h
  - firmware-v3/src/plugins/BuiltinEffectRegistry.cpp
validation: K1v2 build, Batch A symbol guard, WS router native test, MAC-verified upload, and serial memory/status/effect-switch smoke
breaking_change: false
follow_ups:
  - Resume Phase 5 visual-quality tuning on promising effects with RTS parked unless Captain reopens it.
---

## Details

Moves cold/control-path Batch A memory consumers out of permanent internal BSS: capture streaming scratch buffers now require PSRAM, the launcher route allocates its buffer persistently off static DRAM, and the WebSocket/effect registries allocate their tables once during registration. Hardware verification on K1v2 exceeded the `>=22 KB` no-client internal-heap target and accepted the required effect-switch smoke.
