# Requirements: main.cpp Decomposition

**Defined:** 2026-03-21
**Core Value:** Byte-identical runtime behaviour -- every extraction must soak-test clean on K1 hardware.

## v1 Requirements

### Shared State

- [ ] **STATE-01**: Extern-visible globals (`g_factoryPresetIndex`, `g_externalNvsSaveRequest`, `zoneConfigMgr`) accessible via shared header
- [ ] **STATE-02**: All extern declarations in V1ApiRoutes.cpp, WsEffectsCommands.cpp, persistence_trigger.h updated to use shared header

### Serial JSON Gateway

- [ ] **JSON-01**: `processSerialJsonCommand()` and all 30+ JSON handlers extracted to `SerialJsonGateway.cpp/h`
- [ ] **JSON-02**: Gateway receives dependencies via constructor/params (ActorSystem, RendererActor, ZoneComposer, DynamicShowStore)
- [ ] **JSON-03**: No functional change -- all JSON commands produce identical responses

### Capture Streaming

- [ ] **CAP-01**: All capture infrastructure (~15 globals, PSRAM buffers, FreeRTOS task, esp_timer) extracted to `CaptureStreamer.cpp/h`
- [ ] **CAP-02**: Sync and async frame assembly paths unified into single `assembleFrame()` function
- [ ] **CAP-03**: Capture serial commands (`capture stream`, `capture stop`, `capture fps`, etc.) route through extracted module
- [ ] **CAP-04**: PSRAM buffer allocation preserved (no stack migration)

### Serial CLI

- [ ] **CLI-01**: All 40+ multi-char command handlers extracted to `SerialCLI.cpp/h`
- [ ] **CLI-02**: All 35+ single-char hotkey handlers co-located with multi-char handlers
- [ ] **CLI-03**: Static locals (`ambientEffectIds[170]`, `currentEffect`, `currentRegister`, etc.) preserved as static or class members -- NOT stack variables
- [ ] **CLI-04**: WiFi CLI (`wifi connect`) preserved with explicit AP-only warning comment
- [ ] **CLI-05**: Cherry-picked commands preserved: VAL: forwarding, tempo tuning, dbg motion

### System Init

- [ ] **INIT-01**: Init helper functions extracted to `SystemInit.cpp/h`
- [ ] **INIT-02**: setup() remains as thin orchestrator calling helpers in exact current order
- [ ] **INIT-03**: All 12 init ordering dependencies preserved -- no reordering
- [ ] **INIT-04**: Cherry-picked init code preserved: WDT safe-mode, TTP223, boot degraded signals

### Verification

- [ ] **VER-01**: Build succeeds (`pio run -e esp32dev_audio_esv11_k1v2_32khz`) after each phase
- [ ] **VER-02**: Flash to K1 and soak 5+ minutes after each phase -- zero panics, zero RMT errors
- [ ] **VER-03**: Serial `s` command produces identical status output before and after
- [ ] **VER-04**: main.cpp reduced to ~400-500 lines after all phases complete
- [ ] **VER-05**: Reference docs updated (`firmware-v3/docs/reference/codebase-map.md`)

## v2 Requirements

### Testing

- **TEST-01**: Unit tests for SerialJsonGateway command parsing
- **TEST-02**: Unit tests for SerialCLI command routing
- **TEST-03**: Integration test for capture streaming lifecycle

### Further Decomposition

- **DECOMP-01**: Extract ShowPlaybackController from SerialCLI
- **DECOMP-02**: Extract EffectNavigator from SerialCLI hotkeys

## Out of Scope

| Feature | Reason |
|---------|--------|
| Functional changes | This is a pure refactor -- no new features, no parameter changes |
| PipelineCore fixes | Separate concern (C-1), different risk profile |
| API type truncation | Separate concern (C-2), different files entirely |
| WiFi architecture changes | AP-only constraint must be preserved exactly as-is |
| New serial commands | Add features in a follow-up, not during extraction |
| Performance optimisation | Do not change timing, frame pacing, or task priorities |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| STATE-01 | Phase 1: Shared State + JSON Gateway | Pending |
| STATE-02 | Phase 1: Shared State + JSON Gateway | Pending |
| JSON-01 | Phase 1: Shared State + JSON Gateway | Pending |
| JSON-02 | Phase 1: Shared State + JSON Gateway | Pending |
| JSON-03 | Phase 1: Shared State + JSON Gateway | Pending |
| CAP-01 | Phase 2: Capture Streamer | Pending |
| CAP-02 | Phase 2: Capture Streamer | Pending |
| CAP-03 | Phase 2: Capture Streamer | Pending |
| CAP-04 | Phase 2: Capture Streamer | Pending |
| CLI-01 | Phase 3: Serial CLI | Pending |
| CLI-02 | Phase 3: Serial CLI | Pending |
| CLI-03 | Phase 3: Serial CLI | Pending |
| CLI-04 | Phase 3: Serial CLI | Pending |
| CLI-05 | Phase 3: Serial CLI | Pending |
| INIT-01 | Phase 4: System Init + Final Verification | Pending |
| INIT-02 | Phase 4: System Init + Final Verification | Pending |
| INIT-03 | Phase 4: System Init + Final Verification | Pending |
| INIT-04 | Phase 4: System Init + Final Verification | Pending |
| VER-01 | All phases (per-phase gate) | Pending |
| VER-02 | All phases (per-phase gate) | Pending |
| VER-03 | All phases (per-phase gate) | Pending |
| VER-04 | Phase 4: System Init + Final Verification | Pending |
| VER-05 | Phase 4: System Init + Final Verification | Pending |

**Coverage:**
- v1 requirements: 23 total
- Mapped to phases: 23
- Unmapped: 0

---
*Requirements defined: 2026-03-21*
*Last updated: 2026-03-21 after roadmap creation -- Phase 1 = Shared State + JSON Gateway (P0+P1 merged at coarse granularity)*
