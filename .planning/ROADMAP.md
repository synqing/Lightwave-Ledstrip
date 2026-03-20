# Roadmap: main.cpp Decomposition

## Overview

Decompose `firmware-v3/src/main.cpp` from a 4,356-line monolith into four focused modules: shared runtime state header, serial JSON gateway, capture streamer, and serial CLI -- with system init helpers extracted last once all modules are stable. Each phase extracts one coherent responsibility, verified by build + flash + soak test on K1 hardware. Zero functional changes throughout. The file shrinks from 4,356 lines to ~400-500 lines.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3, 4): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Shared State + JSON Gateway** - Create runtime_state.h and extract ~920-line JSON command dispatch to SerialJsonGateway.cpp
- [ ] **Phase 2: Capture Streamer** - Extract ~650 lines of capture infrastructure to CaptureStreamer.cpp, unify sync/async frame assembly
- [ ] **Phase 3: Serial CLI** - Extract ~2,100 lines of CLI command handlers to SerialCLI.cpp
- [ ] **Phase 4: System Init + Final Verification** - Extract ~300 lines of init helpers to SystemInit.cpp, verify final state

## Phase Details

### Phase 1: Shared State + JSON Gateway
**Goal**: External files can include runtime globals from a shared header, and all JSON serial command processing lives in its own module
**Depends on**: Nothing (first phase)
**Requirements**: STATE-01, STATE-02, JSON-01, JSON-02, JSON-03, VER-01, VER-02, VER-03
**Success Criteria** (what must be TRUE):
  1. V1ApiRoutes.cpp and WsEffectsCommands.cpp include runtime_state.h instead of ad-hoc extern declarations, and build succeeds
  2. Sending any JSON command over serial (e.g., `{"cmd":"getStatus"}`) produces the identical response as before extraction
  3. K1 soak test runs 5+ minutes with zero panics, zero RMT errors, serial `s` command output matches pre-extraction baseline
  4. main.cpp is ~920 lines shorter and contains no JSON parsing or JSON handler code
**Plans**: TBD

Plans:
- [ ] 01-01: TBD
- [ ] 01-02: TBD

### Phase 2: Capture Streamer
**Goal**: All capture streaming infrastructure lives in CaptureStreamer.cpp with a unified frame assembly path
**Depends on**: Phase 1 (shared runtime_state.h for extern globals)
**Requirements**: CAP-01, CAP-02, CAP-03, CAP-04, VER-01, VER-02
**Success Criteria** (what must be TRUE):
  1. `capture stream b 30` starts async capture at 30 FPS and `capture stop` halts it -- identical behaviour to pre-extraction
  2. Sync and async capture paths call a single `assembleFrame()` function instead of duplicated frame assembly code
  3. All ~15 capture globals and PSRAM buffer allocations live in CaptureStreamer.cpp, not main.cpp
  4. K1 soak test runs 5+ minutes with capture active -- zero panics, zero frame corruption, PSRAM buffers intact
**Plans**: TBD

Plans:
- [ ] 02-01: TBD

### Phase 3: Serial CLI
**Goal**: All 40+ multi-char and 35+ single-char serial command handlers live in SerialCLI.cpp
**Depends on**: Phase 1 (shared state), Phase 2 (capture commands route through CaptureStreamer)
**Requirements**: CLI-01, CLI-02, CLI-03, CLI-04, CLI-05, VER-01, VER-02, VER-03
**Success Criteria** (what must be TRUE):
  1. Every single-char hotkey and multi-char command produces identical serial output and side effects as before extraction
  2. Static locals (`ambientEffectIds[170]`, `currentEffect`, `currentRegister`) remain static-storage-duration -- not stack variables
  3. WiFi CLI (`wifi connect`) is preserved with a prominent AP-only warning comment, and the STA escape hatch still functions via serial
  4. Cherry-picked commands (VAL: forwarding, tempo tuning, dbg motion) execute correctly
  5. K1 soak test runs 5+ minutes exercising serial commands -- zero panics, `s` command output matches baseline
**Plans**: TBD

Plans:
- [ ] 03-01: TBD
- [ ] 03-02: TBD

### Phase 4: System Init + Final Verification
**Goal**: Init helpers extracted to SystemInit.cpp, setup() is a thin orchestrator, main.cpp is 400-500 lines
**Depends on**: Phase 1, Phase 2, Phase 3
**Requirements**: INIT-01, INIT-02, INIT-03, INIT-04, VER-01, VER-02, VER-03, VER-04, VER-05
**Success Criteria** (what must be TRUE):
  1. setup() calls init helpers in the exact same order as before -- all 12 ordering dependencies preserved
  2. Cherry-picked init code (WDT safe-mode, TTP223 button init, boot degraded signals) executes identically
  3. main.cpp is 400-500 lines: includes, setup() orchestrator, loop() skeleton, and nothing else
  4. K1 cold boot + 5-minute soak test passes with zero panics, identical boot sequence in serial log
  5. firmware-v3/docs/reference/codebase-map.md updated to reflect new module structure
**Plans**: TBD

Plans:
- [ ] 04-01: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Shared State + JSON Gateway | 0/2 | Not started | - |
| 2. Capture Streamer | 0/1 | Not started | - |
| 3. Serial CLI | 0/2 | Not started | - |
| 4. System Init + Final Verification | 0/1 | Not started | - |
