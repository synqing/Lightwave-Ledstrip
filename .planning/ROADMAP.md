# Roadmap: Critical Bug Fixes C-2, C-3, C-4

## Overview

Fix three critical bugs in the network API layer: type truncation in audio mapping endpoints (C-2), dangling pointer in WebSocket categories response (C-3), and cross-core data race via getControlBusMut() (C-4). Two phases: quick wins first, then the architectural fix.

## Phases

- [ ] **Phase 1: Quick Fixes (C-2 + C-3)** — Fix type truncation and dangling pointer (~90 min total)
- [ ] **Phase 2: Data Race Fix (C-4)** — Remove getControlBusMut(), route through message queue (~3-4 hours)

## Phase Details

### Phase 1: Quick Fixes (C-2 + C-3)
**Goal**: Eliminate silent data corruption in audio mapping API and undefined behaviour in WebSocket categories response
**Depends on**: Nothing
**Requirements**: C2-01, C2-02, C2-03, C3-01, C3-02, VER-01, VER-02
**Success Criteria**:
  1. All 5 audio mapping endpoints accept and correctly handle EffectId values >= 256
  2. Invalid effect IDs return HTTP 400 with error message
  3. WebSocket /effects/categories returns correct, distinct family names for all effect families
  4. K1 soak 5+ minutes — zero panics

Plans:
- [ ] 01-01: TBD

### Phase 2: Data Race Fix (C-4)
**Goal**: Eliminate all unsynchronised mutable cross-task access to AudioActor state
**Depends on**: Phase 1
**Requirements**: C4-01, C4-02, C4-03, C4-04, VER-01, VER-02, VER-03
**Success Criteria**:
  1. getControlBusMut() method removed from AudioActor public API — no callers remain
  2. Audio config changes (zone AGC, envelope thresholds) delivered via actor message queue
  3. WebServer/WsCommands use read-only snapshot for audio diagnostics
  4. iOS app can connect, browse effects, and modify parameters without crashes
  5. K1 soak 5+ minutes — zero panics, audio-reactive effects respond correctly

Plans:
- [ ] 02-01: TBD

## Progress

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Quick Fixes (C-2 + C-3) | 0/1 | Not started | - |
| 2. Data Race Fix (C-4) | 0/1 | Not started | - |
