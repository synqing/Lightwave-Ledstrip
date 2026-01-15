# Task List: Legacy Parity Recovery

## Quick Reference (Extracted from PRD & Blueprint)

### Ownership Rules
*Source: PRD §7 + Blueprint §2.1*

| Artifact | Created By | App's Role | DO NOT |
|----------|------------|------------|--------|
| ZoneComposer | App (main.cpp) | Owner/Create | Do NOT create in Router |
| Zone Config (NVS) | App | Manager/Persist | Do NOT lose on reboot |
| WS Error | App | Creator/Send | Do NOT fail silently |
| Legacy Command | External (Hub) | Consumer/Alias | Do NOT reject 't' key |

### State Variables
*Source: PRD §8 + Blueprint §3*

| State Variable | Initial Value | Created When | Cleared When | Persists Across |
|----------------|---------------|--------------|--------------|------------------|
| ZoneConfig | Default/NVS | Boot | Factory Reset | Reboot (NVS) |
| ZoneComposer | Null | `setup()` | Power Off | Runtime Only |

### Critical Boundaries
*Source: Blueprint §2.3*

- ❌ DO NOT: Initialize ZoneComposer inside `WsMessageRouter`.
- ❌ DO NOT: Reject messages with legacy `t` key.
- ❌ DO NOT: Echo updates back to the sender (ping-pong).
- ✅ MUST: Persist zone state changes to NVS (Debounced 500ms).
- ✅ MUST: Rate limit unknown message logs (1 per 2s).

### User Visibility Rules
*Source: PRD §6*

| User Action | User Sees | User Does NOT See | Timing |
|-------------|-----------|-------------------|--------|
| Send Invalid JSON | Error JSON + Log | Internal crash | Immediate |
| Disconnect Hub | Lights stay ON | "Disconnected" blackout | Immediate |
| Reboot Device | Zones restore | Default config | Boot (<2s) |

---

## Requirements Traceability

**Every requirement MUST map to at least one task. Nothing should be lost.**

| Source | Requirement | Mapped To Task |
|--------|-------------|----------------|
| PRD §4.1 FR-1 | Dual Schema Support | Task 1.2 |
| PRD §4.1 FR-2 | Command Aliasing | Task 1.3 |
| PRD §4.1 FR-3 | Zone Initialization | Task 2.2, 2.4 |
| PRD §4.1 FR-4 | Error Feedback | Task 4.2 |
| PRD §4.1 FR-5 | Disconnection Persistence | Task 3.3 |
| PRD §4.1 FR-6 | Control Arbitration | Task 1.4 |
| PRD §4.1 FR-7 | JSON Parsing | Task 1.2 |
| PRD §4.1 FR-8 | Parameter Mapping | Task 1.3 |
| PRD §4.1 FR-9 | State Persistence | Task 3.2, 3.4 |
| PRD §4.2 NFR-1 | Robustness (Log) | Task 4.1 |
| Blueprint §2.1 | Ownership Enforcement | IC-2 |
| Blueprint §4.1 | Message Routing Pipeline | IC-1 |

---

## Overview
This task list executes the "Legacy Parity Recovery" plan to restore compatibility between the `K1.8encoderS3` firmware and the legacy Hub. Key outcomes include Dual Key (`t`/`type`) support, Command Aliasing, and the re-integration of the `ZoneComposer` with NVS persistence.

## Relevant Files
- `firmware/K1.8encoderS3/src/network/WsMessageRouter.cpp`
- `firmware/K1.8encoderS3/src/network/WsMessageRouter.h`
- `firmware/K1.8encoderS3/src/main.cpp`
- `firmware/v2/src/effects/zones/ZoneComposer.cpp` (Reference)

## Build Commands
**⚠️ CRITICAL:** Run commands in `firmware/K1.8encoderS3` environment.
- Build: `cd firmware/K1.8encoderS3 && pio run -e atoms3`
- Upload: `cd firmware/K1.8encoderS3 && pio run -e atoms3 -t upload`
- Test: `cd firmware/K1.8encoderS3 && pio test -e atoms3`

---

## Tasks

### 1.0 Phase 1: Foundation (Refactor Router)

**Pre-condition:** Firmware builds, but ignores legacy Hub commands.

#### Sub-Tasks:

- [ ] 1.1 Review context: PRD §4.1, §7.2 and Blueprint §1.2, §4.1
  - **Relevant Sections:** Protocol Adapter logic, Dual Key requirements.
  - **Key Decisions:** Accept both keys, alias commands internally.
  - **Watch Out For:** Hardcoded string comparisons that miss the alias.

- [ ] 1.2 Implement Dual Key Check
  - **Input:** `StaticJsonDocument` from WebSocket.
  - **Output:** Valid `type` string (from `type` or `t`).
  - **User Sees:** No error log when sending `t`.
  - **Implements:** PRD FR-1, FR-7

- [ ] 1.3 Implement Command Aliasing
  - **Input:** Normalized `type` string.
  - **Output:** Aliased type (e.g., `effects.setCurrent` -> `effects.changed`).
  - **Implements:** PRD FR-2, FR-8

- [ ] 1.4 Implement Source Tagging & Arbitration
  - **Input:** Message payload with `source`.
  - **Output:** Tagged update to prevent echo.
  - **State Change:** Updates routed only if source != self.
  - **Implements:** PRD FR-6

**Post-condition:** Router correctly identifies and logs legacy commands (even if handlers aren't ready).

**Verification:**
- [ ] Send `{"t": "test"}` -> Router accepts it.
- [ ] Send `{"type": "test"}` -> Router accepts it.

---

### 2.0 Phase 2: Core Components (ZoneComposer)

**Pre-condition:** Router logic ready, but ZoneComposer missing.

#### Sub-Tasks:

- [ ] 2.1 Review context: PRD §7.1 and Blueprint §2.1
  - **Relevant Sections:** Ownership rules (Main vs Router).
  - **Key Decisions:** `main.cpp` owns the instance.

- [ ] 2.2 Initialize ZoneComposer in Main
  - **Input:** `main.cpp`.
  - **Output:** Initialized `ZoneComposer` instance.
  - **Ownership:** App (Main).
  - **State Change:** SL-2 (ZoneComposer created).
  - **Implements:** PRD FR-3, Blueprint §4.2

- [ ] 2.3 Inject ZoneComposer into Router
  - **Input:** `WsMessageRouter::init()`.
  - **Output:** Router holds pointer to ZoneComposer.
  - **Implements:** Blueprint §4.2

- [ ] 2.4 Implement `zones.update` Handler
  - **Input:** `zones.update` message.
  - **Output:** Calls `zoneComposer->setZone()`.
  - **Integration:** Router -> ZoneComposer.
  - **Implements:** PRD FR-3

**Post-condition:** Zone commands trigger `ZoneComposer` logic.

**Verification:**
- [ ] Send `zones.update` -> Zone state updates in RAM.

---

### 3.0 Phase 3: Persistence (NVS)

**Pre-condition:** Zones work in RAM but reset on reboot.

#### Sub-Tasks:

- [ ] 3.1 Review context: PRD §8.2 and Blueprint §6.1
  - **Relevant Sections:** NVS Schema, Lifecycle.

- [ ] 3.2 Implement NVS Load/Save
  - **Input:** `ZoneConfig` struct.
  - **Output:** NVS binary blob.
  - **State Change:** SL-1 (ZoneConfig persists).
  - **Implements:** PRD FR-9

- [ ] 3.3 Implement Disconnect Logic
  - **Input:** WebSocket Disconnect event.
  - **Output:** No action (state retained).
  - **User Sees:** Lights stay on.
  - **Implements:** PRD FR-5

- [ ] 3.4 Implement Debounce & CRC Policy
  - **Input:** Rapid `saveToNVS()` calls.
  - **Output:** Single write after 500ms stability + CRC check.
  - **Implements:** PRD FR-9 (Hardened)

**Post-condition:** Zone state survives reboot and disconnect.

**Verification:**
- [ ] Change zones -> Reboot -> Zones restore.

---

### 4.0 Phase 4: Resilience (Error Handling)

**Pre-condition:** Functional but silent on errors.

#### Sub-Tasks:

- [ ] 4.1 Review context: PRD §4.2 and Blueprint §7.1
  - **Relevant Sections:** Error JSON format.

- [ ] 4.2 Implement Rate Limited Logging
  - **Input:** Stream of unknown messages.
  - **Output:** 1 log every 2s per type.
  - **Implements:** PRD NFR-1

- [ ] 4.3 Implement Strict Error Response
  - **Input:** Invalid message.
  - **Output:** JSON Error (`code`, `detail`, `reqId`).
  - **User Sees:** Error feedback.
  - **Implements:** PRD FR-4, Blueprint §7.2

**Post-condition:** Robust error reporting.

**Verification:**
- [ ] Send garbage JSON -> Receive structured error message.

---

### 5.0 Phase 5: Verification & Oracle Capture

**Pre-condition:** All features implemented.

#### Sub-Tasks:

- [ ] 5.1 Legacy Oracle Capture
  - **Input:** Legacy Hub.
  - **Output:** `oracle_*.jsonl` trace file.
  - **Implements:** PRD Appendix B

- [ ] 5.2 Golden Trace Replay
  - **Input:** `oracle_*.jsonl` trace file.
  - **Output:** Verified state changes (Exact Match).
  - **Implements:** PRD M-1

- [ ] 5.3 Reboot Cycle Test
  - **Input:** Power cycle.
  - **Output:** Verified persistence.
  - **Implements:** PRD M-2

---

### 6.0 Phase 6: Deprecation Gate

**Pre-condition:** All tests pass in Tolerant mode.

#### Sub-Tasks:

- [ ] 6.1 Run Golden Trace in STRICT Mode
  - **Input:** Oracle traces.
  - **Output:** PASS/FAIL (Payload hashes match).
  - **Implements:** PRD Appendix B.2

- [ ] 6.2 Execute 10 Reconnect Cycles
  - **Input:** Physical reconnects.
  - **Output:** 0 Divergences.
  - **Implements:** PRD M-1

- [ ] 6.3 Cleanup (Optional)
  - **Input:** Passed strict tests.
  - **Action:** Remove legacy aliases (ONLY if approved).
  - **Decision:** **KEEP aliases** for this release (Strangler Fig).

---

## Integration-Critical Tasks
*Source: Blueprint §4 - Integration Wiring*

### IC-1: Message Routing Pipeline
*Maps to: Blueprint §4.1*

**Critical Sequence:**
```
1. Validate JSON & Keys (type/t)  // REQUIRED: PRD FR-1
2. Normalize Key & Alias Command   // REQUIRED: PRD FR-2
3. Dispatch to Handler             // REQUIRED: Routing logic
```

**Ownership Rules (from PRD §7):**
- Router parses, Handler acts. Router does NOT manage state.

**User Visibility (from PRD §6):**
- User sees: Effect/Zone changes (Success) or Error JSON (Failure).

**State Changes (from Blueprint §3):**
- Before: Raw JSON.
- After: State update or Error.

**Common Mistakes to Avoid:**
- ❌ Hardcoding `type` string (Breaks legacy `t`).
- ❌ Missing aliasing (Breaks `effects.setCurrent`).

**Verification:**
- [ ] Test with both `t` and `type`.

---

### IC-2: Zone Initialization
*Maps to: Blueprint §4.2*

**Critical Sequence:**
```
1. ZoneComposer::begin() (Load NVS) // REQUIRED: Restore state before network
2. WsMessageRouter::init(..., zoneComposer) // REQUIRED: Inject dependency
```

**Ownership Rules (from PRD §7):**
- `main.cpp` creates ZoneComposer. Router only receives pointer.

**Common Mistakes to Avoid:**
- ❌ Creating ZoneComposer inside Router (Lifecycle violation).

**Verification:**
- [ ] Verify logs show ZoneComposer init BEFORE Router init.

---

## Validation Checklist

Before implementation, verify 1:1 mapping is complete:

### PRD Coverage
- [ ] Every §5 acceptance criterion has a corresponding subtask
- [ ] Every §6 visibility rule has "User Sees" in relevant subtask
- [ ] Every §7 ownership rule is in Quick Reference AND relevant subtask "Ownership" field
- [ ] Every §8 state requirement has "State Change" in relevant subtask

### Blueprint Coverage
- [ ] Every §2 boundary rule is in Critical Boundaries AND enforced in tasks
- [ ] Every §3 state transition maps to Pre/Post conditions
- [ ] Every §4 integration wiring maps to an Integration-Critical Task

### Task Quality
- [ ] First subtask of each parent references relevant docs
- [ ] All subtasks are specific and actionable (not vague)
- [ ] All "Implements" fields trace back to PRD/Blueprint sections
