# Critical Bug Fixes: C-2, C-3, C-4

## What This Is

Fixing three critical bugs identified in the codebase audit: API type truncation (C-2), dangling pointer in WebSocket response (C-3), and cross-core data race via getControlBusMut() (C-4). All are production bugs with defined fix approaches.

## Core Value

Eliminate undefined behaviour and silent data corruption in the network API layer before production release.

## Requirements

### Validated

- ✓ Bugs identified and root-caused in CONCERNS.md audit — existing
- ✓ Fix approaches defined with estimated effort — existing
- ✓ Codebase map and architecture docs available — existing

### Active

- [ ] C-2: Fix uint8_t→uint16_t type truncation in 5 audio mapping endpoints
- [ ] C-3: Fix dangling pointer in handleEffectsGetCategories()
- [ ] C-4: Remove getControlBusMut(), route audio config through actor message queue
- [ ] Build + flash + soak test after fixes

### Out of Scope

- C-1 PipelineCore — already mitigated, separate concern
- New API features — fix only, no additions
- WiFi architecture — AP-only constraint preserved

## Context

**C-2** (1 hour): Five endpoints in V1ApiRoutes.cpp parse effect IDs as uint8_t instead of uint16_t (EffectId). Effect IDs >= 256 are silently truncated to 0. Fix: change type + add validation.

**C-3** (30 min): WsEffectsCommands.cpp handleEffectsGetCategories() stores pointers to a stack-local char buffer that gets destroyed each loop iteration. All family name pointers alias dead stack memory. Fix: use array of buffers outside loop.

**C-4** (3-4 hours): WebServer's AsyncTCP task calls getControlBusMut() to mutate AudioActor's ControlBus state without synchronisation. Both run on Core 0 but in different FreeRTOS tasks with preemptive scheduling. Fix: remove getControlBusMut(), route changes through actor message queue, add read-only snapshot for telemetry.

## Constraints

- **Thread safety**: C-4 fix must use actor message queue pattern, not add mutexes
- **No functional changes beyond the fixes**: Don't refactor surrounding code
- **British English**: All comments use centre, colour, initialise
- **Build verification**: pio run -e esp32dev_audio_esv11_k1v2_32khz must succeed
- **Hardware verification**: Flash to K1, soak test 5+ minutes

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Fix C-2 and C-3 first (quick wins) | 30-60 min each, eliminates 2 critical bugs immediately | — Pending |
| C-4 uses message queue, not mutex | Consistent with actor model architecture, avoids deadlock risk | — Pending |
| Remove getControlBusMut() entirely | No mutable cross-task access — force all mutations through message queue | — Pending |

---
*Last updated: 2026-03-21 after project initialisation*
