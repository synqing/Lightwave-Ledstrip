---
abstract: "Phase 1 of iOS↔firmware parity. Lands three GROUNDED architectural fixes (capability discovery, missing inbound-event enum cases, forward-compat parameter decoder) plus the F-1..F-4 BACKLOG entry that documents what gates Phase 2. After this lands, iOS surfaces firmware drift instead of silently desyncing."
---

# iOS ↔ Firmware Parity — Phase 1

**Branch:** `feature/ios-parity-phase-1` (off `main` @ `80ba6ae8`)
**Lane:** Superpowers `/writing-plans` → `/dispatching-parallel-agents` → `/test-driven-development` per task
**RBDO state:** GROUNDED for Tasks A/B2/B3; DEGRADED-MODE flag on B1's API choice (sys.capabilities vs /firmware/version) — agent probes and chooses

## Background

The parity audit (this session) showed iOS at anchor commit `569d3e4b` (2026-03-04) is ~8 weeks behind firmware HEAD `6404cd77`. iOS architecture is clean (zero raw URLSession, all actor-wrapped, all `@MainActor @Observable` ViewModels). Coverage of firmware surface: REST 21%, WS 13%, capability discovery 0%. Full audit findings live in this session's transcript.

Phase 1 is intentionally narrow: the three architectural improvements that have **no upstream calibration debt** plus the BACKLOG entry that surfaces what does. Phase 2 (effect picker, runtime parameter UI, STM/VRMS visualisation, preset surface, show playback) is gated on F-1..F-4 below.

## Goal

After this phase ships:
1. iOS calls a capability/version endpoint at connect time and stores the result on the connection model — future drift becomes visible to the app instead of silent.
2. iOS' WS `Event` enum has cases for every broadcast type firmware currently emits — no broadcast is silently dropped.
3. iOS' effects.parameters decoder accepts both pre- and post-`4398af3b` shapes (with/without the `parameterType` field) without failing.
4. `BACKLOG.md` § Critical — Upstream Calibration Debt has F-1..F-4 entries gating Phase 2 scope decisions.

## Tasks (executed by parallel sandboxed agents)

### Task A — BACKLOG.md F-1..F-4 entry
**Owner:** Agent A (general-purpose, no sandbox — single-file documentation edit)
**Files touched:** `BACKLOG.md` only.
**Content:**
- F-1 — Contract authority (HIGH): is the YAML at `docs/protocol/k1-{rest,ws}-contract.yaml` source-of-truth, or has it drifted past usability? Audit found ~50 REST routes + ~40 WS commands in firmware are absent from contract; 5 WS commands in YAML have firmware handlers commented out. Blocks: every iOS catch-up task (do we align iOS to contract or to firmware reality).
- F-2 — Effect production cohort (HIGH): of the 25+ new effects landed since 569d3e4b, which are PRODUCTION (user-facing) vs EXPERIMENTAL (A/B research, dev-only). Affects effect-picker UX, palette/parameter wiring per effect, and whether iOS hides experimental effect IDs. Affected outputs: ≥3.
- F-3 — Path canonicalisation (MEDIUM): firmware accepts both modern (`/effects/current`, `/palettes/current`) and legacy (`/effects/set`, `/palettes/set`) paths. iOS uses legacy. Standardise on which? Affects RESTClient refactor scope.
- F-4 — Runtime parameter UX scope (HIGH): is `effects.parameters.set` an end-user surface (sliders in effect detail view) or developer-only (debug overlay)? Effect 0x130E exposes `silenceGate`, `decayBase`, `decaySlope`, `onsetBoost` etc. — not consumer-friendly knobs. Affects depth of effect detail view rebuild in Phase 2.

### Task B1 — Capability discovery on connect
**Owner:** Agent B1 (ios-development-specialist, sandbox `/tmp/agent_b1_<TS>/`)
**Why:** iOS currently can't detect firmware drift at runtime. Every install requires hand-matched firmware. Adding capability discovery is the architectural unblock for Phase 2.
**Files expected to touch:** `LightwaveOS/Network/RESTClient.swift`, `LightwaveOS/Network/WebSocketService.swift` (maybe), `LightwaveOS/ViewModels/AppViewModel.swift`, new `LightwaveOS/Models/DeviceCapabilities.swift`, new `LightwaveOSTests/DeviceCapabilitiesTests.swift`.
**Behaviour:**
- After successful `getDeviceInfo` at connect, fetch capabilities. Probe order: `GET /api/v1/openapi.json` first (richest, contract-aware) → fall back to `GET /api/v1/firmware/version` → fall back to `nil` (degrade gracefully, do not block connect on this).
- Decode response into a `DeviceCapabilities` struct (firmware version, build hash if available, list of advertised endpoints/commands).
- Store on `AppViewModel` and expose to feature gates (initial use: log if discovered).
- Failure of capability fetch must NOT break connect — silently degrade, log a single line.
**TDD pattern:**
1. Write `DeviceCapabilitiesTests.swift` with three test cases: (a) decodes a valid `/api/v1/firmware/version` JSON; (b) decodes an `openapi.json` skeleton; (c) gracefully handles 404. Watch them fail.
2. Implement `DeviceCapabilities` struct + `RESTClient.getCapabilities()` actor method until tests pass.
3. Wire into `AppViewModel` connect flow with `[weak self]` and run full xcodebuild test.

### Task B2 — Inbound-event enum cases for silently-dropped broadcasts
**Owner:** Agent B2 (ios-development-specialist, sandbox `/tmp/agent_b2_<TS>/`)
**Why:** Audit found inbound broadcasts that iOS receives but silently drops because no `Event` enum case exists: `cameraMode.changed`, `factoryPresets.changed`, `effectPresets.saved`, `effectPresets.deleted`, `colorCorrection.setGamma`, `colorCorrection.setAutoExposure`, `colorCorrection.setBrownGuardrail`. Adding cases prevents future bugs even before the UI consumes them.
**Files expected to touch:** `LightwaveOS/Network/WebSocketService.swift` (Event enum + decode switch), `LightwaveOS/ViewModels/AppViewModel.swift` (handler — can be no-op stub), `LightwaveOSTests/WebSocketServiceTests.swift` (extend).
**Constraint:** Add new cases in a clearly-marked `// MARK: Phase 1 — broadcast cases` block at the END of the enum and decode switch. Do NOT modify existing case ordering. This minimises merge-conflict surface with B1.
**TDD pattern:**
1. Extend `WebSocketServiceTests.swift` with 7 tests, one per new broadcast type, asserting the JSON decodes to the expected enum case (no payload required for now — empty associated value or raw JSON dict is fine). Watch them fail.
2. Add the 7 enum cases + decode-switch branches.
3. Add a no-op handler in `AppViewModel.handleWebSocketEvent` so the case is exhaustively covered (Swift will warn otherwise).
4. Run full xcodebuild test.

### Task B3 — Forward-compat decoder for effects.parameters
**Owner:** Agent B3 (general-purpose, sandbox `/tmp/agent_b3_<TS>/`)
**Why:** Firmware commit `4398af3b` (2026-03-24) added a `parameterType` field (FLOAT/INT/BOOL/ENUM) to `effects.parameters` responses. iOS' decoder may parse-and-ignore (forward-compat) or break — needs verification, and where missing, hardening.
**Files expected to touch:** Whichever DTO type backs the parameters response (likely in `LightwaveOS/Network/RESTClient.swift` or a `LightwaveOS/Models/EffectParameter.swift` if one exists), and `LightwaveOSTests/RESTClientTests.swift` (extend) or new `LightwaveOSTests/EffectParameterDecodingTests.swift`.
**Constraint:** Decoder must accept BOTH old shape (no `parameterType`) and new shape (`parameterType: "FLOAT"`). Old payloads decode to `parameterType = nil` or `.unknown`, new payloads decode the field. Field must be Optional or have a default-decodable default.
**TDD pattern:**
1. Write `EffectParameterDecodingTests.swift` with two test cases: (a) old shape `{name, value, min, max}` decodes successfully; (b) new shape `{name, value, min, max, parameterType: "FLOAT"}` decodes successfully and exposes the type. Watch them fail or pass-by-luck.
2. If passing, mark Task B3 NO-OP and report.
3. If failing, modify the DTO to add an Optional `parameterType` field with a Codable enum, re-run tests until green.

## Test strategy

Each agent runs `xcodegen generate` then `xcodebuild test -scheme LightwaveOS -destination 'platform=iOS Simulator,name=iPhone 16'` in its sandbox. Agent returns: diff against `lightwave-ios-v2/`, test command output (last 30 lines), files changed, files created.

After all agents return, orchestrator integrates winning patches into the real tree on `feature/ios-parity-phase-1`, runs `xcodegen generate` + full `xcodebuild test` once, then commits.

## Success criteria

- All four tasks land on `feature/ios-parity-phase-1`.
- `xcodebuild test` is green on the real tree.
- `BACKLOG.md` contains F-1..F-4 entries under Critical — Upstream Calibration Debt.
- iOS has a `DeviceCapabilities` model + connect-time fetch with graceful degradation.
- WS `Event` enum has 7 new cases; no compiler exhaustiveness warnings.
- `effects.parameters` decoder accepts both old and new shapes.
- Pre-Commit Confidence Gate: zero render() impact (this is iOS), zero WiFi mode change (K1 AP-only preserved), British English in comments/logs.

## Out of scope (Phase 2)

- Effect picker UX update for new EIDs (depends on F-2)
- Runtime parameter sliders (depends on F-4)
- STM/VRMS audio tab visualisation
- Effect/zone preset surface (PresetsView/PresetsViewModel are stubs)
- Show playback (ShowsView is a stub)
- Path canonicalisation (depends on F-3)
- Contract YAML reconciliation (depends on F-1)

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-04-30 | claude-opus-4-7 | Created from this session's parity audit synthesis. Captures Phase 1 narrow scope: 3 GROUNDED architectural fixes + BACKLOG F-1..F-4 entry. |
