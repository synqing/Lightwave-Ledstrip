---
abstract: "Phase 2 of iOS↔firmware parity. F-1..F-4 resolved by Captain delegation (contract = regeneratable artefact; effect cohort = isExperimental flag; legacy paths kept; runtime params = end-user UX). Five parallel sandboxed SSAs land: runtime parameter sheet, STM/VRMS subscriptions + audio cards, effect/zone preset CRUD surface, show playback transport, effect picker enhancements. Path canonicalisation and contract YAML reconciliation explicitly out of scope per F-1/F-3 decisions."
---

# iOS ↔ Firmware Parity — Phase 2

**Branch:** `feature/ios-parity-phase-2` (off `feature/ios-parity-phase-1`)
**Lane:** Superpowers `/dispatching-parallel-agents` → `/test-driven-development` per task

## Background

Phase 1 (commit `3a080416`) closed the architectural blind spot — capability discovery, broadcast event coverage, parameterType decoder. Phase 2 ships the user-facing iOS surfaces gated on F-1..F-4 calibration debt, now resolved.

## F-1..F-4 resolutions (Captain-delegated 2026-05-01)

- **F-1 — Contract authority:** firmware is source-of-truth; YAML is a regeneratable artefact. iOS targets firmware behaviour. Out of Phase 2 scope.
- **F-2 — Effect cohort:** resolved by `isExperimental` flag already shipped. iOS `filteredEffects()` already filters. Phase 2 adds power-user toggle.
- **F-3 — Path canonicalisation:** keep legacy paths. No-op for Phase 2.
- **F-4 — Runtime parameter UX:** end-user surface; expose every parameter with a `displayName` via type-appropriate controls (FLOAT/INT/BOOL/ENUM).

Full rationale: `BACKLOG.md` § Critical — Upstream Calibration Debt.

## Tasks (5 parallel sandboxed SSAs)

### Task P2-1 — Runtime parameter sheet (F-4 surface)
**Owner:** Agent P2-1 (ios-development-specialist, sandbox `/tmp/agent_p2runtime_<TS>/`)
**Files:** Create `LightwaveOS/Views/Play/EffectParameterSheet.swift`, extend `LightwaveOS/ViewModels/EffectViewModel.swift` with `loadEffectParameters(effectId:)` + `setRuntimeParameter(effectId:name:value:)`, extend `RESTClient.swift` with the GET + POST endpoints for `/api/v1/effects/parameters`, new tests in `LightwaveOSTests/EffectParameterSheetTests.swift`.
**Behaviour:** Sheet presents every `EffectParameter` (Phase 1 type) with a control matched to `parameterType` — FLOAT → `Slider` over `[min, max]` with current value; INT → `Stepper`; BOOL → `Toggle`; ENUM → `Picker` (firmware enum case names not yet exposed; render as integer picker for now); `unknown` → fallback slider. Hide parameters where `displayName == nil` (developer-only heuristic per F-4). Slider sends debounced (150 ms) on change.
**TDD:** test the decode of GET response + the encode of POST body; test the parameterType → control-type mapping helper; test displayName-nil-hides logic.

### Task P2-2 — STM + VRMS subscriptions + audio cards
**Owner:** Agent P2-2 (ios-development-specialist, sandbox `/tmp/agent_p2stm_<TS>/`)
**Files:** Extend `WebSocketService.swift` with `subscribeSTM` / `unsubscribeSTM` / `subscribeVRMS` / `unsubscribeVRMS` actor methods + STM/VRMS binary frame decoders + new `Event` cases, extend `AudioViewModel.swift` with handlers + observable state, create `LightwaveOS/Views/Audio/STMSpectralCard.swift` + `VRMSPerceptualCard.swift`, extend `LightwaveOSTests/WebSocketServiceTests.swift` with new sub command tests + binary frame decoder tests.
**Behaviour:** WS commands `stm.subscribe` / `stm.unsubscribe` / `vrms.subscribe` / `vrms.unsubscribe` per firmware contract. STM frame: 250-byte binary at ~30 FPS containing spectral + temporal + derived metrics (look up wire format in `firmware-v3/src/network/webserver/ws/WsStmCommands.cpp`). VRMS frame: similar pattern (look up). Cards render minimal visualisation (line chart for STM spectral, single value or bar for VRMS).
**TDD:** test enum-case round-trip for new commands; test binary-frame decoder against a known fixture (extract a real frame from firmware source as the test fixture).

### Task P2-3 — Preset surface (effect + zone CRUD)
**Owner:** Agent P2-3 (ios-development-specialist, sandbox `/tmp/agent_p2presets_<TS>/`)
**Files:** Replace stubs in `LightwaveOS/ViewModels/PresetsViewModel.swift` and `LightwaveOS/Views/Device/PresetsView.swift` with real CRUD. Wire effect-preset commands (REST `/api/v1/effect-presets/*`, WS `effectPresets.list/get/saveCurrent/load/delete`) and zone-preset commands (REST `/api/v1/zone-presets/*`, WS `zonePresets.list/get/saveCurrent/load/delete`). Tests: `LightwaveOSTests/PresetsViewModelTests.swift`.
**Behaviour:** PresetsView shows two segmented sections — "Effect Presets" and "Zone Presets". Each section: list with name, load button, delete button; "Save Current" button at top with name input. Use REST for read (list + get), WS for mutations (save/load/delete) so iOS receives broadcast confirmations via Phase 1's new `effectPresets.saved` / `effectPresets.deleted` cases.
**TDD:** test PresetsViewModel state transitions (loading → loaded → error); test that save/load/delete dispatch the correct WS commands; test broadcast-driven refresh.

### Task P2-4 — Show playback transport
**Owner:** Agent P2-4 (ios-development-specialist, sandbox `/tmp/agent_p2shows_<TS>/`)
**Files:** Replace stub in `LightwaveOS/Views/Device/ShowsView.swift`, create `LightwaveOS/ViewModels/ShowViewModel.swift`, extend `RESTClient.swift` with `getShows` / `getCurrentShow`, extend `WebSocketService.swift` with `show.play / pause / resume / stop / seek / status` commands, tests in `LightwaveOSTests/ShowViewModelTests.swift`.
**Behaviour:** ShowsView lists available shows from `/api/v1/shows` + shows the current playing show + transport controls (play/pause/resume/stop) + seek slider. Use WS for transport commands (low-latency); REST for list. Subscribe to `show.status` push updates.
**TDD:** test ShowViewModel state machine (idle → playing → paused → stopped); test command dispatch; test inbound `show.status` event handling.

### Task P2-5 — Effect picker enhancements (F-2 power-user toggle)
**Owner:** Agent P2-5 (ios-development-specialist, sandbox `/tmp/agent_p2picker_<TS>/`)
**Files:** Extend `LightwaveOS/ViewModels/EffectViewModel.swift` and the effect picker view (likely `LightwaveOS/Views/Play/EffectSelectorView.swift` — confirm). Tests: extend existing or create `LightwaveOSTests/EffectViewModelFilterTests.swift`.
**Behaviour:**
1. Verify `filteredEffects()` correctly hides experimental effects by default.
2. Add a `showExperimental: Bool = false` toggle on EffectViewModel.
3. Add a UI control in the effect picker (settings row or icon button) to toggle it.
4. When `showExperimental == true`, picker shows ALL effects with experimentals visually flagged (badge or italic).
5. Verify decode of the post-`isExperimental` `/api/v1/effects` response — `isExperimental` field should already be in the DTO; if not, add it as Optional defaulting to false.
**TDD:** test `filteredEffects(showExperimental: false)` excludes experimentals; `showExperimental: true` includes them; test legacy decoder (no `isExperimental` field) treats absent as `false`.

## Hard constraints (apply to all SSAs)

- All ViewModels `@MainActor @Observable class`
- Network services (`RESTClient`, `WebSocketService`, `UDPStreamReceiver`) are `actor`
- `[weak self]` in Task closures
- All networking through `RESTClient` / `WebSocketService` (no raw `URLSession`)
- 150 ms slider debounce minimum
- British English in comments / docs / logs / UI strings
- K1 AP-only (do NOT touch network mode logic)
- TDD MANDATORY: failing test FIRST, then implementation

## Edit-location discipline (CRITICAL — minimises merge conflict)

Each agent uses `// MARK: Phase 2 — <task>` blocks at the END of any file region they modify. Specifically:

| File | Phase 2 mark blocks expected |
|---|---|
| RESTClient.swift | `Phase 2 — runtime parameter set` (P2-1), `Phase 2 — preset CRUD` (P2-3), `Phase 2 — show transport` (P2-4) |
| WebSocketService.swift | `Phase 2 — STM and VRMS streams` (P2-2), `Phase 2 — show transport WS commands` (P2-4) |
| EffectViewModel.swift | `Phase 2 — runtime parameters` (P2-1), `Phase 2 — picker enhancements` (P2-5) |
| AudioViewModel.swift | `Phase 2 — STM and VRMS visualisation` (P2-2) |
| AppViewModel.swift | only if a new ViewModel needs to be initialised — `Phase 2 — <task>` block |

Agents MUST NOT modify code outside their owned MARK block.

## Out of scope (per F-decisions)

- **Path canonicalisation** (F-3 = no-op). No RESTClient path refactor.
- **Contract YAML reconciliation** (F-1 = separate docs task). No edits to `docs/protocol/k1-{rest,ws}-contract.yaml`.

## Success criteria

- All 5 SSA tasks land tests green on `xcodebuild test -scheme LightwaveOS -destination 'platform=iOS Simulator,OS=18.6,name=iPhone 16'`.
- Existing test suite (Phase 1 + base) continues green — no regression.
- BACKLOG F-1..F-4 marked DECIDED with rationale.
- Pre-commit confidence gate: zero render() touch (iOS), zero WiFi mode change, all networking actor-wrapped, British English.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-05-01 | claude-opus-4-7 | Created. Captures F-1..F-4 Captain-delegated decisions and the 5-SSA Phase 2 execution plan. |
