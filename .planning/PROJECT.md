# LightwaveOS main.cpp Decomposition

## What This Is

Refactoring `firmware-v3/src/main.cpp` from a 4,356-line monolith into focused, testable modules. The file currently conflates 7+ responsibilities: board init, serial JSON gateway, serial CLI (42 command handlers), WiFi management, show playback, effect parameter tuning, and system health monitoring. This is a maintainability-driven decomposition with no functional changes — behaviour must be identical before and after.

## Core Value

Every extraction must produce byte-identical runtime behaviour. No regressions, no new features, no "improvements while we're in there." The K1 must soak-test clean after each phase.

## Requirements

### Validated

- ✓ Risk assessment completed — 11 CONCERNS.md items cross-referenced, safe extraction order identified — existing
- ✓ SRAM/stack headroom confirmed — loopTask 12KB (healthy), renderer 43KB free, internal DRAM 41KB free — existing
- ✓ Cherry-picked translation engine code locations mapped across all sections — existing

### Active

- [ ] P0: Create `config/runtime_state.h` — move extern-visible globals to shared header
- [ ] P1: Extract `SerialJsonGateway.cpp` — ~920 lines, JSON command dispatch
- [ ] P2: Extract `CaptureStreamer.cpp` — ~650 lines, capture infra + unify sync/async frame assembly
- [ ] P3: Extract `SerialCLI.cpp` — ~2,100 lines, multi-char + single-char command handlers
- [ ] P4: Extract `SystemInit.cpp` — ~300 lines, init helpers (setup() stays as thin orchestrator)
- [ ] Build + flash + soak test after each phase
- [ ] main.cpp reduced to ~400-500 lines (includes, setup() orchestrator, loop() skeleton)

### Out of Scope

- Functional changes — no new features, no parameter tuning, no "improvements"
- PipelineCore fixes — broken audio backend is a separate concern (C-1)
- API type truncation fixes — V1ApiRoutes uint8_t issue is separate (C-2)
- WiFi architecture changes — AP-only constraint must be preserved exactly
- Test coverage expansion — testing is a follow-up milestone, not this one

## Context

**Codebase:** firmware-v3 is an ESP32-S3 firmware using FreeRTOS actor model (AudioActor Core 0, RendererActor Core 1). main.cpp runs on the Arduino loopTask (Core 0, priority 1).

**Recent changes:** 363 lines cherry-picked from codex/perceptual-translation-v3 on 2026-03-21, adding factory presets, WDT safe-mode, TTP223 button init, VAL: command forwarding, tempo parameter tuning, and dbg motion domain — all scattered across main.cpp sections.

**Why now:** The file has grown from 3,441 to 4,356 lines. Every modification risks unintended side effects. The translation engine cherry-pick demonstrated the problem — the v3 branch had 363 lines of additions that auto-merged cleanly only because they happened to land in non-overlapping regions. Next time we may not be so lucky.

**Key risks identified in pre-assessment:**
- `g_factoryPresetIndex` extern'd in 2 other files — must create shared header first (P0)
- `ambientEffectIds[170]` static local — must NOT become stack variable (340 bytes)
- 12 init ordering dependencies in setup() — do NOT reorder
- WiFi `wifi connect` is the ONLY STA entry point — must preserve with prominent warning
- Capture frame assembly duplicated in sync/async paths — extraction should unify (net improvement)
- `webServerInstance->update()` must remain on loopTask

## Constraints

- **Thread safety**: All extracted modules must preserve existing thread assumptions — loopTask runs on Core 0, renderer on Core 1. No cross-core state access without existing mutex/snapshot patterns.
- **Stack budget**: loopTask is 12,288 bytes. Static locals must stay static (not become stack vars). No new heap allocation in any path.
- **Init ordering**: setup() has 12 ordering dependencies. Extraction creates helper functions called in sequence — the ORDER stays in one place.
- **AP-only WiFi**: K1 boots AP-only. WiFi CLI extraction must carry explicit warning about STA escape hatch.
- **British English**: All new files use centre, colour, initialise, serialise, behaviour.
- **Build verification**: `pio run -e esp32dev_audio_esv11_k1v2_32khz` must succeed after each phase.
- **Hardware verification**: Flash to K1 via `/dev/cu.usbmodem1101`, soak test 5+ minutes, check serial for errors after each phase.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Full teardown (P0-P4), not incremental | Maintainability debt compounds — partial extraction leaves an awkward middle state | — Pending |
| Build + flash + soak between each phase | Stage 1 experience showed transient stutter can appear and disappear — only hardware soak catches it | — Pending |
| Keep setup() as thin orchestrator (P4) | 12 init ordering dependencies make full extraction dangerous — helpers yes, reordering no | — Pending |
| Unify sync/async capture frame assembly in P2 | 150 lines duplicated — extraction is the natural moment to consolidate | — Pending |
| Create runtime_state.h before any extraction (P0) | Extern declarations in V1ApiRoutes.cpp and WsEffectsCommands.cpp will break without it | — Pending |

---
*Last updated: 2026-03-21 after project initialisation*
